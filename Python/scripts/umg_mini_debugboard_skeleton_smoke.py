"""
Composite smoke test for a mini DebugBoard skeleton using existing UMG Route B primitives.

Builds a skeleton widget with:
1) Root CanvasPanel
2) BoardGrid (UniformGridPanel) laid out via set_canvas_slot_layout_batch
3) SidePanel (VerticalBox) laid out via set_canvas_slot_layout_batch
4) A mini board sample (4x4 cells) added to BoardGrid and laid out via set_uniform_grid_slot_batch
5) Side panel texts/buttons with batch common properties + text labels
6) Widget-tree readback validation

Note:
This is intentionally a mini sample, not a full 9x10 board. Repeated single-node
`add_widget_child` calls compile/save on every invocation and currently hit UMG GUID
ensure issues at larger scales (e.g. 90 cells) in consumer projects. This smoke
validates composition with current primitives and highlights the next capability gap
(batch child creation / deferred compile).
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
    widget_name = f"WBP_McpMiniDebugBoardProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    bp_path = f"/Game/UI/{widget_name}"
    board_cols = 4
    board_rows = 4

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
    }), "clear_widget_children(RootCanvas)")

    # Root children: board + side panel
    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "RootCanvas",
        "widget_class": "UniformGridPanel", "widget_name": "BoardGrid",
    }), "add_widget_child(BoardGrid)")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "RootCanvas",
        "widget_class": "VerticalBox", "widget_name": "SidePanel",
    }), "add_widget_child(SidePanel)")

    canvas_batch = _ok(_send("set_canvas_slot_layout_batch", {
        "blueprint_name": bp,
        "items": [
            {"widget_name": "BoardGrid", "position": [40, 40], "size": [540, 600], "z_order": 1},
            {"widget_name": "SidePanel", "position": [620, 40], "size": [260, 600], "z_order": 2},
        ],
    }), "set_canvas_slot_layout_batch(root children)")

    # Side panel content
    side_texts = ["TxtTitle", "TxtPhase", "TxtTurn", "TxtLastAck"]
    side_buttons = ["BtnJoin", "BtnPull", "BtnCommit", "BtnMove"]
    for name in side_texts:
        _ok(_send("add_widget_child", {
            "blueprint_name": bp, "parent_widget_name": "SidePanel",
            "widget_class": "TextBlock", "widget_name": name,
        }), f"add_widget_child({name})")
    for name in side_buttons:
        _ok(_send("add_widget_child", {
            "blueprint_name": bp, "parent_widget_name": "SidePanel",
            "widget_class": "Button", "widget_name": name,
        }), f"add_widget_child({name})")
        _ok(_send("add_widget_child", {
            "blueprint_name": bp, "parent_widget_name": name,
            "widget_class": "TextBlock", "widget_name": f"{name}_Label",
        }), f"add_widget_child({name}_Label)")

    _ok(_send("set_widget_common_properties_batch", {
        "blueprint_name": bp,
        "items": [
            {"widget_name": "TxtTitle", "visibility": "Visible"},
            {"widget_name": "TxtPhase", "visibility": "Visible"},
            {"widget_name": "TxtTurn", "visibility": "Visible"},
            {"widget_name": "TxtLastAck", "visibility": "Visible"},
            {"widget_name": "BtnJoin", "visibility": "Visible", "is_enabled": True},
            {"widget_name": "BtnPull", "visibility": "Visible", "is_enabled": True},
            {"widget_name": "BtnCommit", "visibility": "Visible", "is_enabled": False},
            {"widget_name": "BtnMove", "visibility": "Visible", "is_enabled": False},
        ],
    }), "set_widget_common_properties_batch(side panel)")

    text_values = {
        "TxtTitle": "Mini DebugBoard",
        "TxtPhase": "Phase: SetupCommit",
        "TxtTurn": "Turn: Red",
        "TxtLastAck": "LastAck: <none>",
        "BtnJoin_Label": "Join",
        "BtnPull_Label": "Pull",
        "BtnCommit_Label": "Commit",
        "BtnMove_Label": "Move",
    }
    for name, text in text_values.items():
        _ok(_send("set_text_block_properties", {
            "blueprint_name": bp,
            "widget_name": name,
            "text": text,
            "color": [1.0, 1.0, 1.0, 1.0] if name.endswith("_Label") else [0.9, 0.9, 0.9, 1.0],
        }), f"set_text_block_properties({name})")

    # Board sample cells: 4x4 = 16 cells
    grid_items = []
    cell_names = []
    for row in range(board_rows):
        for col in range(board_cols):
            cell_name = f"Cell_{row}_{col}"
            cell_names.append(cell_name)
            _ok(_send("add_widget_child", {
                "blueprint_name": bp,
                "parent_widget_name": "BoardGrid",
                "widget_class": "Border",
                "widget_name": cell_name,
            }, timeout=300.0), f"add_widget_child({cell_name})")
            grid_items.append({
                "widget_name": cell_name,
                "row": row,
                "column": col,
                "horizontal_alignment": "Fill",
                "vertical_alignment": "Fill",
            })

    grid_batch = _ok(_send("set_uniform_grid_slot_batch", {
        "blueprint_name": bp,
        "items": grid_items,
    }, timeout=300.0), "set_uniform_grid_slot_batch(board cells)")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree")
    root = tree.get("root") or {}
    board = _find(root, "BoardGrid")
    side = _find(root, "SidePanel")
    if not board or not side:
        raise RuntimeError("BoardGrid or SidePanel missing in widget tree")

    board_children = _children_names(board)
    side_children = _children_names(side)
    if len(board_children) != board_rows * board_cols:
        raise RuntimeError(f"BoardGrid child count mismatch: {len(board_children)}")
    if "TxtTitle" not in side_children or "BtnJoin" not in side_children:
        raise RuntimeError(f"SidePanel children unexpected: {side_children}")

    grid_results = {item["widget_name"]: item for item in (grid_batch.get("results") or [])}
    for probe_name, expected in {
        "Cell_0_0": (0, 0),
        "Cell_0_3": (0, 3),
        "Cell_3_0": (3, 0),
        "Cell_3_3": (3, 3),
    }.items():
        item = grid_results.get(probe_name)
        if not item:
            raise RuntimeError(f"Missing grid batch readback for {probe_name}")
        if (item.get("row"), item.get("column")) != expected:
            raise RuntimeError(f"{probe_name} row/column mismatch: {item}")

    canvas_results = {item["widget_name"]: item for item in (canvas_batch.get("results") or [])}
    summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "root_children": _children_names(root),
        "board": {
            "rows": board_rows,
            "cols": board_cols,
            "cell_count": len(board_children),
            "first_cells": board_children[:5],
            "last_cells": board_children[-5:],
            "batch_updated_count": grid_batch.get("updated_count"),
        },
        "side_panel": {
            "child_count": len(side_children),
            "children": side_children,
        },
        "canvas_layout": {
            "BoardGrid": {
                "position": canvas_results["BoardGrid"].get("position"),
                "size": canvas_results["BoardGrid"].get("size"),
                "z_order": canvas_results["BoardGrid"].get("z_order"),
            },
            "SidePanel": {
                "position": canvas_results["SidePanel"].get("position"),
                "size": canvas_results["SidePanel"].get("size"),
                "z_order": canvas_results["SidePanel"].get("z_order"),
            },
        },
        "spot_check": {
            "Cell_0_0": {"row": grid_results["Cell_0_0"]["row"], "column": grid_results["Cell_0_0"]["column"]},
            "Cell_3_3": {"row": grid_results["Cell_3_3"]["row"], "column": grid_results["Cell_3_3"]["column"]},
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
