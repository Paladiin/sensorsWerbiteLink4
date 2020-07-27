#!/usr/bin/env lua
socket = require "ssocket"
tcpsock = socket.tcp()
ok, err = tcpsock:connect("www.google.com", 80)
if err then
  print