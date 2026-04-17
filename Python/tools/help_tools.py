"""
Help Tools for Unreal MCP.

Provides runtime command discovery via the help command.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_help_tools(mcp: FastMCP):
    """Register help tools with the MCP server."""

    @mcp.tool()
    def help(ctx: Context, command: Optional[str] = None) -> Dict[str, Any]:
        """List all available MCP commands, or get details for a specific command.

        Args:
            command: If provided, return detailed info (description, params) for this command.
                     If omitted, return a list of all commands grouped by category.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {}
            if command:
                params["command"] = command

            response = unreal.send_command("help", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            if "result" in response:
                return response["result"]

            return response

        except Exception as e:
            logger.error(f"Error in help command: {e}")
            return {"error": str(e)}
