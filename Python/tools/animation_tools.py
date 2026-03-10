"""
Animation tools for Unreal MCP.

Currently supported:
  - get_montage_info: Read sections, slot tracks, notifies, and branching points from an AnimMontage.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_animation_tools(mcp: FastMCP):
    @mcp.tool()
    def get_montage_info(
        ctx: Context,
        asset_path: str,
    ) -> Dict[str, Any]:
        """
        Read an AnimMontage asset, including sections, slot tracks, notifies, and branching points.

        Args:
            asset_path: Content-browser path or full object path to the montage.
                        Examples:
                          - "/Game/Animations/MyMontage"
                          - "/Game/Animations/MyMontage.MyMontage"
                          - "/Script/Engine.AnimMontage'/Game/Animations/MyMontage.MyMontage'"
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_montage_info", {
                "asset_path": asset_path,
            })

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(
                "get_montage_info response notify_count=%s branching_point_count=%s",
                response.get("result", {}).get("notifies", []).__len__() if isinstance(response.get("result", {}).get("notifies"), list) else "?",
                response.get("result", {}).get("branching_points", []).__len__() if isinstance(response.get("result", {}).get("branching_points"), list) else "?",
            )
            return response

        except Exception as e:
            error_msg = f"Error getting montage info: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
