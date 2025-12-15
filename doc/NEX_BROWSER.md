# NEX Document Browser

A simple Gemini/NEX protocol browser implemented in Lua for LOAD81 on PicoCalc.

## Overview

The NEX browser (`nex.lua`) is a document viewer that can:
- Load NEX/Gemini protocol documents from network servers
- Display text, headings, and links
- Navigate between pages using link selection
- Maintain browsing history with back navigation
- Scroll through long documents

## Installation

Copy `nex.lua` to the `/load81/` directory on your SD card:

```bash
cp load81/nex.lua /path/to/sdcard/load81/nex.lua
```

## Usage

### Starting the Browser

1. Insert SD card with `nex.lua` in `/load81/` directory
2. Select "nex.lua" from the LOAD81 menu
3. Press ENTER to launch
4. The browser will automatically load `nex://idea.fritz.box`

### Controls

#### Link Navigation
- **UP Arrow** - Move to previous link
- **DOWN Arrow** - Move to next link
- **ENTER** - Follow the selected link

#### Page Scrolling
- **W** - Scroll up one page
- **S** - Scroll down one page

#### History
- **B** - Go back to previous page

#### Exit
- **ESC** - Exit browser and return to menu

## Features

### Document Display

The browser supports Gemini/NEX text format:
- **Headings** - Lines starting with `#` displayed in yellow
- **Links** - Lines starting with `=>` displayed in cyan
- **Text** - Normal text displayed in white
- **Selected Link** - Highlighted in bright cyan with `>` marker

### Link Handling

Links are parsed in Gemini format:
```
=> /path/to/page Link Label
=> nex://example.com/page Another Page
```

The browser supports:
- Absolute URLs: `nex://hostname/path`
- Absolute paths: `/path`
- Relative paths: `page.txt`

### Visual Feedback

- **Loading Indicator** - Shows "Loading..." while fetching pages
- **Error Messages** - Displays connection errors with helpful text
- **URL Display** - Current URL shown in yellow at top
- **Scroll Indicator** - Shows scroll percentage for long documents
- **Control Help** - Always visible at bottom of screen

### History

- Maintains history stack of visited pages
- Use B key to go back
- History is cleared when returning to menu

## Configuration

You can customize the browser by editing `nex.lua`:

```lua
-- Change the starting URL
local current_url = "nex://your.server.com"

-- Adjust display settings
local LINE_HEIGHT = 12        -- Space between lines
local LINES_PER_PAGE = 24     -- Lines visible at once
local MARGIN_X = 5            -- Left/right margin
local MARGIN_Y = 10           -- Top/bottom margin
```

## NEX Protocol

The browser uses the NEX API provided by LOAD81:

### nex.load(url)
Fetches a document from a NEX server:
```lua
local content, err = nex.load("nex://example.com/page")
```

### nex.parse(content)
Parses Gemini/NEX text format into structured lines:
```lua
local lines = nex.parse(content)
-- Returns array of: {type="text|link|heading", text="..."}
```

## Example NEX Server Setup

To test locally, you can run a simple NEX server:

```bash
# Install a Gemini server (e.g., Agate, Gemserv, etc.)
# Configure it to listen on port 1900 (NEX default)
# Place documents in the server's content directory
```

Or use the built-in test server at `nex://idea.fritz.box`

## Troubleshooting

### "DNS lookup failed"
- Ensure WiFi is connected
- Check that hostname is correct
- Verify DNS is working with `wifi.debug_info()`

### "Connection timeout"
- Server may be down or unreachable
- Check firewall settings (port 1900)
- Verify network connectivity

### "Empty response"
- Server may not be sending data
- Check server logs
- Try a different URL

### Links not working
- Ensure link format is correct: `=> URL Label`
- Check that URLs are properly formed
- Look at serial console for parsing errors

## Advanced Usage

### Creating Your Own NEX Content

NEX/Gemini documents are simple text files:

```
# Welcome Page

This is a simple NEX document.

=> /about.gmi About This Site
=> /projects/ My Projects
=> nex://other-server.com/ External Link

## Features

* Simple text format
* Easy to write
* Fast to parse
```

### Customizing the Browser

You can extend the browser with:
- Custom color schemes
- Different fonts
- Mouse/touch support
- Bookmarks
- Download functionality
- Image support (requires extending NEX protocol)

## Technical Details

### Architecture
- **State Management** - Global variables track current page, history, scroll position
- **Event Loop** - Keyboard handling in `draw()` function
- **Async Loading** - NEX requests block until complete (with timeout)
- **Text Parsing** - Line-by-line parsing with regex for link/heading detection

### Performance
- Maximum document size: 64KB (NEX protocol limit)
- Rendering: 30 FPS
- Network timeout: 10 seconds
- DNS cache: Handled by lwIP

### Memory Usage
- Response buffer: 4-64KB (dynamic)
- Parsed lines: ~24 bytes per line
- History stack: ~100 bytes per entry

## Known Limitations

1. **No image support** - Text only (Gemini standard)
2. **No TLS/SSL** - Plain TCP connection
3. **No client certificates** - Not implemented
4. **No input prompts** - Status code 10/11 not handled
5. **No redirects** - Status code 30/31 not handled
6. **Single server port** - Port 1900 only
7. **No bookmark storage** - History cleared on exit

## Future Enhancements

Possible improvements:
- [ ] Add bookmark functionality
- [ ] Support input prompts (status 10)
- [ ] Handle redirects (status 30)
- [ ] Add page caching
- [ ] Support multiple server ports
- [ ] Add URL bar for direct entry
- [ ] Implement find-in-page
- [ ] Add download capability
- [ ] Support gemini:// protocol (TLS)

## Credits

- NEX Protocol: Simplified Gemini protocol for embedded systems
- LOAD81: Lua Fantasy Console by Salvatore Sanfilippo
- Gemini Protocol: Specified by Solderpunk

## License

This browser is provided as-is under the same license as LOAD81.
