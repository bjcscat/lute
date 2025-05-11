--!optimize 2

local net = require("@std/time")

-- net.serve({

--     handler = function()

--     end
-- })

for k, v in net do
    print(k, v)
end
