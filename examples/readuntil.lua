local socket = require "ssocket"

local sock = socket.tcp()
local ok, err = sock:connect('www.baidu.com', 80)
sock:write("GET /robots.txt HTTP/1.1\r\n")
sock:write("Host: www.baidu.com\r\n")
sock:write("Connection: Close\r