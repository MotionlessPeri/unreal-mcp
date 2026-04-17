# LogicDriver Extension Commands

Commands provided by the `UnrealMCPLogicDriver` extension plugin. These are only available when the extension plugin is loaded alongside the base UnrealMCP plugin.

Protocol: `{"type": "<command>", "params": {...}}`

---

## get_logicdriver_state_machine_graph

Read a Logic Driver state machine graph structure.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | no* | Path to StateMachine Blueprint |
| `blueprint_name` | string | no* | Name of StateMachine Blueprint |
| `graph_name` | string | no | Specific graph name (default: root graph) |

\* At least one of `asset_path` or `blueprint_name` is required.

**Returns:**

| Field | Description |
|-------|-------------|
| `blueprint_name` | Blueprint name |
| `asset_path` | Full asset path |
| `graph_name` | Queried graph name |
| `graph_class` | Graph class name |
| `has_any_logic_connections` | Whether graph has connections |
| `node_count` | Total node count |
| `nodes` | Array of generic graph nodes |
| `states` | Array of state nodes |
| `special_nodes` | Array of Entry, AnyState, Conduit, LinkState nodes |
| `transitions` | Array of transition objects |
| `edges` | Array of graph connections |
| `entry_node` | Entry node object |
| `available_state_machine_graphs` | List of available graph names |

**State node fields:** `node_id`, `node_kind`, `title`, `name`, `position`, `state_name`, `is_end_state`, `has_input_connections`, `has_output_connections`, `connected_to_entry`, `bound_graph`, `is_state_machine_reference`, `referenced_blueprint`.

**Transition fields:** `from_node_id`, `from_state_name`, `to_node_id`, `to_state_name`, `transition_name`, `transition_class`, `conditional_evaluation_type`, `condition_graph` (nested node/edge structure).
