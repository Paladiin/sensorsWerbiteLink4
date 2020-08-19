#!/usr/bin/env lua
local socket = require "ssocket"

HOST = '127.0.0.1'
PORT = 12345
tcpsock = socket.tcp()
local ok, err = tcpsock:bind(HOST, PORT)
if err then
  print(err)
  