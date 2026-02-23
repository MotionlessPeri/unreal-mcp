"""
Smoke test for delete_widget_blueprints_by_prefix:
1) create two probe widget blueprints with a shared prefix
2) dry-run cleanup and verify both are matched
3) execute cleanup
4) verify get_widget_tree fails for deleted assets
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


def _is_error_response(response: dict) -> bool:
    return isinstance(response, dict) and response.get("status") == "error"


def main() -> int:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    prefix = f"WBP_McpBulkCleanupProbe_{stamp}"
    names = [f"{prefix}_A", f"{prefix}_B"]
    asset_paths = []

    for name in names:
        result = _ok(_send("create_umg_widget_blueprint", {
            "widget_name": name,
            "path": "/Game/UI",
            "parent_class": "UserWidget",
        }), f"create_umg_widget_blueprint({name})")
        asset_paths.append(result.get("path", f"/Game/UI/{name}"))

    dry_run = _ok(_send("delete_widget_blueprints_by_prefix", {
        "path": "/Game/UI",
        "name_prefix": prefix,
        "recursive": True,
        "dry_run": True,
    }), "delete_widget_blueprints_by_prefix(dry_run)")
    matched_assets = set(dry_run.get("matched_assets") or [])
    for path in asset_paths:
        if path not in matched_assets:
            raise RuntimeError(f"dry-run did not match expected asset: {path}")

    cleanup = _ok(_send("delete_widget_blueprints_by_prefix", {
        "path": "/Game/UI",
        "name_prefix": prefix,
        "recursive": True,
        "dry_run": False,
    }), "delete_widget_blueprints_by_prefix(delete)")
    deleted_assets = set(cleanup.get("deleted_assets") or [])
    if cleanup.get("failed_delete_count", 0) != 0:
        raise RuntimeError(f"cleanup reported failed deletes: {cleanup.get('failed_delete_assets')}")
    for path in asset_paths:
        if path not in deleted_assets:
            raise RuntimeError(f"cleanup did not delete expected asset: {path}")

    deleted_verify = {}
    for path in asset_paths:
        resp = _send("get_widget_tree", {"blueprint_name": path})
        if not _is_error_response(resp):
            raise RuntimeError(f"expected get_widget_tree to fail after deletion for {path}, got {resp}")
        deleted_verify[path] = resp.get("error", "unknown error")

    summary = {
        "prefix": prefix,
        "created_assets": asset_paths,
        "dry_run": {
            "matched_count": dry_run.get("matched_count"),
            "matched_assets": dry_run.get("matched_assets"),
        },
        "cleanup": {
            "deleted_count": cleanup.get("deleted_count"),
            "deleted_assets": cleanup.get("deleted_assets"),
            "failed_delete_count": cleanup.get("failed_delete_count"),
        },
        "post_delete_verify": deleted_verify,
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

