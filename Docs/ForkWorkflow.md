# Fork Workflow

This fork is maintained for real consumer projects (for example: `StupidChess`).

## Branching Strategy

1. Default: commit directly to `main` for small, validated fixes.
2. For larger changes, use short-lived feature branches and squash on merge.

## Consumer Integration Model

1. Source of truth for MCP changes: this fork repo.
2. Consumer project copies plugin source from:
   - `MCPGameProject/Plugins/UnrealMCP`
3. Python server/tooling is used from this repo directly (not copied into consumer runtime builds).
4. Consumer repos should avoid direct edits under vendored plugin copies:
   - implement MCP changes in this fork first
   - then sync to consumer.

## Sync Steps (Fork -> Consumer)

1. Implement + validate in this fork.
2. In consumer repo, sync plugin code from fork (for `StupidChess` this is done via `tools/sync_unreal_mcp.ps1`).
3. Rebuild consumer UE editor target.
4. Restart editor and run command-level smoke flow.

## Change Checklist

For any new or changed command:

1. C++ handler exists and is routed in dispatcher.
2. Python tool wrapper is added/updated.
3. Command parameters are documented.
4. Progress entry updated in `Docs/Progress.md`.
5. Consumer sync impact is documented if workflow changed.

## Debugging Notes

1. If plugin DLL is locked during build:
   - close Unreal Editor and rebuild.
2. If command appears available but behavior is old:
   - verify consumer plugin sync + cold restart.
3. If blueprint compile appears "success" but graph still fails:
   - inspect UE logs for `LogBlueprint: Error: [AssetLog]`.
