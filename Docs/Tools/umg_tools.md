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
3. If root mismatches and `replace_existing=true`, replaces the root widget.

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
