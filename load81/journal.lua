-- Simple Journal App for PicoCalc
-- Fetches current date from internet and opens daily journal
-- Press ESC in the editor to save and exit

-- Fetch current date from internet
function fetch_current_date()
    print("Fetching current date from internet...")
    
    -- Fetch timestamp from nex://idea.fritz.box/now.txt
    -- Format: "2025-12-17 22:09:20"
    local content, err = nex.load("nex://idea.fritz.box/now.txt")
    
    if content then
        -- Extract date part (YYYY-MM-DD) from timestamp
        local date = content:match("(%d%d%d%d%-%d%d%-%d%d)")
        if date then
            print("Current date: " .. date)
            return date
        end
    else
        print("Error fetching date: " .. (err or "unknown error"))
    end
    
    print("Failed to fetch date from internet, using cached date")
    return nil
end

-- Get current date string (YYYY-MM-DD format)
function get_date_string()
    -- Try to fetch from internet first
    local fetched_date = fetch_current_date()
    if fetched_date then
        -- Save it for offline use
        save_date_string(fetched_date)
        return fetched_date
    end
    
    -- Fall back to cached date
    local date_file = "/journal/current_date.txt"
    local f = io.open(date_file, "r")
    if f then
        local date = f:read("*line")
        f:close()
        if date and #date == 10 then
            return date
        end
    end
    
    -- Default starting date if nothing else works
    return "2024-01-01"
end

-- Save current date to file
function save_date_string(date)
    local f = io.open("/journal/current_date.txt", "w")
    if f then
        f:write(date)
        f:close()
    end
end

-- Main function: open today's journal
function open_journal()
    local date = get_date_string()
    local year, month = date:match("(%d+)-(%d+)")
    
    -- Build file path: /journal/YYYY/MM/YYYY-MM-DD.txt
    local journal_file = string.format("/journal/%s/%s/%s.txt", year, month, date)
    
    print("Opening: " .. journal_file)
    
    -- Open in editor (will create file if it doesn't exist)
    edit(journal_file)
    
    print("Journal saved!")
end

-- Run the journal app
open_journal()