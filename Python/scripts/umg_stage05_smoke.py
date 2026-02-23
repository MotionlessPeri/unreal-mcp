"""
Stage 5 smoke test for widget cleanup / repeatability commands:
1) create probe widget and root
2) build a simple hierarchy
3) clear/rebuild same hierarchy (no duplicates)
4) remove a subtree widget and verify readback
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


def _children_names(node: dict | None) -> list[str]:
    if not isinstance(node, dict):
        return []
    return [child.get("name") for child in (node.get("children") or [])]


def _build_basic_panel(bp: str, pass_label: str) -> dict:
    clear_root = _ok(_send("clear_widget_children", {
        "blueprint_name": bp,
        "widget_name": "RootCanvas",
    }), f"clear_widget_children(RootCanvas,{pass_label})")

    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "RootCanvas",
        "widget_class": "VerticalBox", "widget_name": "SidePanel",
    }), f"add_widget_child(SidePanel,{pass_label})")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "SidePanel",
        "widget_class": "TextBlock", "widget_name": "TxtStatus",
    }), f"add_widget_child(TxtStatus,{pass_label})")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "SidePanel",
        "widget_class": "Button", "widget_name": "BtnAction",
    }), f"add_widget_child(BtnAction,{pass_label})")
    _ok(_send("add_widget_child", {
        "blueprint_name": bp, "parent_widget_name": "BtnAction",
        "widget_class": "TextBlock", "widget_name": "TxtButtonLabel",
    }), f"add_widget_child(TxtButtonLabel,{pass_label})")

    _ok(_send("set_canvas_slot_layout", {
        "blueprint_name": bp,
        "widget_name": "SidePanel",
        "position": [620, 40],
        "size": [260, 600],
        "z_order": 2,
    }), f"set_canvas_slot_layout(SidePanel,{pass_label})")

    txt_status = _ok(_send("set_text_block_properties", {
        "blueprint_name": bp,
        "widget_name": "TxtStatus",
        "text": f"Status {pass_label}",
        "color": [0.9, 0.9, 0.2, 1.0],
    }), f"set_text_block_properties(TxtStatus,{pass_label})")
    btn_label = _ok(_send("set_text_block_properties", {
        "blueprint_name": bp,
        "widget_name": "TxtButtonLabel",
        "text": f"Run {pass_label}",
        "color": [1.0, 1.0, 1.0, 1.0],
    }), f"set_text_block_properties(TxtButtonLabel,{pass_label})")

    tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), f"get_widget_tree({pass_label})")
    root = tree.get("root") or {}
    side_panel = _find(root, "SidePanel")
    btn_action = _find(root, "BtnAction")
    txt_status_node = _find(root, "TxtStatus")
    btn_label_node = _find(root, "TxtButtonLabel")
    if not all([side_panel, btn_action, txt_status_node, btn_label_node]):
        raise RuntimeError(f"{pass_label}: expected widgets not found")

    if _children_names(root).count("SidePanel") != 1:
        raise RuntimeError(f"{pass_label}: duplicate SidePanel detected")
    if _children_names(side_panel) != ["TxtStatus", "BtnAction"]:
        raise RuntimeError(f"{pass_label}: unexpected SidePanel children order {_children_names(side_panel)}")
    if _children_names(btn_action) != ["TxtButtonLabel"]:
        raise RuntimeError(f"{pass_label}: unexpected BtnAction children {_children_names(btn_action)}")

    return {
        "clear_root": {
            "removed_direct_children": clear_root.get("removed_direct_children"),
            "removed_total_widgets": clear_root.get("removed_total_widgets"),
        },
        "text_readback": {
            "TxtStatus": txt_status.get("text"),
            "TxtButtonLabel": btn_label.get("text"),
        },
        "tree": {
            "root_children": _children_names(root),
            "side_panel_children": _children_names(side_panel),
            "button_children": _children_names(btn_action),
        },
    }


def main() -> int:
    widget_name = f"WBP_McpUmgProbeS5_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
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

    first_build = _build_basic_panel(bp, "Pass1")

    remove_btn = _ok(_send("remove_widget_from_blueprint", {
        "blueprint_name": bp,
        "widget_name": "BtnAction",
    }), "remove_widget_from_blueprint(BtnAction)")

    tree_after_remove = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree(after_remove)")
    root_after_remove = tree_after_remove.get("root") or {}
    side_after_remove = _find(root_after_remove, "SidePanel")
    if _find(root_after_remove, "BtnAction") is not None or _find(root_after_remove, "TxtButtonLabel") is not None:
        raise RuntimeError("remove_widget_from_blueprint did not remove button subtree")
    if not side_after_remove or _children_names(side_after_remove) != ["TxtStatus"]:
        raise RuntimeError(f"Unexpected SidePanel children after remove: {_children_names(side_after_remove)}")

    second_build = _build_basic_panel(bp, "Pass2")

    final_tree = _ok(_send("get_widget_tree", {"blueprint_name": bp}), "get_widget_tree(final)")
    root_final = final_tree.get("root") or {}
    side_final = _find(root_final, "SidePanel")
    btn_final = _find(root_final, "BtnAction")
    final_summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "first_build": first_build,
        "remove_btn_subtree": {
            "removed_total_widgets": remove_btn.get("removed_total_widgets"),
            "parent_widget_name": remove_btn.get("parent_widget_name"),
            "side_panel_children_after_remove": _children_names(side_after_remove),
        },
        "second_build": second_build,
        "final_tree": {
            "root_children": _children_names(root_final),
            "side_panel_children": _children_names(side_final),
            "button_children": _children_names(btn_final),
        },
    }
    print(json.dumps(final_summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

