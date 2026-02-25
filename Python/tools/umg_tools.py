"""
UMG Tools for Unreal MCP.

This module provides tools for creating and manipulating UMG Widget Blueprints in Unreal Engine.
"""

import logging
from typing import Dict, List, Any
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")


def _require_mcp_success(response: Dict[str, Any], command_name: str) -> Dict[str, Any]:
    """Unwrap Unreal MCP response and raise on command-level failure."""
    if not response:
        raise RuntimeError(f"{command_name}: no response from Unreal Engine")

    if response.get("status") == "error":
        raise RuntimeError(f"{command_name}: {response.get('error', 'unknown error')}")

    result = response.get("result")
    if not isinstance(result, dict):
        raise RuntimeError(f"{command_name}: malformed response ({response})")

    if result.get("success") is False:
        raise RuntimeError(f"{command_name}: result.success=false ({result})")

    return result


def _send_and_require(unreal: Any, command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Send a command to UE plugin and return the unwrapped success result object."""
    response = unreal.send_command(command_name, params)
    return _require_mcp_success(response, command_name)

def register_umg_tools(mcp: FastMCP):
    """Register UMG tools with the MCP server."""

    @mcp.tool()
    def create_umg_widget_blueprint(
        ctx: Context,
        widget_name: str,
        parent_class: str = "UserWidget",
        path: str = "/Game/UI"
    ) -> Dict[str, Any]:
        """
        Create a new UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the widget blueprint to create
            parent_class: Parent class for the widget (default: UserWidget)
            path: Content browser path where the widget should be created
            
        Returns:
            Dict containing success status and widget path
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                # Canonical field consumed by newer plugin versions.
                "widget_name": widget_name,
                # Legacy field kept for compatibility with older plugin handlers.
                "name": widget_name,
                "parent_class": parent_class,
                "path": path
            }
            
            logger.info(f"Creating UMG Widget Blueprint with params: {params}")
            response = unreal.send_command("create_umg_widget_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Create UMG Widget Blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating UMG Widget Blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_debugboard_skeleton_widget(
        ctx: Context,
        widget_name: str,
        path: str = "/Game/UI",
        board_rows: int = 10,
        board_cols: int = 9,
        board_origin: List[float] | None = None,
        cell_size: List[float] | None = None,
        board_padding: float = 30.0,
        side_panel_width: float = 320.0,
        side_panel_min_height: float = 420.0,
        status_text_widgets: List[str] | None = None,
        action_button_widgets: List[str] | None = None,
        button_enabled_widgets: List[str] | None = None,
        status_text_values: Dict[str, str] | None = None,
        button_label_values: Dict[str, str] | None = None
    ) -> Dict[str, Any]:
        """
        Create a reusable DebugBoard skeleton Widget Blueprint using existing UMG batch primitives.

        This is a high-level composition helper that builds:
        1) Root CanvasPanel
        2) BoardGrid (UniformGridPanel) with full board cell skeleton
        3) SidePanel (VerticalBox) with status TextBlocks and action Buttons
        4) Button label TextBlocks
        5) Initial text/common-property state via batch setters

        Args:
            widget_name: Widget Blueprint asset name to create
            path: Content path where the widget blueprint should be created (default: /Game/UI)
            board_rows: Board row count (default: 10)
            board_cols: Board column count (default: 9)
            board_origin: Optional [x, y] top-left position for BoardGrid (default: [40, 40])
            cell_size: Optional [w, h] for per-cell layout sizing used to derive board panel size (default: [70, 70])
            board_padding: Horizontal gap between board and side panel (default: 30)
            side_panel_width: Width of side panel (default: 320)
            side_panel_min_height: Minimum side panel height (default: 420)
            status_text_widgets: Optional status TextBlock widget names for SidePanel
            action_button_widgets: Optional action Button widget names for SidePanel
            button_enabled_widgets: Optional subset of action buttons to enable initially
            status_text_values: Optional mapping of status TextBlock name -> initial text
            button_label_values: Optional mapping of action button name -> label text

        Returns:
            Dict summary with created path and key counts/layout readback.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            if board_rows <= 0 or board_cols <= 0:
                return {"success": False, "message": "board_rows and board_cols must be > 0"}
            if board_padding < 0:
                return {"success": False, "message": "board_padding must be >= 0"}
            if side_panel_width <= 0 or side_panel_min_height <= 0:
                return {"success": False, "message": "side_panel_width and side_panel_min_height must be > 0"}

            if board_origin is None:
                board_origin = [40.0, 40.0]
            if cell_size is None:
                cell_size = [70.0, 70.0]

            if len(board_origin) < 2 or len(cell_size) < 2:
                return {"success": False, "message": "board_origin and cell_size must be [x, y] arrays"}

            board_origin_x = float(board_origin[0])
            board_origin_y = float(board_origin[1])
            cell_width = float(cell_size[0])
            cell_height = float(cell_size[1])
            if cell_width <= 0 or cell_height <= 0:
                return {"success": False, "message": "cell_size values must be > 0"}

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            default_status_texts = [
                "TxtTitle",
                "TxtPhase",
                "TxtTurn",
                "TxtRedCursor",
                "TxtBlackCursor",
                "TxtLastAck",
                "TxtGameOver",
            ]
            default_action_buttons = [
                "BtnJoin",
                "BtnPullRed",
                "BtnPullBlack",
                "BtnCommitReveal",
                "BtnRedMove",
                "BtnBlackResign",
            ]
            status_texts = list(status_text_widgets) if status_text_widgets is not None else default_status_texts
            action_buttons = list(action_button_widgets) if action_button_widgets is not None else default_action_buttons
            if len(status_texts) == 0:
                return {"success": False, "message": "status_text_widgets must not be empty"}
            if len(action_buttons) == 0:
                return {"success": False, "message": "action_button_widgets must not be empty"}
            if len(set(status_texts)) != len(status_texts):
                return {"success": False, "message": "status_text_widgets contains duplicates"}
            if len(set(action_buttons)) != len(action_buttons):
                return {"success": False, "message": "action_button_widgets contains duplicates"}
            if set(status_texts) & set(action_buttons):
                return {"success": False, "message": "status_text_widgets and action_button_widgets overlap"}

            if button_enabled_widgets is None:
                button_enabled_set = {"BtnJoin", "BtnPullRed", "BtnPullBlack"}
            else:
                button_enabled_set = set(button_enabled_widgets)
            unknown_enabled = sorted(button_enabled_set - set(action_buttons))
            if unknown_enabled:
                return {"success": False, "message": f"button_enabled_widgets contains unknown buttons: {unknown_enabled}"}

            status_text_values_map = dict(status_text_values or {})
            button_label_values_map = dict(button_label_values or {})

            default_status_text_values = {
                "TxtTitle": "DebugBoard Skeleton (Auto)",
                "TxtPhase": "Phase: SetupCommit",
                "TxtTurn": "Turn: Red",
                "TxtRedCursor": "RedCursor: 0",
                "TxtBlackCursor": "BlackCursor: 0",
                "TxtLastAck": "LastAck: <none>",
                "TxtGameOver": "GameOver: <none>",
            }
            for status_name in status_texts:
                if status_name not in status_text_values_map:
                    status_text_values_map[status_name] = default_status_text_values.get(status_name, f"{status_name}: <unset>")
            for btn_name in action_buttons:
                if btn_name not in button_label_values_map:
                    button_label_values_map[btn_name] = btn_name.replace("Btn", "")

            create = _send_and_require(unreal, "create_umg_widget_blueprint", {
                "widget_name": widget_name,
                "path": path,
                "parent_class": "UserWidget",
            })
            bp = create.get("path", f"{path.rstrip('/')}/{widget_name}")

            _send_and_require(unreal, "ensure_widget_root", {
                "blueprint_name": bp,
                "widget_class": "CanvasPanel",
                "widget_name": "RootCanvas",
                "replace_existing": True,
            })
            _send_and_require(unreal, "clear_widget_children", {
                "blueprint_name": bp,
                "widget_name": "RootCanvas",
            })

            _send_and_require(unreal, "add_widget_child_batch", {
                "blueprint_name": bp,
                "items": [
                    {"parent_widget_name": "RootCanvas", "widget_class": "UniformGridPanel", "widget_name": "BoardGrid"},
                    {"parent_widget_name": "RootCanvas", "widget_class": "VerticalBox", "widget_name": "SidePanel"},
                ],
            })

            board_width = float(board_cols * cell_width)
            board_height = float(board_rows * cell_height)
            side_x = board_origin_x + board_width + float(board_padding)
            canvas_layout = _send_and_require(unreal, "set_canvas_slot_layout_batch", {
                "blueprint_name": bp,
                "items": [
                    {"widget_name": "BoardGrid", "position": [board_origin_x, board_origin_y], "size": [board_width, board_height], "z_order": 1},
                    {"widget_name": "SidePanel", "position": [side_x, board_origin_y], "size": [float(side_panel_width), max(board_height, float(side_panel_min_height))], "z_order": 2},
                ],
            })

            side_items: List[Dict[str, Any]] = []
            for name in status_texts:
                side_items.append({"parent_widget_name": "SidePanel", "widget_class": "TextBlock", "widget_name": name})
            for name in action_buttons:
                side_items.append({"parent_widget_name": "SidePanel", "widget_class": "Button", "widget_name": name})

            _send_and_require(unreal, "add_widget_child_batch", {
                "blueprint_name": bp,
                "items": side_items,
            })

            _send_and_require(unreal, "add_widget_child_batch", {
                "blueprint_name": bp,
                "items": [
                    {"parent_widget_name": btn_name, "widget_class": "TextBlock", "widget_name": f"{btn_name}_Label"}
                    for btn_name in action_buttons
                ],
            })

            _send_and_require(unreal, "set_widget_common_properties_batch", {
                "blueprint_name": bp,
                "items": (
                    [{"widget_name": name, "visibility": "Visible"} for name in status_texts]
                    + [{"widget_name": "SidePanel", "visibility": "Visible"}]
                    + [{"widget_name": "BoardGrid", "visibility": "Visible"}]
                    + [
                        {
                            "widget_name": btn_name,
                            "visibility": "Visible",
                            "is_enabled": btn_name in button_enabled_set,
                        }
                        for btn_name in action_buttons
                    ]
                ),
            })

            text_items: List[Dict[str, Any]] = []
            for status_name in status_texts:
                if status_name == "TxtTitle":
                    color = [0.95, 0.95, 1.0, 1.0]
                elif status_name == "TxtTurn":
                    color = [1.0, 0.55, 0.55, 1.0]
                elif status_name == "TxtRedCursor":
                    color = [1.0, 0.4, 0.4, 1.0]
                elif status_name == "TxtBlackCursor":
                    color = [0.7, 0.7, 0.7, 1.0]
                elif status_name == "TxtGameOver":
                    color = [0.9, 0.9, 0.7, 1.0]
                else:
                    color = [0.85, 0.85, 0.85, 1.0]
                text_items.append({
                    "widget_name": status_name,
                    "text": status_text_values_map.get(status_name, f"{status_name}: <unset>"),
                    "color": color,
                })
            for btn_name in action_buttons:
                text_items.append({
                    "widget_name": f"{btn_name}_Label",
                    "text": button_label_values_map.get(btn_name, btn_name.replace("Btn", "")),
                    "color": [1.0, 1.0, 1.0, 1.0],
                })

            text_batch = _send_and_require(unreal, "set_text_block_properties_batch", {
                "blueprint_name": bp,
                "items": text_items,
            })

            add_cell_items: List[Dict[str, Any]] = []
            grid_slot_items: List[Dict[str, Any]] = []
            for row in range(board_rows):
                for col in range(board_cols):
                    cell_name = f"Cell_{row}_{col}"
                    add_cell_items.append({
                        "parent_widget_name": "BoardGrid",
                        "widget_class": "Border",
                        "widget_name": cell_name,
                    })
                    grid_slot_items.append({
                        "widget_name": cell_name,
                        "row": row,
                        "column": col,
                        "horizontal_alignment": "Fill",
                        "vertical_alignment": "Fill",
                    })

            add_cells_batch = _send_and_require(unreal, "add_widget_child_batch", {
                "blueprint_name": bp,
                "items": add_cell_items,
            })
            grid_batch = _send_and_require(unreal, "set_uniform_grid_slot_batch", {
                "blueprint_name": bp,
                "items": grid_slot_items,
            })
            tree = _send_and_require(unreal, "get_widget_tree", {"blueprint_name": bp})

            return {
                "success": True,
                "widget_name": widget_name,
                "blueprint_name": bp,
                "board_rows": board_rows,
                "board_cols": board_cols,
                "board_cell_count": board_rows * board_cols,
                "status_text_count": len(status_texts),
                "action_button_count": len(action_buttons),
                "button_enabled_count": len(button_enabled_set),
                "add_cell_created_count": add_cells_batch.get("created_count"),
                "grid_slot_updated_count": grid_batch.get("updated_count"),
                "text_updated_count": text_batch.get("updated_count"),
                "canvas_layout_updated_count": canvas_layout.get("updated_count"),
                "layout": {
                    "board_origin": [board_origin_x, board_origin_y],
                    "cell_size": [cell_width, cell_height],
                    "board_padding": float(board_padding),
                    "side_panel_width": float(side_panel_width),
                    "side_panel_min_height": float(side_panel_min_height),
                },
                "root_children": [child.get("name") for child in ((tree.get("root") or {}).get("children") or [])],
                "side_panel": {
                    "status_text_widgets": status_texts,
                    "action_button_widgets": action_buttons,
                    "button_enabled_widgets": sorted(button_enabled_set),
                },
                "note": "DebugBoard skeleton created using existing Route B batch primitives.",
            }

        except Exception as e:
            error_msg = f"Error creating DebugBoard skeleton widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_text_block_to_widget(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Text Block widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            text_block_name: Name to give the new Text Block
            text: Initial text content
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the text block
            font_size: Font size in points
            color: [R, G, B, A] color values (0.0 to 1.0)
            
        Returns:
            Dict containing success status and text block properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "text_block_name": text_block_name,
                "text": text,
                "position": position,
                "size": size,
                "font_size": font_size,
                "color": color
            }
            
            logger.info(f"Adding Text Block to widget with params: {params}")
            response = unreal.send_command("add_text_block_to_widget", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add Text Block response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding Text Block to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_button_to_widget(
        ctx: Context,
        widget_name: str,
        button_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        background_color: List[float] = [0.1, 0.1, 0.1, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Button widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            button_name: Name to give the new Button
            text: Text to display on the button
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the button
            font_size: Font size for button text
            color: [R, G, B, A] text color values (0.0 to 1.0)
            background_color: [R, G, B, A] button background color values (0.0 to 1.0)
            
        Returns:
            Dict containing success status and button properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "button_name": button_name,
                "text": text,
                "position": position,
                "size": size,
                "font_size": font_size,
                "color": color,
                "background_color": background_color
            }
            
            logger.info(f"Adding Button to widget with params: {params}")
            response = unreal.send_command("add_button_to_widget", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add Button response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding Button to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def bind_widget_event(
        ctx: Context,
        widget_name: str,
        widget_component_name: str,
        event_name: str,
        function_name: str = "",
        node_position = None
    ) -> Dict[str, Any]:
        """
        Bind an event on a widget component to a function.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            widget_component_name: Name of the widget component (button, etc.)
            event_name: Name of the event to bind (OnClicked, etc.)
            function_name: Name of the function to create/bind to (defaults to f"{widget_component_name}_{event_name}")
            node_position: Optional [X, Y] position for the bound event node in EventGraph
            
        Returns:
            Dict containing success status and binding information
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # If no function name provided, create one from component and event names
            if not function_name:
                function_name = f"{widget_component_name}_{event_name}"
            
            params = {
                # Canonical fields expected by UE command handler.
                "blueprint_name": widget_name,
                "widget_name": widget_component_name,
                # Legacy field is kept for compatibility with older handlers.
                "widget_component_name": widget_component_name,
                "event_name": event_name,
                "function_name": function_name
            }
            if node_position is not None:
                params["node_position"] = node_position
            
            logger.info(f"Binding widget event with params: {params}")
            response = unreal.send_command("bind_widget_event", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Bind widget event response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error binding widget event: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_widget_to_viewport(
        ctx: Context,
        widget_name: str,
        z_order: int = 0
    ) -> Dict[str, Any]:
        """
        Add a Widget Blueprint instance to the viewport.
        
        Args:
            widget_name: Name of the Widget Blueprint to add
            z_order: Z-order for the widget (higher numbers appear on top)
            
        Returns:
            Dict containing success status and widget instance information
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "z_order": z_order
            }
            
            logger.info(f"Adding widget to viewport with params: {params}")
            response = unreal.send_command("add_widget_to_viewport", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add widget to viewport response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding widget to viewport: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_text_block_binding(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        binding_property: str,
        binding_type: str = "Text"
    ) -> Dict[str, Any]:
        """
        Set up a property binding for a Text Block widget.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            text_block_name: Name of the Text Block to bind
            binding_property: Name of the property to bind to
            binding_type: Type of binding (Text, Visibility, etc.)
            
        Returns:
            Dict containing success status and binding information
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": widget_name,
                # Canonical fields for C++ handler
                "widget_name": text_block_name,
                "binding_name": binding_property,
                # Legacy aliases kept for compatibility
                "text_block_name": text_block_name,
                "binding_property": binding_property,
                "binding_type": binding_type
            }
            
            logger.info(f"Setting text block binding with params: {params}")
            response = unreal.send_command("set_text_block_binding", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set text block binding response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting text block binding: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_widget_tree(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """
        Read the widget hierarchy of a Widget Blueprint.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint

        Returns:
            Dict containing root widget metadata and recursive children.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
            }

            logger.info(f"Reading widget tree with params: {params}")
            response = unreal.send_command("get_widget_tree", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Get widget tree response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error reading widget tree: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def ensure_widget_root(
        ctx: Context,
        blueprint_name: str,
        widget_class: str,
        widget_name: str,
        replace_existing: bool = False
    ) -> Dict[str, Any]:
        """
        Ensure a Widget Blueprint has a root widget of the requested type.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            widget_class: UMG widget class name (e.g. CanvasPanel, Border)
            widget_name: Root widget instance name
            replace_existing: Replace an existing mismatched root when true

        Returns:
            Dict containing root widget metadata and create/replace flags.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "widget_class": widget_class,
                "widget_name": widget_name,
                "replace_existing": replace_existing,
            }

            logger.info(f"Ensuring widget root with params: {params}")
            response = unreal.send_command("ensure_widget_root", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Ensure widget root response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error ensuring widget root: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_widget_child(
        ctx: Context,
        blueprint_name: str,
        parent_widget_name: str,
        widget_class: str,
        widget_name: str
    ) -> Dict[str, Any]:
        """
        Add a generic UMG widget as a child of a panel widget in a Widget Blueprint.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            parent_widget_name: Parent panel widget name (use root widget name)
            widget_class: UMG widget class name (e.g. VerticalBox, TextBlock, Button)
            widget_name: Child widget instance name

        Returns:
            Dict containing created child metadata and slot class.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "parent_widget_name": parent_widget_name,
                "widget_class": widget_class,
                "widget_name": widget_name,
            }

            logger.info(f"Adding widget child with params: {params}")
            response = unreal.send_command("add_widget_child", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add widget child response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding widget child: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_widget_child_batch(
        ctx: Context,
        blueprint_name: str,
        items: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Batch-add generic UMG widgets as children of panel widgets in a Widget Blueprint.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            items: Array of child descriptors. Each item must include:
                   widget_class, widget_name, and optional parent_widget_name

        Returns:
            Dict containing per-item created child metadata and slot classes.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "items": items,
            }

            logger.info(f"Adding widget children batch with params: blueprint={blueprint_name}, count={len(items)}")
            response = unreal.send_command("add_widget_child_batch", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add widget child batch response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding widget child batch: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_canvas_slot_layout(
        ctx: Context,
        blueprint_name: str,
        widget_name: str,
        position: List[float] | None = None,
        size: List[float] | None = None,
        alignment: List[float] | None = None,
        anchors: List[float] | None = None,
        auto_size: bool | None = None,
        z_order: int | None = None
    ) -> Dict[str, Any]:
        """
        Set layout properties for a widget inside a CanvasPanelSlot.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            widget_name: Target widget name (must be in a CanvasPanelSlot)
            position: Optional [x, y]
            size: Optional [w, h]
            alignment: Optional [x, y]
            anchors: Optional [min_x, min_y, max_x, max_y] (or [x, y] shorthand)
            auto_size: Optional bool
            z_order: Optional int
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "widget_name": widget_name,
            }
            if position is not None:
                params["position"] = position
            if size is not None:
                params["size"] = size
            if alignment is not None:
                params["alignment"] = alignment
            if anchors is not None:
                params["anchors"] = anchors
            if auto_size is not None:
                params["auto_size"] = auto_size
            if z_order is not None:
                params["z_order"] = z_order

            logger.info(f"Setting canvas slot layout with params: {params}")
            response = unreal.send_command("set_canvas_slot_layout", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting canvas slot layout: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_canvas_slot_layout_batch(
        ctx: Context,
        blueprint_name: str,
        items: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Batch-set CanvasPanelSlot layout properties for multiple widgets in one command.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            items: Array of per-widget layout objects. Each item must include widget_name and may include
                   position/size/alignment/anchors/auto_size/z_order fields (same as set_canvas_slot_layout)
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "items": items,
            }

            logger.info(f"Setting canvas slot layout batch with {len(items)} items on {blueprint_name}")
            response = unreal.send_command("set_canvas_slot_layout_batch", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting canvas slot layout batch: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_uniform_grid_slot(
        ctx: Context,
        blueprint_name: str,
        widget_name: str,
        row: int | None = None,
        column: int | None = None,
        horizontal_alignment: str | None = None,
        vertical_alignment: str | None = None
    ) -> Dict[str, Any]:
        """
        Set layout properties for a widget inside a UniformGridSlot.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            widget_name: Target widget name (must be in a UniformGridSlot)
            row: Optional row index
            column: Optional column index
            horizontal_alignment: Optional alignment string (Fill/Left/Center/Right)
            vertical_alignment: Optional alignment string (Fill/Top/Center/Bottom)
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "widget_name": widget_name,
            }
            if row is not None:
                params["row"] = row
            if column is not None:
                params["column"] = column
            if horizontal_alignment is not None:
                params["horizontal_alignment"] = horizontal_alignment
            if vertical_alignment is not None:
                params["vertical_alignment"] = vertical_alignment

            logger.info(f"Setting uniform grid slot with params: {params}")
            response = unreal.send_command("set_uniform_grid_slot", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting uniform grid slot: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_uniform_grid_slot_batch(
        ctx: Context,
        blueprint_name: str,
        items: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Batch-set UniformGridSlot layout properties for multiple widgets in one command.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            items: Array of per-widget objects. Each item must include widget_name and may include
                   row/column/horizontal_alignment/vertical_alignment (same as set_uniform_grid_slot)
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "items": items,
            }

            logger.info(f"Setting uniform grid slot batch with {len(items)} items on {blueprint_name}")
            response = unreal.send_command("set_uniform_grid_slot_batch", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting uniform grid slot batch: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_widget_common_properties(
        ctx: Context,
        blueprint_name: str,
        widget_name: str,
        visibility: str | None = None,
        is_enabled: bool | None = None
    ) -> Dict[str, Any]:
        """
        Set common UWidget properties useful for debug UI automation.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            widget_name: Target widget name
            visibility: Optional visibility string (Visible/Collapsed/Hidden/HitTestInvisible/SelfHitTestInvisible)
            is_enabled: Optional enabled state
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "widget_name": widget_name,
            }
            if visibility is not None:
                params["visibility"] = visibility
            if is_enabled is not None:
                params["is_enabled"] = is_enabled

            logger.info(f"Setting widget common properties with params: {params}")
            response = unreal.send_command("set_widget_common_properties", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting widget common properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_widget_common_properties_batch(
        ctx: Context,
        blueprint_name: str,
        items: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Batch-set common UWidget properties (visibility/is_enabled) for multiple widgets.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            items: Array of per-widget objects with widget_name and optional visibility/is_enabled
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "items": items,
            }

            logger.info(f"Setting widget common properties batch with {len(items)} items on {blueprint_name}")
            response = unreal.send_command("set_widget_common_properties_batch", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting widget common properties batch: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_text_block_properties(
        ctx: Context,
        blueprint_name: str,
        widget_name: str,
        text: str | None = None,
        color: List[float] | None = None
    ) -> Dict[str, Any]:
        """
        Set common TextBlock properties useful for debug UI automation.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            widget_name: Target TextBlock widget name
            text: Optional text content
            color: Optional [r, g, b, a] color array
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "widget_name": widget_name,
            }
            if text is not None:
                params["text"] = text
            if color is not None:
                params["color"] = color

            logger.info(f"Setting text block properties with params: {params}")
            response = unreal.send_command("set_text_block_properties", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting text block properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_text_block_properties_batch(
        ctx: Context,
        blueprint_name: str,
        items: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Batch-set TextBlock properties (text/color) for multiple widgets in one command.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            items: Array of per-widget objects with widget_name and optional text/color
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "items": items,
            }

            logger.info(f"Setting text block properties batch with {len(items)} items on {blueprint_name}")
            response = unreal.send_command("set_text_block_properties_batch", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error setting text block properties batch: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def clear_widget_children(
        ctx: Context,
        blueprint_name: str,
        widget_name: str | None = None
    ) -> Dict[str, Any]:
        """
        Remove all direct children from a panel widget (or root when widget_name is omitted).

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            widget_name: Optional panel widget name; defaults to root widget
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
            }
            if widget_name:
                params["widget_name"] = widget_name

            logger.info(f"Clearing widget children with params: {params}")
            response = unreal.send_command("clear_widget_children", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error clearing widget children: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def remove_widget_from_blueprint(
        ctx: Context,
        blueprint_name: str,
        widget_name: str
    ) -> Dict[str, Any]:
        """
        Remove a widget (and its subtree) from a Widget Blueprint widget tree.

        Args:
            blueprint_name: Name or asset path of the target Widget Blueprint
            widget_name: Widget name to remove (root removal not supported)
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "widget_name": widget_name,
            }

            logger.info(f"Removing widget from blueprint with params: {params}")
            response = unreal.send_command("remove_widget_from_blueprint", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error removing widget from blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def delete_widget_blueprints_by_prefix(
        ctx: Context,
        path: str,
        name_prefix: str,
        recursive: bool = True,
        dry_run: bool = False
    ) -> Dict[str, Any]:
        """
        Delete WidgetBlueprint assets under a content path filtered by asset name prefix.

        Args:
            path: Content path (e.g. /Game/UI)
            name_prefix: Widget Blueprint asset name prefix to match
            recursive: Search recursively under path
            dry_run: When true, only return matched assets without deleting
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "path": path,
                "name_prefix": name_prefix,
                "recursive": recursive,
                "dry_run": dry_run,
            }

            logger.info(f"Deleting widget blueprints by prefix with params: {params}")
            response = unreal.send_command("delete_widget_blueprints_by_prefix", params)
            return response or {"success": False, "message": "No response from Unreal Engine"}
        except Exception as e:
            error_msg = f"Error deleting widget blueprints by prefix: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("UMG tools registered successfully") 
