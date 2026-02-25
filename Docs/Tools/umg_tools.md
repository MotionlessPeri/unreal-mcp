# Unreal MCP UMG Tools

This document describes the current UMG (Widget Blueprint) automation commands exposed by UnrealMCP.

## Scope

These commands operate on `WidgetBlueprint` assets (Designer tree and event bindings), not runtime widget instances.

## Command Notes

### create_umg_widget_blueprint

Create a new Widget Blueprint asset.

Canonical parameters:
1. `widget_name` (required)
2. `path` (optional, default `/Game/Widgets`)
3. `parent_class` (optional, currently only `UserWidget` is supported)

Compatibility:
1. Legacy `name` is still accepted for older clients.

### create_debugboard_skeleton_widget

High-level Python MCP tool that composes existing Route B batch primitives to create a reusable DebugBoard skeleton widget.

This is a tool-layer helper (Python orchestration), not a new plugin command. It builds:
1. `RootCanvas`
2. `BoardGrid` (`UniformGridPanel`)
3. `SidePanel` (`VerticalBox`)
4. full board cell skeleton (`Cell_<row>_<col>`)
5. side-panel status `TextBlock`s and action `Button`s (with label `TextBlock`s)

Canonical parameters:
1. `widget_name`
2. Optional `path` (default `/Game/UI`)
3. Optional `board_rows` (default `10`)
4. Optional `board_cols` (default `9`)

Behavior:
1. Uses existing batch-capable commands (`add_widget_child_batch`, `set_canvas_slot_layout_batch`, `set_uniform_grid_slot_batch`, `set_text_block_properties_batch`, `set_widget_common_properties_batch`) to minimize compile/save churn.
2. Applies initial layout and text/common-property defaults suitable for debug UI iteration.
3. Returns a summary with created asset path, counts, and root child names.

### get_widget_tree

Read the widget hierarchy from a Widget Blueprint.

Parameters:
1. `blueprint_name` (required, short name or asset path)

Response (high level):
1. `has_root`
2. `asset_path`
3. `root` (recursive tree)

Per-node fields:
1. `name`
2. `class`
3. `parent_name`
4. `slot_class`
5. `is_variable`
6. `is_root`
7. `children`

### ensure_widget_root

Ensure the Widget Blueprint has a root widget of a requested type/name.

Canonical parameters:
1. `blueprint_name`
2. `widget_class` (e.g. `CanvasPanel`, `Border`)
3. `widget_name`
4. `replace_existing` (optional, default `false`)

Behavior:
1. If root already matches class+name, returns success with `created=false`.
2. If root mismatches and `replace_existing=false`, returns an error.
3. If only the root name mismatches and `replace_existing=true`, the command renames the existing root (safer than replacement for UMG metadata).
4. If class mismatches and `replace_existing=true`, replaces the root widget.

### add_widget_child

Add a generic UMG widget under a panel parent in the widget tree.

Canonical parameters:
1. `blueprint_name`
2. `parent_widget_name`
3. `widget_class` (e.g. `VerticalBox`, `TextBlock`, `Button`)
4. `widget_name`

Notes:
1. Parent must be a panel widget (`UPanelWidget`).
2. Child widget names must be unique within the widget tree.
3. Command compiles and saves the Widget Blueprint after insertion.

### add_widget_child_batch

Batch-add generic UMG widgets under panel parents in the widget tree.

Canonical parameters:
1. `blueprint_name`
2. `items` (array of objects)
   - each item must include `widget_class`, `widget_name`
   - each item may include `parent_widget_name` (defaults to root widget)

Behavior:
1. Applies the same validation semantics as `add_widget_child` for each item.
2. Fails the command if any item is invalid (missing parent, duplicate widget name, unsupported class, etc.).
3. Compiles and saves the widget blueprint once after all insertions.

Response includes:
1. `created_count`
2. `results` (per-item metadata: `parent_widget_name`, `widget_name`, `widget_class`, `slot_class`, `widget`)

### set_canvas_slot_layout

Set layout values for a widget hosted in a `CanvasPanelSlot`.

Canonical parameters:
1. `blueprint_name`
2. `widget_name`
3. Optional `position` = `[x, y]`
4. Optional `size` = `[w, h]`
5. Optional `alignment` = `[x, y]`
6. Optional `anchors` = `[min_x, min_y, max_x, max_y]` (or `[x, y]` shorthand)
7. Optional `auto_size`
8. Optional `z_order`

Response includes readback values for:
1. `position`
2. `size`
3. `alignment`
4. `anchors`
5. `auto_size`
6. `z_order`

### set_canvas_slot_layout_batch

Batch-set layout values for multiple widgets hosted in `CanvasPanelSlot`.

Canonical parameters:
1. `blueprint_name`
2. `items` (array of objects)
   - each item must include `widget_name`
   - each item may include the same optional fields as `set_canvas_slot_layout`
     (`position`, `size`, `alignment`, `anchors`, `auto_size`, `z_order`)

Behavior:
1. Applies per-item layout updates to existing widgets in the target `WidgetBlueprint`.
2. Validates each item before proceeding to the next; any invalid item fails the command.
3. Compiles and saves the widget blueprint once after all item updates (ergonomic improvement vs repeated single calls).

Response includes:
1. `updated_count`
2. `results` (per-item readback layout values)

### set_uniform_grid_slot

Set layout values for a widget hosted in a `UniformGridSlot`.

Canonical parameters:
1. `blueprint_name`
2. `widget_name`
3. Optional `row`
4. Optional `column`
5. Optional `horizontal_alignment` (`Fill`, `Left`, `Center`, `Right`)
6. Optional `vertical_alignment` (`Fill`, `Top`, `Center`, `Bottom`)

Response includes readback values for:
1. `row`
2. `column`
3. `horizontal_alignment` (enum int)
4. `vertical_alignment` (enum int)

Compatibility note:
1. `UUniformGridSlot` in current UE 5.7 consumer validation does not expose padding setters/getters, so padding is intentionally not part of this command.

### set_uniform_grid_slot_batch

Batch-set layout values for multiple widgets hosted in `UniformGridSlot`.

Canonical parameters:
1. `blueprint_name`
2. `items` (array of objects)
   - each item must include `widget_name`
   - each item may include `row`, `column`, `horizontal_alignment`, `vertical_alignment`

Behavior:
1. Applies per-item grid slot updates using the same validation semantics as `set_uniform_grid_slot`.
2. Fails the command if any item is invalid (missing widget, wrong slot type, invalid alignment string).
3. Compiles and saves the widget blueprint once after all updates.

Response includes:
1. `updated_count`
2. `results` (per-item readback values for row/column/alignment and slot class)

Use case note:
1. This is the recommended primitive for DebugBoard/board-grid cell layout because it reduces command count and compiles/saves only once.

### set_widget_common_properties

Set common `UWidget` properties useful for debug UI automation.

Canonical parameters:
1. `blueprint_name`
2. `widget_name`
3. Optional `visibility` (`Visible`, `Collapsed`, `Hidden`, `HitTestInvisible`, `SelfHitTestInvisible`)
4. Optional `is_enabled`

Response includes readback values for:
1. `visibility` (string)
2. `is_enabled`
3. `is_variable`

### set_widget_common_properties_batch

Batch-set common `UWidget` properties for multiple widgets.

Canonical parameters:
1. `blueprint_name`
2. `items` (array of objects)
   - each item must include `widget_name`
   - each item may include `visibility`, `is_enabled`

Behavior:
1. Applies per-item `UWidget` property updates using the same validation semantics as `set_widget_common_properties`.
2. Fails the command if any item is invalid (missing widget, invalid visibility string, etc.).
3. Compiles and saves the widget blueprint once after all updates.

Response includes:
1. `updated_count`
2. `results` (per-item readback values: `widget_name`, `widget_class`, `visibility`, `is_enabled`, `is_variable`)

### set_text_block_properties

Set common `TextBlock` properties useful for debug UI automation.

Canonical parameters:
1. `blueprint_name`
2. `widget_name`
3. Optional `text`
4. Optional `color` = `[r, g, b, a]`

Response includes readback values for:
1. `text`
2. `color`

### set_text_block_properties_batch

Batch-set `TextBlock` properties for multiple widgets.

Canonical parameters:
1. `blueprint_name`
2. `items` (array of objects)
   - each item must include `widget_name`
   - each item may include `text`, `color`

Behavior:
1. Applies per-item `TextBlock` updates using the same semantics as `set_text_block_properties`.
2. Fails the command if any item is invalid (missing widget, widget not a `TextBlock`, malformed item object, etc.).
3. Compiles and saves the widget blueprint once after all updates.

Response includes:
1. `updated_count`
2. `results` (per-item readback values: `widget_name`, `widget_class`, `text`, `color`)

### clear_widget_children

Remove all direct children from a panel widget (and their subtrees).

Canonical parameters:
1. `blueprint_name`
2. Optional `widget_name` (defaults to root widget)

Behavior:
1. Target widget must be a panel widget (`UPanelWidget`).
2. Each direct child is removed via `WidgetTree->RemoveWidget`, so subtrees are removed too.
3. Command compiles and saves after removal.

Response includes readback values for:
1. `removed_direct_children`
2. `removed_total_widgets`
3. `remaining_child_count`
4. `root` (widget tree readback)

### remove_widget_from_blueprint

Remove a widget from the widget tree (removes its subtree).

Canonical parameters:
1. `blueprint_name`
2. `widget_name`

Behavior:
1. Root widget removal is intentionally rejected (use `ensure_widget_root` or `clear_widget_children` instead).
2. Command compiles and saves after removal.

Response includes readback values for:
1. `removed_widget_name`
2. `parent_widget_name`
3. `removed_total_widgets`
4. `root` (widget tree readback)

### delete_widget_blueprints_by_prefix

Delete `WidgetBlueprint` assets under a content path filtered by asset name prefix.

Canonical parameters:
1. `path` (e.g. `/Game/UI`)
2. `name_prefix`
3. Optional `recursive` (default `true`)
4. Optional `dry_run` (default `false`)

Behavior:
1. Command lists assets under the target path, filters by asset-name prefix, then loads and keeps only `WidgetBlueprint` assets.
2. `dry_run=true` returns the matched set without deleting.
3. `dry_run=false` deletes matched widget blueprint assets and returns deleted/failed lists.

Response includes:
1. `matched_count`
2. `matched_assets`
3. `deleted_count` / `deleted_assets` (non-dry-run)
4. `failed_delete_count` / `failed_delete_assets` (non-dry-run)

### set_text_block_binding

Create a simple text binding variable/function setup for a `TextBlock`.

Canonical parameters:
1. `blueprint_name`
2. `widget_name` (target `TextBlock` name)
3. `binding_name` (binding variable name)

Compatibility:
1. Legacy `text_block_name` is accepted as alias of `widget_name`
2. Legacy `binding_property` is accepted as alias of `binding_name`

## Validation Guidance

1. Compile + save the Widget Blueprint after UMG automation changes.
2. For command-level validation, prefer readback via `get_widget_tree`.
3. For runtime validation (button interaction / Construct / viewport behavior), use a real consumer project PIE session.
