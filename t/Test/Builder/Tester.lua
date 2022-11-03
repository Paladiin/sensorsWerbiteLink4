
--
-- lua-TestMore : <http://fperrad.github.com/lua-TestMore/>
--

local error = error
local pairs = pairs
local type = type
local _G = _G
local debug = require 'debug'

local tb  = require 'Test.Builder'.new()
local out = require 'Test.Builder.Tester.File'.new 'out'
local err = require 'Test.Builder.Tester.File'.new 'err'

_ENV = nil
local m = {}

-- for remembering that we're testing and where we're testing at
local testin