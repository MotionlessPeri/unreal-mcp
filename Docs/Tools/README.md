# Unreal MCP Tools

This document is an index to all the tools supported.

- [Actor Tools](actor_tools.md)
- [Editor Tools](editor_tools.md)
- [Blueprint Tools](blueprint_tools.md)

## Blueprint Node Tooling Notes

The Blueprint node manipulation surface is implemented in `Python/tools/node_tools.py`.
Recent graph-automation additions include:

1. `clear_blueprint_event_graph`
2. `add_blueprint_dynamic_cast_node`
3. `bind_blueprint_multicast_delegate`
4. `add_blueprint_subsystem_getter_node`
5. `add_blueprint_make_struct_node`
6. `break_blueprint_node_pin_links`
7. `clear_blueprint_event_exec_chain`
8. `dedupe_blueprint_component_bound_events`
