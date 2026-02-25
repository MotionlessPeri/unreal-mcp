"""
Behavior Tree Tools for Unreal MCP.

This module provides tools for reading Behavior Tree assets in Unreal Engine.
Currently supported commands:
  - get_behavior_tree_info: Read the full node structure of a BehaviorTree asset.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_behavior_tree_tools(mcp: FastMCP):
    """Register Behavior Tree tools with the MCP server."""

    @mcp.tool()
    def get_behavior_tree_info(
        ctx: Context,
        asset_path: str,
    ) -> Dict[str, Any]:
        """
        Read the structure of a Behavior Tree asset.

        Traverses the tree and returns a JSON representation of all nodes
        (Composite, Task, Decorator, Service) with their class names and
        display names.

        Args:
            asset_path: Content-browser path to the BehaviorTree asset,
                        e.g. "/Game/AIPointTree".

        Returns:
            Dict with fields:
              - success (bool)
              - asset_path (str)
              - asset_name (str)
              - blackboard (str)  -- path of the blackboard asset, or ""
              - root (dict)       -- recursive node tree

            Each node dict contains:
              - node_type: "Composite" | "Task" | "Decorator" | "Service"
              - class: C++ class name (e.g. "BTComposite_Sequence")
              - name: display name shown in the editor

            Composite nodes also contain:
              - services: list of service node dicts
              - children: list of child entries, each with:
                  - decorators: list of decorator node dicts
                  - node: the child node dict (Composite or Task)

        Example:
            get_behavior_tree_info("/Game/AIPointTree")
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_behavior_tree_info", {
                "asset_path": asset_path,
            })

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"get_behavior_tree_info response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting behavior tree info: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
