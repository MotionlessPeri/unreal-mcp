# Progress

## Last Updated

1. 2026-02-19
2. 2026-02-23
3. 2026-02-24 (behavior tree read support + blueprint graph read support)
4. 2026-02-25 (bt decorator details + blackboard key list + plugin-path blueprint lookup fix)
5. 2026-02-25 (blueprint node precise delete/disconnect/pin-default commands)
6. 2026-02-25 (blueprint node move commands for layout cleanup)
7. 2026-02-25 (blueprint graph size/overlap metadata for layout tooling)
8. 2026-03-06 (dialogue system read commands + live smoke validation)
9. 2026-03-09 (MCP-2: dialogue system write commands + persistent NodeId)
10. 2026-03-09 (split DialogueSystem support into optional extension plugin)
11. 2026-03-09 (Logic Driver full state machine graph read extension)
12. 2026-03-09 (AnimMontage read support in base UnrealMCP)
13. 2026-04-17 (agent usage guide + command reference docs + help command)

## Current Milestone

1. Stabilize blueprint graph automation for consumer projects (`StupidChess` as first consumer).

## Completed

1. Agent documentation and help command (2026-04-17):
   - added `Docs/commands.md`: complete command reference for all ~76 built-in commands with parameters, types, and descriptions.
   - added `Docs/commands-dialogue.md`: Dialogue extension command reference (10 commands).
   - added `Docs/commands-logicdriver.md`: LogicDriver extension command reference (1 command).
   - added `Docs/agent-usage-guide.md`: guide for AI agents covering TCP connection, usage rules, common pitfalls, and rebuild workflow.
   - added `help` command (C++ + Python): runtime command discovery.
     - list mode: `{"type": "help"}` returns all commands grouped by category.
     - detail mode: `{"type": "help", "params": {"command": "spawn_actor"}}` returns description and parameter metadata.
   - per-class metadata registration: each handler class provides `GetCommandMetadata()` returning `TArray<FMCPCommandMeta>`.
   - extension handlers can opt-in via `IUnrealMCPCommandHandler::GetCommandMetadata()` virtual method (backward-compatible default empty).
   - `FUnrealMCPCommandRegistry::GetAllExtensionMetadata()` aggregates metadata from all registered extension handlers.
   - Python tool `help_tools.py` registered in `unreal_mcp_server.py`.
   - updated `Docs/README.md` index with new documentation entries.

1. UnrealMCP consumer split: optional DialogueSystem extension plugin (2026-03-09):
   - removed direct `DialogueSystem` / `DialogueSystemEditor` dependencies from base `UnrealMCP`.
   - added extensible command registry in base plugin so consumer-specific command groups can register without coupling the core module.
   - moved dialogue commands into new `MCPGameProject/Plugins/UnrealMCPDialogue` editor plugin.
   - Python server now registers `dialogue_tools` only when `UnrealMCPDialogue` is present beside the consumer project or source repo.
   - result: generic consumers such as `BattleDemo` can sync `UnrealMCP` without inheriting DialogueSystem compile dependencies.

1. Logic Driver graph read extension plugin (2026-03-09):
   - added `MCPGameProject/Plugins/UnrealMCPLogicDriver` as an optional consumer extension plugin.
   - added `get_logicdriver_state_machine_graph` command to read a full Logic Driver state machine graph.
   - transition serialization now includes:
     - `from_node_id` / `to_node_id`
     - `transition_name`
     - `conditional_evaluation_type`
     - `condition_graph` with nested nodes, edges, and result node
   - graph response includes `nodes`, `states`, `special_nodes`, `transitions`, `edges`, and `entry_node`.
   - Python server now registers `logicdriver_tools` only when `UnrealMCPLogicDriver` is present beside the consumer project or source repo.
   - BattleDemo runtime smoke validated against `BP_InputSM`:
     - root graph read succeeded
     - transition condition subgraphs were returned
     - sample transitions included both `SM_Graph` and `SM_AlwaysTrue` evaluation types.

1. AnimMontage read support in base UnrealMCP (2026-03-09):
   - added `get_montage_info` to base `UnrealMCP` for UE-native animation asset reads.
   - command accepts content-browser paths, object paths, or full class-qualified object paths for `UAnimMontage`.
   - response now includes:
     - `sections`
     - `slot_tracks` with segment context
     - normalized `notifies` covering `notify`, `notify_state`, and `branching_point`
     - `branching_points` summary derived from montage branching-point notifies
   - each notify includes timing, track metadata, section name, slot/segment context, linked sequence path, and notify class names.
   - BattleDemo runtime smoke validated against `AS_Combo_Attack_01_01_Seq_Montage`:
     - read succeeded using full class-qualified object path
     - returned `notify_count=11`
     - returned both `notify` and `notify_state` entries with per-notify timing and track data.

1. MCP-2: Dialogue System write commands (2026-03-09):
   - **Prerequisite**: Added persistent `FGuid NodeId` to `UDialogueNode` (consumer-side). `PostLoad()` auto-generates GUID for existing assets. All MCP read commands now use `NodeId.ToString()` instead of `UObject::GetName()`.
   - Added `"DialogueSystemEditor"` to `UnrealMCP.Build.cs` dependencies (write commands need `UDialogueGraphNode`).
   - `add_dialogue_node`: Creates Speech/Choice/Exit node with graph node + runtime node. Returns persistent GUID. Choice nodes include default ChoiceItem with its own GUID.
   - `set_dialogue_node_properties`: Sets speaker_name/dialogue_text (Speech), choice_text (ChoiceItem), pos_x/pos_y (any node). Syncs graph node position.
   - `connect_dialogue_nodes`: Creates edge via Schema `TryCreateConnection` (visual + runtime sync). Supports Entry/Speech "Out" pin and Choice "Item_N" pins.
   - `disconnect_dialogue_nodes`: Removes edge via Schema `BreakSinglePinLink` (visual + runtime sync).
   - `delete_dialogue_node`: Breaks all pin links, removes graph node and runtime node. Rejects Entry/ChoiceItem deletion.
   - `get_dialogue_graph` enhanced: Choice nodes now return `choices` as array of `{text, item_node_id}` objects instead of plain strings.
   - All 5 write commands routed in `UnrealMCPBridge`, exposed in `Python/tools/dialogue_tools.py`.
   - Helper methods extracted: `LoadDialogueAsset`, `FindNodeByGuid`, `FindOutputPin`, `FindInputPin`.

1. UMG widget commands fixes (2026-03-09):
   - `create_umg_widget_blueprint`: `parent_class` parameter now accepts full class path (e.g. `/Script/DialogueSystem.DialogueWidget`). Uses `StaticLoadClass` to dynamically resolve, validates inheritance from `UUserWidget`.
   - `add_widget_child` / `add_widget_child_batch`: `is_variable` parameter now correctly sets `Widget->bIsVariable`.
   - `add_widget_child` / `add_widget_child_batch`: `parent_name` accepted as alias for `parent_widget_name` (fixes silent fallback to root when using wrong param name).
   - `set_widget_common_properties`: `is_variable` parameter now supported.
   - Dialogue read commands (`get_dialogue_graph` / `get_dialogue_connections`) adapted to new data model (`Items` / `OutTransitions` instead of `Choices` / `NextNode`).

1. Dialogue System read commands (2026-03-06):
   - added `get_dialogue_graph`: loads `UDialogueAsset`, serializes all nodes with type, position, and type-specific fields (speaker_name/dialogue_text for Speech; choices list for Choice). `node_id` = `UObject::GetName()`.
   - added `get_dialogue_connections`: traverses runtime node pointers (NextNode / Choices[i].TargetNode), serializes directed edges with from/to node_id and pin names matching DialogueGraphSchema convention.
   - added `"DialogueSystem"` to `UnrealMCP.Build.cs` public dependencies; added `DialogueSystem` plugin dependency to `UnrealMCP.uplugin`.
   - new command class `FUnrealMCPDialogueCommands`; routed in `UnrealMCPBridge`, exposed in `Python/tools/dialogue_tools.py`, registered in `unreal_mcp_server.py`.
   - live smoke test validated (2026-03-06) against `DialogueSystemSample`/`/Game/NewDialogueAsset`:
     - `get_dialogue_graph` returns 8 nodes (Entry/Speech/Choice/Exit types with correct position and type-specific fields).
     - `get_dialogue_connections` returns 3 directed edges with correct from/to node_id and pin names.
     - invalid path (`/Game/DoesNotExist`) returns `{"status": "error", "error": "Could not load asset at path: ..."}` as expected.

1. `get_blueprint_graph_info` layout metadata enhancement (2026-02-25):
   - each serialized node now includes `width`, `height`, and `size_source` (`cached` from `NodeWidth/NodeHeight` if available, otherwise `estimated` fallback).
   - command response now includes overlap analysis for external formatters:
     - `overlaps` (pairwise overlap entries with overlap rect + both node bounds)
     - `overlap_pair_count`
     - `overlap_node_count`
   - intended for external layout/beautify scripts to detect and resolve node collisions without re-querying node geometry in-editor.

1. Blueprint graph node move commands (2026-02-25):
   - added `move_blueprint_node` (single node absolute position by GUID).
   - added `move_blueprint_nodes` (batch absolute positions by GUID).
   - supports optional `graph_name` (default `EventGraph`) for function/macro graph layout edits.
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/node_tools.py`, documented in `Docs/Tools/node_tools.md`.

1. Blueprint graph precise edit commands for node cleanup / pin-defaulting (2026-02-25):
   - added `disconnect_blueprint_nodes` to remove one specific link between two node pins.
   - added `delete_blueprint_node` and `delete_blueprint_nodes` (single/batch by node GUID).
   - added `set_blueprint_node_pin_default` for input-pin default values (string/number/bool/null, FVector/FRotator arrays/maps, class/object-path where supported).
   - new commands accept optional `graph_name` (default `EventGraph`) so function/macro graph edits are supported.
   - extended `break_blueprint_node_pin_links` to accept optional `graph_name`.
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/node_tools.py`, documented in `Docs/Tools/node_tools.md`.
   - also fixed dispatcher routing for existing node commands already implemented in C++/Python but missing bridge registration:
     - `add_blueprint_branch_node`
     - `add_blueprint_switch_enum_node`
     - `add_blueprint_array_get_node`

1. `get_behavior_tree_info` decorator + blackboard key enhancements (2026-02-25):
   - `SerializeDecorator` now returns `description` (engine-generated human-readable text, e.g. `"( aborts both )\nBlackboard: HasTarget is Is Set"`), `flow_abort_mode` (None / Self / LowerPriority / Both), and `bb_key` (selected blackboard key name for `BTDecorator_Blackboard` and subclasses).
   - `FlowAbortMode` and `BlackboardKey.SelectedKeyName` are protected fields; accessed via UE property reflection (`FindFProperty` + `GetPropertyValue_InContainer`).
   - `get_behavior_tree_info` now also returns `blackboard_keys`: array of `{name, type}` for every key in the `UBlackboardData` asset, including inherited keys from the parent chain.
   - new include: `BehaviorTree/Decorators/BTDecorator_BlackboardBase.h`.
2. `FindBlueprintByName` asset-registry scope fix (2026-02-25):
   - removed `PackagePaths.Add(FName(TEXT("/Game")))` filter from the fallback `FARFilter` in `UnrealMCPCommonUtils::FindBlueprintByName`.
   - now searches all registered asset paths including plugin content (e.g. `/AIPoint/BTTask_WriteBackToPatrol`).
   - fixes "Blueprint not found" error for blueprints that live outside `/Game/`.
3. `get_blueprint_graph_info` command (2026-02-24):
   - new read command to dump a Blueprint's node graph including pin connections (edges).
   - accepts `blueprint_name` (name-based lookup, same as existing commands) and optional `graph_name` (default "EventGraph").
   - searches UbergraphPages, FunctionGraphs, and MacroGraphs; returns `available_graphs` on miss.
   - each node entry includes: `node_id` (GUID), `node_class`, `title`, canvas position, and extra fields for CallFunction (`function_name`), Event/CustomEvent (`event_name`), DynamicCast (`cast_target`).
   - each pin entry (exec or connected-only) includes `pin_name`, `direction`, `pin_type`, and `connected_to` list of `{node_id, pin_name}`.
   - implemented in `FUnrealMCPBlueprintNodeCommands::HandleGetBlueprintGraphInfo`.
   - routed in `UnrealMCPBridge`, exposed in `Python/tools/blueprint_tools.py`.
   - no new module dependencies required (all existing includes cover it).
2. `get_behavior_tree_info` command (2026-02-24):
   - new command to read BehaviorTree asset structure via MCP.
   - loads `UBehaviorTree` asset via `UEditorAssetLibrary::LoadAsset`.
   - recursively serializes the node tree: Composite (Sequence/Selector), Task, Decorator, Service.
   - returns asset name, blackboard path, and the full node hierarchy as JSON.
   - new C++ handler class: `FUnrealMCPBehaviorTreeCommands`.
   - added `AIModule` and `GameplayTasks` to `PublicDependencyModuleNames` in `UnrealMCP.Build.cs`.
   - routed in `UnrealMCPBridge`, exposed in `Python/tools/behavior_tree_tools.py`.
   - registered in `unreal_mcp_server.py`.
2. `set_actor_property` struct support (2026-02-24):
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
24. UMG post-Stage5 ergonomics enhancement (batch widget common properties) completed:
   - added `set_widget_common_properties_batch`:
     - accepts `items[]` with `widget_name` and optional `visibility` / `is_enabled`
     - reuses single-item validation semantics for visibility parsing
     - applies all updates and compiles/saves once
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_widget_common_batch_smoke.py` and validated in `StupidChessUE`:
     - create probe widget with `TextBlock`/`Button`/`Border`
     - batch-apply mixed visibility/is_enabled states
     - verify per-item readback values (including `SelfHitTestInvisible`) in response.
25. UMG post-Stage5 ergonomics enhancement (batch uniform-grid layout) completed:
   - added `set_uniform_grid_slot_batch`:
     - accepts `items[]` with `widget_name` and optional `row/column/horizontal_alignment/vertical_alignment`
     - reuses single-item validation semantics for alignment parsing
     - applies all updates and compiles/saves once
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_uniform_grid_batch_smoke.py` and validated in `StupidChessUE`:
     - create probe widget with `UniformGridPanel`
     - add four grid child widgets
     - batch-apply row/column (+ alignment) values
     - verify per-item readback and `UniformGridSlot` placement via widget tree.
26. UMG post-Stage5 ergonomics enhancement (batch text updates) completed:
   - added `set_text_block_properties_batch`:
     - accepts `items[]` with `widget_name` and optional `text` / `color`
     - reuses single-item `TextBlock` update semantics and readback shape
     - applies all updates and compiles/saves once
   - routed via `UnrealMCPBridge`, exposed in `Python/tools/umg_tools.py`, documented in `Docs/Tools/umg_tools.md`.
   - added `Python/scripts/umg_text_block_batch_smoke.py` and validated in `StupidChessUE`:
     - create probe widget with three `TextBlock` children
     - batch-apply text + RGBA color values
     - verify per-item readback (`text`, `color`) values in response.
27. Composite smoke (full DebugBoard skeleton via existing primitives) completed:
   - added `Python/scripts/umg_full_debugboard_skeleton_smoke.py`.
   - smoke composes current Route B primitives + batch helpers to build a full 9x10 debug-board skeleton:
     - `RootCanvas`
     - `BoardGrid` (`UniformGridPanel`) + `SidePanel` (`VerticalBox`) with canvas batch layout
     - 90 board cells via `add_widget_child_batch`
     - 90 grid slot assignments via `set_uniform_grid_slot_batch`
     - side-panel status texts + buttons + button labels
     - batch text/common-property updates and widget-tree readback validation
   - validated on consumer `StupidChessUE`; command chain passes end-to-end and confirms `cell_count=90`.
   - operational note: one run under regular D3D12 editor UI crashed in render thread during compile/save (`D3D12RHI` callstack, not MCP command error). Re-running under `-NullRHI` completed successfully; keep `-NullRHI` in mind for heavy automation smoke runs.
28. Composite smoke stability harness (full DebugBoard skeleton) added:
   - added `Python/scripts/umg_full_debugboard_stress_smoke.py` to repeat-run the full composite smoke with MCP health checks (`ping`) before/after each iteration.
   - purpose: turn intermittent D3D12 crash observations into a repeatable validation loop and capture pass/fail counts + iteration-level timing.
   - script is harness-only (no new MCP commands) and is intended for consumer-side D3D12 stability probing.
29. UMG tool-layer template helper (DebugBoard skeleton) added:
   - added Python MCP tool `create_debugboard_skeleton_widget` in `Python/tools/umg_tools.py`.
   - helper composes existing Route B batch primitives to create a reusable 9x10-style DebugBoard skeleton widget:
     - `RootCanvas`, `BoardGrid`, `SidePanel`
     - full cell skeleton creation + grid slot layout
     - side-panel status texts / buttons / button labels
     - initial text and common-property defaults
   - no new plugin command required; this is a tool-layer composition capability intended to reduce command pressure in consumers.
   - added `Python/scripts/umg_create_debugboard_skeleton_tool_smoke.py` to validate the helper orchestration itself (tool-layer path) against a live UE editor using a lightweight local stub for `unreal_mcp_server` import wiring.
30. UMG tool-layer template helper (DebugBoard skeleton) parameterization expanded:
   - `create_debugboard_skeleton_widget` now supports layout configuration inputs:
     - `board_origin`
     - `cell_size`
     - `board_padding`
     - `side_panel_width`
     - `side_panel_min_height`
   - defaults remain backward-compatible with previous helper behavior.
   - tool-layer smoke updated to validate non-default layout parameters and readback summary.
31. UMG tool-layer template helper (DebugBoard skeleton) side-panel customization expanded:
   - `create_debugboard_skeleton_widget` now supports custom side-panel schema:
     - `status_text_widgets`
     - `action_button_widgets`
     - `button_enabled_widgets`
     - `status_text_values`
     - `button_label_values`
   - default behavior remains compatible with the original StupidChess debug-panel naming set.
   - tool-layer smoke updated to validate a non-default side-panel schema (custom text/button names and enable-state subset).

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

Composite Smoke (Mini DebugBoard Skeleton, 2026-02-24):
1. Added `Python/scripts/umg_mini_debugboard_skeleton_smoke.py` to exercise current Route B primitives together.
2. Smoke builds a mini DebugBoard skeleton in a real consumer project:
   - `RootCanvas`
   - `BoardGrid` (`UniformGridPanel`) + batch canvas layout
   - `SidePanel` (`VerticalBox`) + status text/buttons
   - board sample cells with `set_uniform_grid_slot_batch`
   - widget-tree readback validation
3. Smoke validated end-to-end composition on consumer `StupidChessUE` using a 4x4 board sample.

Known Limitation Discovered (Composite Smoke):
1. Repeating `add_widget_child` at full-board scale (e.g. 90 single calls for a 9x10 grid) can trigger UMG GUID ensure during per-call compile/save:
   - `Widget [Cell_*] was added but did not get a GUID`
2. This is a tooling scalability limitation in the current per-command compile/save model, not a failure of layout/property primitives.
3. Follow-up capability candidate: batch child creation and/or deferred compile/save mode for bulk widget-tree assembly.

Route B Follow-up (2026-02-24): Full-board batch child creation unblock
1. Added `add_widget_child_batch` to reduce 90x repeated `add_widget_child` calls into a single compile/save.
2. Fixed WidgetBlueprint widget GUID metadata maintenance in UMG commands:
   - add: populate `WidgetVariableNameToGuidMap`
   - rename/remove/cleanup: keep GUID map in sync
3. Fixed MCP server large-payload handling for batch commands:
   - accumulate JSON across multiple `Recv` chunks before parse
   - fix receive-buffer off-by-one null-termination risk
   - truncate oversized request/response logging
4. Added `Python/scripts/umg_add_widget_child_batch_smoke.py` and validated full 9x10 board generation (90 cells) in consumer `StupidChessUE`:
   - `add_widget_child_batch` created 90 `Border` children under `BoardGrid`
   - `set_uniform_grid_slot_batch` updated 90 slots
   - `get_widget_tree` confirmed `board_child_count=90`
   - no `WidgetVariableNameToGuidMap` ensure observed in the validated run

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
7. Evaluate deferred compile/save transaction mode (post-`add_widget_child_batch`) for even larger composite builds and lower editor churn.
8. Consider template/helper layer for repeatable consumer UI skeleton generation now that full DebugBoard skeleton composition smoke passes.
9. Investigate intermittent D3D12 editor-side instability during heavy UMG automation if stress harness shows reproducible failures (renderer-thread crash vs command bug separation).
10. Evaluate a plugin-side deferred compile/save transaction mode to further reduce editor churn for large composite automation flows.

## Validation Baseline

1. Plugin build succeeds in UE 5.7 (consumer project integration tested).
2. Python tools can issue new commands against a live Unreal editor.
3. Blueprint automation flow validated on `WBP_LocalMatchDebug` consumer asset:
   - clear graph
   - rebind events
   - rebuild chains
   - compile without `AssetLog` errors.
