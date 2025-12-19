function setup()
    background(0,0,0);
end
-- edited with load81r
function draw()
    -- draw a few lines
    fill(math.random(255),math.random(255),math.random(255),math.random())
    line (math.random(WIDTH),math.random(HEIGHT),
             math.random(WIDTH),math.random(HEIGHT))
end

