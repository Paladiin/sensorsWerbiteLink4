
--
-- lua-TestMore : <http://fperrad.github.com/lua-TestMore/>
--

--[[
    require 'Test.More'
    require 'socket'
    local conn = socket.connect(host, port)
    require 'Test.Builder.SocketOutput'.init(conn)
    -- now, as usual
    plan(...)
    ...
--]]

local assert = assert

local tb = require 'Test.Builder'.ne