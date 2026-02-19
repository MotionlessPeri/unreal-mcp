"""
Blueprint Node Tools for Unreal MCP.

This module provides tools for manipulating Blueprint graph nodes and connections.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_blueprint_node_tools(mcp: FastMCP):
    """Register Blueprint node manipulation tools with the MCP server."""
    
    @mcp.tool()
    def add_blueprint_event_node(
        ctx: Context,
        blueprint_name: str,
        event_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add an event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            event_name: Name of the event. Use 'Receive' prefix for standard events:
                       - 'ReceiveBeginPlay' for Begin Play
                       - 'ReceiveTick' for Tick
                       - etc.
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "event_name": event_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding event node '{event_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_event_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Event node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding event node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_input_action_node(
        ctx: Context,
        blueprint_name: str,
        action_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add an input action event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            action_name: Name of the input action to respond to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "action_name": action_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding input action node for '{action_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_input_action_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Input action node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding input action node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_function_node(
        ctx: Context,
        blueprint_name: str,
        target: str,
        function_name: str,
        params = None,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a function call node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            target: Target object for the function (component name or self)
            function_name: Name of the function to call
            params: Optional parameters to set on the function node
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default values within the method body
            if params is None:
                params = {}
            if node_position is None:
                node_position = [0, 0]
            
            command_params = {
                "blueprint_name": blueprint_name,
                "target": target,
                "function_name": function_name,
                "params": params,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding function node '{function_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_function_node", command_params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Function node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding function node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
            
    @mcp.tool()
    def connect_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str
    ) -> Dict[str, Any]:
        """
        Connect two nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            source_node_id: ID of the source node
            source_pin: Name of the output pin on the source node
            target_node_id: ID of the target node
            target_pin: Name of the input pin on the target node
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Connecting nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("connect_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node connection response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error connecting nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False
    ) -> Dict[str, Any]:
        """
        Add a variable to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable
            variable_type: Type of the variable (Boolean, Integer, Float, Vector, etc.)
            is_exposed: Whether to expose the variable to the editor
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "variable_type": variable_type,
                "is_exposed": is_exposed
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding variable '{variable_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Variable creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding variable: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_get_self_component_reference(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a node that gets a reference to a component owned by the current Blueprint.
        This creates a node similar to what you get when dragging a component from the Components panel.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the component to get a reference to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle None case explicitly in the function
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding self component reference node for '{component_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_get_self_component_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Self component reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self component reference node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_self_reference(
        ctx: Context,
        blueprint_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a 'Get Self' node to a Blueprint's event graph that returns a reference to this actor.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            if node_position is None:
                node_position = [0, 0]
                
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding self reference node to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_self_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Self reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self reference node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_blueprint_dynamic_cast_node(
        ctx: Context,
        blueprint_name: str,
        target_class: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a dynamic cast node to a Blueprint event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            target_class: Class name or script path to cast to
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            if node_position is None:
                node_position = [0, 0]

            params = {
                "blueprint_name": blueprint_name,
                "target_class": target_class,
                "node_position": node_position
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(f"Adding dynamic cast node to '{target_class}' in blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_dynamic_cast_node", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Dynamic cast node creation response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding dynamic cast node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_blueprint_subsystem_getter_node(
        ctx: Context,
        blueprint_name: str,
        subsystem_class: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a dedicated subsystem getter node (UK2Node_GetSubsystem).

        Args:
            blueprint_name: Name/path of the target Blueprint
            subsystem_class: Subsystem class name or script path
            node_position: Optional [X, Y] position

        Returns:
            Response containing created node id.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            if node_position is None:
                node_position = [0, 0]

            params = {
                "blueprint_name": blueprint_name,
                "subsystem_class": subsystem_class,
                "node_position": node_position
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(
                "Adding subsystem getter node '%s' in blueprint '%s'",
                subsystem_class,
                blueprint_name,
            )
            response = unreal.send_command("add_blueprint_subsystem_getter_node", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Subsystem getter node creation response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding subsystem getter node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_blueprint_make_struct_node(
        ctx: Context,
        blueprint_name: str,
        struct_type: str,
        node_position = None,
        values: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        Add a MakeStruct node and optionally initialize field defaults.

        Args:
            blueprint_name: Name/path of the target Blueprint
            struct_type: Struct type name or script path
            node_position: Optional [X, Y] position
            values: Optional field default values map

        Returns:
            Response containing node id and output pin name.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            if node_position is None:
                node_position = [0, 0]
            if values is None:
                values = {}

            params = {
                "blueprint_name": blueprint_name,
                "struct_type": struct_type,
                "node_position": node_position,
                "values": values,
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(
                "Adding make-struct node '%s' in blueprint '%s'",
                struct_type,
                blueprint_name,
            )
            response = unreal.send_command("add_blueprint_make_struct_node", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"MakeStruct node creation response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding make-struct node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def break_blueprint_node_pin_links(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        pin_name: str
    ) -> Dict[str, Any]:
        """
        Break all links on a specific node pin.

        Args:
            blueprint_name: Name/path of the target Blueprint
            node_id: Guid string of the target node
            pin_name: Pin name to clear links from

        Returns:
            Response containing removed link count.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "pin_name": pin_name,
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info("Breaking links on node '%s' pin '%s'", node_id, pin_name)
            response = unreal.send_command("break_blueprint_node_pin_links", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Break pin links response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error breaking blueprint node pin links: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def clear_blueprint_event_exec_chain(
        ctx: Context,
        blueprint_name: str,
        event_node_id: str,
        event_output_pin: str = "Then"
    ) -> Dict[str, Any]:
        """
        Clear an event node's downstream execution chain and dependent nodes.

        Args:
            blueprint_name: Name/path of the target Blueprint
            event_node_id: Guid string of the event node
            event_output_pin: Output exec pin name (default: Then)

        Returns:
            Response containing removed node count.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "event_node_id": event_node_id,
                "event_output_pin": event_output_pin,
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info("Clearing event exec chain from event node '%s'", event_node_id)
            response = unreal.send_command("clear_blueprint_event_exec_chain", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Clear event exec chain response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error clearing blueprint event exec chain: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def dedupe_blueprint_component_bound_events(
        ctx: Context,
        blueprint_name: str,
        widget_name: str,
        event_name: str,
        keep_node_id: str = "",
        event_output_pin: str = "Then"
    ) -> Dict[str, Any]:
        """
        Dedupe duplicated UK2Node_ComponentBoundEvent nodes for one widget event.

        Args:
            blueprint_name: Name/path of the target Blueprint
            widget_name: Widget component variable name in widget blueprint
            event_name: Delegate/event name (e.g. OnClicked)
            keep_node_id: Optional node guid to keep if duplicates exist
            event_output_pin: Output exec pin to clear (default: Then)

        Returns:
            Response containing kept node id and removed node counts.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "widget_name": widget_name,
                "event_name": event_name,
                "keep_node_id": keep_node_id,
                "event_output_pin": event_output_pin,
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(
                "Deduping bound events for widget '%s' event '%s' in blueprint '%s'",
                widget_name,
                event_name,
                blueprint_name,
            )
            response = unreal.send_command("dedupe_blueprint_component_bound_events", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Dedupe bound events response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error deduping blueprint component bound events: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def bind_blueprint_multicast_delegate(
        ctx: Context,
        blueprint_name: str,
        target_class: str,
        delegate_name: str,
        target_node_id: str = "",
        target_output_pin: str = "ReturnValue",
        assign_target_pin: str = "self",
        exec_source_node_id: str = "",
        exec_source_pin: str = "Then",
        assign_exec_pin: str = "Execute",
        node_position = None,
        custom_event_position = None
    ) -> Dict[str, Any]:
        """
        Add an Assign Delegate node for a multicast delegate and auto-create its custom event node.

        Args:
            blueprint_name: Name of the target Blueprint
            target_class: Class that owns the multicast delegate property
            delegate_name: Delegate property name (e.g. OnJoinAckParsed)
            target_node_id: Optional source object node id to connect to assign target pin
            target_output_pin: Source output pin name on target_node_id (default ReturnValue)
            assign_target_pin: Assign-node target pin name (default self)
            exec_source_node_id: Optional source exec node id to chain into assign node
            exec_source_pin: Source exec pin name (default Then)
            assign_exec_pin: Assign-node exec input pin (default Execute)
            node_position: Optional [X, Y] for assign node
            custom_event_position: Optional [X, Y] for generated custom event node

        Returns:
            Response containing assign/custom-event node ids.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            if node_position is None:
                node_position = [0, 0]

            command_params = {
                "blueprint_name": blueprint_name,
                "target_class": target_class,
                "delegate_name": delegate_name,
                "target_node_id": target_node_id,
                "target_output_pin": target_output_pin,
                "assign_target_pin": assign_target_pin,
                "exec_source_node_id": exec_source_node_id,
                "exec_source_pin": exec_source_pin,
                "assign_exec_pin": assign_exec_pin,
                "node_position": node_position,
            }
            if custom_event_position is not None:
                command_params["custom_event_position"] = custom_event_position

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(
                "Binding multicast delegate '%s' on blueprint '%s'",
                delegate_name,
                blueprint_name,
            )
            response = unreal.send_command("bind_blueprint_multicast_delegate", command_params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Delegate bind response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error binding blueprint multicast delegate: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def find_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type = None,
        event_type = None
    ) -> Dict[str, Any]:
        """
        Find nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_type: Optional type of node to find (Event, Function, Variable, etc.)
            event_type: Optional specific event type to find (BeginPlay, Tick, etc.)
            
        Returns:
            Response containing array of found node IDs and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_type": node_type,
                "event_type": event_type
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Finding nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("find_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node find response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error finding nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def clear_blueprint_event_graph(
        ctx: Context,
        blueprint_name: str,
        keep_bound_events: bool = False
    ) -> Dict[str, Any]:
        """
        Remove nodes from a Blueprint event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            keep_bound_events: If true, keep UK2Node_ComponentBoundEvent nodes

        Returns:
            Response containing removed/kept node counts
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "keep_bound_events": keep_bound_events
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(f"Clearing event graph for blueprint '{blueprint_name}'")
            response = unreal.send_command("clear_blueprint_event_graph", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Clear graph response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error clearing blueprint event graph: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Blueprint node tools registered successfully")
