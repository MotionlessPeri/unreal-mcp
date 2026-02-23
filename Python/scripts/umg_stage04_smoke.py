"""
Stage 4 smoke test for common widget property setters:
1) create probe widget
2) build simple hierarchy (canvas -> side panel -> text/button)
3) set canvas layout + widget common properties + textblock properties
4) read tree and print property readback
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
    widget_name = f"WBP_McpUmgProbeS4_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
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
        "widget_class": "VerticalBox",
        "widget_name": "SidePanel",
    }), "add_widget_child(SidePanel)")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp,
        "parent_widget_name": "SidePanel",
        "widget_class": "TextBlock",
        "widget_name": "TxtStatus",
    }), "add_widget_child(TxtStatus)")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp,
        "parent_widget_name": "SidePanel",
        "widget_class": "Button",
        "widget_name": "BtnAction",
    }), "add_widget_child(BtnAction)")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp,
        "parent_widget_name": "BtnAction",
        "widget_class": "TextBlock",
        "widget_name": "TxtButtonLabel",
    }), "add_widget_child(TxtButtonLabel)")

    side_canvas = _ok(_send("set_canvas_slot_layout", {
        "blueprint_name": bp,
        "widget_name": "SidePanel",
        "position": [620, 40],
        "size": [260, 600],
        "z_order": 2,
    }), "set_canvas_slot_layout(SidePanel)")

    txt_common = _ok(_send("set_widget_common_properties", {
        "blueprint_name": bp,
        "widget_name": "TxtStatus",
        "visibility": "Visible",
        "is_enabled": True,
    }), "set_widget_common_properties(TxtStatus)")
    btn_common = _ok(_send("set_widget_common_properties", {
        "blueprint_name": bp,
        "widget_name": "BtnAction",
        "visibility": "Visible",
        "is_enabled": False,
    }), "set_widget_common_properties(BtnAction)")

    txt_props = _ok(_send("set_text_block_properties", {
        "blueprint_name": bp,
        "widget_name": "TxtStatus",
        "text": "Stage4 smoke status",
        "color": [0.2, 1.0, 0.4, 1.0],
    }), "set_text_block_properties(TxtStatus)")
    btn_label_props = _ok(_send("set_text_block_properties", {
        "blueprint_name": bp,
        "widget_name": "TxtButtonLabel",
        "text": "Run",
        "color": [1.0, 1.0, 1.0, 1.0],
    }), "set_text_block_properties(TxtButtonLabel)")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree")
    root = tree.get("root") or {}
    side_panel = _find(root, "SidePanel")
    txt_status = _find(root, "TxtStatus")
    btn_action = _find(root, "BtnAction")
    btn_label = _find(root, "TxtButtonLabel")
    if not all([side_panel, txt_status, btn_action, btn_label]):
        raise RuntimeError("Expected Stage4 smoke widgets not found in tree")

    summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "layout": {
            "SidePanel": {
                "position": side_canvas.get("position"),
                "size": side_canvas.get("size"),
                "z_order": side_canvas.get("z_order"),
            }
        },
        "common_props": {
            "TxtStatus": {"visibility": txt_common.get("visibility"), "is_enabled": txt_common.get("is_enabled")},
            "BtnAction": {"visibility": btn_common.get("visibility"), "is_enabled": btn_common.get("is_enabled")},
        },
        "text_props": {
            "TxtStatus": {"text": txt_props.get("text"), "color": txt_props.get("color")},
            "TxtButtonLabel": {"text": btn_label_props.get("text"), "color": btn_label_props.get("color")},
        },
        "tree": {
            "root_children": [child.get("name") for child in root.get("children") or []],
            "side_panel_children": [child.get("name") for child in side_panel.get("children") or []],
            "button_children": [child.get("name") for child in btn_action.get("children") or []],
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

