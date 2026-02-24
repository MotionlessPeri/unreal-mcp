"""
Smoke test for set_canvas_slot_layout_batch:
1) create probe widget and simple hierarchy under root canvas
2) batch-apply layout for multiple canvas children
3) verify per-item readback values and widget tree
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
    widget_name = f"WBP_McpCanvasBatchProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
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
        "widget_class": "Border", "widget_name": "PanelA",
    }), "add_widget_child(PanelA)")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "RootCanvas",
        "widget_class": "Border", "widget_name": "PanelB",
    }), "add_widget_child(PanelB)")

    batch = _ok(_send("set_canvas_slot_layout_batch", {
        "blueprint_name": bp,
        "items": [
            {"widget_name": "PanelA", "position": [40, 50], "size": [300, 200], "z_order": 1},
            {"widget_name": "PanelB", "position": [380, 50], "size": [260, 200], "z_order": 2},
        ],
    }), "set_canvas_slot_layout_batch")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree")
    root = tree.get("root") or {}
    panel_a = _find(root, "PanelA")
    panel_b = _find(root, "PanelB")
    if not panel_a or not panel_b:
        raise RuntimeError("Expected PanelA/PanelB not found in widget tree")

    results = {item["widget_name"]: item for item in (batch.get("results") or [])}
    if set(results.keys()) != {"PanelA", "PanelB"}:
        raise RuntimeError(f"Unexpected batch readback keys: {sorted(results.keys())}")

    summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "batch": {
            "updated_count": batch.get("updated_count"),
            "PanelA": {
                "position": results["PanelA"].get("position"),
                "size": results["PanelA"].get("size"),
                "z_order": results["PanelA"].get("z_order"),
            },
            "PanelB": {
                "position": results["PanelB"].get("position"),
                "size": results["PanelB"].get("size"),
                "z_order": results["PanelB"].get("z_order"),
            },
        },
        "tree": {
            "root_children": [child.get("name") for child in (root.get("children") or [])],
            "panel_a_slot": panel_a.get("slot_class"),
            "panel_b_slot": panel_b.get("slot_class"),
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

