#include "compat.h"

#include <lua.h>
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include "timeout.h"
#include "buffer.h"

#define _VERSION "0.0.1"

#define TCPSOCK_TYPENAME     "TCPSOCKET*"
#define UDPSOCK_TYPENAME     "UDPSOCKET*"

/* Socket address */
typedef union {
  struct sockaddr sa;
  struct sockaddr_in in;
  struct sockaddr_un un;
} sockaddr_t;

/* Convert "sockaddr_t" to "struct sockaddr *". */
#define SAS2SA(x) (&((x)->sa))

/* Socket Object */
struct sockobj {
    int fd;
    int sock_family;
    double sock_timeout;        /* in seconds */
    struct buffer *buf;         /* used for buffer reading */
};

#define getsockobj(L) ((struct sockobj *)lua_touserdata(L, 1));
#define CHECK_ERRNO(expected)   (errno == expected)

/* Custom socket error strings */
#define ERROR_TIMEOUT   "Operation timed out"
#define ERROR_CLOSED    "Connection closed"
#define ERROR_REFUSED   "Connection refused"

/* Options */
#define OPT_TCP_NODELAY   "tcp_nodelay"
#define OPT_TCP_KEEPALIVE "tcp_keepalive"
#define OPT_TCP_REUSEADDR "tcp_reuseaddr"

#define RECV_BUFSIZE 8192

/**
 * Function to perform the setting of socket blocking mode.
 */
static void
__setblocking(int fd, int block)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (block) {
        flags &= (~O_NONBLOCK);
    } else {
        flags |= O_NONBLOCK;
    }
    fcntl(fd, F_SETFL, flags);
}

/**
 * Do a event polling on the socket, if necessary (sock_timeout > 0).
 *
 * Returns:
 *  1   on timeout
 *  -1  on error
 *  0   success
 */
#define EVENT_NONE      0
#define EVENT_READABLE  POLLIN
#define EVENT_WRITABLE  POLLOUT
#define EVENT_ANY       (POLLIN | POLLOUT)
static int
__waitfd(struct sockobj *s, int event, struct timeout *tm)
{
    int ret;

    // Nothing to do if socket is closed.
    if (s->fd < 0)
        return 0;

    struct pollfd pollfd;
    pollfd.fd = s->fd;
    pollfd.events = event;

    do {
        // Handling this condition here simplifies the loops.
        double left = timeout_left(tm);
        if (left == 0.0)
            return 1;
        int timeout = (int)(left * 1e3);
        ret = poll(&pollfd, 1, timeout >= 0 ? timeout : -1);
    } while (ret == -1 && CHECK_ERRNO(EINTR));

    if (ret < 0) {
        return -1;
    } else if (ret == 0) {
        return 1;
    } else {
        return 0;
    }
}

int
__select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * errorfds,
         struct timeout *tm)
{
    int ret;
    do {
        struct timeval tv = { 0, 0 };
        double t = timeout_left(tm);
        if (t >= 0) {
            tv.tv_sec = (int)t;
            tv.tv_usec = (int)((t - tv.tv_sec) * 1.0e6);
        }

        ret = select(nfds, readfds, writefds, errorfds, (t >= 0) ? &tv : NULL);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

/**
 * Get the address length according to the socket object's address family.
 * Return 1 if the family is known, 0 otherwise. The length is returned through
 * len_ret.
 */
static int
__getsockaddrlen(struct sockobj *s, socklen_t * len_ret)
{
    switch (s->sock_family) {
    case AF_UNIX:
        *len_ret = sizeof(struct sockaddr_un);
        return 1;
    case AF_INET:
        *len_ret = sizeof(struct sockaddr_in);
        return 1;
    case AF_INET6:
        *len_ret = sizeof(struct sockaddr_in6);
        return 1;
    default:
        return 0;
    }
}

/* 
 * Convert a string specifying a host name or one of a few symbolic names to a
 * numeric IP address.
 */
static int
__sockobj_setipaddr(lua_State *L, const char *name, struct sockaddr *addr_ret, size_t addr_ret_size, int af)
{
    struct addrinfo hints, *res;
    int err;
    int d1, d2, d3, d4;
    char ch;
    memset((void *)addr_ret, 0, addr_ret_size);
    
    if (sscanf(name, "%d.%d.%d.%d%c", &d1, &d2, &d3, &d4, &ch) == 4
        && 0 <= d1 && d1 <= 255
        && 0 <= d2 && d2 <= 255
        && 0 <= d3 && d3 <= 255
        && 0 <= d4 && d4 <= 255) {
        struct sockaddr_in *sin;
        sin = (struct sockaddr_in *)addr_ret;
        sin->sin_addr.s_addr = htonl(((long)d1 << 24) | ((long)d2 << 16) | ((long)d3 << 8) | ((long)d4 << 0));
        sin->sin_family = AF_INET;
        return 0;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    err = getaddrinfo(name, NULL, &hints, &res);
    if (err) {
        lua_pushnil(L);
        lua_pushstring(L, gai_strerror(errno));
        return -1;
    }
    if (res->ai_addrlen < addr_ret_size)
        addr_ret_size = res->ai_addrlen;
    memcpy((char *)addr_ret, res->ai_addr, addr_ret_size);
    freeaddrinfo(res);
    return 0;
}

/**
 * Parse socket address arguments.
 *
 * Socket addresses are represented as follows:

 *  - A single string is used for the AF_UNIX address family.
 *  - Two arguments (host, port) is used for the AF_INET address family,
 *    where host is a string representing either a hostname in Internet Domain
 *    Notation like 'www.example.com' or an IPv4 address like '8.8.8.8', and port
 *    is an number.

 * If you use a hostname in the host portion of IPv4/IPv6 socket address, the
 * program may show a nondeterministic behavior, as we use the first address
 * returned from the DNS resolution. The socket address will be resolved
 * differently into an actual IPv4/v6 address, depending on the results from DNS
 * resolution and/or the host configuration. For deterministic behavior use a
 * numeric address in host portion.
 *
 * This method assumed that address arguments start after offset index.
 *
 * Returns 0 on success, -1 on failure.
 */
static int
__sockobj_getaddrfromarg(lua_State * L, struct sockobj *s, struct sockaddr *addr_ret,
                     socklen_t * len_ret, int offset)
{
    int n;
    n = lua_gettop(L);
    if (n != 1 + offset && n != 2 + offset) {
        lua_pushnil(L);
        lua_pushfstring(L, "expecting %d or %d arguments"
        " (including the object itself), but seen %d", offset + 1, offset + 2, n);
        return -1;
    }

    if (n == 2 + offset) {
        s->sock_family = AF_INET;
    } else if (n == 1 + offset) {
        s->sock_family = AF_UNIX;
    }
    if (s->sock_family == AF_INET) {
        struct sockaddr_in *addr = (struct sockaddr_in *)addr_ret;
        const char *host;
        int port;
        host = luaL_checkstring(L, 1 + offset);
        port = luaL_checknumber(L, 2 + offset);
        if (__sockobj_setipaddr(L, host, (struct sockaddr *)addr, sizeof(*addr), AF_INET) != 0) {
            return -1;
        }
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        *len_ret = sizeof(*addr);
    } else if (s->sock_family == AF_UNIX) {
        struct sockaddr_un *addr = (struct sockaddr_un *)addr_ret;
        const char *path = luaL_checkstring(L, 1 + offset);
        addr->sun_family = AF_UNIX;
        strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);
        *len_ret = sizeof(*addr);
    } else {
        assert(0);
    }
    return 0;
}

/**
 * Create a table, push address info into it.
 *
 * The family field os the socket object is inspected to determine what kind of
 * address it really is.
 *
 * In case of success, a table associated with address info pushed on the stack;
 * In case of error, a nil value with a string describing the error pushed on
 * the stack.
 */
static int
__sockobj_makeaddr(lua_State * L, struct sockobj *s, struct sockaddr *addr,
               socklen_t addrlen)
{
    lua_newtable(L);

    switch (addr->sa_family) {
    case AF_INET:
        {
            struct sockaddr_in *a = (struct sockaddr_in *)addr;
            char buf[NI_MAXHOST];
            int err = getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0,
                                  NI_NUMERICHOST);
            if (err) {
                err = errno;
                lua_pushnil(L);
                lua_pushstring(L, gai_strerror(errno));
                return -1;
            }
            lua_pushnumber(L, 1);
            lua_pushstring(L, buf);
            lua_settable(L, -3);
            lua_pushnumber(L, 2);
            lua_pushnumber(L, ntohs(a->sin_port));
            lua_settable(L, -3);
            return 0;
        }
    case AF_UNIX:
        {
            struct sockaddr_un *a = (struct sockaddr_un *)addr;
#ifdef linux
            if (a->sun_path[0] == 0) { /* Linux abstract namespace */
                addrlen -= offset(struct sockaddr_un, sun_path);
                lua_pushlstring(L, a->sun_path, addrlen);
            } else
#endif
            {
                /* regular NULL-terminated string */
                lua_pushstring(L, a->sun_path);
            }
            return 0;
        }
    default:
        /* If we don't know the address family, return it as an {int, bytes}
         * table. */
        lua_pushnumber(L, 1);
        lua_pushnumber(L, addr->sa_family);
        lua_settable(L, -3);
        lua_pushnumber(L, 2);
        lua_pushstring(L, addr->sa_data);
        lua_settable(L, -3);
        return 0;
    }
}

/**
 * Generic socket object creation.
 */
struct sockobj *
__sockobj_create(lua_State *L, const char *tname)
{
    struct sockobj *s =
        (struct sockobj *)lua_newuserdata(L, sizeof(struct sockobj));
    if (!s) {
        return NULL;
    }
    s->fd = -1;
    s->sock_timeout = -1;
    s->sock_family = 0;
    s->buf = NULL;
    luaL_setmetatable(L, tname);
    return s;
}

/**
 * Generic socket fd creation.
 */
static int
__sockobj_createsocket(lua_State *L, struct sockobj *s, int type)
{
    int fd;
    assert(s->fd == -1);

    if ((fd = socket(s->sock_family, type, 0)) == -1) {
        lua_pushnil(L);
        lua_pushfstring(L, "failed to create socket: %s", strerror(errno));
        return -1;
    }
    s->fd = fd;

    // 100% non-blocking
    __setblocking(s->fd, 0);

    return 0;
}

/**
 * Close associated socket and buffers.
 */
static int
__sockobj_close(lua_State *L, struct sockobj *s)
{
    if (s->fd != -1) {
        if (close(s->fd) != 0) {
            lua_pushnil(L);
            lua_pushstring(L, strerror(errno));
            return -1;
        }
        s->fd = -1;
    }
    if (s->buf) {
        buffer_delete(s->buf);
        s->buf = NULL;
    }
    return 0;
}

/**
 * Generic socket connection.
 */
static int
__sockobj_connect(lua_State *L, struct sockobj *s, struct sockaddr *addr, socklen_t len)
{
    int ret;
    char *errstr = NULL;
    struct timeout tm;
    timeout_init(&tm, s->sock_timeout);
    assert(s->fd > 0);

    errno = 0;
    ret = connect(s->fd, addr, len);

    if (CHECK_ERRNO(EINPROGRESS)) {
        /* Connecting in progress with timeout, wait until we have the result of
         * the connection attempt or timeout.
         */
        int timeout = __waitfd(s, EVENT_WRITABLE, &tm);
        if (timeout == 1) {
            errstr = ERROR_TIMEOUT;
            goto err;
        } else if (timeout == 0) {
            // In case of EINPROGRESS, use getsockopt(SO_ERROR) to get the real
            // error, when the connection attempt finished.
            socklen_t ret_size = sizeof(ret);
            getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &ret, &ret_size);
            if (ret == EISCONN) {
                errno = 0;
            } else {
                errno = ret;
            }
        } else {
            errstr = strerror(errno);
            goto err;
        }
    }

    if (errno) {
        errstr = strerror(errno);
        goto err;
    }
    return 0;

err:
    assert(errstr);
    __sockobj_close(L, s);
    lua_pushnil(L);
    lua_pushstring(L, errstr);
    return -1;
}

static int
__sockobj_send(lua_State *L, struct sockobj *s, const char *buf, size_t len, size_t *sent, struct timeout *tm) {
    char *errstr;
    if (s->fd == -1) {
        errstr = ERROR_CLOSED;
        goto err;
    }

    while (1) {
        int timeout = __waitfd(s, EVENT_WRITABLE, tm);
        if (timeout == -1) {
            errstr = strerror(errno);
            goto err;
        } else if (timeout == 1) {
            errstr = ERROR_TIMEOUT;
            goto err;
        } else {
            int n = send(s->fd, buf, len, 0);
            if (n < 0) {
                switch (errno) {
                case EINTR:
                case EAGAIN:
                    continue;
                case EPIPE:
                    // EPIPE means the connection was closed.
                    errstr = ERROR_CLOSED;
                    goto err;
                default:
                    errstr = strerror(errno);
                    goto err;
                }
            } else {
                *sent = n;
                return 0;
            }
        }
    }

err:
    assert(errstr);
    lua_pushnil(L);
    lua_pushstring(L, errstr);
    return -1;
}

static int
__sockobj_sendto(lua_State *L, struct sockobj *s, const char *buf, size_t len, size_t *sent, struct sockaddr *addr, socklen_t addrlen, struct timeout *tm) {
    char *errstr;
    if (s->fd == -1) {
        errstr = ERROR_CLOSED;
        goto err;
    }

    while (1) {
        int timeout = __waitfd(s, EVENT_WRITABLE, tm);
        if (timeout == -1) {
            errstr = strerror(errno);
            goto err;
        } else if (timeout == 1) {
            errstr = ERROR_TIMEOUT;
            goto err;
        } else {
            int n = sendto(s->fd, buf, len, 0, addr, addrlen);
            if (n < 0) {
                switch (errno) {
                case EINTR:
                case EAGAIN:
                    continue;
                case EPIPE:
                    // EPIPE means the connection was closed.
                    errstr = ERROR_CLOSED;
                    goto err;
                default:
                    errstr = strerror(errno);
                    goto err;
                }
            } else {
                *sent = n;
                return 0;
            }
        }
    }

err:
    assert(errstr);
    lua_pushnil(L);
    lua_pushstring(L, errstr);
    return -1;
}

static int
__sockobj_write(lua_State *L, struct sockobj *s, const char *buf, size_t len) {
    char *errstr;
    size_t total_sent = 0;
    if (s->fd == -1) {
        errstr = ERROR_CLOSED;
        goto err;
    }

    struct timeout tm;
    timeout_init(&tm, s->sock_timeout);
    while (1) {
        int timeout = __waitfd(s, EVENT_WRITABLE, &tm);
        if (timeout == -1) {
            errstr = strerror(errno);
            goto err;
        } else if (timeout == 1) {
            errstr = ERROR_TIMEOUT;
            goto err;
        } else {
            int n = send(s->fd, buf + total_sent, len - total_sent, 0);
            if (n < 0) {
                switch (errno) {
                case EINTR:
                case EAGAIN:
                    continue;
                case EPIPE:
                    // EPIPE means the connection was closed.
                    errstr = ERROR_CLOSED;
                    goto err;
                default:
                    errstr = strerror(errno);
                    goto err;
                }
            } else {
                total_sent += n;
                if (len - total_sent <= 0) {
                    break;
                }
            }
        }
    }

    assert(total_sent == len);
    lua_pushinteger(L, total_sent);
    return 0;

err:
    assert(errstr);
    lua_pushnil(L);
    lua_pushstring(L, errstr);
    return -1;
}

static int
__sockobj_recv(lua_State *L, struct sockobj *s, char *buf, size_t buffersize, size_t *received, struct timeout *tm)
{
    char *errstr = NULL;

    if (s->fd == -1) {
        errstr = ERROR_CLOSED;
        goto err;
    }

    while (1) {
        int timeout = __waitfd(s, EVENT_READABLE, tm);
        if (timeout == -1) {
            errstr = strerror(errno);
            goto err;
        } else if (timeout == 1) {
            errstr = ERROR_TIMEOUT;
            goto err;
        } else {
            int bytes_read = recv(s->fd, buf, buffersize, 0);
            if (bytes_read > 0) {
                *received = bytes_read;
                return 0;
            } else if (bytes_read == 0) {
                errstr = ERROR_CLOSED;
                goto err;
            } else {
                switch (errno) {
                case EINTR:
                case EAGAIN:
                    // do nothing, continue
                    continue;
                default:
                    errstr = strerror(errno);
                    goto err;
                }
            }
        }
    }

err:
    assert(errstr);
    lua_pushnil(L);
    lua_pushstring(L, errstr);
    return -1;
}

static int
__sockobj_recvfrom(lua_State *L, struct sockobj *s, char *buf, size_t buffersize, size_t *received, struct sockaddr *addr, socklen_t *addrlen, struct timeout *tm)
{
    char *errstr = NULL;

    if (s->fd == -1) {
        errstr = ERROR_CLOSED;
        goto err;
    }

    while (1) {
        int timeout = __waitfd(s, EVENT_READABLE, tm);
        if (timeout == -1) {
            errstr = strerror(errno);
            goto err;
        } else if (timeout == 1) {
            errstr = ERROR_TIMEOUT;
            goto err;
        } else {
            int bytes_read = recvfrom(s->fd, buf, buffersize, 0, addr, addrlen);
            if (bytes_read > 0) {
                *received = bytes_read;
                return 0;
            } else if (bytes_read == 0) {
                errstr = ERROR_CLOSED;
                goto err;
            } else {
                switch (errno) {
                case EINTR:
                case EAGAIN:
                    // do nothing, continue
                    continue;
                default:
                    errstr = strerror(errno);
                    goto err;
                }
            }
        }
    }

err:
    assert(errstr);
    lua_pushnil(L);
    lua_pushstring(L, errstr);
    return -1;
}
/**
 * tcpsock, err = socket.tcp()
 */
static int
socket_tcp(lua_State * L)
{
    struct sockobj *s = __sockobj_create(L, TCPSOCK_TYPENAME);
    if (!s) {
        return luaL_error(L, "out of memory");
    }
    return 1;
}

/**
 * udpsock, err = socket.udp()
 */
static int
socket_udp(lua_State * L)
{
    struct sockobj *s = __sockobj_create(L, UDPSOCK_TYPENAME);
    if (!s) {
        return luaL_error(L, "out of memory");
    }
    return 1;
}

static void
__collect_fds(lua_State * L, int tab, fd_set * set, int *max_fd)
{
    if (lua_isnil(L, tab))
        return;

    luaL_checktype(L, tab, LUA_TTABLE);

    int i = 1;
    while (1) {
        int fd = -1;
        lua_pushnumber(L, i);
        lua_gettable(L, tab);   // get ith fd

        if (lua_isnil(L, -1)) {
            // end of table loop
            lua_pop(L, 1);
            break;
        }

        if (lua_isnumber(L, -1)) {
            fd = lua_tonumber(L, -1);
            if (fd < 0) {
                fd = -1;
            }
        } else {
            // ignore
            lua_pop(L, 1);
            continue;
        }

        if (fd != -1) {
            if (fd >= FD_SETSIZE) {
                luaL_argerror(L, tab, "descriptor too large for set size");
            }
            if (*max_fd < fd) {
                *max_fd = fd;
            }
            FD_SET(fd, set);
        }

        lua_pop(L, 1);
        i++;
    }
}

static void
__return_fd(lua_State * L, fd_set * set, int max_fd)
{
    int fd;
    int i = 1;
    lua_newtable(L);
    for (fd = 0; fd < max_fd; fd++) {
        if (FD_ISSET(fd, set)) {
            lua_pushnumber(L, i);
            lua_pushnumber(L, fd);
            lua_settable(L, -3);
        }
    }
}

/**
 * readfds, writefds, err = socket.select(readfds, writefds[, timeout=-1])
 *
 *  `readfds`, `writefds` are all table of fds (which was returned from
 *  sockobj:fileno()).
 */
static int
socket_select(lua_State * L)
{
    int max_fd = -1;
    fd_set rset, wset;
    struct timeout tm;
    double timeout = luaL_optnumber(L, 3, -1);
    timeout_init(&tm, timeout);
    FD_ZERO(&rset);
    FD_ZERO(&wset);
    __collect_fds(L, 1, &rset, &max_fd);
    __collect_fds(L, 2, &wset, &max_fd);

    int ret = __select(max_fd + 1, &rset, &wset, NULL, &tm);
    if (ret > 0) {
        __return_fd(L, &rset, max_fd + 1);
        __return_fd(L, &wset, max_fd + 1);
        return 2;
    } else if (ret == 0) {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushstring(L, ERROR_TIMEOUT);
        return 3;
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 3;
    }
}

/*** sock_* methods are common to tcpsocket or udpsocket ***/

/**
 * fd = sockobj:fileno()
 *
 * Return the integer file descriptor of the socket.
 */
static int
sockobj_fileno(lua_State * L)
{
    struct sockobj *s = getsockobj(L);
    lua_pushnumber(L, s->fd);
    return 1;
}

/**
 * ok, err = sockobj:close()
 *
 * Close the socket.
 */
static int
sockobj_close(lua_State * L)
{
    struct sockobj *s = getsockobj(L);

    if (__sockobj_close(L, s) == -1)
        return 2;

    lua_pushboolean(L, 1);
    return 1;
}

static int
sockobj_tostring(lua_State * L)
{
    struct sockobj *s = getsockobj(L);
    assert(lua_getmetatable(L, -1));
    luaL_getmetatable(L, TCPSOCK_TYPENAME);
    if (lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2);
        lua_pushfstring(L, "<tcpsock: %d>", s->fd);
    } else {
        lua_pop(L, 2);
        lua_pushfstring(L, "<udpsock: %d>", s->fd);
    }
    return 1;
}

/**
 * sockobj:settimeout(timeout)
 *
 * Set the timeout in seconds for subsequent socket operations.
 * A negative timeout indicates that timeout is disabled, which is default.
 */
static int
sockobj_settimeout(lua_State * L)
{
    struct sockobj *s = getsockobj(L);
    double timeout = (double)luaL_checknumber(L, 2);
    s->sock_timeout = timeout;
    return 0;
}

/*
 * timeout = sockobj:gettimeout()
 *
 * Returns the timeout in seconds associated with socket.
 * A negative timeout indicates that timeout is disabled, which is default.
 */
static int
sockobj_gettimeout(lua_State * L)
{
    struct sockobj *s = getsockobj(L);
    lua_pushnumber(L, s->sock_timeout);
    return 1;
}

/**
 * ok, err = tcpsock:connect(host, port)