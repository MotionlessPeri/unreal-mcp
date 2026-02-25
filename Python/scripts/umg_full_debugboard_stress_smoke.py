"""
Repeat-run the full DebugBoard skeleton composite smoke and fail fast if the
Unreal MCP endpoint dies or any iteration fails.

Purpose:
1) Turn ad-hoc manual reruns into a repeatable stability probe
2) Provide a concrete D3D12 validation loop for consumer projects

This is a script-level harness. It does not add new MCP commands.
"""

from __future__ import annotations

import argparse
import json
import socket
import subprocess
import sys
import time
from pathlib import Path


THIS_DIR = Path(__file__).resolve().parent
FULL_SMOKE_SCRIPT = THIS_DIR / "umg_full_debugboard_skeleton_smoke.py"


def _mcp_ping(timeout: float = 5.0) -> tuple[bool, str]:
    payload = json.dumps({"type": "ping", "params": {}}).encode("utf-8")
    try:
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
                    response = json.loads(buf.decode("utf-8"))
                    status = response.get("status")
                    if status == "success":
                        return True, "ok"
                    return False, f"bad status: {status}, response={response}"
                except json.JSONDecodeError:
                    continue
        return False, "empty response"
    except Exception as exc:
        return False, str(exc)


def _run_once(iteration: int, timeout_sec: int) -> tuple[bool, str]:
    proc = subprocess.run(
        [sys.executable, str(FULL_SMOKE_SCRIPT)],
        capture_output=True,
        text=True,
        timeout=timeout_sec,
    )
    if proc.returncode != 0:
        return False, (
            f"iteration {iteration} failed (exit={proc.returncode})\n"
            f"stdout:\n{proc.stdout}\n"
            f"stderr:\n{proc.stderr}"
        )
    return True, proc.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description="Repeat-run full DebugBoard skeleton smoke")
    parser.add_argument("--count", type=int, default=5, help="Number of iterations to run")
    parser.add_argument("--timeout-sec", type=int, default=600, help="Per-iteration timeout")
    parser.add_argument("--sleep-sec", type=float, default=0.5, help="Sleep between iterations")
    parser.add_argument("--print-each", action="store_true", help="Print each iteration stdout")
    args = parser.parse_args()

    if args.count <= 0:
        raise SystemExit("--count must be > 0")

    if not FULL_SMOKE_SCRIPT.exists():
        raise RuntimeError(f"Full smoke script not found: {FULL_SMOKE_SCRIPT}")

    ok, msg = _mcp_ping()
    if not ok:
        raise RuntimeError(f"MCP endpoint not ready before start: {msg}")

    started_at = time.time()
    passed = 0
    failed_iteration = None
    failure_reason = None
    samples: list[dict] = []

    for i in range(1, args.count + 1):
        ok, ping_msg = _mcp_ping()
        if not ok:
            failed_iteration = i
            failure_reason = f"MCP endpoint unavailable before iteration {i}: {ping_msg}"
            break

        iter_start = time.time()
        success, output = _run_once(i, args.timeout_sec)
        iter_elapsed = round(time.time() - iter_start, 3)

        if not success:
            failed_iteration = i
            failure_reason = output
            break

        try:
            parsed = json.loads(output)
        except Exception:
            parsed = {"raw_output_truncated": output[:800]}

        samples.append({
            "iteration": i,
            "elapsed_sec": iter_elapsed,
            "summary": {
                "path": (parsed.get("created") or {}).get("path"),
                "cell_count": ((parsed.get("board") or {}).get("cell_count")),
                "grid_batch_updated_count": ((parsed.get("board") or {}).get("grid_batch_updated_count")),
                "text_batch_updated_count": ((parsed.get("text_batch") or {}).get("updated_count")),
            },
        })
        passed += 1

        if args.print_each:
            print(f"=== Iteration {i} ===")
            print(output)

        ok_after, ping_after_msg = _mcp_ping()
        if not ok_after:
            failed_iteration = i
            failure_reason = f"MCP endpoint unavailable after iteration {i}: {ping_after_msg}"
            break

        if i < args.count and args.sleep_sec > 0:
            time.sleep(args.sleep_sec)

    summary = {
        "success": failed_iteration is None,
        "requested_count": args.count,
        "passed_count": passed,
        "failed_iteration": failed_iteration,
        "failure_reason": failure_reason,
        "total_elapsed_sec": round(time.time() - started_at, 3),
        "samples": samples[: min(len(samples), 5)],
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0 if failed_iteration is None else 1


if __name__ == "__main__":
    raise SystemExit(main())

