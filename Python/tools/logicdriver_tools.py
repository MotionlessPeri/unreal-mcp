"""
Logic Driver tools for Unreal MCP.

MCP-1 (read):
  - get_logicdriver_state_machine_graph: Read a full Logic Driver state machine graph,
    including transition condition subgraphs.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def _send(command: str, params: dict) -> dict:
    from unreal_mcp_server import get_unreal_connection

    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    response = unreal.send_command(command, params)
    if not response:
        return {"success": False, "message": "No response from Unreal Engine"}

    return response


def register_logicdriver_tools(mcp: FastMCP):
    @mcp.tool()
    def get_logicdriver_state_machine_graph(
        ctx: Context,
        blueprint_name: str,
        asset_path: Optional[str] = None,
        graph_name: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Read a Logic Driver state machine graph and each transition's condition subgraph.

        Args:
            blueprint_name: Blueprint asset short name or content path.
            asset_path: Optional explicit content-browser path, e.g. "/Game/Blueprints/StateMachine/BP_InputSM".
            graph_name: Optional specific Logic Driver graph name. Defaults to the root state machine graph.
        """
        try:
            params: Dict[str, Any] = {"blueprint_name": blueprint_name}
            if asset_path is not None:
                params["asset_path"] = asset_path
            if graph_name is not None:
                params["graph_name"] = graph_name

            return _send("get_logicdriver_state_machine_graph", params)
        except Exception as e:
            return {"success": False, "message": f"Error: {e}"}
