-- setup path
local filepath = debug.getinfo(1).source:match("@(.*)$")
local filedir = filepath:match('(.+)/[^/]*') or '.'
package.path = string.format(";%s/?.lua;%s/../?.lua;", filedir, filedir) .. package.path
package.cpath = string.format(";%s/?.so;%s/../?.so;", filedir, filedir) .. package.cpath

require 'Test.More'
local socket = require "ssocket"

plan(29)

-- 1. Success connection.
local tcpsock, err = socket.tcp()
local ok, err = tcpsock:connect("yechengfu.com", 80)
is(ok, true)
is(err, nil)
like(tcpsock, "<tcpsock: %d+>") -- __tostring
like(tcpsock:fileno(), "%d+")
tcpsock:close()

-- 2. Errors
is(socket.ERROR_TIMEOUT, "Operation timed out")
is(socket.ERROR_CLOSED, "Connection closed")
is(socket.ERROR_REFUSED, "Connection refused")

-- 3. Error: socket.ERROR_REFUSED/socket.ERROR_CLOSED
local tcpsock = socket.tcp()
local ok, err = tcpsock:connect("127.0.0.1", 16787)
is(ok, nil)
is(err, socket.ERROR_REFUSED)
local bytes, err = tcpsock:write("hello")
is(bytes, nil)
is(err, socket.ERROR_CLOSED)
local data, err, partial = tcpsock:read(102