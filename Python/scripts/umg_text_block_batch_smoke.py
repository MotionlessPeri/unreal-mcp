"""
Smoke test for set_text_block_properties_batch:
1) create probe widget and simple hierarchy with multiple TextBlocks
2) batch-apply text/color to all TextBlocks
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
    probe_widget_name = f"WBP_McpTextBatchProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    bp_path = f"/Game/UI/{probe_widget_name}"

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
    }), "clear_widget_children")

    for idx in range(3):
        _ok(_send("add_widget_child", {
            "blueprint_name": bp,
            "parent_widget_name": "RootCanvas",
            "widget_class": "TextBlock",
            "widget_name": f"Txt{idx}",
        }), f"add_widget_child(Txt{idx})")

    batch = _ok(_send("set_text_block_properties_batch", {
        "blueprint_name": bp,
        "items": [
            {"widget_name": "Txt0", "text": "Alpha", "color": [1.0, 0.0, 0.0, 1.0]},
            {"widget_name": "Txt1", "text": "Beta", "color": [0.0, 1.0, 0.0, 1.0]},
            {"widget_name": "Txt2", "text": "Gamma", "color": [0.0, 0.0, 1.0, 1.0]},
        ],
    }), "set_text_block_properties_batch")

    results = {item["widget_name"]: item for item in (batch.get("results") or [])}
    expected_names = {"Txt0", "Txt1", "Txt2"}
    if set(results.keys()) != expected_names:
        raise RuntimeError(f"Unexpected batch result keys: {sorted(results.keys())}")

    expected = {
        "Txt0": ("Alpha", [1.0, 0.0, 0.0, 1.0]),
        "Txt1": ("Beta", [0.0, 1.0, 0.0, 1.0]),
        "Txt2": ("Gamma", [0.0, 0.0, 1.0, 1.0]),
    }
    for text_widget_name, (text, color) in expected.items():
        item = results[text_widget_name]
        if item.get("text") != text:
            raise RuntimeError(f"{text_widget_name} text mismatch: {item}")
        read_color = item.get("color")
        if not isinstance(read_color, list) or len(read_color) != 4:
            raise RuntimeError(f"{text_widget_name} color malformed: {item}")
        for a, b in zip(read_color, color):
            if abs(float(a) - float(b)) > 1e-3:
                raise RuntimeError(f"{text_widget_name} color mismatch: {item}")

    summary = {
        "created": {"widget_name": probe_widget_name, "path": bp},
        "batch": {
            "updated_count": batch.get("updated_count"),
            "results": batch.get("results"),
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
