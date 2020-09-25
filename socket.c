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
 