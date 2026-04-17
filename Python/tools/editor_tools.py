"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""
    
    @mcp.tool()
    def get_actors_in_level(ctx: Context) -> List[Dict[str, Any]]:
        """Get a list of all actors in the current level."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return []
                
            response = unreal.send_command("get_actors_in_level", {})
            
            if not response:
                logger.warning("No response from Unreal Engine")
                return []
                
            # Log the complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check response format
            if "result" in response and "actors" in response["result"]:
                actors = response["result"]["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return actors
            elif "actors" in response:
                actors = response["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return actors
                
            logger.warning(f"Unexpected response format: {response}")
            return []
            
        except Exception as e:
            logger.error(f"Error getting actors: {e}")
            return []

    @mcp.tool()
    def find_actors_by_name(ctx: Context, pattern: str) -> List[str]:
        """Find actors by name pattern."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return []
                
            response = unreal.send_command("find_actors_by_name", {
                "pattern": pattern
            })
            
            if not response:
                return []
                
            return response.get("actors", [])
            
        except Exception as e:
            logger.error(f"Error finding actors: {e}")
            return []
    
    @mcp.tool()
    def spawn_actor(
        ctx: Context,
        name: str,
        type: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Create a new actor in the current level.
        
        Args:
            ctx: The MCP context
            name: The name to give the new actor (must be unique)
            type: The type of actor to create (e.g. StaticMeshActor, PointLight)
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the created actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "name": name,
                "type": type.upper(),  # Make sure type is uppercase
                "location": location,
                "rotation": rotation
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Creating actor '{name}' of type '{type}' with params: {params}")
            response = unreal.send_command("spawn_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            # Log the complete response for debugging
            logger.info(f"Actor creation response: {response}")
            
            # Handle error responses correctly
            if response.get("status") == "error":
                error_message = response.get("error", "Unknown error")
                logger.error(f"Error creating actor: {error_message}")
                return {"success": False, "message": error_message}
            
            return response
            
        except Exception as e:
            error_msg = f"Error creating actor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("delete_actor", {
                "name": name
            })
            return response or {}
            
        except Exception as e:
            logger.error(f"Error deleting actor: {e}")
            return {}
    
    @mcp.tool()
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: List[float]  = None,
        rotation: List[float]  = None,
        scale: List[float] = None
    ) -> Dict[str, Any]:
        """Set the transform of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {"name": name}
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale
                
            response = unreal.send_command("set_actor_transform", params)
            return response or {}
            
        except Exception as e:
            logger.error(f"Error setting transform: {e}")
            return {}
    
    @mcp.tool()
    def get_actor_properties(ctx: Context, name: str) -> Dict[str, Any]:
        """Get all properties of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("get_actor_properties", {
                "name": name
            })
            return response or {}
            
        except Exception as e:
            logger.error(f"Error getting properties: {e}")
            return {}

    @mcp.tool()
    def set_actor_property(
        ctx: Context,
        name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """
        Set a property on an actor.
        
        Args:
            name: Name of the actor
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Dict containing response from Unreal with operation status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("set_actor_property", {
                "name": name,
                "property_name": property_name,
                "property_value": property_value
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set actor property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting actor property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    # @mcp.tool() commented out because it's buggy
    def focus_viewport(
        ctx: Context,
        target: str = None,
        location: List[float] = None,
        distance: float = 1000.0,
        orientation: List[float] = None
    ) -> Dict[str, Any]:
        """
        Focus the viewport on a specific actor or location.
        
        Args:
            target: Name of the actor to focus on (if provided, location is ignored)
            location: [X, Y, Z] coordinates to focus on (used if target is None)
            distance: Distance from the target/location
            orientation: Optional [Pitch, Yaw, Roll] for the viewport camera
            
        Returns:
            Response from Unreal Engine
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {}
            if target:
                params["target"] = target
            elif location:
                params["location"] = location
            
            if distance:
                params["distance"] = distance
                
            if orientation:
                params["orientation"] = orientation
                
            response = unreal.send_command("focus_viewport", params)
            return response or {}
            
        except Exception as e:
            logger.error(f"Error focusing viewport: {e}")
            return {"status": "error", "message": str(e)}

    @mcp.tool()
    def spawn_blueprint_actor(
        ctx: Context,
        blueprint_name: str,
        actor_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint.
        
        Args:
            ctx: The MCP context
            blueprint_name: Name of the Blueprint to spawn from
            actor_name: Name to give the spawned actor
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the spawned actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "actor_name": actor_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0]
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Spawning blueprint actor with params: {params}")
            response = unreal.send_command("spawn_blueprint_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Spawn blueprint actor response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error spawning blueprint actor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def save_dirty_assets(
        ctx: Context,
        save_maps: bool = True,
        save_content: bool = True
    ) -> Dict[str, Any]:
        """
        Save dirty map/content packages in the editor.

        Args:
            save_maps: Save dirty map packages
            save_content: Save dirty content packages

        Returns:
            Dict with before/after dirty-package counts and save result
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("save_dirty_assets", {
                "save_maps": save_maps,
                "save_content": save_content,
            })
            return response or {}

        except Exception as e:
            error_msg = f"Error saving dirty assets: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def request_editor_exit(
        ctx: Context,
        force: bool = False,
        delay_seconds: float = 0.25
    ) -> Dict[str, Any]:
        """
        Request Unreal Editor to exit asynchronously after a short delay.

        Args:
            force: Force exit
            delay_seconds: Delay before exit (allows MCP response to flush)

        Returns:
            Dict containing scheduling result
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("request_editor_exit", {
                "force": force,
                "delay_seconds": delay_seconds,
            })
            return response or {}

        except Exception as e:
            error_msg = f"Error requesting editor exit: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def save_and_exit_editor(
        ctx: Context,
        save_maps: bool = True,
        save_content: bool = True,
        force: bool = False,
        delay_seconds: float = 0.35
    ) -> Dict[str, Any]:
        """
        Save dirty packages and request Unreal Editor exit asynchronously.

        Args:
            save_maps: Save dirty map packages before exit
            save_content: Save dirty content packages before exit
            force: Force exit
            delay_seconds: Delay before exit (allows MCP response to flush)

        Returns:
            Dict containing save stats and exit scheduling result
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("save_and_exit_editor", {
                "save_maps": save_maps,
                "save_content": save_content,
                "force": force,
                "delay_seconds": delay_seconds,
            })
            return response or {}

        except Exception as e:
            error_msg = f"Error saving and exiting editor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}



    @mcp.tool()
    def call_subsystem_function(
        ctx: Context,
        subsystem_class: str,
        function_name: str,
        parameters: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """
        Call a BlueprintCallable function on a WorldSubsystem.

        This enables calling editor subsystem functions to properly create
        and manage actors through the subsystem's workflow (e.g., using
        UAIPointEditorSubsystem::CreateRouteActor).

        Args:
            ctx: The MCP context
            subsystem_class: Full class path of the subsystem (e.g. "/Script/AIPoint.AIPointWorldSubsystem")
            function_name: Name of the function to call (e.g. "CreateRoute")
            parameters: Dict of parameter names and values to pass to the function

        Returns:
            Dict containing the function's return value(s) and success status

        Examples:
            # Create a new AIPointRoute through the subsystem
            call_subsystem_function(
                subsystem_class="/Script/AIPoint.AIPointWorldSubsystem",
                function_name="CreateRoute",
                parameters={"Label": "MyRoute"}
            )
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "subsystem_class": subsystem_class,
                "function_name": function_name
            }

            if parameters:
                params["parameters"] = parameters

            logger.info(f"Calling subsystem function '{function_name}' on '{subsystem_class}'")
            response = unreal.send_command("call_subsystem_function", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Subsystem function call response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error calling subsystem function: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_to_actor_array_property(
        ctx: Context,
        actor_name: str,
        property_name: str,
        element_name: str = None,
        element_names: List[str] = None
    ) -> Dict[str, Any]:
        """
        Add actor(s) to an array property on a target actor.

        This is useful for setting up relationships between actors, such as
        adding AIPointInstance actors to an AIPointRoute's AIPoints array.

        Args:
            ctx: The MCP context
            actor_name: Name of the target actor to modify
            property_name: Name of the array property (e.g. "AIPoints")
            element_name: Name of a single actor to add to the array
            element_names: Names of multiple actors to add to the array

        Returns:
            Dict containing success status, added count, and new array size

        Examples:
            # Add a single point to a route
            add_to_actor_array_property(
                actor_name="MyRoute",
                property_name="AIPoints",
                element_name="MyPoint1"
            )

            # Add multiple points to a route
            add_to_actor_array_property(
                actor_name="MyRoute",
                property_name="AIPoints",
                element_names=["MyPoint1", "MyPoint2", "MyPoint3"]
            )
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "actor_name": actor_name,
                "property_name": property_name
            }

            if element_name:
                params["element_name"] = element_name
            elif element_names:
                params["element_names"] = element_names
            else:
                return {"success": False, "message": "Must provide either element_name or element_names"}

            logger.info(f"Adding element(s) to array property '{property_name}' on actor '{actor_name}'")
            response = unreal.send_command("add_to_actor_array_property", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Array property update response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding to actor array property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_data_asset(
        ctx: Context,
        path: str,
        property_path: str = "",
        allow_any_object: bool = False,
    ) -> Dict[str, Any]:
        """
        Read a DataAsset (or any UObject asset) as JSON properties.

        Loads the asset via StaticLoadObject and serializes its property container
        via FJsonObjectConverter. Transient properties are skipped.

        Args:
            path: Content-browser path (/Game/.../MyAsset) or full object path
                  (/Game/.../MyAsset.MyAsset).
            property_path: Optional top-level property name. When set, only that
                  property's value is returned, keyed under `properties[property_path]`.
                  Nested paths (e.g. "Comp.Field") are not yet supported.
            allow_any_object: When false (default) the command rejects assets that
                  aren't UDataAsset. Set true to read arbitrary UObject assets.

        Returns:
            Dict with:
              - path, asset_name
              - asset_class (full path), asset_class_short
              - property_path (echoed when provided)
              - properties (object) — all properties by default, or just
                  {property_path: value} when narrowed.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params: Dict[str, Any] = {"path": path}
            if property_path:
                params["property_path"] = property_path
            if allow_any_object:
                params["allow_any_object"] = True
            response = unreal.send_command("get_data_asset", params)
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            return response
        except Exception as e:
            error_msg = f"Error reading data asset: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def find_assets(
        ctx: Context,
        path: str = "/Game",
        class_name: str = "",
        recursive: bool = True,
        name_pattern: str = "",
    ) -> Dict[str, Any]:
        """
        List assets via the AssetRegistry without loading them.

        Args:
            path: Package path to search (default /Game).
            class_name: Filter by asset class. Short name ("Blueprint", "DataAsset")
                  or full path ("/Script/Engine.Blueprint"). Recursive over subclasses.
                  NOTE: Blueprint assets all report class="Blueprint" regardless of
                  their parent class; to narrow by Blueprint parent, filter the
                  returned `parent_class` field client-side.
            recursive: Recurse into subfolders (default true).
            name_pattern: Wildcard glob on asset name, case-insensitive.

        Returns:
            Dict with:
              - assets (list[dict]): each entry has name, path, package_path, class,
                  and parent_class (when available from asset tags — filled in for BPs).
              - total_count
              - search_path, recursive, class_filter, class_resolution, name_pattern
                  (echoed back for debugging).
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            params: Dict[str, Any] = {"path": path, "recursive": recursive}
            if class_name:
                params["class_name"] = class_name
            if name_pattern:
                params["name_pattern"] = name_pattern
            response = unreal.send_command("find_assets", params)
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            logger.info(f"find_assets total={response.get('result', {}).get('total_count', '?')}")
            return response
        except Exception as e:
            error_msg = f"Error listing assets: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Editor tools registered successfully")
