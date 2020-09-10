local socket = require "ssocket"

local sock = socket.tcp()
local ok, err = sock:connect('www.baidu.com', 80)
sock:write("GET /robots.txt HT