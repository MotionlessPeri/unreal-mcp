#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UDialogueAsset;
class UDialogueNode;
class UEdGraphPin;

/**
 * Handler class for Dialogue System MCP commands.
 * MCP-1 (read): get_dialogue_graph, get_dialogue_connections
 * MCP-2 (write): create_dialogue_asset, add_dialogue_node, set_dialogue_node_properties,
 *                connect_dialogue_nodes, disconnect_dialogue_nodes, delete_dialogue_node
 */
class UNREALMCP_API FUnrealMCPDialogueCommands
{
public:
    FUnrealMCPDialogueCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // --- MCP-1: Read ---
    TSharedPtr<FJsonObject> HandleGetDialogueGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetDialogueConnections(const TSharedPtr<FJsonObject>& Params);

    // --- MCP-2: Write ---
    TSharedPtr<FJsonObject> HandleCreateDialogueAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddDialogueNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetDialogueNodeProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectDialogueNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDisconnectDialogueNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteDialogueNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddDialogueChoiceItem(const TSharedPtr<FJsonObject>& Params);

    // --- Helpers ---
    UDialogueAsset* LoadDialogueAsset(const FString& AssetPath, TSharedPtr<FJsonObject>& OutError);
    UDialogueNode* FindNodeByGuid(UDialogueAsset* Asset, const FString& NodeIdStr);
    UEdGraphPin* FindOutputPin(UDialogueAsset* Asset, UDialogueNode* Node, const FString& PinName);
    UEdGraphPin* FindInputPin(UDialogueAsset* Asset, UDialogueNode* Node);
};
