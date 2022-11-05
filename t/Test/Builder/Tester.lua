
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
local testing = false
local testing_num

-- remembering where the file handles were originally connected
local original_output_handle
local original_failure_handle
local original_todo_handle

local function _start_testing ()
    -- remember what the handles were set to
    original_output_handle  = tb:output()
    original_failure_handle = tb:failure_output()
    original_todo_handle    = tb:todo_output()

    -- switch out to our own handles
    tb:output(out)
    tb:failure_output(err)
    tb:todo_output(err)

    -- clear the expected list
    out:reset()
    err:reset()

    -- remeber that we're testing
    testing = true
    testing_num = tb:current_test()
    tb:current_test(0)

    -- look, we shouldn't do the ending stuff
    tb.no_ending = true
end

function m.test_out (...)
    if not testing then
        _start_testing()
    end
    out:expect(...)
end

function m.test_err (...)
    if not testing then
        _start_testing()
    end
    err:expect(...)
end

function m.test_fail (offset)
    offset = offset or 0
    if not testing then
        _start_testing()
    end
    local info = debug.getinfo(2)
    local prog = info.short_src
    local lin