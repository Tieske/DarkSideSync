---------------------------------------------------------------------
-- This module contains the DarkSideSync-Lua side notification implementation based
-- on the LuaSocket network library (this module is not required, it is just a quick start if 
-- you will be using LuaSocket with darksidesync). If you intend to use another
-- network library, you need to rewrite/reimplement this code. If you do not want
-- to use the UDP notifications, then this module is not necessary.
-- Upon requiring this Lua module, it will load the additional
-- C libraries (`darksidesync/core.so` or `darksidesync/core.dll`).
-- @class module
-- @name dss
-- @copyright 2012-2013 Thijs Schreijer, DarkSideSync is free software under the MIT/X11 license
-- @release Version 1.0, DarkSideSync.
local socket = require("socket")
local darksidesync = require("darksidesync")
require("coxpcall")
local skt, port

local unpack = unpack or table.unpack  -- 5.1/5.2 compatibility

----------------------------------------------------------------------------------------
-- creates and initializes the UDP socket to be listened on
-- @return a luasocket.udp socket and the port number, or nil and an error message
local createsocket = function()

    local trysocket = function(port)
        local status
        local skt, err = socket.udp()   -- create a new socket
        if not skt then
            -- error occured
            return skt, err
        end
        status, err = skt:setsockname("localhost", port)  -- only listen on LOCALHOST, loopback adapter
        if not status then
            -- error occured
            return nil, err
        end
        return skt  -- return the created socket
    end

    local port = 50000
    local skt
    repeat
        skt = trysocket(port)
        if not skt then port = port + 1 end
    until skt or port > 50200
    if not skt then
        return nil, "Could not configure a UDP socket"
    end
    return skt, port
end

-- default error handler, see dss.seterrorhandler() below
local _ehandler = function(msg)
    if msg then
        msg = tostring(msg) .. "\n" .. debug.traceback("DSS error: callback function had an error;\n")
    else
        msg = debug.traceback("DSS error: callback function had an error;\n")
    end
    print (msg)
end
local ehandler = _ehandler

----------------------------------------------------------------------------------------
-- reads incoming data on the socket, dismisses the data and calls poll()
-- any data returned will have a first argument being the number of items
-- remaining on the queue. And a second being a callback to be called with
-- the remaining arguments
local sockethandler = function(skt)
    -- collect data from socket, can be dismissed, won't be used
    skt:receive(8192)   -- size not optional if using copas, add it to be sure
    -- now call poll() to collect the actual data in a new table with values
    local count, callback, args = darksidesync.poll() 
    if count ~= -1 then
        if type(callback) ~= "function" then
            print (debug.traceback("error: the first argument returned should have been a Lua function!"))
        else
            -- now call the callback with the other arguments as parameters, in a protected (coxpcall) mode
            xpcall(function() callback(unpack(args)) end, ehandler)
        end
    end
end

-- define module table
local dss = {}

----------------------------------------------------------------------------------------
-- Returns a socket where the helper module will be listening for incoming UDP
-- signals that data is ready to be collected through `darksidesync.poll`. When data arrives it
-- MUST be read, and next `darksidesync.poll` should be called (the sockethandler, see `gethandler`, will do this).
-- If no socket was allocated yet, a new socket will be allocated. It will by default
-- listen on `localhost` and try to pick a port number from 50000 and 50200.
-- After allocating the socket, the DarkSideSync (C-side) function `darksidesync.setport`
-- will be called to instruct the synchronization mechanism to send notifications on this port.
-- @return Socket: Existing or newly created UDP socket
-- @return Port: port number the socket is listening on
-- @see gethandler
-- @see darksidesync.poll
-- @see darksidesync.setport
dss.getsocket = function()
    if not skt then
        skt, port = createsocket()
        if skt then
            -- socket was created succesfully, now must tell my C side helper lib on what port
            -- I'm listening for incoming data
            darksidesync.setport(port)
        end
    end
    return skt, port
end

-----------------------------------------------------------------------------------------
-- Returns the socket handler function. This socket handler function will do a single
-- read on the socket to empty the buffer, call `darksidesync.poll` for the asynchroneous
-- data received, and call the appropriate callback with the arguments. So whenever
-- a UDP notification packet is received, the socket handler function should be called
-- to initiate the execution of the async callback.
-- @return sockethandler function (the function returned requires a single argument; the socket to read from)
-- @see darksidesync.poll
-- @usage
-- copas.addserver(       -- assumes using the Copas scheduler
--   dss.getsocket(), function(skt)
--     skt = copas.wrap(skt)
--     local hdlr = dss.gethandler()
--     while true do
--       hdlr(skt)
--     end
--   end)
dss.gethandler = function()
    return sockethandler
end

-----------------------------------------------------------------------------------------
-- Returns the current queue size.
-- @return number of elements currently waiting in the queue to be handled.
dss.queuesize = function()
  return darksidesync.queuesize()
end

-----------------------------------------------------------------------------------------
-- Sets the error handler when calling the callback function returned from DarkSideSync.
-- When the sockethandler function executes the callback, the function set though
-- `seterrorhandler()` will be used as the error function on the `coxpcall`.
-- The default errorhandler will print the error and a stack traceback.
-- @param f the error handler function to be set (or `nil` to restore the default error handler)
dss.seterrorhandler = function(f)
    assert(type(f) == "function", "The errorhandler must be a function.")
    ehandler = f or _ehandler
end

return dss
