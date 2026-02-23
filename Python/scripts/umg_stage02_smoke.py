"""
Stage 2 smoke test for UMG Designer automation:
1) create a probe Widget Blueprint
2) ensure/replace root widget
3) add generic child widgets (panel + leaf widgets)
4) read back widget tree and verify hierarchy

Usage:
  python Python/scripts/umg_stage02_smoke.py
"""

from __future__ import annotations

import json
import socket
from datetime import datetime


def _send_unreal_command(command: str, params: dict) -> dict:
    payload = json.dumps({"type": command, "params": params}).encode("utf-8")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(30)
        sock.connect(("127.0.0.1", 55557))
        sock.sendall(payload)
        chunks: list[bytes] = []
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
            try:
                return json.loads(b"".join(chunks).decode("utf-8"))
            except json.JSONDecodeError:
                continue
    if not chunks:
        raise RuntimeError(f"{command}: empty response")
    return json.loads(b"".join(chunks).decode("utf-8"))


def _require_success(response: dict, command_name: str) -> dict:
    if not response:
        raise RuntimeError(f"{command_name}: no response")
    if response.get("status") == "error":
        raise RuntimeError(f"{command_name}: {response.get('error', 'unknown error')}")
    result = response.get("result")
    if not isinstance(result, dict):
        raise RuntimeError(f"{command_name}: malformed response (missing result object)")
    if result.get("success") is False:
        raise RuntimeError(f"{command_name}: result.success=false ({result})")
    return result


def _find_node(node: dict | None, target_name: str) -> dict | None:
    if not isinstance(node, dict):
        return None
    if node.get("name") == target_name:
        return node
    for child in node.get("children") or []:
        found = _find_node(child, target_name)
        if found:
            return found
    return None


def main() -> int:
    widget_name = f"WBP_McpUmgProbeS2_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    asset_path = f"/Game/UI/{widget_name}"

    create_result = _require_success(
        _send_unreal_command(
            "create_umg_widget_blueprint",
            {"widget_name": widget_name, "path": "/Game/UI", "parent_class": "UserWidget"},
        ),
        "create_umg_widget_blueprint",
    )

    ensure_result = _require_success(
        _send_unreal_command(
            "ensure_widget_root",
            {
                "blueprint_name": create_result.get("path", asset_path),
                "widget_class": "CanvasPanel",
                "widget_name": "RootCanvas",
                "replace_existing": True,
            },
        ),
        "ensure_widget_root",
    )

    for command_name, params in [
        (
            "add_widget_child",
            {
                "blueprint_name": create_result.get("path", asset_path),
                "parent_widget_name": "RootCanvas",
                "widget_class": "VerticalBox",
                "widget_name": "DebugPanel",
            },
        ),
        (
            "add_widget_child",
            {
                "blueprint_name": create_result.get("path", asset_path),
                "parent_widget_name": "DebugPanel",
                "widget_class": "TextBlock",
                "widget_name": "TxtStatus",
            },
        ),
        (
            "add_widget_child",
            {
                "blueprint_name": create_result.get("path", asset_path),
                "parent_widget_name": "DebugPanel",
                "widget_class": "Button",
                "widget_name": "BtnAction",
            },
        ),
    ]:
        _require_success(_send_unreal_command(command_name, params), command_name)

    tree_result = _require_success(
        _send_unreal_command("get_widget_tree", {"blueprint_name": create_result.get("path", asset_path)}),
        "get_widget_tree",
    )

    root = tree_result.get("root") or {}
    debug_panel = _find_node(root, "DebugPanel")
    txt_status = _find_node(root, "TxtStatus")
    btn_action = _find_node(root, "BtnAction")

    if root.get("name") != "RootCanvas":
        raise RuntimeError(f"Unexpected root name: {root.get('name')}")
    if debug_panel is None or txt_status is None or btn_action is None:
        raise RuntimeError("Expected widget hierarchy not found in get_widget_tree result")

    summary = {
        "created": {
            "widget_name": create_result.get("widget_name", widget_name),
            "path": create_result.get("path", asset_path),
        },
        "ensure_root": {
            "created": ensure_result.get("created"),
            "replaced": ensure_result.get("replaced"),
            "root_name": (ensure_result.get("root") or {}).get("name"),
            "root_class": (ensure_result.get("root") or {}).get("class"),
        },
        "tree": {
            "root_name": root.get("name"),
            "root_class": root.get("class"),
            "root_children": [child.get("name") for child in root.get("children") or []],
            "debug_panel_children": [child.get("name") for child in debug_panel.get("children") or []],
            "txt_status_slot": txt_status.get("slot_class"),
            "btn_action_slot": btn_action.get("slot_class"),
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
