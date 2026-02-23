"""
Stage 3 smoke test for UMG layout primitives:
1) create probe widget
2) build hierarchy with root canvas + uniform grid + side panel
3) set canvas slot layouts
4) add a grid child and set uniform-grid slot layout
5) read back tree and print layout responses
"""

from __future__ import annotations

import json
import socket
from datetime import datetime


def _send(command: str, params: dict, timeout: float = 60.0) -> dict:
    payload = json.dumps({"type": command, "params": params}).encode("utf-8")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
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
        raise RuntimeError(f"{command}: empty response")
    return json.loads(buf.decode("utf-8"))


def _ok(response: dict, command_name: str) -> dict:
    if not response:
        raise RuntimeError(f"{command_name}: no response")
    if response.get("status") == "error":
        raise RuntimeError(f"{command_name}: {response.get('error', 'unknown error')}")
    result = response.get("result")
    if not isinstance(result, dict):
        raise RuntimeError(f"{command_name}: malformed response")
    if result.get("success") is False:
        raise RuntimeError(f"{command_name}: result.success=false ({result})")
    return result


def _find(node: dict | None, name: str) -> dict | None:
    if not isinstance(node, dict):
        return None
    if node.get("name") == name:
        return node
    for child in node.get("children") or []:
        found = _find(child, name)
        if found:
            return found
    return None


def main() -> int:
    widget_name = f"WBP_McpUmgProbeS3_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    asset_path = f"/Game/UI/{widget_name}"

    create = _ok(_send("create_umg_widget_blueprint", {
        "widget_name": widget_name,
        "path": "/Game/UI",
        "parent_class": "UserWidget",
    }), "create_umg_widget_blueprint")
    bp = create.get("path", asset_path)

    _ok(_send("ensure_widget_root", {
        "blueprint_name": bp,
        "widget_class": "CanvasPanel",
        "widget_name": "RootCanvas",
        "replace_existing": True,
    }), "ensure_widget_root")

    _ok(_send("add_widget_child", {
        "blueprint_name": bp,
        "parent_widget_name": "RootCanvas",
        "widget_class": "UniformGridPanel",
        "widget_name": "BoardGrid",
    }), "add_widget_child(BoardGrid)")

    _ok(_send("add_widget_child", {
        "blueprint_name": bp,
        "parent_widget_name": "RootCanvas",
        "widget_class": "VerticalBox",
        "widget_name": "SidePanel",
    }), "add_widget_child(SidePanel)")

    board_canvas = _ok(_send("set_canvas_slot_layout", {
        "blueprint_name": bp,
        "widget_name": "BoardGrid",
        "position": [40, 40],
        "size": [540, 600],
        "anchors": [0, 0],
        "alignment": [0, 0],
        "z_order": 1,
    }), "set_canvas_slot_layout(BoardGrid)")

    side_canvas = _ok(_send("set_canvas_slot_layout", {
        "blueprint_name": bp,
        "widget_name": "SidePanel",
        "position": [620, 40],
        "size": [260, 600],
        "anchors": [0, 0],
        "alignment": [0, 0],
        "z_order": 2,
    }), "set_canvas_slot_layout(SidePanel)")

    _ok(_send("add_widget_child", {
        "blueprint_name": bp,
        "parent_widget_name": "BoardGrid",
        "widget_class": "Border",
        "widget_name": "Cell00",
    }), "add_widget_child(Cell00)")

    cell_grid = _ok(_send("set_uniform_grid_slot", {
        "blueprint_name": bp,
        "widget_name": "Cell00",
        "row": 0,
        "column": 0,
        "horizontal_alignment": "Fill",
        "vertical_alignment": "Fill",
    }), "set_uniform_grid_slot(Cell00)")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree")
    root = tree.get("root") or {}
    board = _find(root, "BoardGrid")
    side = _find(root, "SidePanel")
    cell = _find(root, "Cell00")

    if root.get("name") != "RootCanvas":
        raise RuntimeError(f"Unexpected root: {root.get('name')}")
    if not all([board, side, cell]):
        raise RuntimeError("Expected widgets not found in tree")

    summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "canvas_layout": {
            "BoardGrid": {
                "position": board_canvas.get("position"),
                "size": board_canvas.get("size"),
                "z_order": board_canvas.get("z_order"),
            },
            "SidePanel": {
                "position": side_canvas.get("position"),
                "size": side_canvas.get("size"),
                "z_order": side_canvas.get("z_order"),
            },
        },
        "grid_slot": {
            "Cell00": {
                "row": cell_grid.get("row"),
                "column": cell_grid.get("column"),
                "slot_class": cell_grid.get("slot_class"),
            }
        },
        "tree": {
            "root_children": [child.get("name") for child in root.get("children") or []],
            "board_children": [child.get("name") for child in (board.get("children") or [])],
            "cell_slot_class": cell.get("slot_class"),
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
