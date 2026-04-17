# Dialogue Extension Commands

Commands provided by the `UnrealMCPDialogue` extension plugin. These are only available when the extension plugin is loaded alongside the base UnrealMCP plugin.

Protocol: `{"type": "<command>", "params": {...}}`

---

## get_dialogue_graph

Read all nodes from a Dialogue asset.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset (e.g. `/Game/Dialogues/DA_Test`) |

**Returns:** `asset_name`, `nodes` — array of node objects:
- All nodes: `node_id` (GUID), `node_type` (Entry/Speech/Choice/Exit), `position` (`{x, y}`)
- Speech: `speaker_name`, `speaker_asset_path`, `dialogue_text`
- Choice: `speaker_name`, `speaker_asset_path`, `dialogue_text`, `choices` (array of `{text, item_node_id}`)

---

## get_dialogue_connections

Read all connections (edges) in a Dialogue asset.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |

**Returns:** `connections` — array of `{from_node_id, from_pin, to_node_id, to_pin}`.

---

## create_dialogue_asset

Create a new Dialogue asset with a default Entry node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path for the new asset |

**Returns:** `asset_path`, `entry_node_id`.

---

## add_dialogue_node

Add a new node to a Dialogue asset.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `node_type` | string | yes | `Speech`, `Choice`, `Exit`, or `Reroute` |
| `pos_x` | number | no | X position in editor (default: 0) |
| `pos_y` | number | no | Y position in editor (default: 0) |

**Returns:** `node_id`, `node_type`. Choice nodes also return `choice_item_node_ids` (array of GUIDs for default choice items).

---

## set_dialogue_node_properties

Set properties on a Dialogue node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `node_id` | string | yes | Node GUID |
| `speaker_asset_path` | string | no | Path to DialogueSpeakerAsset |
| `speaker_name` | string | no | Speaker display name |
| `dialogue_text` | string | no | Dialogue text (Speech nodes) |
| `choice_text` | string | no | Choice item text (ChoiceItem nodes only) |
| `pos_x` | number | no | X position in editor |
| `pos_y` | number | no | Y position in editor |

**Returns:** `node_id`.

---

## connect_dialogue_nodes

Create a connection between two Dialogue nodes.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `from_node_id` | string | yes | Source node GUID |
| `to_node_id` | string | yes | Target node GUID |
| `from_pin` | string | no | Source pin name (default: `Out`). For Choice nodes, use `Item_N` for specific choice item pins |

**Returns:** `from_node_id`, `from_pin`, `to_node_id`.

---

## disconnect_dialogue_nodes

Remove a connection between two Dialogue nodes.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `from_node_id` | string | yes | Source node GUID |
| `to_node_id` | string | yes | Target node GUID |
| `from_pin` | string | no | Source pin name (default: `Out`) |

**Returns:** `from_node_id`, `to_node_id`.

---

## delete_dialogue_node

Delete a node from a Dialogue asset. Entry nodes and ChoiceItem nodes cannot be deleted.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `node_id` | string | yes | Node GUID to delete |

**Returns:** `deleted_node_id`.

---

## add_dialogue_choice_item

Add a new choice item to a Choice node.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `node_id` | string | yes | Choice node GUID |
| `choice_text` | string | no | Text for the new choice item |

**Returns:** `item_node_id`, `item_index`.

---

## set_transition_condition

Set or clear a condition evaluator on a transition between nodes.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `from_node_id` | string | yes | Source node GUID |
| `to_node_id` | string | yes | Target node GUID |
| `condition_class_path` | string | no | Class path to condition evaluator (empty or omitted to clear) |

**Returns:** `from_node_id`, `to_node_id`, `condition_class`.
