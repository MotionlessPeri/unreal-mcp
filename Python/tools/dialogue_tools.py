"""
Dialogue Tools for Unreal MCP.

MCP-1 (read):
  - get_dialogue_graph: Read all nodes of a DialogueAsset.
  - get_dialogue_connections: Read all connections between nodes.

MCP-2 (write):
  - add_dialogue_node: Create a new node in a DialogueAsset.
  - set_dialogue_node_properties: Set properties on a dialogue node.
  - connect_dialogue_nodes: Create an edge between two nodes.
  - disconnect_dialogue_nodes: Remove an edge between two nodes.
  - delete_dialogue_node: Remove a node from a DialogueAsset.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def _send(command: str, params: dict) -> dict:
    """Helper: send command to Unreal and return response."""
    from unreal_mcp_server import get_unreal_connection

    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    response = unreal.send_command(command, params)
    if not response:
        return {"success": False, "message": "No response from Unreal Engine"}

    return response


def register_dialogue_tools(mcp: FastMCP):
    """Register Dialogue System tools with the MCP server."""

    # ================================================================
    # MCP-1: Read
    # ================================================================

    @mcp.tool()
    def get_dialogue_graph(
        ctx: Context,
        asset_path: str,
    ) -> Dict[str, Any]:
        """
        Read all nodes of a DialogueAsset.

        Returns the full node list with type, position, and type-specific
        properties (speaker_name/dialogue_text for Speech; choices for Choice).
        Node IDs are persistent GUIDs.

        Args:
            asset_path: Content-browser path, e.g. "/Game/Dialogues/DA_Test".
        """
        try:
            return _send("get_dialogue_graph", {"asset_path": asset_path})
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def get_dialogue_connections(
        ctx: Context,
        asset_path: str,
    ) -> Dict[str, Any]:
        """
        Read all directed edges (connections) between nodes in a DialogueAsset.

        Pin names: Entry/Speech output = "Out", Choice item output = "Item_0"/"Item_1"/...,
        all input pins = "In".

        Args:
            asset_path: Content-browser path, e.g. "/Game/Dialogues/DA_Test".
        """
        try:
            return _send("get_dialogue_connections", {"asset_path": asset_path})
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    # ================================================================
    # MCP-2: Write
    # ================================================================

    @mcp.tool()
    def create_dialogue_asset(
        ctx: Context,
        asset_path: str,
    ) -> Dict[str, Any]:
        """
        Create a new DialogueAsset with a default Entry node.

        Args:
            asset_path: Content-browser path, e.g. "/Game/Dialogues/DA_Test".

        Returns:
            Dict with asset_path and entry_node_id.
        """
        try:
            return _send("create_dialogue_asset", {"asset_path": asset_path})
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def add_dialogue_node(
        ctx: Context,
        asset_path: str,
        node_type: str,
        pos_x: float = 0.0,
        pos_y: float = 0.0,
    ) -> Dict[str, Any]:
        """
        Create a new node in a DialogueAsset.

        Args:
            asset_path: Content-browser path to the DialogueAsset.
            node_type: "Speech", "Choice", or "Exit".
            pos_x: X position on canvas.
            pos_y: Y position on canvas.

        Returns:
            Dict with node_id (GUID string), node_type, and for Choice nodes
            also choice_item_node_ids (list of GUID strings for default items).
        """
        try:
            return _send("add_dialogue_node", {
                "asset_path": asset_path,
                "node_type": node_type,
                "pos_x": pos_x,
                "pos_y": pos_y,
            })
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def set_dialogue_node_properties(
        ctx: Context,
        asset_path: str,
        node_id: str,
        speaker_name: Optional[str] = None,
        dialogue_text: Optional[str] = None,
        choice_text: Optional[str] = None,
        pos_x: Optional[float] = None,
        pos_y: Optional[float] = None,
    ) -> Dict[str, Any]:
        """
        Set properties on a dialogue node.

        For Speech nodes: speaker_name, dialogue_text.
        For ChoiceItem nodes: choice_text.
        For any node: pos_x, pos_y (editor canvas position).

        Args:
            asset_path: Content-browser path to the DialogueAsset.
            node_id: GUID string of the node to modify.
            speaker_name: (Speech only) Speaker display name.
            dialogue_text: (Speech only) Dialogue line text.
            choice_text: (ChoiceItem only) Choice option text.
            pos_x: X position on canvas.
            pos_y: Y position on canvas.
        """
        try:
            params: dict = {
                "asset_path": asset_path,
                "node_id": node_id,
            }
            if speaker_name is not None:
                params["speaker_name"] = speaker_name
            if dialogue_text is not None:
                params["dialogue_text"] = dialogue_text
            if choice_text is not None:
                params["choice_text"] = choice_text
            if pos_x is not None:
                params["pos_x"] = pos_x
            if pos_y is not None:
                params["pos_y"] = pos_y

            return _send("set_dialogue_node_properties", params)
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def connect_dialogue_nodes(
        ctx: Context,
        asset_path: str,
        from_node_id: str,
        to_node_id: str,
        from_pin: str = "Out",
    ) -> Dict[str, Any]:
        """
        Create a directed edge between two nodes in a DialogueAsset.

        Uses the graph Schema's TryCreateConnection to keep visual graph
        and runtime OutTransitions in sync.

        Args:
            asset_path: Content-browser path to the DialogueAsset.
            from_node_id: GUID string of the source node.
            to_node_id: GUID string of the target node.
            from_pin: Output pin name. "Out" for Entry/Speech, "Item_0"/"Item_1"/... for Choice.
        """
        try:
            return _send("connect_dialogue_nodes", {
                "asset_path": asset_path,
                "from_node_id": from_node_id,
                "to_node_id": to_node_id,
                "from_pin": from_pin,
            })
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def disconnect_dialogue_nodes(
        ctx: Context,
        asset_path: str,
        from_node_id: str,
        to_node_id: str,
        from_pin: str = "Out",
    ) -> Dict[str, Any]:
        """
        Remove a directed edge between two nodes in a DialogueAsset.

        Uses the graph Schema's BreakSinglePinLink to keep visual graph
        and runtime OutTransitions in sync.

        Args:
            asset_path: Content-browser path to the DialogueAsset.
            from_node_id: GUID string of the source node.
            to_node_id: GUID string of the target node.
            from_pin: Output pin name. "Out" for Entry/Speech, "Item_0"/"Item_1"/... for Choice.
        """
        try:
            return _send("disconnect_dialogue_nodes", {
                "asset_path": asset_path,
                "from_node_id": from_node_id,
                "to_node_id": to_node_id,
                "from_pin": from_pin,
            })
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def delete_dialogue_node(
        ctx: Context,
        asset_path: str,
        node_id: str,
    ) -> Dict[str, Any]:
        """
        Delete a node from a DialogueAsset.

        Cannot delete Entry nodes or ChoiceItem nodes directly.
        All connections to/from the node are broken before removal.

        Args:
            asset_path: Content-browser path to the DialogueAsset.
            node_id: GUID string of the node to delete.
        """
        try:
            return _send("delete_dialogue_node", {
                "asset_path": asset_path,
                "node_id": node_id,
            })
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}

    @mcp.tool()
    def add_dialogue_choice_item(
        ctx: Context,
        asset_path: str,
        node_id: str,
        choice_text: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Add a new choice item to an existing Choice node.

        Args:
            asset_path: Content-browser path to the DialogueAsset.
            node_id: GUID string of the Choice node.
            choice_text: Optional text for the new choice item.

        Returns:
            Dict with item_node_id (GUID) and item_index.
        """
        try:
            params: dict = {
                "asset_path": asset_path,
                "node_id": node_id,
            }
            if choice_text is not None:
                params["choice_text"] = choice_text
            return _send("add_dialogue_choice_item", params)
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}
