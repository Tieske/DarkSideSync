-- helperc, should be replaced by helper.dll
-- this lua file is simply a lua-mock for the c module
local socket = require('socket')
local queue = {}
local count = 0
local ip = socket.dns.toip('localhost')
local targetport

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
        status, err = skt:setsockname("localhost", port)
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

-- the deliver() function to be called by background libraries, access to this function can be gotten from the
-- register, or in this case the mock-up in Lua, see below.
local deliver = function(decodecallback, data)
    -- store data in queue, table with 2 elements;
    -- 1) the callback to decode the data
    -- 2) the data itself
    table.insert(queue, { cb = decodecallback, data = data } )
    -- send UDP packet notification
    skt = createsocket()
    count = count + 1
    skt:sendto(tostring(count), ip, targetport)   -- UDP never blocks while sending
end

-- define the module table
local helperc = {

    poll = function()
        local result
        if queue[1] then
            -- get the first item from the queue
            result = queue[1]
            table.remove(queue, 1)
            -- extract the decode callback function and the data itself
            local decode =  result.cb
            local data = result.data
            -- call the decode callback provided by the background lib
            -- and pass its own data to decode. Return the results.
            return decode(data)
        end
        return result
    end,

    start = function(port)
        targetport = port
    end,
}

-- create a global variable as a mock-up of the Lua register, which is to be used
-- from the C side of Lua
-- post the decode function in here, so it is available to all utility/background libraries
register = { helper = deliver }

return helperc
