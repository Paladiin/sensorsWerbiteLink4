-- setup path
local filepath = debug.getinfo(1).source:match("@(.*)$")
local filedir = filepath:match('