"""
Stage 0 + 1 smoke test for UMG automation:
1) create a probe Widget Blueprint
2) read back widget tree (root hierarchy)

Usage examples:
  python Python/scripts/umg_stage01_smoke.py
  python Python/scripts/umg_stage01_smoke.py --widget-name WBP_McpUmgProbe --path /Game
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
from datetime import datetime
from pathlib import Path


def _require_success(response: dict, command_name: str) -> dict:
    if not response:
        raise RuntimeError(f"{command_name}: no response")
    if response.get("status") == "error":
        raise RuntimeError(f"{command_name}: {response.get('error', 'unknown error')}")
    result = response.get("result")
    if not isinstance(result, dict):
        raise RuntimeError(f"{command_name}: malformed response (missing result object)")
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="UMG Stage0+1 smoke test (create widget + get_widget_tree)")
    parser.add_argument("--widget-name", default="", help="Widget blueprint asset name. Defaults to timestamped probe name.")
    parser.add_argument("--path", default="/Game/UI", help="Content path for create_umg_widget_blueprint")
    parser.add_argument("--parent-class", default="UserWidget", help="Parent class (currently UserWidget only)")
    return parser.parse_args()


def _send_unreal_command(command: str, params: dict) -> dict:
    payload = json.dumps({"type": command, "params": params}).encode("utf-8")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(5)
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


def main() -> int:
    args = parse_args()
    widget_name = args.widget_name or f"WBP_McpUmgProbe_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

    create_resp = _send_unreal_command(
        "create_umg_widget_blueprint",
        {
            "widget_name": widget_name,
            "path": args.path,
            "parent_class": args.parent_class,
        },
    )
    create_result = _require_success(create_resp, "create_umg_widget_blueprint")

    tree_resp = _send_unreal_command(
        "get_widget_tree",
        {
            "blueprint_name": create_result.get("path", f"{args.path.rstrip('/')}/{widget_name}"),
        },
    )
    tree_result = _require_success(tree_resp, "get_widget_tree")

    summary = {
        "created": {
            "widget_name": create_result.get("widget_name", widget_name),
            "path": create_result.get("path"),
        },
        "tree": {
            "has_root": tree_result.get("has_root"),
            "root_name": (tree_result.get("root") or {}).get("name"),
            "root_class": (tree_result.get("root") or {}).get("class"),
            "child_count": len(((tree_result.get("root") or {}).get("children") or [])),
        },
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
