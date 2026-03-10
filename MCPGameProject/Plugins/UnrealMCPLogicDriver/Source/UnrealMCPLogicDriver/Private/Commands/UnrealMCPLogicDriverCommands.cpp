#include "Commands/UnrealMCPLogicDriverCommands.h"

#include "Blueprints/SMBlueprint.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "EditorAssetLibrary.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_RootNode.h"
#include "Engine/Blueprint.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionResultNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_Base.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineEntryNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNodeBase.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/SMGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionEnteredNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPostEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionPreEvaluateNode.h"
#include "Graph/Nodes/RootNodes/SMGraphK2Node_TransitionShutdownNode.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "ExposedFunctions/SMExposedFunctions.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

namespace
{
FString GetNodeIdentifier(const UEdGraphNode* Node)
{
	if (!IsValid(Node))
	{
		return TEXT("");
	}

	if (const USMGraphNode_Base* LogicNode = Cast<USMGraphNode_Base>(Node))
	{
		const FGuid& Guid = LogicNode->GetCorrectNodeGuid();
		if (Guid.IsValid())
		{
			return Guid.ToString();
		}
	}

	if (Node->NodeGuid.IsValid())
	{
		return Node->NodeGuid.ToString();
	}

	return Node->GetName();
}

FString GetNodeKind(const UEdGraphNode* Node)
{
	if (Cast<USMGraphNode_StateMachineEntryNode>(Node))
	{
		return TEXT("entry");
	}
	if (Cast<USMGraphNode_TransitionEdge>(Node))
	{
		return TEXT("transition");
	}
	if (Cast<USMGraphNode_AnyStateNode>(Node))
	{
		return TEXT("any_state");
	}
	if (Cast<USMGraphNode_ConduitNode>(Node))
	{
		return TEXT("conduit");
	}
	if (Cast<USMGraphNode_LinkStateNode>(Node))
	{
		return TEXT("link_state");
	}
	if (Cast<USMGraphNode_StateMachineStateNode>(Node))
	{
		return TEXT("state_machine_state");
	}
	if (Cast<USMGraphNode_StateNodeBase>(Node))
	{
		return TEXT("state");
	}
	if (Cast<USMGraphK2Node_TransitionResultNode>(Node))
	{
		return TEXT("transition_result");
	}

	return Node ? Node->GetClass()->GetName() : TEXT("unknown");
}

FString GetPinDirectionString(EEdGraphPinDirection Direction)
{
	switch (Direction)
	{
	case EGPD_Input:
		return TEXT("input");
	case EGPD_Output:
		return TEXT("output");
	default:
		return TEXT("unknown");
	}
}

FString GetConditionalEvaluationTypeString(ESMConditionalEvaluationType Type)
{
	switch (Type)
	{
	case ESMConditionalEvaluationType::SM_Graph:
		return TEXT("SM_Graph");
	case ESMConditionalEvaluationType::SM_NodeInstance:
		return TEXT("SM_NodeInstance");
	case ESMConditionalEvaluationType::SM_AlwaysFalse:
		return TEXT("SM_AlwaysFalse");
	case ESMConditionalEvaluationType::SM_AlwaysTrue:
		return TEXT("SM_AlwaysTrue");
	default:
		return TEXT("Unknown");
	}
}

TSharedPtr<FJsonObject> MakePositionObject(const UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> Position = MakeShared<FJsonObject>();
	Position->SetNumberField(TEXT("x"), Node ? Node->NodePosX : 0);
	Position->SetNumberField(TEXT("y"), Node ? Node->NodePosY : 0);
	return Position;
}

TArray<TSharedPtr<FJsonValue>> SerializePins(const UEdGraphNode* Node)
{
	TArray<TSharedPtr<FJsonValue>> Pins;
	if (!IsValid(Node))
	{
		return Pins;
	}

	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
		PinObject->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
		PinObject->SetStringField(TEXT("direction"), GetPinDirectionString(Pin->Direction));
		PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
		PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
		PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);

		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			PinObject->SetStringField(TEXT("subcategory_object"), Pin->PinType.PinSubCategoryObject->GetPathName());
		}

		TArray<TSharedPtr<FJsonValue>> LinkedTo;
		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (!LinkedPin)
			{
				continue;
			}

			const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
			TSharedPtr<FJsonObject> LinkObject = MakeShared<FJsonObject>();
			LinkObject->SetStringField(TEXT("node_id"), GetNodeIdentifier(LinkedNode));
			LinkObject->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
			LinkObject->SetStringField(TEXT("node_class"), LinkedNode ? LinkedNode->GetClass()->GetPathName() : TEXT(""));
			LinkedTo.Add(MakeShared<FJsonValueObject>(LinkObject));
		}

		PinObject->SetArrayField(TEXT("linked_to"), LinkedTo);
		Pins.Add(MakeShared<FJsonValueObject>(PinObject));
	}

	return Pins;
}

TArray<TSharedPtr<FJsonValue>> SerializeEdges(const UEdGraph* Graph)
{
	TArray<TSharedPtr<FJsonValue>> Edges;
	if (!IsValid(Graph))
	{
		return Edges;
	}

	TSet<FString> SeenEdges;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (!IsValid(Node))
		{
			continue;
		}

		const FString FromNodeId = GetNodeIdentifier(Node);
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin)
				{
					continue;
				}

				const UEdGraphNode* TargetNode = LinkedPin->GetOwningNode();
				const FString ToNodeId = GetNodeIdentifier(TargetNode);
				const FString EdgeKey = FString::Printf(
					TEXT("%s|%s|%s|%s"),
					*FromNodeId,
					*Pin->PinName.ToString(),
					*ToNodeId,
					*LinkedPin->PinName.ToString());

				if (SeenEdges.Contains(EdgeKey))
				{
					continue;
				}
				SeenEdges.Add(EdgeKey);

				TSharedPtr<FJsonObject> EdgeObject = MakeShared<FJsonObject>();
				EdgeObject->SetStringField(TEXT("from_node_id"), FromNodeId);
				EdgeObject->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
				EdgeObject->SetStringField(TEXT("to_node_id"), ToNodeId);
				EdgeObject->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
				Edges.Add(MakeShared<FJsonValueObject>(EdgeObject));
			}
		}
	}

	return Edges;
}

template<typename T>
bool HasExecutionLogicNode(const USMTransitionGraph* Graph)
{
	if (!IsValid(Graph))
	{
		return false;
	}

	TArray<T*> MatchingNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<T>(const_cast<USMTransitionGraph*>(Graph), MatchingNodes);

	for (USMGraphK2Node_RootNode* Node : MatchingNodes)
	{
		if (IsValid(Node) && Node->GetOutputNode())
		{
			return true;
		}
	}

	return false;
}
}

FUnrealMCPLogicDriverCommands::FUnrealMCPLogicDriverCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPLogicDriverCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("get_logicdriver_state_machine_graph"))
	{
		return HandleGetLogicDriverStateMachineGraph(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown Logic Driver command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPLogicDriverCommands::HandleGetLogicDriverStateMachineGraph(
	const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UBlueprint* Blueprint = ResolveBlueprint(Params, Error);
	if (!Blueprint)
	{
		return Error;
	}

	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);

	TArray<TSharedPtr<FJsonValue>> AvailableGraphs;
	USMGraph* StateMachineGraph = ResolveStateMachineGraph(Blueprint, GraphName, AvailableGraphs, Error);
	if (!StateMachineGraph)
	{
		return Error;
	}

	TSharedPtr<FJsonObject> Result = SerializeStateMachineGraph(Blueprint, StateMachineGraph);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("available_state_machine_graphs"), AvailableGraphs);
	return Result;
}

UBlueprint* FUnrealMCPLogicDriverCommands::ResolveBlueprint(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutError) const
{
	FString AssetPath;
	if (Params->TryGetStringField(TEXT("asset_path"), AssetPath) && !AssetPath.IsEmpty())
	{
		if (UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(AssetPath))
		{
			return Blueprint;
		}

		UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(AssetPath);
		if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadedObject))
		{
			return Blueprint;
		}
	}

	FString BlueprintName;
	if (Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) && !BlueprintName.IsEmpty())
	{
		if (UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName))
		{
			return Blueprint;
		}
	}

	OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
		TEXT("Missing or invalid Blueprint. Provide 'blueprint_name' or 'asset_path'."));
	return nullptr;
}

USMGraph* FUnrealMCPLogicDriverCommands::ResolveStateMachineGraph(
	UBlueprint* Blueprint,
	const FString& RequestedGraphName,
	TArray<TSharedPtr<FJsonValue>>& OutAvailableGraphs,
	TSharedPtr<FJsonObject>& OutError) const
{
	if (!IsValid(Blueprint))
	{
		OutError = FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint is invalid."));
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	USMGraph* RequestedGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (USMGraph* StateMachineGraph = Cast<USMGraph>(Graph))
		{
			OutAvailableGraphs.Add(MakeShared<FJsonValueString>(StateMachineGraph->GetName()));
			if (!RequestedGraphName.IsEmpty() &&
				StateMachineGraph->GetName().Equals(RequestedGraphName, ESearchCase::IgnoreCase))
			{
				RequestedGraph = StateMachineGraph;
			}
		}
	}

	if (RequestedGraph)
	{
		return RequestedGraph;
	}

	if (!RequestedGraphName.IsEmpty())
	{
		OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not find Logic Driver graph '%s' on Blueprint '%s'."),
				*RequestedGraphName,
				*Blueprint->GetName()));
		return nullptr;
	}

	if (USMGraph* RootGraph = FSMBlueprintEditorUtils::GetRootStateMachineGraph(Blueprint))
	{
		return RootGraph;
	}

	OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Blueprint '%s' does not contain a Logic Driver state machine graph."),
			*Blueprint->GetName()));
	return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPLogicDriverCommands::SerializeStateMachineGraph(
	UBlueprint* Blueprint,
	USMGraph* Graph) const
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	Result->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	Result->SetStringField(TEXT("graph_name"), Graph->GetName());
	Result->SetStringField(TEXT("graph_class"), Graph->GetClass()->GetPathName());
	Result->SetBoolField(TEXT("has_any_logic_connections"), Graph->HasAnyLogicConnections());
	Result->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

	TArray<TSharedPtr<FJsonValue>> Nodes;
	TArray<TSharedPtr<FJsonValue>> States;
	TArray<TSharedPtr<FJsonValue>> SpecialNodes;
	TArray<TSharedPtr<FJsonValue>> Transitions;

	for (const UEdGraphNode* GraphNode : Graph->Nodes)
	{
		if (!IsValid(GraphNode))
		{
			continue;
		}

		if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(GraphNode))
		{
			TSharedPtr<FJsonObject> TransitionObject = SerializeTransitionNode(Transition);
			Nodes.Add(MakeShared<FJsonValueObject>(TransitionObject));
			Transitions.Add(MakeShared<FJsonValueObject>(TransitionObject));
			continue;
		}

		if (const USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(GraphNode))
		{
			TSharedPtr<FJsonObject> StateObject = SerializeStateNode(StateNode);
			Nodes.Add(MakeShared<FJsonValueObject>(StateObject));

			const FString NodeKind = StateObject->GetStringField(TEXT("node_kind"));
			if (NodeKind == TEXT("entry") ||
				NodeKind == TEXT("any_state") ||
				NodeKind == TEXT("conduit") ||
				NodeKind == TEXT("link_state"))
			{
				SpecialNodes.Add(MakeShared<FJsonValueObject>(StateObject));
			}
			else
			{
				States.Add(MakeShared<FJsonValueObject>(StateObject));
			}
			continue;
		}

		Nodes.Add(MakeShared<FJsonValueObject>(SerializeGraphNode(GraphNode)));
	}

	Result->SetArrayField(TEXT("nodes"), Nodes);
	Result->SetArrayField(TEXT("states"), States);
	Result->SetArrayField(TEXT("special_nodes"), SpecialNodes);
	Result->SetArrayField(TEXT("transitions"), Transitions);
	Result->SetArrayField(TEXT("edges"), SerializeEdges(Graph));

	if (USMGraphNode_StateMachineEntryNode* EntryNode = Graph->GetEntryNode())
	{
		Result->SetObjectField(TEXT("entry_node"), SerializeStateNode(EntryNode));
	}

	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPLogicDriverCommands::SerializeTransitionGraph(
	USMTransitionGraph* Graph) const
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	if (!IsValid(Graph))
	{
		return Result;
	}

	Result->SetStringField(TEXT("graph_name"), Graph->GetName());
	Result->SetStringField(TEXT("graph_class"), Graph->GetClass()->GetPathName());
	Result->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
	Result->SetBoolField(TEXT("has_any_logic_connections"), Graph->HasAnyLogicConnections());
	Result->SetStringField(TEXT("conditional_evaluation_type"), GetConditionalEvaluationTypeString(Graph->GetConditionalEvaluationType()));
	Result->SetBoolField(TEXT("has_transition_entered_logic"), HasExecutionLogicNode<USMGraphK2Node_TransitionEnteredNode>(Graph));
	Result->SetBoolField(TEXT("has_pre_eval_logic"), HasExecutionLogicNode<USMGraphK2Node_TransitionPreEvaluateNode>(Graph));
	Result->SetBoolField(TEXT("has_post_eval_logic"), HasExecutionLogicNode<USMGraphK2Node_TransitionPostEvaluateNode>(Graph));
	Result->SetBoolField(TEXT("has_init_logic"), HasExecutionLogicNode<USMGraphK2Node_TransitionInitializedNode>(Graph));
	Result->SetBoolField(TEXT("has_shutdown_logic"), HasExecutionLogicNode<USMGraphK2Node_TransitionShutdownNode>(Graph));

	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (!IsValid(Node))
		{
			continue;
		}

		Nodes.Add(MakeShared<FJsonValueObject>(SerializeGraphNode(Node)));
	}

	Result->SetArrayField(TEXT("nodes"), Nodes);
	Result->SetArrayField(TEXT("edges"), SerializeEdges(Graph));

	if (IsValid(Graph->ResultNode))
	{
		Result->SetObjectField(TEXT("result_node"), SerializeGraphNode(Graph->ResultNode));
	}

	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPLogicDriverCommands::SerializeGraphNode(
	const UEdGraphNode* Node,
	bool bIncludePins) const
{
	TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
	if (!IsValid(Node))
	{
		return NodeObject;
	}

	NodeObject->SetStringField(TEXT("node_id"), GetNodeIdentifier(Node));
	NodeObject->SetStringField(TEXT("node_kind"), GetNodeKind(Node));
	NodeObject->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName());
	NodeObject->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	NodeObject->SetStringField(TEXT("name"), Node->GetName());
	NodeObject->SetObjectField(TEXT("position"), MakePositionObject(Node));
	NodeObject->SetBoolField(TEXT("can_rename"), Node->GetCanRenameNode());

	if (bIncludePins)
	{
		NodeObject->SetArrayField(TEXT("pins"), SerializePins(Node));
	}

	return NodeObject;
}

TSharedPtr<FJsonObject> FUnrealMCPLogicDriverCommands::SerializeStateNode(
	const USMGraphNode_StateNodeBase* Node) const
{
	TSharedPtr<FJsonObject> NodeObject = SerializeGraphNode(Node);
	if (!IsValid(Node))
	{
		return NodeObject;
	}

	NodeObject->SetStringField(TEXT("state_name"), Node->GetStateName());
	NodeObject->SetBoolField(TEXT("has_input_connections"), Node->HasInputConnections());
	NodeObject->SetBoolField(TEXT("has_output_connections"), Node->HasOutputConnections());
	NodeObject->SetBoolField(TEXT("is_end_state"), Node->IsEndState());
	NodeObject->SetNumberField(TEXT("num_input_connections"), Node->GetNumInputConnections());
	NodeObject->SetNumberField(TEXT("num_output_connections"), Node->GetNumOutputConnections());
	NodeObject->SetBoolField(TEXT("connected_to_entry"), Node->GetConnectedEntryPin() != nullptr);

	if (const UEdGraph* BoundGraph = Node->GetBoundGraph())
	{
		TSharedPtr<FJsonObject> BoundGraphObject = MakeShared<FJsonObject>();
		BoundGraphObject->SetStringField(TEXT("graph_name"), BoundGraph->GetName());
		BoundGraphObject->SetStringField(TEXT("graph_class"), BoundGraph->GetClass()->GetPathName());
		BoundGraphObject->SetNumberField(TEXT("node_count"), BoundGraph->Nodes.Num());
		NodeObject->SetObjectField(TEXT("bound_graph"), BoundGraphObject);
	}

	if (const USMGraphNode_StateMachineStateNode* StateMachineStateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
	{
		NodeObject->SetBoolField(TEXT("is_state_machine_reference"), StateMachineStateNode->IsStateMachineReference());

		if (const USMBlueprint* ReferencedBlueprint = StateMachineStateNode->GetStateMachineReference())
		{
			NodeObject->SetStringField(TEXT("referenced_blueprint"), ReferencedBlueprint->GetPathName());
		}
	}

	return NodeObject;
}

TSharedPtr<FJsonObject> FUnrealMCPLogicDriverCommands::SerializeTransitionNode(
	const USMGraphNode_TransitionEdge* Transition) const
{
	TSharedPtr<FJsonObject> TransitionObject = SerializeGraphNode(Transition);
	if (!IsValid(Transition))
	{
		return TransitionObject;
	}

	if (const USMGraphNode_StateNodeBase* FromState = Transition->GetFromState(true))
	{
		TransitionObject->SetStringField(TEXT("from_node_id"), GetNodeIdentifier(FromState));
		TransitionObject->SetStringField(TEXT("from_state_name"), FromState->GetStateName());
	}

	if (const USMGraphNode_StateNodeBase* ToState = Transition->GetToState(true))
	{
		TransitionObject->SetStringField(TEXT("to_node_id"), GetNodeIdentifier(ToState));
		TransitionObject->SetStringField(TEXT("to_state_name"), ToState->GetStateName());
	}

	TransitionObject->SetBoolField(TEXT("is_rerouted"), Transition->IsRerouted());
	TransitionObject->SetStringField(TEXT("transition_name"), Transition->GetTransitionName());
	TransitionObject->SetStringField(TEXT("default_transition_name"), Transition->GetDefaultTransitionName());

	if (UClass* TransitionClass = Transition->GetNodeClass())
	{
		TransitionObject->SetStringField(TEXT("transition_class"), TransitionClass->GetPathName());
	}

	if (USMTransitionGraph* TransitionGraph = Transition->GetTransitionGraph())
	{
		TransitionObject->SetStringField(
			TEXT("conditional_evaluation_type"),
			GetConditionalEvaluationTypeString(TransitionGraph->GetConditionalEvaluationType()));
		TransitionObject->SetObjectField(TEXT("condition_graph"), SerializeTransitionGraph(TransitionGraph));
	}

	return TransitionObject;
}
