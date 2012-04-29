local dss = require("darksidesync")

for k,v in pairs(dss) do
    print (k, type(v))
end
print ("\nDSS loaded, Press enter...")
io.read()

local luaexit = require("luaexit")
for k,v in pairs(luaexit) do
    print (k, type(v))
end

print ("\nLuaExit loaded, Press enter...")
io.read()
