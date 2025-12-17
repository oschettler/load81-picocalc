function setup()
    print("Press any key to see the corresponding name.")
end

function draw()
    if keyboard.state == "down" then
        for k,v in pairs(keyboard.pressed) do
            print(k)
        end
    end
end
