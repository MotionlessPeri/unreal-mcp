#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Dialogue System read commands.
 * Supports: get_dialogue_graph, get_dialogue_connections
 */
class UNREALMCP_API FUnrealMCPDialogueCommands
{
public:
    FUnrealMCPDialogueCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetDialogueGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetDialogueConnections(const TSharedPtr<FJsonObject>& Params);
};
