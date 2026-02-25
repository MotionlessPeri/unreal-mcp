"""
Smoke test for the Python MCP tool-layer helper `create_debugboard_skeleton_widget`.

This validates the orchestration wrapper itself (not only the underlying plugin commands)
by registering UMG tools into a fake MCP registry and invoking the helper function directly.
"""

from __future__ import annotations

import json
import socket
import sys
import types
from datetime import datetime
from pathlib import Path


PYTHON_DIR = Path(__file__).resolve().parents[1]
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

# Provide a lightweight stub when the local environment does not have the `mcp` package.
if "mcp.server.fastmcp" not in sys.modules:
    mcp_mod = types.ModuleType("mcp")
    server_mod = types.ModuleType("mcp.server")
    fastmcp_mod = types.ModuleType("mcp.server.fastmcp")

    class _FastMCP:  # pragma: no cover - smoke stub only
        pass

    class _Context:  # pragma: no cover - smoke stub only
        pass

    fastmcp_mod.FastMCP = _FastMCP
    fastmcp_mod.Context = _Context

    sys.modules["mcp"] = mcp_mod
    sys.modules["mcp.server"] = server_mod
    sys.modules["mcp.server.fastmcp"] = fastmcp_mod


class _RawUnrealConnection:
    def send_command(self, command: str, params: dict | None = None):
        payload = json.dumps({"type": command, "params": params or {}}).encode("utf-8")
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(300.0)
            sock.connect(("127.0.0.1", 55557))
            sock.sendall(payload)
            buf = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                buf += chunk
                try:
                    return json.loads(buf.decode("utf-8"))
                except json.JSONDecodeError:
                    continue
        if not buf:
            return None
        return json.loads(buf.decode("utf-8"))


if "unreal_mcp_server" not in sys.modules:
    unreal_server_stub = types.ModuleType("unreal_mcp_server")

    def _get_unreal_connection():
        return _RawUnrealConnection()

    unreal_server_stub.get_unreal_connection = _get_unreal_connection
    sys.modules["unreal_mcp_server"] = unreal_server_stub

from tools.umg_tools import register_umg_tools  # noqa: E402


class _FakeMCP:
    def __init__(self) -> None:
        self.tools = {}

    def tool(self):
        def _decorator(func):
            self.tools[func.__name__] = func
            return func
        return _decorator


def main() -> int:
    fake = _FakeMCP()
    register_umg_tools(fake)

    helper = fake.tools.get("create_debugboard_skeleton_widget")
    if helper is None:
        raise RuntimeError("create_debugboard_skeleton_widget not registered")

    widget_name = f"WBP_McpToolDebugBoardProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    result = helper(
        None,
        widget_name=widget_name,
        path="/Game/UI",
        board_rows=4,
        board_cols=4,
        board_origin=[120.0, 80.0],
        cell_size=[52.0, 48.0],
        board_padding=18.0,
        side_panel_width=240.0,
        side_panel_min_height=260.0,
    )
    if not isinstance(result, dict):
        raise RuntimeError(f"Unexpected helper return type: {type(result)}")
    if result.get("success") is not True:
        raise RuntimeError(f"Helper returned failure: {result}")

    if result.get("board_cell_count") != 16:
        raise RuntimeError(f"board_cell_count mismatch: {result}")
    if result.get("add_cell_created_count") != 16:
        raise RuntimeError(f"add_cell_created_count mismatch: {result}")
    if result.get("grid_slot_updated_count") != 16:
        raise RuntimeError(f"grid_slot_updated_count mismatch: {result}")
    layout = result.get("layout") or {}
    if layout.get("board_origin") != [120.0, 80.0]:
        raise RuntimeError(f"layout.board_origin mismatch: {result}")
    if layout.get("cell_size") != [52.0, 48.0]:
        raise RuntimeError(f"layout.cell_size mismatch: {result}")
    if abs(float(layout.get("board_padding", -1)) - 18.0) > 1e-6:
        raise RuntimeError(f"layout.board_padding mismatch: {result}")

    print(json.dumps({
        "widget_name": result.get("widget_name"),
        "blueprint_name": result.get("blueprint_name"),
        "board_cell_count": result.get("board_cell_count"),
        "add_cell_created_count": result.get("add_cell_created_count"),
        "grid_slot_updated_count": result.get("grid_slot_updated_count"),
        "text_updated_count": result.get("text_updated_count"),
        "root_children": result.get("root_children"),
        "layout": result.get("layout"),
    }, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
