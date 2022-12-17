

--
-- lua-TestMore : <http://fperrad.github.com/lua-TestMore/>
--

local loadstring = loadstring or load
local pairs = pairs
local pcall = pcall
local require = require
local tostring = tostring
local type = type
local unpack = require 'table'.unpack or unpack
local _G = _G

local tb = require 'Test.Builder'.new()

_ENV = nil
local m = {}

function m.plan (arg)
    tb:plan(arg)
end

function m.done_testing (num_tests)
    tb:done_testing(num_tests)
end

function m.skip_all (reason)
    tb:skip_all(reason)
end

function m.BAIL_OUT (reason)
    tb:BAIL_OUT(reason)
end

function m.ok (test, name)
    tb:ok(test, name)
end

function m.nok (test, name)
    tb:ok(not test, name)
end

function m.is (got, expected, name)
    local pass = got == expected
    tb:ok(pass, name)
    if not pass then
        tb:diag("         got: " .. tostring(got)
           .. "\n    expected: " .. tostring(expected))
    end
end

function m.isnt (got, expected, name)
    local pass = got ~= expected
    tb:ok(pass, name)
    if not pass then
        tb:diag("         got: " .. tostring(got)
           .. "\n    expected: anything else")
    end
end

function m.like (got, pattern, name)
    if type(pattern) ~= 'string' then
        tb:ok(false, name)
        tb:diag("pattern isn't a string : " .. tostring(pattern))
        return
    end
    got = tostring(got)
    local pass = got:match(pattern)
    tb:ok(pass, name)
    if not pass then
        tb:diag("                  '" .. got .. "'"
           .. "\n    doesn't match '" .. pattern .. "'")
    end
end

function m.unlike (got, pattern, name)
    if type(pattern) ~= 'string' then
        tb:ok(false, name)
        tb:diag("pattern isn't a string : " .. tostring(pattern))
        return
    end
    got = tostring(got)
    local pass = not got:match(pattern)
    tb:ok(pass, name)
    if not pass then
        tb:diag("                  '" .. got .. "'"
           .. "\n          matches '" .. pattern .. "'")
    end
end

local cmp = {
    ['<']  = function (a, b) return a <  b end,
    ['<='] = function (a, b) return a <= b end,
    ['>']  = function (a, b) return a >  b end,
    ['>='] = function (a, b) return a >= b end,
    ['=='] = function (a, b) return a == b end,
    ['~='] = function (a, b) return a ~= b end,
}

function m.cmp_ok (this, op, that, name)
    local f = cmp[op]
    if not f then
        tb:ok(false, name)
        tb:diag("unknown operator : " .. tostring(op))
        return
    end
    local pass = f(this, that)
    tb:ok(pass, name)
    if not pass then
        tb:diag("    " .. tostring(this)
           .. "\n        " .. op
           .. "\n    " .. tostring(that))
    end
end
