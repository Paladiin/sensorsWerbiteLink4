local socket = require "ssocket"

local sock = socket.tcp()
local ok, err = sock:connect('www.baidu.com', 80)
sock:write("GET /robots.txt HTTP/1.1\r\n")
sock:write("Host: www.baidu.com\r\n")
sock:write("Connection: Close\r\n")
sock:write("\r\n")
reader, err = sock:readuntil("\r\n", false)
slices = {}
while true do
  local data, err, partial = reader()
  if not data then
    if err then
      print(s