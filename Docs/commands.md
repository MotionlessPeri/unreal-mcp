# UnrealMCP Command Reference

Complete reference for all built-in commands. Extension plugin commands are documented separately:
- [Dialogue Extension Commands](commands-dialogue.md)
- [LogicDriver Extension Commands](commands-logicdriver.md)

Protocol: `{"type": "<command>", "params": {...}}`

---

## Table of Contents

- [System](#system)
- [Editor / Actor](#editor--actor)
- [Blueprint](#blueprint)
- [Blueprint Node](#blueprint-node)
- [UMG Widget](#umg-widget)
- [Project](#project)
- [Behavior Tree](#behavior-tree)
- [Animation](#animation)

---

## System

### ping

Health check.

**Parameters:** none

**Returns:** `{ "message": "pong" }`

---

## Editor / Actor

### get_actors_in_level

List all actors in the current level.

**Parameters:** none

**Returns:** `actors` — array of actor objects with name, class, transform, components.

---

### find_actors_by_name

Find actors matching a name pattern.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `pattern` | string | yes | Pattern to match against actor names |

**Returns:** `actors` — array of matching actor objects.

---

### spawn_actor

Spawn an actor in the level.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `type` | string | yes | Actor class. Built-in shortcuts: `StaticMeshActor`, `PointLight`, `SpotLight`, `DirectionalLight`, `CameraActor`. Also accepts full class path (e.g. `/Script/AIPoint.AIPointInstance`) |
| `name` | string | yes | Unique actor instance name |
| `location` | object | no | `{x, y, z}` — default `{0, 0, 0}` |
| `rotation` | object | no | `{pitch, yaw, roll}` — default `{0, 0, 0}` |
| `scale` | object | no | `{x, y, z}` — default `{1, 1, 1}` |

**Returns:** full actor object with properties and transform.

> **Note:** `create_actor` is a deprecated alias for `spawn_actor`.

---

### delete_actor

Delete an actor from the level.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | yes | Actor name |

**Returns:** `deleted_actor` — actor details before deletion.

---

### set_actor_transform

Set an actor's transform (location, rotation, scale).

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | yes | Actor name |
| `location` | object | no | `{x, y, z}` |
| `rotation` | object | no | `{pitch, yaw, roll}` |
| `scale` | object | no | `{x, y, z}` |

**Returns:** updated actor object.

---

### get_actor_properties

Get detailed properties of an actor.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | yes | Actor name |

**Returns:** full actor object with properties.

---

### set_actor_property

Set a single property on an actor. Supports basic types, `FVector`, `FRotator`, `FLinearColor`, and struct properties.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | yes | Actor name |
| `property_name` | string | yes | Property name (e.g. `ActorLabel`, `PointColor`) |
| `property_value` | any | yes | Value to set (type varies by property) |

**Returns:** `actor`, `property`, `success`, `actor_details`.

---

### spawn_blueprint_actor

Spawn an instance of a Blueprint class.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Blueprint name or path |
| `actor_name` | string | yes | Instance label name |
| `location` | object | no | `{x, y, z}` — default `{0, 0, 0}` |
| `rotation` | object | no | `{pitch, yaw, roll}` — default `{0, 0, 0}` |
| `scale` | object | no | `{x, y, z}` — default `{1, 1, 1}` |

**Returns:** spawned actor object.

---

### focus_viewport

Move the editor viewport to a target.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `target` | string | no* | Actor name to focus on |
| `location` | object | no* | `{x, y, z}` to focus on |
| `distance` | number | no | View distance (default: 1000) |
| `orientation` | object | no | `{pitch, yaw, roll}` |

\* Provide either `target` or `location`, not both.

**Returns:** `success`.

---

### take_screenshot

Capture a viewport screenshot.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `filepath` | string | yes | Output file path (auto-appends `.png` if missing) |

**Returns:** `filepath` — actual saved path.

---

### save_dirty_assets

Save all dirty (modified) assets.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `save_maps` | bool | no | Save map packages (default: true) |
| `save_content` | bool | no | Save content packages (default: true) |

**Returns:** `success`, `dirty_maps_before`, `dirty_maps_after`, `dirty_content_before`, `dirty_content_after`.

---

### request_editor_exit

Schedule an asynchronous editor exit.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `force` | bool | no | Force exit without prompt (default: false) |
| `delay_seconds` | number | no | Delay before exit (default: 0.25) |

**Returns:** `success`, `force`, `delay_seconds`.

---

### save_and_exit_editor

Save dirty assets then schedule editor exit.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `save_maps` | bool | no | Save map packages (default: true) |
| `save_content` | bool | no | Save content packages (default: true) |
| `force` | bool | no | Force exit (default: false) |
| `delay_seconds` | number | no | Delay before exit (default: 0.35) |

**Returns:** save counts + `exit_scheduled`, `force`, `delay_seconds`.

---

### call_subsystem_function

Call a BlueprintCallable function on a WorldSubsystem.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `subsystem_class` | string | yes | Full class path (e.g. `/Script/Engine.EditorActorSubsystem`) |
| `function_name` | string | yes | Function to call |
| `parameters` | object | no | Function parameters as key-value pairs |

**Returns:** `success` + dynamic return value fields.

---

### add_to_actor_array_property

Add actor references to an array property on an actor.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `actor_name` | string | yes | Actor containing the array |
| `property_name` | string | yes | Array property name |
| `element_name` | string | no* | Single actor name to add |
| `element_names` | array | no* | Array of actor names to add |

\* Provide either `element_name` or `element_names`.

**Returns:** `success`, `added_count`, `new_array_size`.

---

### get_data_asset

Read a DataAsset (or any UObject asset with `allow_any_object=true`) as JSON properties.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `path` | string | yes | Content-browser path (`/Game/.../Asset`) or full object path (`/Game/.../Asset.Asset`) |
| `property_path` | string | no | Top-level property name to narrow the dump |
| `allow_any_object` | bool | no | Read non-DataAsset UObject assets (default `false`) |

**Returns:** `path`, `asset_name`, `asset_class` (full path), `asset_class_short`, `properties` (object). Transient properties are skipped.

---

### find_assets

List assets via the `AssetRegistry` without loading them.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `path` | string | no | Package path to search (default: `/Game`) |
| `class_name` | string | no | Filter by asset class — short name (`"Blueprint"`) or full path (`"/Script/Engine.Blueprint"`). Recursive over subclasses. |
| `recursive` | bool | no | Recurse into subfolders (default: `true`) |
| `name_pattern` | string | no | Wildcard glob on asset name, case-insensitive |

**Returns:** `assets` (array of `{name, path, package_path, class, parent_class?}`), `total_count`, plus echoed filter fields for debugging.

> Blueprint assets all report `class="Blueprint"` regardless of their parent; filter by the returned `parent_class` tag client-side to narrow by Blueprint parent class.

---

## Blueprint

### create_blueprint

Create a new Blueprint asset.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | yes | Blueprint name |
| `parent_class` | string | no | Parent class (default: AActor) |

**Returns:** `name`, `path`.

---

### add_component_to_blueprint

Add a component to a Blueprint.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `component_type` | string | yes | Component class (e.g. `StaticMesh`, `Sphere`, `Camera`) |
| `component_name` | string | yes | Instance name |
| `location` | array | no | `[x, y, z]` |
| `rotation` | array | no | `[pitch, yaw, roll]` |
| `scale` | array | no | `[x, y, z]` |

**Returns:** `component_name`, `component_type`.

---

### set_component_property

Set a property on a Blueprint component.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `component_name` | string | yes | Component instance name |
| `property_name` | string | yes | Property to set |
| `property_value` | any | yes | Value (number, string, bool, array, object) |

**Returns:** `component`, `property`, `success`.

---

### set_physics_properties

Set physics properties on a primitive component.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `component_name` | string | yes | Primitive component name |
| `simulate_physics` | bool | no | Enable physics simulation |
| `mass` | number | no | Mass in kg |
| `linear_damping` | number | no | Linear damping |
| `angular_damping` | number | no | Angular damping |

**Returns:** `component`.

---

### compile_blueprint

Compile a Blueprint.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Blueprint to compile |

**Returns:** `name`, `compiled`.

---

### set_blueprint_property

Set a class default property on a Blueprint.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `property_name` | string | yes | Property name |
| `property_value` | any | yes | Value to set |

**Returns:** `property`, `success`.

---

### set_static_mesh_properties

Set mesh/material on a StaticMeshComponent.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `component_name` | string | yes | StaticMeshComponent name |
| `static_mesh` | string | no | Asset path to StaticMesh |
| `material` | string | no | Asset path to Material |

**Returns:** `component`.

---

### set_pawn_properties

Set pawn-specific properties on a Blueprint (must derive from Pawn).

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `auto_possess_player` | any | no | AutoPossessPlayer enum value |
| `use_controller_rotation_yaw` | bool | no | Use controller yaw |
| `use_controller_rotation_pitch` | bool | no | Use controller pitch |
| `use_controller_rotation_roll` | bool | no | Use controller roll |
| `can_be_damaged` | bool | no | Can receive damage |

**Returns:** `blueprint`, `success`, `results` (per-property).

---

### get_blueprint_info

Read a Blueprint's parent class, implemented interfaces, and component tree (SCS-added + native).
Complementary to `get_blueprint_graph_info` — this answers "what IS this Blueprint?", not "what does its graph look like".

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |

**Returns:**
- `blueprint_name`, `path`
- `parent_class` (full path), `parent_class_short` (e.g. `Character`)
- `generated_class` (path of the BlueprintGeneratedClass)
- `blueprint_type` (e.g. `BPTYPE_Normal`)
- `implemented_interfaces` — array of `{name, path, graph_count}`
- `components` — array of `{name, component_class, component_class_short, is_native, parent_component?}`; `is_native=true` means inherited from the parent C++ class's CDO, `false` means added via SCS at the BP level.

---

### get_blueprint_defaults

Read a Blueprint CDO's property values (Details-panel-level data).

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `property_path` | string | no | Top-level property name to narrow the dump (e.g. `GrantAbilitiesOnStart`). Nested paths not yet supported. |

**Returns:** `blueprint_name`, `path`, `parent_class`, `generated_class`, `properties` (object — all properties by default, or just `{property_path: value}` when narrowed). Transient properties are skipped. UObject/asset references serialize as object paths; arrays and structs recurse.

---

## Blueprint Node

All blueprint node commands operate on a Blueprint's event graph. Most accept an optional `graph_name` (default: `EventGraph`) and `node_position` (`{x, y}`) where applicable.

### get_blueprint_graph_info

Read a Blueprint graph's node structure, pins, and connections.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:**
- `nodes` — array with `node_id`, `node_class`, `title`, `x`/`y` position, `width`/`height`, and `pins`.
- `available_graphs` — flat list of every graph on the blueprint, including interface override graphs (searches `UbergraphPages`, `FunctionGraphs`, `MacroGraphs`, and `ImplementedInterfaces[].Graphs`).
- `interface_graphs` — map `{interface_name: [graph_names]}` for graphs found under `ImplementedInterfaces`. Use to tell overrides apart from plain function graphs.
- `overlaps` — layout overlap analysis for external formatters.

Each pin entry includes:
- `pin_name`, `direction`, `pin_type`.
- `pin_subtype` / `pin_subtype_object` — needed to interpret default values for struct / enum / class pins (e.g. `GameplayTag`).
- `connected_to` — array of `{node_id, pin_name}`.
- `default_value`, `default_object_path`, `default_text`, `autogenerated_default` — literal values set on unconnected input pins (at least one is present when the pin carries an explicit default). This is how planner-facing configuration like `MakeStruct` field values and `Send Gameplay Event` tag are surfaced.

A pin is emitted if it has connections, is an exec pin, or carries an explicit default value; otherwise it is omitted to keep output readable.

---

### add_blueprint_event_node

Add an event node (lifecycle/override events use `AddDefaultEventNode` internally).

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `event_name` | string | yes | Event name (e.g. `ReceiveBeginPlay`, `Construct`) |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`.

---

### add_blueprint_input_action_node

Add an input action event node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `action_name` | string | yes | Input action name |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`.

---

### add_blueprint_function_node

Add a function call node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `function_name` | string | yes | Function name |
| `target` | string | no | Target class for the function |
| `params` | object | no | Parameter default values (key-value pairs) |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`.

---

### add_blueprint_get_self_component_reference

Add a "Get Component" variable reference node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `component_name` | string | yes | Component to reference |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`.

---

### add_blueprint_get_component_node

Add a GetComponentByClass node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `component_name` | string | yes | Component class name |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`.

---

### add_blueprint_self_reference

Add a self-reference node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`.

---

### add_blueprint_dynamic_cast_node

Add a dynamic cast node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `target_class` | string | yes | Class to cast to |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`.

---

### add_blueprint_subsystem_getter_node

Add a GetSubsystem node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `subsystem_class` | string | yes | Subsystem class name |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`, `subsystem_class`.

---

### add_blueprint_make_struct_node

Add a MakeStruct node with optional default field values.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `struct_type` | string | yes | Struct type name |
| `values` | object | no | Struct field default values |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`, `struct_type`, `output_pin`.

---

### add_blueprint_branch_node

Add an If/Branch node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`, `condition_pin`, `true_pin`, `false_pin`.

---

### add_blueprint_switch_enum_node

Add a Switch on Enum node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `enum_type` | string | yes | Enum type name or path |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`, `enum_type`, `selection_pin`, `output_pins`.

---

### add_blueprint_array_get_node

Add an Array Get (by index) node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_position` | object | no | `{x, y}` |

**Returns:** `node_id`, `array_pin`, `index_pin`, `output_pin`.

---

### add_blueprint_variable

Add a member variable to a Blueprint.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `variable_name` | string | yes | Variable name |
| `variable_type` | string | yes | Type: `Boolean`, `Integer`, `Float`, `String`, `Vector` |
| `is_exposed` | bool | no | Expose to editor |

**Returns:** `variable_name`, `variable_type`.

---

### bind_blueprint_multicast_delegate

Create an AssignDelegate node bound to a multicast delegate with an auto-created CustomEvent.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `target_class` | string | yes | Class owning the delegate |
| `delegate_name` | string | yes | Delegate property name |
| `node_position` | object | no | `{x, y}` for assign node |
| `custom_event_position` | object | no | `{x, y}` for custom event node |
| `target_node_id` | string | no | Node providing the target object |
| `target_output_pin` | string | no | Pin on target node (default: `ReturnValue`) |
| `assign_target_pin` | string | no | Pin on assign node (default: `self`) |
| `exec_source_node_id` | string | no | Node to chain exec from |
| `exec_source_pin` | string | no | Exec pin name (default: `Then`) |
| `assign_exec_pin` | string | no | Assign node exec pin (default: `Execute`) |

**Returns:** `assign_node_id`, `delegate_name`, `target_class`, `custom_event_node_id`, `custom_event_name`.

---

### connect_blueprint_nodes

Connect two node pins.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `source_node_id` | string | yes | Source node GUID |
| `source_pin` | string | yes | Source pin name |
| `target_node_id` | string | yes | Target node GUID |
| `target_pin` | string | yes | Target pin name |

**Returns:** `source_node_id`, `target_node_id`.

---

### disconnect_blueprint_nodes

Disconnect a specific link between two pins.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `source_node_id` | string | yes | Source node GUID |
| `source_pin` | string | yes | Source pin name |
| `target_node_id` | string | yes | Target node GUID |
| `target_pin` | string | yes | Target pin name |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:** `source_node_id`, `source_pin`, `target_node_id`, `target_pin`.

---

### break_blueprint_node_pin_links

Break all links on a specific pin.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_id` | string | yes | Node GUID |
| `pin_name` | string | yes | Pin name |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:** `node_id`, `pin_name`, `removed_links`.

---

### move_blueprint_node

Move a single node to an absolute position.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_id` | string | yes | Node GUID |
| `node_position` | object | yes | `{x, y}` |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:** `node_id`, `x`, `y`.

---

### move_blueprint_nodes

Batch move multiple nodes.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `moves` | array | yes | Array of `{node_id, node_position: {x, y}}` |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:** `moved_count`, `missing_count`, `invalid_count`, `moved_nodes`, `missing_node_ids`.

---

### delete_blueprint_node

Delete a single node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_id` | string | yes | Node GUID |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:** `node_id`, `deleted`.

---

### delete_blueprint_nodes

Batch delete multiple nodes.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_ids` | array | yes | Array of node GUID strings |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:** `deleted_count`, `missing_count`, `deleted_node_ids`, `missing_node_ids`.

---

### set_blueprint_node_pin_default

Set the default value of an input pin.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_id` | string | yes | Node GUID |
| `pin_name` | string | yes | Input pin name |
| `value` | any | yes | Default value (string, number, bool, array, object) |
| `graph_name` | string | no | Graph name (default: `EventGraph`) |

**Returns:** `node_id`, `pin_name`, `default_value`, `has_links`.

---

### find_blueprint_nodes

Find nodes by type in a graph.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `node_type` | string | yes | Node type (currently: `Event`) |
| `event_name` | string | no | Filter by event name (when `node_type` is `Event`) |

**Returns:** `node_guids` — array of GUID strings.

---

### clear_blueprint_event_graph

Clear all nodes from an event graph.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `keep_bound_events` | bool | no | Preserve component bound events (default: false) |

**Returns:** `removed_count`, `kept_count`.

---

### clear_blueprint_event_exec_chain

Remove all nodes chained after an event node's execution pin.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `event_node_id` | string | yes | Event node GUID |
| `event_output_pin` | string | no | Exec output pin name (default: `Then`) |

**Returns:** `event_node_id`, `removed_nodes`.

---

### dedupe_blueprint_component_bound_events

Remove duplicate component-bound event nodes, keeping one.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Target Blueprint |
| `widget_name` | string | yes | Component/widget name |
| `event_name` | string | yes | Event name (e.g. `OnClicked`) |
| `keep_node_id` | string | no | GUID of the node to keep |
| `event_output_pin` | string | no | Exec pin name (default: `Then`) |

**Returns:** `kept_node_id`, `removed_count`, `removed_node_ids`.

---

## UMG Widget

### create_umg_widget_blueprint

Create a new Widget Blueprint.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `widget_name` | string | yes | Widget name (legacy alias: `name`) |
| `path` | string | no | Folder path (default: `/Game/Widgets`) |
| `parent_class` | string | no | Parent class path (default: `UUserWidget`) |

**Returns:** `name`, `widget_name`, `path`.

---

### get_widget_tree

Read the widget hierarchy of a Widget Blueprint.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |

**Returns:** `has_root`, `root` — recursive tree of widgets with name, class, children.

---

### ensure_widget_root

Create or reuse a root widget.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_class` | string | yes | Root widget class (e.g. `CanvasPanel`) |
| `widget_name` | string | yes | Root widget name |
| `replace_existing` | bool | no | Replace existing root (default: false) |

**Returns:** `created`, `replaced`, `renamed`, `root`.

---

### add_widget_child

Add a child widget under a parent panel.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_class` | string | yes | Child widget class |
| `widget_name` | string | yes | Child widget name |
| `parent_widget_name` | string | no | Parent panel name (legacy alias: `parent_name`; default: root) |
| `is_variable` | bool | no | Mark as variable (default: false) |

**Returns:** `widget_name`, `widget_class`, `slot_class`.

---

### add_widget_child_batch

Batch add multiple child widgets in one compile/save cycle.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `items` | array | yes | Array of `{widget_class, widget_name, parent_widget_name?, is_variable?}` |

**Returns:** `created_count`, `results`.

---

### set_canvas_slot_layout

Set layout properties for a widget in a CanvasPanel.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | Widget name |
| `position` | array | no | `[x, y]` |
| `size` | array | no | `[width, height]` |
| `alignment` | array | no | `[x, y]` (0.0–1.0) |
| `anchors` | array | no | `[min_x, min_y, max_x, max_y]` or `[x, y]` |
| `auto_size` | bool | no | Enable auto-sizing |
| `z_order` | number | no | Z-order |

**Returns:** readback of all applied properties.

---

### set_canvas_slot_layout_batch

Batch set canvas slot layout for multiple widgets.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `items` | array | yes | Array of `{widget_name, position?, size?, alignment?, anchors?, auto_size?, z_order?}` |

**Returns:** `updated_count`, `results`.

---

### set_uniform_grid_slot

Set grid slot properties for a widget in a UniformGridPanel.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | Widget name |
| `row` | number | no | Grid row index |
| `column` | number | no | Grid column index |
| `horizontal_alignment` | string | no | `Fill`, `Left`, `Center`, `Right` |
| `vertical_alignment` | string | no | `Fill`, `Top`, `Center`, `Bottom` |

**Returns:** readback of applied properties.

---

### set_uniform_grid_slot_batch

Batch set uniform grid slot properties.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `items` | array | yes | Array of `{widget_name, row?, column?, horizontal_alignment?, vertical_alignment?}` |

**Returns:** `updated_count`, `results`.

---

### set_widget_common_properties

Set common widget properties (visibility, enabled, variable).

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | Widget name |
| `visibility` | string | no | `Visible`, `Collapsed`, `Hidden`, `HitTestInvisible`, `SelfHitTestInvisible` |
| `is_enabled` | bool | no | Enable/disable widget |
| `is_variable` | bool | no | Mark as variable |

**Returns:** readback of `visibility`, `is_enabled`, `is_variable`.

---

### set_widget_common_properties_batch

Batch set common widget properties.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `items` | array | yes | Array of `{widget_name, visibility?, is_enabled?, is_variable?}` |

**Returns:** `updated_count`, `results`.

---

### set_text_block_properties

Set text and color on a TextBlock widget.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | TextBlock widget name |
| `text` | string | no | Text content |
| `color` | array | no | `[r, g, b, a]` (0.0–1.0) |

**Returns:** readback of `text`, `color`.

---

### set_text_block_properties_batch

Batch set text block properties.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `items` | array | yes | Array of `{widget_name, text?, color?}` |

**Returns:** `updated_count`, `results`.

---

### add_text_block_to_widget

Add a TextBlock to a widget.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | TextBlock name |
| `text` | string | no | Initial text (default: `New Text Block`) |
| `position` | array | no | `[x, y]` |

**Returns:** `widget_name`, `text`.

---

### add_button_to_widget

Add a Button to a widget.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | Button name |
| `text` | string | yes | Button label text |
| `position` | array | no | `[x, y]` |

**Returns:** `widget_name`.

---

### bind_widget_event

Bind a widget event (e.g. OnClicked) to a component-bound event node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path (legacy alias: `widget_name`) |
| `widget_name` | string | yes | Widget name (legacy alias: `widget_component_name`) |
| `event_name` | string | yes | Event name (e.g. `OnClicked`) |
| `node_position` | array | no | `[x, y]` node graph position |

**Returns:** `widget_name`, `event_name`, `node_id`.

---

### set_text_block_binding

Set a property binding on a TextBlock.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | TextBlock name (legacy alias: `text_block_name`) |
| `binding_name` | string | yes | Binding function name (legacy alias: `binding_property`) |

**Returns:** `binding_name`.

---

### add_widget_to_viewport

Add a widget to the viewport at runtime.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `z_order` | number | no | Z-order (default: 0) |

**Returns:** `blueprint_name`, `z_order`.

---

### clear_widget_children

Clear all children of a panel widget.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | no | Target panel (default: root) |

**Returns:** `removed_direct_children`, `removed_total_widgets`, `root`.

---

### remove_widget_from_blueprint

Remove a widget and its subtree.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `blueprint_name` | string | yes | Widget Blueprint name/path |
| `widget_name` | string | yes | Widget to remove |

**Returns:** `removed_widget_name`, `removed_total_widgets`, `root`.

---

### delete_widget_blueprints_by_prefix

Delete Widget Blueprints matching a name prefix.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `path` | string | yes | Content folder path |
| `name_prefix` | string | yes | Name prefix to match |
| `recursive` | bool | no | Recurse into subdirs (default: true) |
| `dry_run` | bool | no | Preview only, no deletion (default: false) |

**Returns:** `matched_count`, `matched_assets`, `deleted_count` (if not dry_run).

---

## Project

### create_input_mapping

Create an input action mapping.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `action_name` | string | yes | Input action name |
| `key` | string | yes | Key to map (e.g. `A`, `Space`) |
| `shift` | bool | no | Shift modifier |
| `ctrl` | bool | no | Ctrl modifier |
| `alt` | bool | no | Alt modifier |
| `cmd` | bool | no | Cmd modifier |

**Returns:** `action_name`, `key`.

---

## Behavior Tree

### get_behavior_tree_info

Read a BehaviorTree asset structure.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Path to BehaviorTree asset |

**Returns:** `asset_name`, `blackboard`, `blackboard_keys` (array of `{name, type}`), `root` — recursive node tree with composites, tasks, decorators, services.

---

## Animation

### get_montage_info

Read an AnimMontage asset's structure.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Path to AnimMontage asset |

**Returns:** `asset_name`, `play_length`, `rate_scale`, `sections`, `slot_tracks`, `notifies`, `branching_points`.
