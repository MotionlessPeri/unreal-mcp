#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class UBTNode;
class UBTCompositeNode;
class UBTTaskNode;
class UBTDecorator;
class UBTService;

/**
 * Handler class for Behavior Tree read commands.
 * Currently supports: get_behavior_tree_info
 */
class UNREALMCP_API FUnrealMCPBehaviorTreeCommands
{
public:
    FUnrealMCPBehaviorTreeCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetBehaviorTreeInfo(const TSharedPtr<FJsonObject>& Params);

    // Recursively serialize a composite node and its subtree
    TSharedPtr<FJsonObject> SerializeCompositeNode(UBTCompositeNode* Node);
    // Serialize a task leaf node
    TSharedPtr<FJsonObject> SerializeTaskNode(UBTTaskNode* Node);
    // Serialize a decorator
    TSharedPtr<FJsonObject> SerializeDecorator(UBTDecorator* Node);
    // Serialize a service
    TSharedPtr<FJsonObject> SerializeService(UBTService* Node);
};
