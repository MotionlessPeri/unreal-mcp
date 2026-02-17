# AGENTS.md

## Read Before Work

Before implementing, refactoring, or fixing anything, read in order:

1. `README.md`
2. `Docs/README.md`
3. `Docs/Progress.md`
4. `Docs/ForkWorkflow.md`

Conflict handling:

1. If chat instructions conflict with repo docs, docs win.
2. If docs conflict with each other, earlier file in the list wins.
3. Fix doc inconsistency in the same change when possible.

## Project Goals

1. Keep Unreal plugin + Python server stable for day-to-day MCP automation.
2. Keep the fork easy to consume from external projects.
3. Prefer predictable, scriptable workflows over one-off manual fixes.

## Hard Constraints

1. New MCP commands must be registered in both:
   - C++ plugin command dispatcher
   - Python tool layer (`Python/tools/*`)
2. Any command behavior change must be documented in `Docs/` and logged in `Docs/Progress.md`.
3. Do not rely on Live Coding for plugin compatibility validation; prefer cold rebuild + editor restart.
4. Avoid silent success:
   - Prefer schema-validated graph connections.
   - Surface actionable errors when command preconditions are not met.

## Collaboration Rules

1. Keep commits small and complete: one feature/fix per commit.
2. Every commit should leave the repo in a usable state:
   - Plugin builds
   - Python server starts
3. Update `Docs/Progress.md` in the same change set for any code edit.
4. For fork-to-consumer changes, update `Docs/ForkWorkflow.md` with any workflow impact.

## Validation Baseline

Run relevant checks before shipping a change:

1. UE plugin build (example):
   - `Build.bat <Project>Editor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`
2. Python layer import/start smoke test:
   - `python Python/unreal_mcp_server.py` (or project venv equivalent)
3. For blueprint graph commands, validate against a real test asset and check UE logs for compile errors.
