# Unreal MCP Node Tools

This document provides detailed information about the Blueprint node tools available in the Unreal MCP integration.

## Overview

Node tools allow you to manipulate Blueprint graph nodes and connections programmatically, including adding event nodes, function nodes, variables, and creating connections between nodes.

## Node Tools

### add_blueprint_event_node

Add an event node to a Blueprint's event graph.

Behavior note:
- Override/lifecycle events (for example `Event Construct` in `WidgetBlueprint`) are created via Unreal's default event helper so the node carries the correct override metadata and will fire at runtime.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `event_type` (string) - Type of event (BeginPlay, Tick, etc.)
- `node_position` (array, optional) - [X, Y] position in the graph (default: [0, 0])

**Returns:**
- Response containing the node ID and success status

**Example:**
```json
{
  "command": "add_blueprint_event_node",
  "params": {
    "blueprint_name": "MyActor",
    "event_type": "BeginPlay",
    "node_position": [100, 100]
  }
}
```

### add_blueprint_input_action_node

Add an input action event node to a Blueprint's event graph.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `action_name` (string) - Name of the input action to respond to
- `node_position` (array, optional) - [X, Y] position in the graph (default: [0, 0])

**Returns:**
- Response containing the node ID and success status

**Example:**
```json
{
  "command": "add_blueprint_input_action_node",
  "params": {
    "blueprint_name": "MyActor",
    "action_name": "Jump",
    "node_position": [200, 200]
  }
}
```

### add_blueprint_function_node

Add a function call node to a Blueprint's event graph.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `target` (string) - Target object for the function (component name or self)
- `function_name` (string) - Name of the function to call
- `params` (object, optional) - Parameters to set on the function node
- `node_position` (array, optional) - [X, Y] position in the graph (default: [0, 0])

**Returns:**
- Response containing the node ID and success status

**Example:**
```json
{
  "command": "add_blueprint_function_node",
  "params": {
    "blueprint_name": "MyActor",
    "target": "Mesh",
    "function_name": "SetRelativeLocation",
    "params": {
      "NewLocation": [0, 0, 100]
    },
    "node_position": [300, 300]
  }
}
```

### connect_blueprint_nodes

Connect two nodes in a Blueprint's event graph.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `source_node_id` (string) - ID of the source node
- `source_pin` (string) - Name of the output pin on the source node
- `target_node_id` (string) - ID of the target node
- `target_pin` (string) - Name of the input pin on the target node

**Returns:**
- Response indicating success or failure

**Example:**
```json
{
  "command": "connect_blueprint_nodes",
  "params": {
    "blueprint_name": "MyActor",
    "source_node_id": "node_1",
    "source_pin": "exec",
    "target_node_id": "node_2",
    "target_pin": "exec"
  }
}
```

### disconnect_blueprint_nodes

Disconnect one specific link between two node pins.

**Parameters:**
- `blueprint_name` (string) - Name/path of the target Blueprint
- `source_node_id` (string) - Source node GUID
- `source_pin` (string) - Source pin name
- `target_node_id` (string) - Target node GUID
- `target_pin` (string) - Target pin name
- `graph_name` (string, optional) - Graph name (`EventGraph`, function graph, macro graph). Default `EventGraph`

**Returns:**
- Response containing the disconnected edge identifiers

### move_blueprint_node

Move one graph node to an absolute position.

**Parameters:**
- `blueprint_name` (string) - Name/path of the target Blueprint
- `node_id` (string) - Node GUID to move
- `node_position` (array[number]) - Absolute graph position `[X, Y]`
- `graph_name` (string, optional) - Graph name. Default `EventGraph`

**Returns:**
- Response containing `node_id`, `x`, and `y`

### move_blueprint_nodes

Move multiple graph nodes to absolute positions.

**Parameters:**
- `blueprint_name` (string) - Name/path of the target Blueprint
- `moves` (array[object]) - Each object has:
- `node_id` (string) - Node GUID
- `node_position` (array[number]) - Absolute graph position `[X, Y]`
- `graph_name` (string, optional) - Graph name. Default `EventGraph`

**Returns:**
- `moved_nodes` with resolved positions
- `missing_node_ids` / `invalid_entries`
- `moved_count` / `missing_count` / `invalid_count`

### delete_blueprint_node

Delete one graph node by node GUID.

**Parameters:**
- `blueprint_name` (string) - Name/path of the target Blueprint
- `node_id` (string) - Node GUID to delete
- `graph_name` (string, optional) - Graph name. Default `EventGraph`

**Returns:**
- Response indicating the deleted node

### delete_blueprint_nodes

Delete multiple graph nodes by node GUID.

**Parameters:**
- `blueprint_name` (string) - Name/path of the target Blueprint
- `node_ids` (array[string]) - Node GUIDs to delete
- `graph_name` (string, optional) - Graph name. Default `EventGraph`

**Returns:**
- `deleted_node_ids` / `missing_node_ids`
- `deleted_count` / `missing_count`

### set_blueprint_node_pin_default

Set an input pin default value on a node.

**Parameters:**
- `blueprint_name` (string) - Name/path of the target Blueprint
- `node_id` (string) - Node GUID
- `pin_name` (string) - Input pin name
- `value` (any) - New default value
- `graph_name` (string, optional) - Graph name. Default `EventGraph`

**Supported value shapes:**
- `string`, `number`, `boolean`, `null`
- `[x, y, z]` for `FVector` / `FRotator` pins
- `{"X":...,"Y":...,"Z":...}` for `FVector`
- `{"Pitch":...,"Yaw":...,"Roll":...}` for `FRotator`
- `{"object_path":"..."}` for object/class-like pins where schema supports object defaults

**Returns:**
- Response containing `node_id`, `pin_name`, resolved `default_value`, and `has_links`

### add_blueprint_variable

Add a variable to a Blueprint.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `variable_name` (string) - Name of the variable
- `variable_type` (string) - Type of the variable (Boolean, Integer, Float, Vector, etc.)
- `default_value` (any, optional) - Default value for the variable
- `is_exposed` (boolean, optional) - Whether to expose the variable to the editor (default: false)

**Returns:**
- Response indicating success or failure

**Example:**
```json
{
  "command": "add_blueprint_variable",
  "params": {
    "blueprint_name": "MyActor",
    "variable_name": "Health",
    "variable_type": "Float",
    "default_value": 100.0,
    "is_exposed": true
  }
}
```

### create_input_mapping

Create an input mapping for the project.

**Parameters:**
- `action_name` (string) - Name of the input action
- `key` (string) - Key to bind (SpaceBar, LeftMouseButton, etc.)
- `input_type` (string, optional) - Type of input mapping (Action or Axis, default: "Action")

**Returns:**
- Response indicating success or failure

**Example:**
```json
{
  "command": "create_input_mapping",
  "params": {
    "action_name": "Jump",
    "key": "SpaceBar",
    "input_type": "Action"
  }
}
```

### add_blueprint_get_self_component_reference

Add a node that gets a reference to a component owned by the current Blueprint.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `component_name` (string) - Name of the component to get a reference to
- `node_position` (array, optional) - [X, Y] position in the graph (default: [0, 0])

**Returns:**
- Response containing the node ID and success status

**Example:**
```json
{
  "command": "add_blueprint_get_self_component_reference",
  "params": {
    "blueprint_name": "MyActor",
    "component_name": "Mesh",
    "node_position": [400, 400]
  }
}
```

### add_blueprint_self_reference

Add a 'Get Self' node to a Blueprint's event graph.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `node_position` (array, optional) - [X, Y] position in the graph (default: [0, 0])

**Returns:**
- Response containing the node ID and success status

**Example:**
```json
{
  "command": "add_blueprint_self_reference",
  "params": {
    "blueprint_name": "MyActor",
    "node_position": [500, 500]
  }
}
```

### find_blueprint_nodes

Find nodes in a Blueprint's event graph.

**Parameters:**
- `blueprint_name` (string) - Name of the target Blueprint
- `node_type` (string, optional) - Type of node to find (Event, Function, Variable, etc.)
- `event_type` (string, optional) - Specific event type to find (BeginPlay, Tick, etc.)

**Returns:**
- Response containing array of found node IDs and success status

**Example:**
```json
{
  "command": "find_blueprint_nodes",
  "params": {
    "blueprint_name": "MyActor",
    "node_type": "Event",
    "event_type": "BeginPlay"
  }
}
```

## Error Handling

Notes:
- `break_blueprint_node_pin_links` now also accepts optional `graph_name` (default `EventGraph`) for function/macro graph usage.
- Commands that edit graphs by GUID assume the `node_id` came from `get_blueprint_graph_info` on the same graph.
- `get_blueprint_graph_info` now also returns layout metadata for formatting tools:
  - per-node `width`, `height`, `size_source`
  - graph-level `overlaps`, `overlap_pair_count`, `overlap_node_count`

All command responses include a "success" field indicating whether the operation succeeded, and an optional "message" field with details in case of failure.

```json
{
  "success": false,
  "message": "Blueprint 'MyActor' not found in the project",
  "command": "add_blueprint_event_node"
}
```

## Type Reference

### Node Types

Common node types for the `find_blueprint_nodes` command:

- `Event` - Event nodes (BeginPlay, Tick, etc.)
- `Function` - Function call nodes
- `Variable` - Variable nodes
- `Component` - Component reference nodes
- `Self` - Self reference nodes

### Variable Types

Common variable types for the `add_blueprint_variable` command:

- `Boolean` - True/false values
- `Integer` - Whole numbers
- `Float` - Decimal numbers
- `Vector` - 3D vector values
- `String` - Text values
- `Object Reference` - References to other objects
- `Actor Reference` - References to actors
- `Component Reference` - References to components
