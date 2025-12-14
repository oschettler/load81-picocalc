-- NEX Document Browser for LOAD81
-- A simple Gemini/NEX protocol browser

-- Browser state
local current_url = "nex://idea.fritz.box"
local page_content = nil
local parsed_lines = {}
local scroll_offset = 0
local selected_link = 1
local link_indices = {}
local error_message = nil
local loading = false
local history = {}

-- Display settings
local LINE_HEIGHT = 12
local LINES_PER_PAGE = 24
local MARGIN_X = 5
local MARGIN_Y = 10

-- Load a NEX page
function load_page(url)
    loading = true
    error_message = nil
    
    print("Loading: " .. url)
    
    local content, err = nex.load(url)
    
    if content then
        page_content = content
        parsed_lines = nex.parse(content)
        
        -- Find all links
        link_indices = {}
        for i, line in ipairs(parsed_lines) do
            if line.type == "link" then
                table.insert(link_indices, i)
            end
        end
        
        current_url = url
        scroll_offset = 0
        selected_link = 1
        error_message = nil
        
        print("Loaded " .. #parsed_lines .. " lines, " .. #link_indices .. " links")
    else
        error_message = err or "Failed to load page"
        print("Error: " .. error_message)
    end
    
    loading = false
end

-- Extract URL from link line
function extract_url(link_text)
    -- Link format: "=> url [label]"
    local url = link_text:match("^%s*=>%s*(%S+)")
    return url
end

-- Extract label from link line
function extract_label(link_text)
    local url, label = link_text:match("^%s*=>%s*(%S+)%s+(.+)")
    if label then
        return label
    else
        return url or link_text
    end
end

-- Resolve relative URL
function resolve_url(base, relative)
    if relative:sub(1, 6) == "nex://" then
        -- Absolute URL
        return relative
    elseif relative:sub(1, 1) == "/" then
        -- Absolute path
        local protocol, host = base:match("(nex://)([^/]+)")
        if protocol and host then
            return protocol .. host .. relative
        end
    else
        -- Relative path
        local base_path = base:match("(.*/)")
        if base_path then
            return base_path .. relative
        end
    end
    return relative
end

-- Navigate to link
function navigate_to_link()
    if #link_indices == 0 then return end
    
    local line_idx = link_indices[selected_link]
    local line = parsed_lines[line_idx]
    
    if line and line.type == "link" then
        local url = extract_url(line.text)
        if url then
            -- Add current page to history
            table.insert(history, current_url)
            
            -- Resolve and load new URL
            local full_url = resolve_url(current_url, url)
            load_page(full_url)
        end
    end
end

-- Go back in history
function go_back()
    if #history > 0 then
        local prev_url = table.remove(history)
        load_page(prev_url)
    end
end

-- Setup function
function setup()
    -- Load initial page
    load_page(current_url)
end

-- Draw function
function draw()
    -- Clear screen
    background(0, 0, 50)
    
    if loading then
        -- Show loading message
        fill(255, 255, 255, 1)
        text(WIDTH/2 - 50, HEIGHT/2, "Loading...")
        return
    end
    
    if error_message then
        -- Show error
        fill(255, 50, 50, 1)
        text(MARGIN_X, HEIGHT - 30, "Error:")
        fill(255, 200, 200, 1)
        text(MARGIN_X, HEIGHT - 45, error_message)
        
        fill(150, 150, 150, 1)
        text(MARGIN_X, HEIGHT - 70, "Press ESC to exit")
        if #history > 0 then
            text(MARGIN_X, HEIGHT - 85, "Press B to go back")
        end
        return
    end
    
    -- Draw title bar
    fill(200, 200, 100, 1)
    text(MARGIN_X, HEIGHT - MARGIN_Y, current_url)
    
    -- Draw page content
    local y = HEIGHT - MARGIN_Y - 20
    local visible_start = scroll_offset + 1
    local visible_end = math.min(scroll_offset + LINES_PER_PAGE, #parsed_lines)
    
    for i = visible_start, visible_end do
        local line = parsed_lines[i]
        
        if line.type == "heading" then
            -- Heading in yellow
            fill(255, 255, 100, 1)
            text(MARGIN_X, y, line.text)
        elseif line.type == "link" then
            -- Check if this link is selected
            local is_selected = false
            for idx, link_idx in ipairs(link_indices) do
                if link_idx == i and idx == selected_link then
                    is_selected = true
                    break
                end
            end
            
            if is_selected then
                -- Selected link in bright cyan with marker
                fill(0, 255, 255, 1)
                text(MARGIN_X, y, "> " .. extract_label(line.text))
            else
                -- Unselected link in cyan
                fill(100, 200, 255, 1)
                text(MARGIN_X + 10, y, extract_label(line.text))
            end
        else
            -- Regular text in white
            fill(200, 200, 200, 1)
            text(MARGIN_X, y, line.text)
        end
        
        y = y - LINE_HEIGHT
        if y < MARGIN_Y then break end
    end
    
    -- Draw controls at bottom
    fill(150, 150, 150, 1)
    text(MARGIN_X, 25, "UP/DN: Navigate  ENTER: Follow link")
    text(MARGIN_X, 12, "W/S: Scroll  B: Back  ESC: Exit")
    
    -- Draw scroll indicator
    if #parsed_lines > LINES_PER_PAGE then
        local scroll_pct = scroll_offset / (#parsed_lines - LINES_PER_PAGE)
        fill(100, 100, 255, 1)
        text(WIDTH - 30, HEIGHT/2, string.format("%d%%", scroll_pct * 100))
    end
    
    -- Handle keyboard input
    handle_input()
end

-- Previous key states (for detecting key press vs hold)
local prev_keys = {}

-- Handle keyboard input
function handle_input()
    -- Navigation between links (UP/DOWN arrows)
    if keyboard.pressed['up'] and not prev_keys['up'] then
        if selected_link > 1 then
            selected_link = selected_link - 1
            
            -- Auto-scroll to keep selected link visible
            local line_idx = link_indices[selected_link]
            if line_idx <= scroll_offset then
                scroll_offset = math.max(0, line_idx - 1)
            end
        end
    end
    
    if keyboard.pressed['down'] and not prev_keys['down'] then
        if selected_link < #link_indices then
            selected_link = selected_link + 1
            
            -- Auto-scroll to keep selected link visible
            local line_idx = link_indices[selected_link]
            if line_idx > scroll_offset + LINES_PER_PAGE then
                scroll_offset = math.min(#parsed_lines - LINES_PER_PAGE, line_idx - LINES_PER_PAGE)
            end
        end
    end
    
    -- Page scrolling (W/S keys)
    if keyboard.pressed['w'] and not prev_keys['w'] then
        scroll_offset = math.max(0, scroll_offset - LINES_PER_PAGE)
    end
    
    if keyboard.pressed['s'] and not prev_keys['s'] then
        scroll_offset = math.min(math.max(0, #parsed_lines - LINES_PER_PAGE), scroll_offset + LINES_PER_PAGE)
    end
    
    -- Follow link (ENTER/RETURN)
    if keyboard.pressed['return'] and not prev_keys['return'] then
        navigate_to_link()
    end
    
    -- Go back (B key)
    if keyboard.pressed['b'] and not prev_keys['b'] then
        go_back()
    end
    
    -- Update previous key states
    prev_keys['up'] = keyboard.pressed['up']
    prev_keys['down'] = keyboard.pressed['down']
    prev_keys['w'] = keyboard.pressed['w']
    prev_keys['s'] = keyboard.pressed['s']
    prev_keys['return'] = keyboard.pressed['return']
    prev_keys['b'] = keyboard.pressed['b']
end
