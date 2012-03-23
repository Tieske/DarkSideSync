require("luarocks.require")

local copas = require('copas.timer')        -- load Copas socket scheduler
local dss = require('dss')      -- load darksidesync module

-- the darksidesync lib has a Lua side piece of code that listens to the UDP signal whenever
-- a background lib has delivered something.
-- It provides a socket we need to listen on, and a callback that needs to be called when
-- the socket is ready to read data. All we need to do is add them to our socket scheduler.
-- We're using Copas as a socket scheduler, so add the darksidesync socket and the handler in a
-- Copas way to the scheduler
copas.addserver(dss.getsocket(), function(skt)
        skt = copas.wrap(skt)
        local hdlr = dss.gethandler()
        while true do
            hdlr(skt)
        end
    end)

-- the darksidesync library has been loaded and is ready to go, now load our test
-- background library, the signal handler. It only handles SIGTERM.
local le = require('luaexit')

-- initialize luaexit and provide it with a callback
le.start(function(sig)
    print("Received signal; ", sig, ", now preparing for exit...")
	copas.exitloop()
end)

-- start the socket scheduler loop, so the socket gets read.
print ("Starting the loop, send SIGTERM to test;")
print ("      kill -s SIGTERM <pid>")
print ("Use this pid;")
os.execute("ps -A | grep lua")

copas.loop()

print ("Copas loop exited gracefully. Bye...")

return 0
	
