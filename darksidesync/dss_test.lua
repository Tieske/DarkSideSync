
-- LUAEXIT tests

-- darksidesync not loaded first, no delivery possibility
--   call start before requiring darksidesync
-- Expected; nil + error msg
local dss = require("dss")
local luaexit, result, err
luaexit = require('luaexit')
result, err = luaexit.start(function() end)
print(result, err)
assert(result == nil, "nil expected because darksidesync was not loaded yet")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")

local darksidesync = require ("darksidesync")

-- First argument to start must be a function
--   call start without argument
--   call start with a string argument
-- Expected; nil + error msg
result, err = luaexit.start()
print(result, err)
assert(result == nil, "nil expected because no callback function was provided")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")

result, err = luaexit.start("this should fail")
print(result, err)
assert(result == nil, "nil expected because no callback function was provided, but a string")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")


-- no callback set when calling poll
--   1) wait for signal
--   2) call stop
--   3) call poll
-- Expected; nil + error msg

local socket = require ("socket")
local dss = require("dss")
local skt, port = dss.getsocket()
luaexit.start(function(...) print("LuaExit callback received: ", ...) end)
print("Press CTRL+C to initiate a signal now (on windows cannot do this from within SCITE editor! use a terminal)")
skt:receive(8192)   -- wait for UDP packet
luaexit.stop()
result, err = darksidesync.poll()
print(result, err)
assert(result == nil, "nil expected because the callback was no longer available after stopping luaexit")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")





-- DARKSIDESYNC tests

-- GetPort should report the correct port number
print ("checking getport function")
assert(type(darksidesync.getport()) == "number", "Expected a numeric value for the port number")
assert(darksidesync.getport() > 0, "Expected a numeric value greater than 0")
print ("Ok\n")


-- Start with a portnumber <0 or >65535
--   call start with -5
--   call start with 100000
--   call start with no args
--   call start with a string arg
-- Expected; nil + error msg
result, err = darksidesync.start(-5)
print(result, err)
assert(result == nil, "nil expected because a negative port number should not be accepted")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")
result, err = darksidesync.start(100000)
print(result, err)
assert(result == nil, "nil expected because a port number greater than 65535 should not be accepted")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")
result, err = darksidesync.start()
print(result, err)
assert(result == nil, "nil expected because a missing port number should not be accepted")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")
print(result, err)
assert(result == nil, "nil expected because a string argument as port number should not be accepted")
assert(type(err) == "string", "expected 2nd arg to be error string")
print ("Ok\n")

print ("All tests passed!")
