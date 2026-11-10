-- The example mod's script. Every function here is optional; blocks.* is
-- the game (see blocks/game/scripts.c for the full list).

local cozy = 0

function on_start()
    blocks.say("Example mod: break a glowing stone with a pickaxe")
end

-- Roasted berries also warm you up a little.
function on_eat(thing)
    if thing == "roasted_berries" then
        blocks.warmth(blocks.warmth() + 1)
    end
end

-- Every minute next to a glowing stone, a stick falls into your hands.
function on_tick(dt)
    local x, y, z = blocks.player()
    x, y, z = math.floor(x), math.floor(y), math.floor(z)
    for dx = -2, 2 do
        for dz = -2, 2 do
            if blocks.get_block(x + dx, y, z + dz) == "glow_stone" then
                cozy = cozy + dt
                if cozy >= 60 then
                    cozy = 0
                    blocks.give("stick", 1)
                end
                return
            end
        end
    end
end
