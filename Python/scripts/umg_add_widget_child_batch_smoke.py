"""
Smoke test for add_widget_child_batch:
1) create probe widget with Canvas root + UniformGridPanel board
2) batch-add a full 9x10 board (90 Border children) into the grid
3) batch-apply uniform-grid row/column layout
4) verify widget-tree child count and spot-check row/column readback

This smoke specifically targets the scalability gap found in the mini DebugBoard composite
smoke, where repeated single `add_widget_child` calls (each compile/save) triggered UMG GUID
ensure paths at full-board scale.
"""

from __future__ import annotations

import json
import socket
from datetime import datetime


def _send(command: str, params: dict, timeout: float = 180.0) -> dict:
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


def _children_names(node: dict | None) -> list[str]:
    if not isinstance(node, dict):
        return []
    return [child.get("name") for child in (node.get("children") or [])]


def main() -> int:
    widget_name = f"WBP_McpAddChildBatchProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    bp_path = f"/Game/UI/{widget_name}"
    rows = 10
    cols = 9

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
        "blueprint_name": bp,
        "parent_widget_name": "RootCanvas",
        "widget_class": "UniformGridPanel",
        "widget_name": "BoardGrid",
    }), "add_widget_child(BoardGrid)")
    _ok(_send("set_canvas_slot_layout", {
        "blueprint_name": bp,
        "widget_name": "BoardGrid",
        "position": [20, 20],
        "size": [600, 660],
        "z_order": 1,
    }), "set_canvas_slot_layout(BoardGrid)")

    add_items = []
    grid_items = []
    for row in range(rows):
        for col in range(cols):
            cell = f"Cell_{row}_{col}"
            add_items.append({
                "parent_widget_name": "BoardGrid",
                "widget_class": "Border",
                "widget_name": cell,
            })
            grid_items.append({
                "widget_name": cell,
                "row": row,
                "column": col,
                "horizontal_alignment": "Fill",
                "vertical_alignment": "Fill",
            })

    add_batch = _ok(_send("add_widget_child_batch", {
        "blueprint_name": bp,
        "items": add_items,
    }, timeout=300.0), "add_widget_child_batch(90 cells)")

    grid_batch = _ok(_send("set_uniform_grid_slot_batch", {
        "blueprint_name": bp,
        "items": grid_items,
    }, timeout=300.0), "set_uniform_grid_slot_batch(90 cells)")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree")
    root = tree.get("root") or {}
    board = _find(root, "BoardGrid")
    if not board:
        raise RuntimeError("BoardGrid not found")

    board_children = _children_names(board)
    if len(board_children) != rows * cols:
        raise RuntimeError(f"BoardGrid child count mismatch: {len(board_children)} != {rows * cols}")

    grid_results = {item["widget_name"]: item for item in (grid_batch.get("results") or [])}
    for probe, expected in {
        "Cell_0_0": (0, 0),
        "Cell_0_8": (0, 8),
        "Cell_9_0": (9, 0),
        "Cell_9_8": (9, 8),
    }.items():
        item = grid_results.get(probe)
        if not item:
            raise RuntimeError(f"Missing grid readback for {probe}")
        if (item.get("row"), item.get("column")) != expected:
            raise RuntimeError(f"{probe} row/column mismatch: {item}")

    add_results = add_batch.get("results") or []
    summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "add_batch": {
            "created_count": add_batch.get("created_count"),
            "first": add_results[0] if add_results else None,
            "last": add_results[-1] if add_results else None,
        },
        "grid_batch": {
            "updated_count": grid_batch.get("updated_count"),
            "spot_check": {
                "Cell_0_0": {"row": grid_results["Cell_0_0"]["row"], "column": grid_results["Cell_0_0"]["column"]},
                "Cell_9_8": {"row": grid_results["Cell_9_8"]["row"], "column": grid_results["Cell_9_8"]["column"]},
            },
        },
        "tree": {
            "root_children": _children_names(root),
            "board_child_count": len(board_children),
            "first_cells": board_children[:5],
            "last_cells": board_children[-5:],
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

