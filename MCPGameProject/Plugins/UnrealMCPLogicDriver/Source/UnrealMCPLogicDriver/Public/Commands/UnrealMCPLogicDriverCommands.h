#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UBlueprint;
class UEdGraphNode;
class USMGraph;
class USMGraphNode_StateNodeBase;
class USMGraphNode_TransitionEdge;
class USMTransitionGraph;

class FUnrealMCPLogicDriverCommands
{
public:
	FUnrealMCPLogicDriverCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleGetLogicDriverStateMachineGraph(
		const TSharedPtr<FJsonObject>& Params);

	UBlueprint* ResolveBlueprint(
		const TSharedPtr<FJsonObject>& Params,
		TSharedPtr<FJsonObject>& OutError) const;

	USMGraph* ResolveStateMachineGraph(
		UBlueprint* Blueprint,
		const FString& RequestedGraphName,
		TArray<TSharedPtr<FJsonValue>>& OutAvailableGraphs,
		TSharedPtr<FJsonObject>& OutError) const;

	TSharedPtr<FJsonObject> SerializeStateMachineGraph(
		UBlueprint* Blueprint,
		USMGraph* Graph) const;

	TSharedPtr<FJsonObject> SerializeTransitionGraph(
		USMTransitionGraph* Graph) const;

	TSharedPtr<FJsonObject> SerializeGraphNode(
		const UEdGraphNode* Node,
		bool bIncludePins = true) const;

	TSharedPtr<FJsonObject> SerializeStateNode(
		const USMGraphNode_StateNodeBase* Node) const;

	TSharedPtr<FJsonObject> SerializeTransitionNode(
		const USMGraphNode_TransitionEdge* Transition) const;
};
