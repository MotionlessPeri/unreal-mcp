# Unreal MCP Editor Tools

This document provides detailed information about the editor tools available in the Unreal MCP integration.

## Overview

Editor tools allow you to control the Unreal Editor viewport and other editor functionality through MCP commands. These tools are particularly useful for automating tasks like focusing the camera on specific actors or locations.

## Editor Tools

### focus_viewport

Focus the viewport on a specific actor or location.

**Parameters:**
- `target` (string, optional) - Name of the actor to focus on (if provided, location is ignored)
- `location` (array, optional) - [X, Y, Z] coordinates to focus on (used if target is None)
- `distance` (float, optional) - Distance from the target/location (default: 1000.0)
- `orientation` (array, optional) - [Pitch, Yaw, Roll] for the viewport camera

**Returns:**
- Response from Unreal Engine containing the result of the focus operation

**Example:**
```json
{
  "command": "focus_viewport",
  "params": {
    "target": "PlayerStart",
    "distance": 500,
    "orientation": [0, 180, 0]
  }
}
```

### take_screenshot

Capture a screenshot of the viewport.

**Parameters:**
- `filename` (string, optional) - Name of the file to save the screenshot as (default: "screenshot.png")
- `show_ui` (boolean, optional) - Whether to include UI elements in the screenshot (default: false)
- `resolution` (array, optional) - [Width, Height] for the screenshot

**Returns:**
- Result of the screenshot operation

**Example:**
```json
{
  "command": "take_screenshot",
  "params": {
    "filename": "my_scene.png",
    "show_ui": false,
    "resolution": [1920, 1080]
  }
}
```

### save_dirty_assets

Save dirty map/content packages in the current editor session.

**Parameters:**
- `save_maps` (boolean, optional) - Save dirty map packages (default: `true`)
- `save_content` (boolean, optional) - Save dirty content packages (default: `true`)

**Returns:**
- `dirty_maps_before`, `dirty_content_before`
- `dirty_maps_after`, `dirty_content_after`
- `success`

Notes:
- `success=true` means the editor save operation completed without command-level failure.
- Always inspect `dirty_*_after` counts; some dirty packages may remain (e.g. prompt-required or nonstandard save cases).

### request_editor_exit

Request Unreal Editor to exit asynchronously after a short delay.

Why delay:
- The command returns over MCP first, then schedules exit via ticker, so the response is less likely to be truncated by shutdown.

**Parameters:**
- `force` (boolean, optional) - Force exit (default: `false`)
- `delay_seconds` (number, optional) - Delay before exit (default: `0.25`)

### save_and_exit_editor

Convenience command that runs `save_dirty_assets` and then schedules editor exit.

**Parameters:**
- `save_maps` (boolean, optional)
- `save_content` (boolean, optional)
- `force` (boolean, optional)
- `delay_seconds` (number, optional; default `0.35`)

**Returns:**
- Save stats (same as `save_dirty_assets`)
- `exit_scheduled`
- `force`
- `delay_seconds`

## Error Handling

All command responses include a "status" field indicating whether the operation succeeded, and an optional "message" field with details in case of failure.

```json
{
  "status": "error",
  "message": "Failed to get active viewport"
}
```

## Usage Examples

### Python Example

```python
from unreal_mcp_server import get_unreal_connection

# Get connection to Unreal Engine
unreal = get_unreal_connection()

# Focus on a specific actor
focus_response = unreal.send_command("focus_viewport", {
    "target": "PlayerStart",
    "distance": 500,
    "orientation": [0, 180, 0]
})
print(focus_response)

# Take a screenshot
screenshot_response = unreal.send_command("take_screenshot", {"filename": "my_scene.png"})
print(screenshot_response)
```

## Troubleshooting

- **Command fails with "Failed to get active viewport"**: Make sure Unreal Editor is running and has an active viewport.
- **Actor not found**: Verify that the actor name is correct and the actor exists in the current level.
- **Invalid parameters**: Ensure that location and orientation arrays contain exactly 3 values (X, Y, Z for location; Pitch, Yaw, Roll for orientation).

## Future Enhancements

- Support for setting viewport display mode (wireframe, lit, etc.)
- Camera animation paths for cinematic viewport control
- Support for multiple viewports
