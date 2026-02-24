"""
Smoke test for set_uniform_grid_slot_batch:
1) create probe widget with UniformGridPanel
2) add multiple child widgets into the grid
3) batch-apply row/column/alignment settings
4) verify per-item readback and widget-tree slot placement
"""

from __future__ import annotations

import json
import socket
from datetime import datetime


def _send(command: str, params: dict, timeout: float = 90.0) -> dict:
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
    widget_name = f"WBP_McpUniformGridBatchProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    bp_path = f"/Game/UI/{widget_name}"

    create = _ok(_send("create_umg_widget_blueprint", {
        "widget_name": widget_name,
        "path": "/Game/UI",
        "parent_class": "UserWidget",
    }), "create_umg_widget_blueprint")
    bp = create.get("path", bp_path)

    _ok(_send("ensure_widget_root", {
        "blueprint_name": bp,
        "widget_class": "CanvasPanel",
        "widget_name": "RootCanvas",
        "replace_existing": True,
    }), "ensure_widget_root")
    _ok(_send("clear_widget_children", {
        "blueprint_name": bp,
        "widget_name": "RootCanvas",
    }), "clear_widget_children")

    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "RootCanvas",
        "widget_class": "UniformGridPanel", "widget_name": "BoardGrid",
    }), "add_widget_child(BoardGrid)")
    _ok(_send("set_canvas_slot_layout", {
        "blueprint_name": bp, "widget_name": "BoardGrid",
        "position": [20, 20], "size": [400, 400], "z_order": 1,
    }), "set_canvas_slot_layout(BoardGrid)")

    cell_names = ["Cell00", "Cell01", "Cell10", "Cell11"]
    for cell in cell_names:
        _ok(_send("add_widget_child", {
            "blueprint_name": bp,
            "parent_widget_name": "BoardGrid",
            "widget_class": "Border",
            "widget_name": cell,
        }), f"add_widget_child({cell})")

    batch = _ok(_send("set_uniform_grid_slot_batch", {
        "blueprint_name": bp,
        "items": [
            {"widget_name": "Cell00", "row": 0, "column": 0, "horizontal_alignment": "Fill", "vertical_alignment": "Fill"},
            {"widget_name": "Cell01", "row": 0, "column": 1, "horizontal_alignment": "Center", "vertical_alignment": "Center"},
            {"widget_name": "Cell10", "row": 1, "column": 0, "horizontal_alignment": "Left", "vertical_alignment": "Top"},
            {"widget_name": "Cell11", "row": 1, "column": 1, "horizontal_alignment": "Right", "vertical_alignment": "Bottom"},
        ],
    }), "set_uniform_grid_slot_batch")

    results = {item["widget_name"]: item for item in (batch.get("results") or [])}
    if set(results.keys()) != set(cell_names):
        raise RuntimeError(f"Unexpected batch result keys: {sorted(results.keys())}")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree")
    root = tree.get("root") or {}
    board = _find(root, "BoardGrid")
    if not board:
        raise RuntimeError("BoardGrid not found")
    for cell in cell_names:
        node = _find(root, cell)
        if not node:
            raise RuntimeError(f"{cell} not found in widget tree")
        if node.get("slot_class") != "UniformGridSlot":
            raise RuntimeError(f"{cell} expected UniformGridSlot, got {node.get('slot_class')}")

    summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "batch": {
            "updated_count": batch.get("updated_count"),
            "Cell00": {"row": results["Cell00"]["row"], "column": results["Cell00"]["column"]},
            "Cell01": {"row": results["Cell01"]["row"], "column": results["Cell01"]["column"]},
            "Cell10": {"row": results["Cell10"]["row"], "column": results["Cell10"]["column"]},
            "Cell11": {"row": results["Cell11"]["row"], "column": results["Cell11"]["column"]},
        },
        "tree": {
            "root_children": [child.get("name") for child in (root.get("children") or [])],
            "board_children": [child.get("name") for child in (board.get("children") or [])],
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

