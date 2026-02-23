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
