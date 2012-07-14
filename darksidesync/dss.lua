local socket = require("socket")
local darksidesync = require("darksidesync")
require("coxpcall")
local skt, port


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
local ehandler = function(msg)
    if msg then
        msg = tostring(msg) .. "\n" .. debug.traceback("DSS error: callback function had an error;\n")
    else
        msg = debug.traceback("DSS error: callback function had an error;\n")
    end
    print (msg)
end

-- argument rotate left (remove 1st in list)
local rotate = function(drop, ...) return ... end

-- reads incoming data on the socket, dismisses the data and calls poll()
-- any data returned will have a first argument being a callback to be called with the remaining
-- arguments
local sockethandler = function(skt)
    -- collect data from socket, can be dismissed, won't be used
    skt:receive(8192)   -- size not optional if using copas, add it to be sure
    -- now call poll() to collect the actual data in a new table with values
    local values = { darksidesync.poll() }  -- catch return values in a table
    if values[1] == -1 then
        -- there was nothing in the queue, nothing was executed
    else
        values = { rotate(unpack(values)) }
        local cb = values[1] -- get the callback, always the first argument
        if cb then
            if type(cb) ~= "function" then
                print ("error: the first argument returned should have been a lua function!")
            else
                -- now call the callback with the other arguments as parameters, in a protected (coxpcall) mode
                local f = values[1]
                values = { rotate(unpack(values)) }
                xpcall(function() f(unpack(values)) end, ehandler)
            end
        end
    end
end

-- define module table
local dss = {

    -- function to return the socket where the helper module will be listening for incoming UDP
    -- signals that data is ready to be collected through poll()
    getsocket = function()
        if not skt then
            skt, port = createsocket()
            if skt then
                -- socket was created succesfully, now must tell my C side helper lib on what port
                -- I'm listening for incoming data
                darksidesync.setport(port)
            end
        end
        return skt, port
    end,

    -- function to return the socket handler function. This socket handler function will do a single
    -- read on the socket and call the appropriate callback with the arguments
    gethandler = function()
        return sockethandler
    end,

    -- sets the error handler when calling the callback function returned from DSS
    seterrorhandler = function(f)
        assert(type(f) == "function", "The errorhandler must be a function.")
        ehandler = f
    end,

}

return dss
