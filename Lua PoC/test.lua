local copas = require('copas')        -- load Copas socket scheduler
local helper = require('helper')      -- load helper module

-- the helper lib has a Lua side piece of code that listens to the UDP signal whenever
-- a background lib has delivered something.
-- It provides a socket we need to listen on, and a callback that needs to be called when
-- the socket is ready to read data. All we need to do is add them to our socket scheduler.
-- We're using Copas as a socket scheduler, so add the helper socket and the handler in a
-- Copas way to the scheduler
copas.addserver(helper.getsocket(), function(skt)
        skt = copas.wrap(skt)
        local hdlr = helper.gethandler()
        while true do
            hdlr(skt)
        end
    end)

-- the helper library has been loaded and is ready to go, now load our test
-- background library
local utility = require('utilityc')    -- load utility module

-- We need a callback function that can be called whenever the background library has data
-- to deliver. Normally we would handle our data, but for test purposes, printing will do.
local utilitycallback = function(...)
    -- print whatever data is received
    print( ... )
end

-- initialize utility and provide it with the created callback function
utility.start(utilitycallback)

-- start the socket scheduler loop, so the socket gets read.
copas.loop()
