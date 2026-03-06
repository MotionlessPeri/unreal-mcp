"""
Dialogue Tools for Unreal MCP.

This module provides tools for reading Dialogue System assets in Unreal Engine.
Currently supported commands:
  - get_dialogue_graph: Read all nodes of a DialogueAsset.
  - get_dialogue_connections: Read all connections between nodes.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_dialogue_tools(mcp: FastMCP):
    """Register Dialogue System tools with the MCP server."""

    @mcp.tool()
    def get_dialogue_graph(
        ctx: Context,
        asset_path: str,
    ) -> Dict[str, Any]:
        """
        Read all nodes of a DialogueAsset.

        Returns the full node list with type, position, and type-specific
        properties (speaker_name/dialogue_text for Speech; choices list for Choice).

        Args:
            asset_path: Content-browser path to the DialogueAsset,
                        e.g. "/Game/Dialogues/DA_TestDialogue".

        Returns:
            Dict with fields:
              - success (bool)
              - asset_path (str)
              - asset_name (str)
              - nodes (list of dicts), each with:
                  - node_id (str): UObject name, unique within the asset
                  - node_type (str): "Entry" | "Speech" | "Choice" | "Exit"
                  - position (dict): {x, y} canvas position
                  - speaker_name (str): Speech only
                  - dialogue_text (str): Speech only
                  - choices (list of str): Choice only, indexed as Choice_0, Choice_1, ...

        Example:
            get_dialogue_graph("/Game/Dialogues/DA_TestDialogue")
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_dialogue_graph", {
                "asset_path": asset_path,
            })

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"get_dialogue_graph response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting dialogue graph: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_dialogue_connections(
        ctx: Context,
        asset_path: str,
    ) -> Dict[str, Any]:
        """
        Read all directed edges (connections) between nodes in a DialogueAsset.

        Pin name convention (matches DialogueGraphSchema):
          - Entry output pin: "Out"
          - Speech output pin: "Next"
          - Choice output pins: "Choice_0", "Choice_1", ...
          - All input pins: "In"

        Args:
            asset_path: Content-browser path to the DialogueAsset,
                        e.g. "/Game/Dialogues/DA_TestDialogue".

        Returns:
            Dict with fields:
              - success (bool)
              - asset_path (str)
              - connections (list of dicts), each with:
                  - from_node_id (str)
                  - from_pin (str)
                  - to_node_id (str)
                  - to_pin (str)  -- always "In"

        Example:
            get_dialogue_connections("/Game/Dialogues/DA_TestDialogue")
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_dialogue_connections", {
                "asset_path": asset_path,
            })

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"get_dialogue_connections response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting dialogue connections: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
