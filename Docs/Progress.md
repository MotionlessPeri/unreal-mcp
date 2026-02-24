# Progress

## Last Updated

1. 2026-02-19
2. 2026-02-23
3. 2026-02-24

## Current Milestone

1. Stabilize blueprint graph automation for consumer projects (`StupidChess` as first consumer).

## Completed

1. `set_actor_property` struct support (2026-02-24):
   - extended `SetObjectProperty` to support struct properties.
   - added support for `FLinearColor` (used for colors like Route PointColor).
   - added support for `FVector` (accepts object or array format).
   - added support for `FRotator` (accepts object or array format).
   - uses `TBaseStructure<T>::Get()` for type comparison.
   - uses `CopyCompleteValue` for safe struct assignment.
   - enables setting struct properties via `set_actor_property` command.
2. `spawn_actor` command enhancement (2026-02-24):
   - extended to support arbitrary Actor classes via class path (e.g., `/Script/AIPoint.AIPointInstance`).
   - maintains backward compatibility with built-in types (`StaticMeshActor`, `PointLight`, etc.).
   - uses `FindObject<UClass>` and `StaticLoadClass` for dynamic class resolution.
   - validates that loaded classes inherit from `AActor`.
   - enables spawning custom game-specific Actor types without hardcoding.
2. New commands for subsystem interaction and object array manipulation (2026-02-24):
   - added `call_subsystem_function` command for invoking BlueprintCallable functions on WorldSubsystems.
   - enables proper editor workflows (e.g. calling `UAIPointEditorSubsystem::CreateRouteActor` via `UAIPointWorldSubsystem::CreateRoute`).
   - supports parameter passing and return value extraction for common types (String, Int, Float, Bool, Object).
   - added `add_to_actor_array_property` command for adding actor references to array properties.
   - enables setting up actor relationships (e.g. adding `AAIPointInstance` to `AAIPointRoute::AIPoints` array).
   - properly handles undo/redo via `Modify()` and marks packages dirty via `MarkPackageDirty()`.
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/editor_tools.py`.
   - follows UnrealMCP code guidelines: no `Optional[T]`, detailed docstrings with examples.
2. Blueprint lookup fallback improved:
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
15. Build fix for UE pointer-array sort wrapper:
   - fixed `dedupe_blueprint_component_bound_events` comparator signature from pointer args to reference args.
   - resolves UE5.7 compile error in `TArray<UK2Node_ComponentBoundEvent*>::Sort` (`cannot convert argument from T to pointer`).
16. `add_blueprint_event_node` override/lifecycle event creation fix:
   - switched event node creation from manual `NewObject<UK2Node_Event>` to `FKismetEditorUtilities::AddDefaultEventNode`.
   - preserves required override metadata (notably `bOverrideFunction`) for lifecycle/override events such as `UUserWidget::Construct`.
   - fixes "node looks like Event Construct but never fires at runtime" behavior observed in consumer `WBP_LocalMatchDebug`.
17. Editor lifecycle automation commands (workflow efficiency):
   - added `save_dirty_assets` (dirty map/content package save with before/after counts).
   - added `request_editor_exit` (asynchronous delayed exit scheduling to avoid truncating MCP responses).
   - added `save_and_exit_editor` convenience command (save + schedule exit).
   - routed through `UnrealMCPBridge`, exposed in `Python/tools/editor_tools.py`, and documented in `Docs/Tools/editor_tools.md`.
   - consumer smoke validated against `StupidChessUE`: `save_dirty_assets` response path works and `save_and_exit_editor` returns before editor exits (delayed shutdown confirmed).
18. UMG Route B Stage 2 (generic widget-tree creation) completed:
   - added `ensure_widget_root` (create/reuse root widget, optional replace_existing).
   - added `add_widget_child` (generic child insertion under `UPanelWidget` parent).
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - consumer smoke validated against `StupidChessUE` (`Python/scripts/umg_stage02_smoke.py`):
     - create probe widget
     - replace root with named `CanvasPanel`
     - add `VerticalBox` + `TextBlock` + `Button`
     - read back hierarchy via `get_widget_tree` successfully.
19. UMG Route B Stage 3 (layout primitives) completed:
   - added `set_canvas_slot_layout` (position/size/alignment/anchors/auto_size/z_order) with readback values.
   - added `set_uniform_grid_slot` (row/column/horizontal_alignment/vertical_alignment) with readback values.
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_stage03_smoke.py` and validated in `StupidChessUE`:
     - root `CanvasPanel`
     - `UniformGridPanel` + side panel in canvas with layout set/readback
     - `UniformGridSlot` row/column layout set/readback on child cell widget.
   - stability fixes during validation:
     - `NormalizeWidgetClassKey` no longer strips leading `U` from class names like `UniformGridPanel`.
     - `ensure_widget_root` now renames same-class root instead of replacing it (avoids UMG variable GUID ensure during compile).
20. UMG Route B Stage 4 (common widget properties) completed:
   - added `set_widget_common_properties` for `UWidget`-level debug UI properties:
     - `visibility`
     - `is_enabled`
     - returns readback (`visibility`, `is_enabled`, `is_variable`)
   - added `set_text_block_properties` for `UTextBlock`:
     - `text`
     - `color` (`[r,g,b,a]`)
     - returns readback (`text`, `color`)
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_stage04_smoke.py` and validated in `StupidChessUE`:
     - create probe widget
     - build `CanvasPanel -> VerticalBox -> TextBlock/Button` hierarchy
     - apply canvas slot layout and common/text properties
     - read back hierarchy and property values successfully.
21. UMG Route B Stage 5 (cleanup / repeatability) completed:
   - added `clear_widget_children`:
     - clears direct children of a target panel widget (or root when omitted)
     - removes child subtrees via `WidgetTree->RemoveWidget`
     - returns removal counts and root readback
   - added `remove_widget_from_blueprint`:
     - removes a non-root widget and its subtree
     - returns subtree removal count and root readback
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_stage05_smoke.py` and validated in `StupidChessUE`:
     - build hierarchy
     - remove subtree (`BtnAction`)
     - clear/rebuild root children on same probe widget
     - verify final widget tree has no duplicate hierarchy after repeat build.
22. UMG post-Stage5 ergonomics enhancement (probe asset cleanup) completed:
   - added `delete_widget_blueprints_by_prefix` command:
     - filters assets under a content path by asset-name prefix
     - keeps only `WidgetBlueprint` assets
     - supports `dry_run` preview before deletion
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_bulk_cleanup_smoke.py` and validated in `StupidChessUE`:
     - create two probe widgets with shared prefix
     - dry-run returns both assets
     - delete run removes both assets
     - `get_widget_tree` fails afterward for deleted assets.
23. UMG post-Stage5 ergonomics enhancement (batch canvas layout) completed:
   - added `set_canvas_slot_layout_batch`:
     - accepts `items[]` where each item targets a widget by `widget_name`
     - each item supports the same fields as `set_canvas_slot_layout`
     - applies all updates and compiles/saves once (better than repeated single-command calls)
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_canvas_batch_layout_smoke.py` and validated in `StupidChessUE`:
     - create probe widget
     - add two canvas children
     - batch-apply layout for both widgets
     - verify per-item readback and `CanvasPanelSlot` tree placement.

## In Progress

1. Better compile-result reliability (explicitly report asset compile errors beyond command-level success).
2. Delegate binding ergonomics for large graphs (batch helper + layout utilities).
3. Add compile-error query/read command(s) to reduce manual log scraping in consumers.
4. UMG Designer automation capability expansion (Route B for consumer `StupidChess` DebugBoard automation).
5. UMG post-Stage5 ergonomics planning (template helpers and higher-level build macros).

### UMG Automation Roadmap (Decision Update, 2026-02-23)

Goal:
1. Prioritize reusable UMG Designer automation capability over one-off consumer UI completion.
2. Build capabilities in small, testable increments and validate each increment with a smoke widget.

Stage 0 (Contract alignment / existing UMG commands):
1. Align Python tool wrappers and C++ handlers for existing UMG commands.
2. Fix parameter-name drift (`create_umg_widget_blueprint`, `set_text_block_binding`) and document canonical names.
3. Add smoke: create a probe widget blueprint successfully with current commands.

Stage 1 (Read capability first):
1. Add `get_widget_tree` command to inspect widget hierarchy for a WidgetBlueprint.
2. Return enough metadata for automation validation (name, type, parent/children, bindability metadata when available).
3. Add smoke: create widget -> read tree -> verify root.

Stage 2 (Generic widget creation):
1. Add `add_widget_child` command (parent + widget class + widget name).
2. Add `ensure_widget_root` command (stable root creation/replacement).
3. Add smoke: auto-create panel + text + button hierarchy.

Stage 3 (Layout primitives for DebugBoard):
1. Add slot-layout setters for `CanvasPanelSlot`.
2. Add slot-layout setters for `UniformGridPanel` (row/column, alignment).
3. Add smoke: auto-layout a simple grid + side panel.

Stage 4 (Common widget properties):
1. Add minimal property setters for common widgets (visibility, enabled, text, color).
2. Add focused setters for `TextBlock` and `Button` properties used by debug UIs.
3. Add smoke: verify visible text/button panel generated end-to-end.

Stage 5 (Cleanup / repeatability):
1. Add widget-tree cleanup/removal commands to support deterministic rebuilds.
2. Ensure scripts can be rerun without accumulating stale widgets.
3. Add smoke: run creation script twice and confirm no duplicate hierarchy.

Stage 5 status (2026-02-24 update):
1. `clear_widget_children` and `remove_widget_from_blueprint` implemented as the minimum cleanup set.
2. Stage 5 smoke validates repeatable rebuild behavior by clearing/rebuilding the same hierarchy on a probe widget.

Smoke Strategy (applies after each stage):
1. Use a disposable probe asset (e.g. `WBP_McpUmgProbe`) in a real consumer project.
2. Compile + save after each automation run.
3. Validate via command response + widget tree readback + UE logs (no `AssetLog` errors).

## Next Steps

1. Add graph layout helper command(s) for deterministic readability at scale.
2. Add lightweight command-level regression scripts under `Python/scripts/`.
3. Add command-level smoke scripts for:
   - `bind_blueprint_multicast_delegate`
   - `clear_blueprint_event_exec_chain`
   - `dedupe_blueprint_component_bound_events`
4. Add CI-friendly command smoke entrypoint for consumer-side pre-sync validation.
5. Document known limitations for editor lifecycle commands (PIE / prompts / debugger attach / remaining dirty-package counts after save attempts).
6. Evaluate whether generic asset cleanup should be promoted beyond UMG-specific cleanup (cross-tooling ergonomics).
7. Evaluate batch helpers for non-canvas operations (`set_widget_common_properties_batch`, `set_uniform_grid_slot_batch`) based on DebugBoard command pressure.

## Validation Baseline

1. Plugin build succeeds in UE 5.7 (consumer project integration tested).
2. Python tools can issue new commands against a live Unreal editor.
3. Blueprint automation flow validated on `WBP_LocalMatchDebug` consumer asset:
   - clear graph
   - rebind events
   - rebuild chains
   - compile without `AssetLog` errors.
