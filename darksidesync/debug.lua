local dss = require("darksidesync")

for k,v in pairs(dss) do
    print (k, type(v))
end

--local luaexit = require("luaexit")

print ("\nPress enter...")
io.read()
