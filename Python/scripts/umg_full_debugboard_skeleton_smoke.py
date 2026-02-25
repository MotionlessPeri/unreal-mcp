"""
Composite smoke test for a full DebugBoard skeleton (9x10 board + side panel)
using current UMG Route B primitives and batch helpers.

Builds a disposable WidgetBlueprint with:
1) Root CanvasPanel
2) BoardGrid (UniformGridPanel) + SidePanel (VerticalBox) laid out via canvas batch layout
3) Full 9x10 board cells (90 Border widgets) added via add_widget_child_batch
4) 90 UniformGridSlot row/column assignments via set_uniform_grid_slot_batch
5) Side-panel status TextBlocks + Buttons + button label TextBlocks
6) Batch updates for TextBlock text/color and common widget visibility/is_enabled
7) Widget-tree readback validation for hierarchy and counts
"""

from __future__ import annotations

import json
import socket
from datetime import datetime


def _send(command: str, params: dict, timeout: float = 300.0) -> dict:
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
    probe_widget_name = f"WBP_McpFullDebugBoardProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    bp_path = f"/Game/UI/{probe_widget_name}"
    board_rows = 10
    board_cols = 9

    status_texts = [
        "TxtTitle",
        "TxtPhase",
        "TxtTurn",
        "TxtRedCursor",
        "TxtBlackCursor",
        "TxtLastAck",
        "TxtGameOver",
    ]
    action_buttons = [
        "BtnJoin",
        "BtnPullRed",
        "BtnPullBlack",
        "BtnCommitReveal",
        "BtnRedMove",
        "BtnBlackResign",
    ]

    create = _ok(_send("create_umg_widget_blueprint", {
        "widget_name": probe_widget_name,
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

    # Root sections
    _ok(_send("add_widget_child_batch", {
        "blueprint_name": bp,
        "items": [
            {"parent_widget_name": "RootCanvas", "widget_class": "UniformGridPanel", "widget_name": "BoardGrid"},
            {"parent_widget_name": "RootCanvas", "widget_class": "VerticalBox", "widget_name": "SidePanel"},
        ],
    }), "add_widget_child_batch(root sections)")

    canvas_batch = _ok(_send("set_canvas_slot_layout_batch", {
        "blueprint_name": bp,
        "items": [
            {"widget_name": "BoardGrid", "position": [40, 40], "size": [630, 700], "z_order": 1},
            {"widget_name": "SidePanel", "position": [700, 40], "size": [320, 700], "z_order": 2},
        ],
    }), "set_canvas_slot_layout_batch(root sections)")

    # Side panel hierarchy (status texts + buttons)
    side_items = []
    for name in status_texts:
        side_items.append({"parent_widget_name": "SidePanel", "widget_class": "TextBlock", "widget_name": name})
    for name in action_buttons:
        side_items.append({"parent_widget_name": "SidePanel", "widget_class": "Button", "widget_name": name})
    _ok(_send("add_widget_child_batch", {
        "blueprint_name": bp,
        "items": side_items,
    }), "add_widget_child_batch(side panel items)")

    # Add button labels as children of each button
    label_items = []
    for btn_name in action_buttons:
        label_items.append({
            "parent_widget_name": btn_name,
            "widget_class": "TextBlock",
            "widget_name": f"{btn_name}_Label",
        })
    _ok(_send("add_widget_child_batch", {
        "blueprint_name": bp,
        "items": label_items,
    }), "add_widget_child_batch(button labels)")

    _ok(_send("set_widget_common_properties_batch", {
        "blueprint_name": bp,
        "items": (
            [{"widget_name": name, "visibility": "Visible"} for name in status_texts]
            + [{"widget_name": "SidePanel", "visibility": "Visible"}]
            + [{"widget_name": "BoardGrid", "visibility": "Visible"}]
            + [
                {"widget_name": "BtnJoin", "visibility": "Visible", "is_enabled": True},
                {"widget_name": "BtnPullRed", "visibility": "Visible", "is_enabled": True},
                {"widget_name": "BtnPullBlack", "visibility": "Visible", "is_enabled": True},
                {"widget_name": "BtnCommitReveal", "visibility": "Visible", "is_enabled": False},
                {"widget_name": "BtnRedMove", "visibility": "Visible", "is_enabled": False},
                {"widget_name": "BtnBlackResign", "visibility": "Visible", "is_enabled": False},
            ]
        ),
    }), "set_widget_common_properties_batch(side panel/common)")

    text_items = [
        {"widget_name": "TxtTitle", "text": "DebugBoard Skeleton (Auto)", "color": [0.95, 0.95, 1.0, 1.0]},
        {"widget_name": "TxtPhase", "text": "Phase: SetupCommit", "color": [0.85, 0.9, 0.95, 1.0]},
        {"widget_name": "TxtTurn", "text": "Turn: Red", "color": [1.0, 0.55, 0.55, 1.0]},
        {"widget_name": "TxtRedCursor", "text": "RedCursor: 0", "color": [1.0, 0.4, 0.4, 1.0]},
        {"widget_name": "TxtBlackCursor", "text": "BlackCursor: 0", "color": [0.7, 0.7, 0.7, 1.0]},
        {"widget_name": "TxtLastAck", "text": "LastAck: <none>", "color": [0.85, 0.85, 0.85, 1.0]},
        {"widget_name": "TxtGameOver", "text": "GameOver: <none>", "color": [0.9, 0.9, 0.7, 1.0]},
    ]
    for btn_name in action_buttons:
        text_items.append({
            "widget_name": f"{btn_name}_Label",
            "text": btn_name.replace("Btn", ""),
            "color": [1.0, 1.0, 1.0, 1.0],
        })
    text_batch = _ok(_send("set_text_block_properties_batch", {
        "blueprint_name": bp,
        "items": text_items,
    }), "set_text_block_properties_batch(side texts + labels)")

    # Full board (90 cells)
    add_cell_items = []
    grid_slot_items = []
    for row in range(board_rows):
        for col in range(board_cols):
            cell_name = f"Cell_{row}_{col}"
            add_cell_items.append({
                "parent_widget_name": "BoardGrid",
                "widget_class": "Border",
                "widget_name": cell_name,
            })
            grid_slot_items.append({
                "widget_name": cell_name,
                "row": row,
                "column": col,
                "horizontal_alignment": "Fill",
                "vertical_alignment": "Fill",
            })

    add_cells_batch = _ok(_send("add_widget_child_batch", {
        "blueprint_name": bp,
        "items": add_cell_items,
    }, timeout=600.0), "add_widget_child_batch(board cells)")

    grid_batch = _ok(_send("set_uniform_grid_slot_batch", {
        "blueprint_name": bp,
        "items": grid_slot_items,
    }, timeout=600.0), "set_uniform_grid_slot_batch(board cells)")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}, timeout=300.0), "get_widget_tree")
    root = tree.get("root") or {}
    board = _find(root, "BoardGrid")
    side = _find(root, "SidePanel")
    if not board or not side:
        raise RuntimeError("BoardGrid or SidePanel missing in widget tree")

    board_children = _children_names(board)
    side_children = _children_names(side)
    if len(board_children) != board_rows * board_cols:
        raise RuntimeError(f"BoardGrid child count mismatch: {len(board_children)} != {board_rows * board_cols}")

    expected_side_children = set(status_texts + action_buttons)
    if not expected_side_children.issubset(set(side_children)):
        raise RuntimeError(f"SidePanel missing children: {sorted(expected_side_children - set(side_children))}")

    grid_results = {item["widget_name"]: item for item in (grid_batch.get("results") or [])}
    for probe_name, expected in {
        "Cell_0_0": (0, 0),
        "Cell_0_8": (0, 8),
        "Cell_9_0": (9, 0),
        "Cell_9_8": (9, 8),
    }.items():
        item = grid_results.get(probe_name)
        if not item:
            raise RuntimeError(f"Missing grid batch readback for {probe_name}")
        if (item.get("row"), item.get("column")) != expected:
            raise RuntimeError(f"{probe_name} row/column mismatch: {item}")

    text_results = {item["widget_name"]: item for item in (text_batch.get("results") or [])}
    if text_results.get("TxtTitle", {}).get("text") != "DebugBoard Skeleton (Auto)":
        raise RuntimeError(f"TxtTitle text mismatch: {text_results.get('TxtTitle')}")
    if text_results.get("BtnJoin_Label", {}).get("text") != "Join":
        raise RuntimeError(f"BtnJoin_Label text mismatch: {text_results.get('BtnJoin_Label')}")

    canvas_results = {item["widget_name"]: item for item in (canvas_batch.get("results") or [])}
    summary = {
        "created": {"widget_name": probe_widget_name, "path": bp},
        "root_children": _children_names(root),
        "board": {
            "rows": board_rows,
            "cols": board_cols,
            "cell_count": len(board_children),
            "add_batch_created_count": add_cells_batch.get("created_count"),
            "grid_batch_updated_count": grid_batch.get("updated_count"),
            "first_cells": board_children[:4],
            "last_cells": board_children[-4:],
        },
        "side_panel": {
            "child_count": len(side_children),
            "status_text_count": len(status_texts),
            "button_count": len(action_buttons),
            "contains": ["TxtTitle", "BtnJoin", "BtnBlackResign"],
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
        "text_batch": {
            "updated_count": text_batch.get("updated_count"),
            "spot_check": {
                "TxtTitle": text_results["TxtTitle"]["text"],
                "BtnJoin_Label": text_results["BtnJoin_Label"]["text"],
            },
        },
        "grid_spot_check": {
            "Cell_0_0": {"row": grid_results["Cell_0_0"]["row"], "column": grid_results["Cell_0_0"]["column"]},
            "Cell_9_8": {"row": grid_results["Cell_9_8"]["row"], "column": grid_results["Cell_9_8"]["column"]},
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

