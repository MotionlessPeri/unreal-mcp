"""
Smoke test for set_widget_common_properties_batch:
1) create probe widget and simple hierarchy
2) batch-apply visibility/is_enabled for multiple widgets
3) verify per-item readback values
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


def main() -> int:
    widget_name = f"WBP_McpWidgetCommonBatchProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
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

    for widget_class, widget_inst in [
        ("TextBlock", "TxtStatus"),
        ("Button", "BtnAction"),
        ("Border", "PanelChrome"),
    ]:
        _ok(_send("add_widget_child", {
            "blueprint_name": bp,
            "parent_widget_name": "RootCanvas",
            "widget_class": widget_class,
            "widget_name": widget_inst,
        }), f"add_widget_child({widget_inst})")

    batch = _ok(_send("set_widget_common_properties_batch", {
        "blueprint_name": bp,
        "items": [
            {"widget_name": "TxtStatus", "visibility": "Visible", "is_enabled": True},
            {"widget_name": "BtnAction", "visibility": "Visible", "is_enabled": False},
            {"widget_name": "PanelChrome", "visibility": "SelfHitTestInvisible"},
        ],
    }), "set_widget_common_properties_batch")

    results = {item["widget_name"]: item for item in (batch.get("results") or [])}
    expected = {"TxtStatus", "BtnAction", "PanelChrome"}
    if set(results.keys()) != expected:
        raise RuntimeError(f"Unexpected batch result keys: {sorted(results.keys())}")

    if results["TxtStatus"]["visibility"] != "Visible" or results["TxtStatus"]["is_enabled"] is not True:
        raise RuntimeError(f"TxtStatus readback mismatch: {results['TxtStatus']}")
    if results["BtnAction"]["visibility"] != "Visible" or results["BtnAction"]["is_enabled"] is not False:
        raise RuntimeError(f"BtnAction readback mismatch: {results['BtnAction']}")
    if results["PanelChrome"]["visibility"] != "SelfHitTestInvisible":
        raise RuntimeError(f"PanelChrome visibility mismatch: {results['PanelChrome']}")

    summary = {
        "created": {"widget_name": widget_name, "path": bp},
        "batch": {
            "updated_count": batch.get("updated_count"),
            "results": batch.get("results"),
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

