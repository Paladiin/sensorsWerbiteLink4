#!/usr/bin/env lua
socket = require "ssocket"
tcpsock = socket.tcp()
ok, err = tcpsock:connect("www.google.com", 80)
if err then
  print(err)
end
tcpsock:write("GET / HTTP/1.1\r\n")
tcpsock:write("\r\n")
tcpsock