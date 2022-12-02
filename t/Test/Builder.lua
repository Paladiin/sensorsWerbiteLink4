
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
    local arg = {...}
    for k, v in pairs(arg) do
        arg[k] = tostring(v)
    end
    local msg = table.concat(arg)
    msg = msg:gsub("\n", "\n# ")
    msg = msg:gsub("\n# \n", "\n#\n")
    msg = msg:gsub("\n# $", '')
    _print_to_fh(self, f, "# ", msg)
end

function m.create ()
    local o = {
        data = setmetatable({}, { __index = m }),
    }
    setmetatable(o, {
        __index = function (t, k)
                        return rawget(t, 'data')[k]
                  end,
        __newindex = function (t, k, v)
                        rawget(o, 'data')[k] = v
                  end,
    })
    o:reset()
    return o
end

local test
function m.new ()
    test = test or m.create()
    return test
end

local function in_todo (self)
    return self.todo_upto >= self.curr_test
end

function m:child (name)
    if self.child_name then
        error("You already have a child named (" .. self.child_name .. " running")
    end
    local child = m.create()
    child.indent    = self.indent .. '    '
    child.out_file  = self.out_file
    child.fail_file = in_todo(self) and self.todo_file or self.fail_file
    child.todo_file = self.todo_file
    child.parent    = self
    self.child_name = name
    return child
end

local function plan_handled (self)
    return self.have_plan or self.no_plan or self._skip_all
end

function m:subtest (name, func)
    if type(func) ~= 'function' then
        error("subtest()'s second argument must be a function")
    end
    local child = self:child(name)
    local parent = self.data
    self.data = child.data
    local r, msg = pcall(func)
    child.data = self.data
    self.data = parent
    if not r and not child._skip_all then
        error(msg, 0)
    end
    if not plan_handled(child) then
        child:done_testing()
    end
    child:finalize()
end

function m:finalize ()
    if not self.parent then
        return
    end
    if self.child_name then
        error("Can't call finalize() with child (" .. self.child_name .. " active")
    end
    local parent = self.parent
    local name = parent.child_name
    parent.child_name = nil
    if self._skip_all then
        parent:skip(self._skip_all)
    elseif self.curr_test == 0 then
        parent:ok(false, "No tests run for subtest \"" .. name .. "\"", 2)
    else
        parent:ok(self.is_passing, name, 2)
    end
    self.parent = nil
end

function m:reset ()
    self.curr_test = 0
    self._done_testing = false
    self.expected_tests = 0
    self.is_passing = true
    self.todo_upto = -1
    self.todo_reason = nil
    self.have_plan = false
    self.no_plan = false
    self._skip_all = false
    self.have_output_plan = false
    self.indent = ''
    self.parent = false
    self.child_name = false
    self:reset_outputs()
end

local function _output_plan (self, max, directive, reason)
    if self.have_output_plan then
        error("The plan was already output")
    end
    local out = "1.." .. max
    if directive then
        out = out .. " # " .. directive
    end
    if reason then
        out = out .. " " .. reason
    end
    _print(self, out)
    self.have_output_plan = true
end

function m:plan (arg)
    if self.have_plan then
        error("You tried to plan twice")
    end
    if type(arg) == 'string' and arg == 'no_plan' then
        self.have_plan = true
        self.no_plan = true
        return true
    elseif type(arg) ~= 'number' then
        error("Need a number of tests")
    elseif arg < 0 then
        error("Number of tests must be a positive integer.  You gave it '" .. arg .."'.")
    else
        self.expected_tests = arg
        self.have_plan = true
        _output_plan(self, arg)
        return arg
    end
end

function m:done_testing (num_tests)
    if num_tests then
        self.no_plan = false
    end
    num_tests = num_tests or self.curr_test
    if self._done_testing then
        tb:ok(false, "done_testing() was already called")
        return
    end
    self._done_testing = true
    if self.expected_tests > 0 and num_tests ~= self.expected_tests then
        self:ok(false, "planned to run " .. self.expected_tests
                    .. " bu