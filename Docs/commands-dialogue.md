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
- All nodes: `node_id` (GUID), `node_type` (Entry/Speech/Choice/Exit), `position` (`{x, y}`), `callback_class_path` (string, empty if none), `line_id` (string, empty for Entry/Exit/Conduit/Reroute)
- Speech: `speaker_name`, `speaker_asset_path`, `dialogue_text`
- Choice: `speaker_name`, `speaker_asset_path`, `dialogue_text`, `choices` (array of `{text, item_node_id, line_id}`)

---

## get_dialogue_connections

Read all connections (edges) in a Dialogue asset.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |

**Returns:** `connections` — array of `{from_node_id, from_pin, to_node_id, to_pin, condition_class_path}`. `condition_class_path` is empty when no condition is attached.

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

---

## set_node_callback_class

Set or clear the `CallbackClass` on a dialogue node. Equivalent to the Details panel's Callback Class picker (or the double-click-creates-callback flow). Only nodes whose `SupportsCallbacks()` returns true accept this.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `node_id` | string | yes | Node GUID |
| `callback_class_path` | string | no | Class path (`/Game/.../BP_MyCB.BP_MyCB_C`) or BP asset path (`/Game/.../BP_MyCB.BP_MyCB`). Omit or pass `""` to clear. |

**Returns:** `node_id`, `callback_class` (resolved path or `""`).

---

## bind_dialogue_node_line

Bind a LineId to a dialogue node. Equivalent to the Details panel "Pick Line..." button.

- Speech node: sets `LineId` only.
- Choice node: sets `LineId` AND batch-fills every `ChoiceItem` whose LineId matches the convention `<Choice>_<suffix>` in the DB (replaces existing items, same as the GUI Picker's Choice mode).
- ChoiceItem nodes: returns an error. ChoiceItems are driven by their parent Choice's batch fill; bind the parent Choice instead.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `node_id` | string | yes | Speech or Choice node GUID |
| `line_id` | string | yes | Target LineId in the Line Database (e.g. `L_GE_Halt`) |

**Returns:** `node_id`, `line_id`, `node_type`, `items_filled` (0 for Speech), `item_line_ids` (Choice only, sorted ChoiceItem LineIds).

---

## unbind_dialogue_node_line

Clear a node's LineId. Equivalent to the Details panel "Unbind" button.

- Speech node: `LineId` -> `None`.
- Choice node: `LineId` -> `None` AND cascades into every `ChoiceItem` (their `LineId` -> `None`). Pin connections, `ConditionClass`, `CallbackClass` are preserved.
- ChoiceItem nodes: returns an error (unbind parent Choice instead).

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | yes | Content path to DialogueAsset |
| `node_id` | string | yes | Speech or Choice node GUID |

**Returns:** `node_id`, `choice_items_cleared` (0 for Speech).

---

## query_dialogue_line

Look up a single Line row from the editor `ULineRegistry`.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `line_id` | string | yes | LineId to query (e.g. `L_GE_Halt`) |

**Returns:** `found` (bool). When found: `line_type`, `dialogue_id`, `speaker_id`, `speaker_display_name`, `text`, `notes`, `next_nodes` (array of LineIds; includes `__EXIT__` sentinel).

---

## list_dialogue_lines

List all Line rows from the editor `ULineRegistry`, optionally filtered by `DialogueID`.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `dialogue_id` | string | no | If present, return only rows whose `DialogueID` matches |

**Returns:** `count`, `lines` (array of `{line_id, line_type, dialogue_id, speaker_id, text}`, sorted by LineId). If `dialogue_id` was given, also echoes it back as `dialogue_id_filter`.

---

## dialogue_registry_info

Debug snapshot of `ULineRegistry` state.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| (none) | | | |

**Returns:** `registry_available` (bool), `line_database_path` (string — `UDialogueSettings::LineDatabase`, may be empty), `line_count`, `speaker_count`. The counts trigger `EnsureFullCache` on first call (subsequent calls are O(1)).
