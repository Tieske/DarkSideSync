-- utility helper, should be replaced by utility.dll

local copas = require ('copas.timer')
local cb        -- my callback
local count = 0

-- look up the deliver method to the helper library from the register (in this case the lua mock-up)
local deliver = register.helper

local decode = function(data)
    -- decode the data delivered to the deliver() function by dostuff()
    local cb = data[1]
    local count = data[2]
    local text = data[3]
    local result = string.sub(text, 1, 12) .. tostring(count) .. string.sub(text, 12)
    return cb, result
end

local dostuff = function()
    count = count + 1
    -- call deliver() to drop our background data;
    -- 1st argument is a call back to a function to decode my data
    -- 2nd argument is my data itself
    deliver(decode, {cb, count, "hello world from utility background"} )
end

-- define the module table
local utilityc = {
    start = function(callback)
        -- set callback
        cb = callback
        -- Create timer and arm it immediately, to be run every 1 second
        copas.newtimer(nil, dostuff, nil, true):arm(1)
    end,
}

return utilityc
