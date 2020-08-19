#!/usr/bin/env lua
local socket = require "ssocket"

HOST = '127.0.0.1'
PORT = 12345
tcpsock = socket.tcp()
local ok, err = tcpsock:bind(HOST, PORT)
if err then
  print(err)
  os.exit()
end
tcpsock:listen(5)

addr, err = tcpsock:getsockname()
print(string.format("Listening on %s:%d..