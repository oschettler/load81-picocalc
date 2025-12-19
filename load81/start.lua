-- Example startup script for LOAD81 PicoCalc
-- Place this file as /load81/start.lua on your SD card
-- It will be executed automatically on boot

print("=== LOAD81 Startup Script ===")

-- WiFi Configuration
local WIFI_SSID = "schettler"
local WIFI_PASSWORD = "1122334455667"

-- Connect to WiFi
print("Connecting to WiFi: " .. WIFI_SSID)
if wifi.connect(WIFI_SSID, WIFI_PASSWORD) then
    print("WiFi connected!")
    print("IP Address: " .. wifi.ip())
    
    -- Example: Load a NEX page
    print("\nTesting NEX protocol...")
    local content, err = nex.load("nex://idea.fritz.box/")
    if content then
        print("NEX page loaded successfully!")
        local parsed = nex.parse(content)
        print("Found " .. #parsed .. " lines")
    else
        print("NEX load failed: " .. (err or "unknown error"))
    end
else
    print("WiFi connection failed")
    print("Status: " .. wifi.status())
end

print("\n=== Startup Complete ===")
print("Press any key to continue to menu...")
