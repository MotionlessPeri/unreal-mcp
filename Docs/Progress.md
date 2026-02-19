# Progress

## Last Updated

1. 2026-02-19

## Current Milestone

1. Stabilize blueprint graph automation for consumer projects (`StupidChess` as first consumer).

## Completed

1. Blueprint lookup fallback improved:
   - supports full paths and name-based discovery without assuming `/Game/Blueprints` or `/Game/Widgets`.
2. FastMCP constructor compatibility added for 0.x/2.x differences in server initialization.
3. `bind_widget_event` fixed:
   - switched to component-bound event creation.
   - supports canonical and legacy parameter shapes.
4. New command: `clear_blueprint_event_graph`
   - allows deterministic graph reset before rebuild.
5. New command: `add_blueprint_dynamic_cast_node`
   - enables explicit cast flows for subsystem/object wiring.
6. Graph connect behavior hardened:
   - connection now goes through schema validation (`TryCreateConnection`), avoiding invalid forced links.
7. UMG event node placement improved:
   - `bind_widget_event` now supports optional `node_position`.
8. Python tool layer synced with new commands/params:
   - `Python/tools/node_tools.py`
   - `Python/tools/umg_tools.py`
9. New command: `bind_blueprint_multicast_delegate`
   - creates `UK2Node_AssignDelegate` for a target multicast delegate.
   - manually creates and connects matching `CustomEvent` (avoids UE5.7 `UK2Node_AssignDelegate::PostPlacedNewNode` crash path).
   - supports optional target-object pin connection and exec-chain connection in one call.
   - exposed in Python tools as `bind_blueprint_multicast_delegate(...)`.
10. Parameter persistence fix for function nodes:
   - moved `ReconstructNode()` before param assignment in `add_blueprint_function_node`.
   - avoids post-assignment reset of pin defaults (e.g. `PrintString.InString` reverting to `Hello`).
   - switched generic string parameter assignment to schema default setter for better editor persistence.
11. New command: `add_blueprint_subsystem_getter_node`
   - creates `UK2Node_GetSubsystem` directly for a specified subsystem class.
   - avoids fragile generic getter + cast node chains.
12. New command: `add_blueprint_make_struct_node`
   - creates `UK2Node_MakeStruct` and applies optional field defaults.
   - enables by-ref struct input workflows (e.g. move command struct inputs).
13. New command set for graph cleanup:
   - `break_blueprint_node_pin_links`
   - `clear_blueprint_event_exec_chain`
   - `dedupe_blueprint_component_bound_events`
   - supports preserving manual graph sections while cleaning stale duplicated event branches.
14. Python tool layer updated for all new node commands:
   - `add_blueprint_subsystem_getter_node`
   - `add_blueprint_make_struct_node`
   - `break_blueprint_node_pin_links`
   - `clear_blueprint_event_exec_chain`
   - `dedupe_blueprint_component_bound_events`

## In Progress

1. Better compile-result reliability (explicitly report asset compile errors beyond command-level success).
2. Delegate binding ergonomics for large graphs (batch helper + layout utilities).
3. Add compile-error query/read command(s) to reduce manual log scraping in consumers.

## Next Steps

1. Add graph layout helper command(s) for deterministic readability at scale.
2. Add lightweight command-level regression scripts under `Python/scripts/`.
3. Add command-level smoke scripts for:
   - `bind_blueprint_multicast_delegate`
   - `clear_blueprint_event_exec_chain`
   - `dedupe_blueprint_component_bound_events`
4. Add CI-friendly command smoke entrypoint for consumer-side pre-sync validation.

## Validation Baseline

1. Plugin build succeeds in UE 5.7 (consumer project integration tested).
2. Python tools can issue new commands against a live Unreal editor.
3. Blueprint automation flow validated on `WBP_LocalMatchDebug` consumer asset:
   - clear graph
   - rebind events
   - rebuild chains
   - compile without `AssetLog` errors.
