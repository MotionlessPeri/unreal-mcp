"""
Blueprint Tools for Unreal MCP.

This module provides tools for creating and manipulating Blueprint assets in Unreal Engine.
"""

import logging
from typing import Dict, List, Any
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_blueprint_tools(mcp: FastMCP):
    """Register Blueprint tools with the MCP server."""
    
    @mcp.tool()
    def create_blueprint(
        ctx: Context,
        name: str,
        parent_class: str
    ) -> Dict[str, Any]:
        """Create a new Blueprint class."""
        # Import inside function to avoid circular imports
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("create_blueprint", {
                "name": name,
                "parent_class": parent_class
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Blueprint creation response: {response}")
            return response or {}
            
        except Exception as e:
            error_msg = f"Error creating blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_component_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        location: List[float] = [],
        rotation: List[float] = [],
        scale: List[float] = [],
        component_properties: Dict[str, Any] = {}
    ) -> Dict[str, Any]:
        """
        Add a component to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_type: Type of component to add (use component class name without U prefix)
            component_name: Name for the new component
            location: [X, Y, Z] coordinates for component's position
            rotation: [Pitch, Yaw, Roll] values for component's rotation
            scale: [X, Y, Z] values for component's scale
            component_properties: Additional properties to set on the component
        
        Returns:
            Information about the added component
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "component_type": component_type,
                "component_name": component_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0],
                "scale": scale or [1.0, 1.0, 1.0]
            }
            
            # Add component_properties if provided
            if component_properties and len(component_properties) > 0:
                params["component_properties"] = component_properties
            
            # Validate location, rotation, and scale formats
            for param_name in ["location", "rotation", "scale"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            logger.info(f"Adding component to blueprint with params: {params}")
            response = unreal.send_command("add_component_to_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Component addition response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding component to blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_static_mesh_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        static_mesh: str = "/Engine/BasicShapes/Cube.Cube"
    ) -> Dict[str, Any]:
        """
        Set static mesh properties on a StaticMeshComponent.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the StaticMeshComponent
            static_mesh: Path to the static mesh asset (e.g., "/Engine/BasicShapes/Cube.Cube")
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "static_mesh": static_mesh
            }
            
            logger.info(f"Setting static mesh properties with params: {params}")
            response = unreal.send_command("set_static_mesh_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set static mesh properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting static mesh properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on a component in a Blueprint."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting component property with params: {params}")
            response = unreal.send_command("set_component_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set component property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting component property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_physics_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        simulate_physics: bool = True,
        gravity_enabled: bool = True,
        mass: float = 1.0,
        linear_damping: float = 0.01,
        angular_damping: float = 0.0
    ) -> Dict[str, Any]:
        """Set physics properties on a component."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "simulate_physics": simulate_physics,
                "gravity_enabled": gravity_enabled,
                "mass": float(mass),
                "linear_damping": float(linear_damping),
                "angular_damping": float(angular_damping)
            }
            
            logger.info(f"Setting physics properties with params: {params}")
            response = unreal.send_command("set_physics_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set physics properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting physics properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def compile_blueprint(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """Compile a Blueprint."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name
            }
            
            logger.info(f"Compiling blueprint: {blueprint_name}")
            response = unreal.send_command("compile_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Compile blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error compiling blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_blueprint_property(
        ctx: Context,
        blueprint_name: str,
        property_name: str,
        property_value
    ) -> Dict[str, Any]:
        """
        Set a property on a Blueprint class default object.
        
        Args:
            blueprint_name: Name of the target Blueprint
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting blueprint property with params: {params}")
            response = unreal.send_command("set_blueprint_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set blueprint property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting blueprint property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    # @mcp.tool() commented out, just use set_component_property instead
    def set_pawn_properties(
        ctx: Context,
        blueprint_name: str,
        auto_possess_player: str = "",
        use_controller_rotation_yaw: bool = None,
        use_controller_rotation_pitch: bool = None,
        use_controller_rotation_roll: bool = None,
        can_be_damaged: bool = None
    ) -> Dict[str, Any]:
        """
        Set common Pawn properties on a Blueprint.
        This is a utility function that sets multiple pawn-related properties at once.
        
        Args:
            blueprint_name: Name of the target Blueprint (must be a Pawn or Character)
            auto_possess_player: Auto possess player setting (None, "Disabled", "Player0", "Player1", etc.)
            use_controller_rotation_yaw: Whether the pawn should use the controller's yaw rotation
            use_controller_rotation_pitch: Whether the pawn should use the controller's pitch rotation
            use_controller_rotation_roll: Whether the pawn should use the controller's roll rotation
            can_be_damaged: Whether the pawn can be damaged
            
        Returns:
            Response indicating success or failure with detailed results for each property
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Define the properties to set
            properties = {}
            if auto_possess_player and auto_possess_player != "":
                properties["auto_possess_player"] = auto_possess_player
            
            # Only include boolean properties if they were explicitly set
            if use_controller_rotation_yaw is not None:
                properties["bUseControllerRotationYaw"] = use_controller_rotation_yaw
            if use_controller_rotation_pitch is not None:
                properties["bUseControllerRotationPitch"] = use_controller_rotation_pitch
            if use_controller_rotation_roll is not None:
                properties["bUseControllerRotationRoll"] = use_controller_rotation_roll
            if can_be_damaged is not None:
                properties["bCanBeDamaged"] = can_be_damaged
                
            if not properties:
                logger.warning("No properties specified to set")
                return {"success": True, "message": "No properties specified to set", "results": {}}
            
            # Set each property using the generic set_blueprint_property function
            results = {}
            overall_success = True
            
            for prop_name, prop_value in properties.items():
                params = {
                    "blueprint_name": blueprint_name,
                    "property_name": prop_name,
                    "property_value": prop_value
                }
                
                logger.info(f"Setting pawn property {prop_name} to {prop_value}")
                response = unreal.send_command("set_blueprint_property", params)
                
                if not response:
                    logger.error(f"No response from Unreal Engine for property {prop_name}")
                    results[prop_name] = {"success": False, "message": "No response from Unreal Engine"}
                    overall_success = False
                    continue
                
                results[prop_name] = response
                if not response.get("success", False):
                    overall_success = False
            
            return {
                "success": overall_success,
                "message": "Pawn properties set" if overall_success else "Some pawn properties failed to set",
                "results": results
            }
            
        except Exception as e:
            error_msg = f"Error setting pawn properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_blueprint_graph_info(
        ctx: Context,
        blueprint_name: str,
        graph_name: str = "EventGraph",
    ) -> Dict[str, Any]:
        """
        Read the node graph of a Blueprint, including all pin connections (edges).

        Returns every non-hidden node in the requested graph with its class,
        title, position, and the connections on its exec/data pins.

        Args:
            blueprint_name: Asset name of the Blueprint (e.g. "BTTask_WriteBackToPatrol",
                            "BP_Test"). Name-based lookup, same as other blueprint commands.
            graph_name: Name of the graph to read. Defaults to "EventGraph".
                        Pass "available_graphs" in the error response to discover
                        other graph names (functions, macros, etc.).

        Returns:
            Dict with fields:
              - success (bool)
              - blueprint_name (str)
              - graph_name (str)
              - available_graphs (list[str])   -- all graphs in this blueprint,
                                                  flat list including interface override
                                                  graphs. See interface_graphs for the
                                                  per-interface breakdown.
              - interface_graphs (dict[str, list[str]])
                                                  -- map { interface_name -> [graph_names] }
                                                  for graphs stored under
                                                  ImplementedInterfaces[].Graphs. Use this
                                                  to distinguish interface overrides from
                                                  plain function graphs.
              - node_count (int)
              - nodes (list[dict]) where each node has:
                  - node_id (str)           -- stable GUID string
                  - node_class (str)        -- C++ class, e.g. "K2Node_CallFunction"
                  - title (str)             -- display title shown in editor
                  - x, y (float)            -- canvas position
                  - function_name (str)     -- only for CallFunction nodes
                  - event_name (str)        -- only for Event / CustomEvent nodes
                  - cast_target (str)       -- only for DynamicCast nodes
                  - pins (list[dict]):
                      - pin_name (str)
                      - direction (str)     -- "input" | "output"
                      - pin_type (str)      -- "exec", "bool", "object", etc.
                      - pin_subtype (str)   -- PinType.PinSubCategory (e.g. "GameplayTag"),
                                                 omitted when None
                      - pin_subtype_object (str)
                                             -- PinType.PinSubCategoryObject name
                                                (e.g. struct / enum / class name),
                                                omitted when None
                      - connected_to (list[{node_id, pin_name}])
                      - default_value (str) -- literal set on an input pin
                                                (e.g. "TBIA.Interaction.Animation.Cosmetic")
                      - default_object_path (str)
                                             -- path when pin default is a UObject reference
                      - default_text (str)  -- FText default value as string
                      - autogenerated_default (str)
                                             -- UE-autogenerated fallback (e.g. "0" or "0.0"),
                                                included only when non-empty; does not mean
                                                the user set a value
                    A pin entry is included when it has connections, is an exec pin, OR
                    carries an explicit default value. Pins with none of the above are
                    omitted to keep output small.

        Example:
            get_blueprint_graph_info("BTTask_WriteBackToPatrol")
            get_blueprint_graph_info("BP_Test", graph_name="EventGraph")
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_blueprint_graph_info", {
                "blueprint_name": blueprint_name,
                "graph_name": graph_name,
            })

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"get_blueprint_graph_info response node_count="
                        f"{response.get('result', {}).get('node_count', '?')}")
            return response

        except Exception as e:
            error_msg = f"Error getting blueprint graph info: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_blueprint_info(
        ctx: Context,
        blueprint_name: str,
    ) -> Dict[str, Any]:
        """
        Read a Blueprint's parent class, implemented interfaces, and component tree.

        Complementary to get_blueprint_graph_info (which reads graphs/nodes/pins) — this
        command answers "what IS this Blueprint?": what does it inherit from, which
        interfaces does it implement, and what components are attached (both SCS-added
        at the BP level and native components inherited from the parent C++ class).

        Args:
            blueprint_name: Asset name of the Blueprint (e.g. "BP_PlayerCharacter").

        Returns:
            Dict with fields:
              - blueprint_name (str)
              - path (str)                      -- full asset object path
              - parent_class (str)              -- full path of the C++/BP parent class
              - parent_class_short (str)        -- short name (e.g. "Character")
              - generated_class (str)           -- path of the BlueprintGeneratedClass
              - blueprint_type (str)            -- BPTYPE_Normal / BPTYPE_Interface / etc.
              - implemented_interfaces (list[dict]) where each entry has:
                  - name (str)                  -- interface short name
                  - path (str)                  -- full interface path
                  - graph_count (int)           -- override graphs under this interface
              - components (list[dict]) where each entry has:
                  - name (str)                  -- component variable name
                  - component_class (str)       -- full class path
                  - component_class_short (str) -- class short name
                  - is_native (bool)            -- True = inherited from parent CDO;
                                                    False = added via SCS at BP level
                  - parent_component (str)      -- SCS attach parent variable name
                                                    (only present when attached)

        Example:
            get_blueprint_info("BP_PlayerCharacter")
            get_blueprint_info("BP_PlayerCharacter_ASC_OnPlayerState")
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_blueprint_info", {
                "blueprint_name": blueprint_name,
            })

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"get_blueprint_info response parent="
                        f"{response.get('result', {}).get('parent_class_short', '?')}")
            return response

        except Exception as e:
            error_msg = f"Error getting blueprint info: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_blueprint_defaults(
        ctx: Context,
        blueprint_name: str,
        property_path: str = "",
    ) -> Dict[str, Any]:
        """
        Read a Blueprint CDO's property values (Details-panel-level data).

        Returns the default values for every UPROPERTY on the generated class's CDO.
        This covers arrays (e.g. `GrantAbilitiesOnStart`), enums, struct fields,
        and UObject/asset references (serialized as object paths). Transient
        (runtime-only) fields are skipped.

        Args:
            blueprint_name: Asset name of the Blueprint.
            property_path: Optional top-level property name to narrow the dump
                  (e.g. "GrantAbilitiesOnStart"). Nested paths
                  (e.g. "CharacterMovement.MaxWalkSpeed") are not yet supported —
                  follow the returned object path with a separate command.

        Returns:
            Dict with:
              - blueprint_name, path
              - parent_class (full path), generated_class (full path)
              - property_path (echoed when provided)
              - properties (object) — all properties by default, or just
                  {property_path: value} when narrowed.

        Example:
            # Find which abilities a player BP grants at start
            get_blueprint_defaults("BP_PlayerCharacter", "GrantAbilitiesOnStart")
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params: Dict[str, Any] = {"blueprint_name": blueprint_name}
            if property_path:
                params["property_path"] = property_path
            response = unreal.send_command("get_blueprint_defaults", params)
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            return response
        except Exception as e:
            error_msg = f"Error getting blueprint defaults: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Blueprint tools registered successfully")