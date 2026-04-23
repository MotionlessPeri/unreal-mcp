#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UDialogueAsset;
class UStateGraphNode;
class UEdGraphPin;

/**
 * Handler class for Dialogue System MCP commands.
 * MCP-1 (read): get_dialogue_graph, get_dialogue_connections
 * MCP-2 (write): create_dialogue_asset, add_dialogue_node, set_dialogue_node_properties,
 *                connect_dialogue_nodes, disconnect_dialogue_nodes, delete_dialogue_node
 * MCP-3 (Line ID): bind_dialogue_node_line, unbind_dialogue_node_line,
 *                  query_dialogue_line, list_dialogue_lines, dialogue_registry_info
 */
class UNREALMCPDIALOGUE_API FUnrealMCPDialogueCommands
{
public:
	FUnrealMCPDialogueCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// MCP-1 / MCP-2
	TSharedPtr<FJsonObject> HandleGetDialogueGraph(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDialogueConnections(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateDialogueAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddDialogueNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetDialogueNodeProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectDialogueNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnectDialogueNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteDialogueNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddDialogueChoiceItem(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetTransitionCondition(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNodeCallbackClass(const TSharedPtr<FJsonObject>& Params);

	// MCP-3: Line ID
	TSharedPtr<FJsonObject> HandleBindDialogueNodeLine(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleUnbindDialogueNodeLine(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleQueryDialogueLine(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListDialogueLines(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDialogueRegistryInfo(const TSharedPtr<FJsonObject>& Params);

	UDialogueAsset* LoadDialogueAsset(const FString& AssetPath, TSharedPtr<FJsonObject>& OutError);
	UStateGraphNode* FindNodeByGuid(UDialogueAsset* Asset, const FString& NodeIdStr);
	UEdGraphPin* FindOutputPin(UDialogueAsset* Asset, UStateGraphNode* Node, const FString& PinName);
	UEdGraphPin* FindInputPin(UDialogueAsset* Asset, UStateGraphNode* Node);
};
