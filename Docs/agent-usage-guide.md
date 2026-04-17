# UnrealMCP Agent Usage Guide

Guide for AI agents using UnrealMCP to automate Unreal Engine editor operations. This document is synced to consumer projects as `UnrealMCP_Docs/agent-usage-guide.md`.

---

## Connection Methods

### Recommended: Python TCP Direct Connection

The Claude Code MCP integration is unreliable (`/mcp` frequently shows "No MCP servers configured" even when properly configured). Use a Python TCP client to connect directly to the UnrealMCP plugin.

**Protocol:**
- Host: `127.0.0.1`
- Port: `55557`
- Format: JSON — `{"type": "<command>", "params": {...}}`
- Response: JSON — `{"status": "success", "result": {...}}` or `{"status": "error", "error": "..."}`

**Example client script** (`ue_cmd.py`):

```python
import socket
import json
import sys

def send_command(command_type, params=None, host="127.0.0.1", port=55557):
    payload = {"type": command_type}
    if params:
        payload["params"] = params
    
    data = json.dumps(payload).encode("utf-8")
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        s.sendall(data)
        
        chunks = []
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
        
        return json.loads(b"".join(chunks).decode("utf-8"))

if __name__ == "__main__":
    cmd = sys.argv[1]
    params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else None
    result = send_command(cmd, params)
    print(json.dumps(result, indent=2))
```

**Usage:**
```bash
python ue_cmd.py ping
python ue_cmd.py get_actors_in_level
python ue_cmd.py spawn_actor '{"type": "PointLight", "name": "MyLight", "location": {"x": 0, "y": 0, "z": 200}}'
```

### Alternative: Claude Code MCP Integration

If MCP integration is working in your environment, UnrealMCP tools are registered automatically via `Python/unreal_mcp_server.py`. Note that MCP tools are only loaded when a **new conversation starts** — resuming an old conversation will not pick up newly registered tools.

---

## Discovering Available Commands

1. **Static reference:** Read [commands.md](commands.md) for the full command list with parameters.
2. **Runtime query:** Send `{"type": "help"}` to get a list of all registered commands, or `{"type": "help", "params": {"command": "<name>"}}` for details on a specific command.
3. **Extension commands:** Check [commands-dialogue.md](commands-dialogue.md) and [commands-logicdriver.md](commands-logicdriver.md) for extension plugin commands.

---

## Usage Rules

### After spawn_actor: Set ActorLabel Separately

`spawn_actor` creates the actor but does not set its display label. Always follow up with `set_actor_property` to set `ActorLabel`:

```bash
python ue_cmd.py spawn_actor '{"type": "PointLight", "name": "MyLight"}'
python ue_cmd.py set_actor_property '{"name": "MyLight", "property_name": "ActorLabel", "property_value": "My Light"}'
```

### Wait After Heavy Commands

After `spawn_actor` or `call_subsystem_function`, wait approximately **4 seconds** before sending the next command. These commands may trigger Perforce operations or asset processing that blocks the game thread.

### Parameter Names Must Be Exact

UnrealMCP validates parameter names (`CheckUnknownParams`). Misspelled parameter names will produce an error. Refer to [commands.md](commands.md) for exact names.

### MCP Capability Gap Policy

If UnrealMCP lacks the capability needed for a task, **ask the user first** whether to extend UnrealMCP before working around it. Do not silently bypass MCP with manual file edits or C++ workarounds when an MCP extension would be the right fix.

---

## Common Pitfalls

### MCP Tools Only Load on New Conversations

When using Claude Code MCP integration, tools are registered at conversation start. Resuming an old conversation will not see newly added tools. Start a new conversation after updating the Python tool layer.

### sync_unreal_mcp.sh Deletes .venv

The sync script deletes `UnrealMCP_Python/.venv` each time it runs. `uv run` will automatically recreate it. This is expected behavior.

### Large Batch Commands and Response Size

Batch commands (e.g. `add_widget_child_batch` with 90+ items) produce large JSON responses. The MCP TCP layer accumulates chunks; ensure your client reads until socket close, not just the first recv.

### Cold Rebuild Required for C++ Changes

After modifying UnrealMCP C++ code, always use a cold rebuild (close editor → build → relaunch). Live Coding frequently leaves stale state that produces behavior inconsistent with the actual code.

### Editor Lifecycle Commands

Use `save_and_exit_editor` for graceful shutdown before rebuilds. The command saves dirty assets and schedules a delayed exit so the response reaches the client before the editor process terminates.

```bash
python ue_cmd.py save_and_exit_editor
```

Fallback (if editor is unresponsive): `taskkill /IM UnrealEditor.exe /F` — last resort only, risks losing unsaved work.

---

## Workflow: Rebuild Cycle

A typical agent rebuild cycle:

1. **Save and close editor:** `python ue_cmd.py save_and_exit_editor`
2. **Wait for process exit** (poll or sleep ~5s)
3. **Build:** `Build.bat <Project>Editor Win64 Development -Project="<uproject>" -WaitMutex -FromMSBuild`
4. **Launch editor:** `start "" UnrealEditor.exe "<uproject>"`
5. **Wait for TCP reconnection:** poll `python ue_cmd.py ping` until it responds

---

## Extension Plugins

UnrealMCP supports optional extension plugins that register additional commands via `FUnrealMCPCommandRegistry`:

- **UnrealMCPDialogue** — Dialogue asset read/write commands
- **UnrealMCPLogicDriver** — Logic Driver state machine graph read commands

Extension commands are automatically available when the extension plugin is loaded. The Python tool layer conditionally registers extension tools based on plugin availability.
