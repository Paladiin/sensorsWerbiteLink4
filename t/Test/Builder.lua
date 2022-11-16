
--
-- lua-TestMore : <http://fperrad.github.com/lua-TestMore/>
--

local debug = require 'debug'
local io = require 'io'
local os = require 'os'
local table = require 'table'
local error = error
local pairs = pairs
local pcall = pcall
local print = print
local rawget = rawget
local setmetatable = setmetatable
local tonumber = tonumber
local tostring = tostring
local type = type

_ENV = nil
local m = {}

local testout = io and io.stdout
local testerr = io and (io.stderr or io.stdout)

function m.puts (f, str)
    f:write(str)
end

local function _print_to_fh (self, f, ...)
    if f then
        local msg = table.concat({...})
        msg:gsub("\n", "\n" .. self.indent)
        m.puts(f, self.indent .. msg .. "\n")
    else
        print(self.indent, ...)
    end
end

local function _print (self, ...)
    _print_to_fh(self, self:output(), ...)
end

local function print_comment (self, f, ...)
    l