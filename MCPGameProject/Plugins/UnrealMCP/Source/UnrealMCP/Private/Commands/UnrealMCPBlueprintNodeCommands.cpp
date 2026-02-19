#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_GetSubsystem.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/InputSettings.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Subsystems/Subsystem.h"

// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogUnrealMCP, Log, All);

namespace
{
UClass* ResolveClassByName(const FString& InClassName)
{
    FString ClassName = InClassName;
    ClassName.TrimStartAndEndInline();
    if (ClassName.IsEmpty())
    {
        return nullptr;
    }

    UClass* ResolvedClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
    if (!ResolvedClass && ClassName.StartsWith(TEXT("/Script/")))
    {
        ResolvedClass = LoadObject<UClass>(nullptr, *ClassName);
    }
    if (!ResolvedClass && !ClassName.StartsWith(TEXT("U")))
    {
        const FString WithUPrefix = FString(TEXT("U")) + ClassName;
        ResolvedClass = FindObject<UClass>(ANY_PACKAGE, *WithUPrefix);
    }

    if (!ResolvedClass)
    {
        const FString TargetNoPrefix = ClassName.StartsWith(TEXT("U")) ? ClassName.RightChop(1) : ClassName;
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            UClass* CandidateClass = *ClassIt;
            if (!IsValid(CandidateClass))
            {
                continue;
            }

            const FString CandidateName = CandidateClass->GetName();
            const FString CandidateNoPrefix = CandidateName.StartsWith(TEXT("U")) ? CandidateName.RightChop(1) : CandidateName;
            if (CandidateName.Equals(ClassName, ESearchCase::IgnoreCase) ||
                CandidateNoPrefix.Equals(TargetNoPrefix, ESearchCase::IgnoreCase))
            {
                ResolvedClass = CandidateClass;
                break;
            }
        }
    }

    return ResolvedClass;
}

UEdGraphPin* FindNodePinByName(UEdGraphNode* Node, const FString& PinName)
{
    if (!IsValid(Node))
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinName.ToString() == PinName)
        {
            return Pin;
        }
    }

    return nullptr;
}

TSet<UEdGraphNode*> CollectExecChainNodes(UEdGraphNode* EventNode, const FString& EventOutputPinName)
{
    TSet<UEdGraphNode*> NodesToRemove;
    if (!IsValid(EventNode))
    {
        return NodesToRemove;
    }

    UEdGraphPin* EventExecPin = FindNodePinByName(EventNode, EventOutputPinName);
    if (!EventExecPin)
    {
        return NodesToRemove;
    }

    TArray<UEdGraphNode*> ExecQueue;
    int32 QueueIndex = 0;

    for (UEdGraphPin* LinkedPin : EventExecPin->LinkedTo)
    {
        if (!LinkedPin)
        {
            continue;
        }

        UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
        if (IsValid(LinkedNode) && LinkedNode != EventNode && !NodesToRemove.Contains(LinkedNode))
        {
            NodesToRemove.Add(LinkedNode);
            ExecQueue.Add(LinkedNode);
        }
    }

    while (QueueIndex < ExecQueue.Num())
    {
        UEdGraphNode* CurrentNode = ExecQueue[QueueIndex++];
        if (!IsValid(CurrentNode))
        {
            continue;
        }

        for (UEdGraphPin* Pin : CurrentNode->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output)
            {
                continue;
            }

            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (!LinkedPin)
                {
                    continue;
                }

                UEdGraphNode* NextNode = LinkedPin->GetOwningNode();
                if (IsValid(NextNode) && NextNode != EventNode && !NodesToRemove.Contains(NextNode))
                {
                    NodesToRemove.Add(NextNode);
                    ExecQueue.Add(NextNode);
                }
            }
        }
    }

    UEdGraph* Graph = EventNode->GetGraph();
    if (!Graph)
    {
        return NodesToRemove;
    }

    bool bAddedDependencyNode = true;
    while (bAddedDependencyNode)
    {
        bAddedDependencyNode = false;
        for (UEdGraphNode* CandidateNode : Graph->Nodes)
        {
            if (!IsValid(CandidateNode) || CandidateNode == EventNode || NodesToRemove.Contains(CandidateNode))
            {
                continue;
            }

            bool bHasAnyLink = false;
            bool bAllLinksPointToRemovalSet = true;
            for (UEdGraphPin* CandidatePin : CandidateNode->Pins)
            {
                if (!CandidatePin)
                {
                    continue;
                }

                for (UEdGraphPin* LinkedPin : CandidatePin->LinkedTo)
                {
                    if (!LinkedPin)
                    {
                        continue;
                    }

                    UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
                    if (!IsValid(LinkedNode))
                    {
                        continue;
                    }

                    bHasAnyLink = true;
                    if (LinkedNode != EventNode && !NodesToRemove.Contains(LinkedNode))
                    {
                        bAllLinksPointToRemovalSet = false;
                        break;
                    }
                }

                if (!bAllLinksPointToRemovalSet)
                {
                    break;
                }
            }

            if (bHasAnyLink && bAllLinksPointToRemovalSet)
            {
                NodesToRemove.Add(CandidateNode);
                bAddedDependencyNode = true;
            }
        }
    }

    return NodesToRemove;
}
}

FUnrealMCPBlueprintNodeCommands::FUnrealMCPBlueprintNodeCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        return HandleConnectBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_get_self_component_reference"))
    {
        return HandleAddBlueprintGetSelfComponentReference(Params);
    }
    else if (CommandType == TEXT("add_blueprint_event_node"))
    {
        return HandleAddBlueprintEvent(Params);
    }
    else if (CommandType == TEXT("add_blueprint_function_node"))
    {
        return HandleAddBlueprintFunctionCall(Params);
    }
    else if (CommandType == TEXT("add_blueprint_variable"))
    {
        return HandleAddBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("add_blueprint_input_action_node"))
    {
        return HandleAddBlueprintInputActionNode(Params);
    }
    else if (CommandType == TEXT("add_blueprint_self_reference"))
    {
        return HandleAddBlueprintSelfReference(Params);
    }
    else if (CommandType == TEXT("add_blueprint_dynamic_cast_node"))
    {
        return HandleAddBlueprintDynamicCastNode(Params);
    }
    else if (CommandType == TEXT("add_blueprint_subsystem_getter_node"))
    {
        return HandleAddBlueprintSubsystemGetterNode(Params);
    }
    else if (CommandType == TEXT("add_blueprint_make_struct_node"))
    {
        return HandleAddBlueprintMakeStructNode(Params);
    }
    else if (CommandType == TEXT("break_blueprint_node_pin_links"))
    {
        return HandleBreakBlueprintNodePinLinks(Params);
    }
    else if (CommandType == TEXT("clear_blueprint_event_exec_chain"))
    {
        return HandleClearBlueprintEventExecChain(Params);
    }
    else if (CommandType == TEXT("dedupe_blueprint_component_bound_events"))
    {
        return HandleDedupeBlueprintComponentBoundEvents(Params);
    }
    else if (CommandType == TEXT("bind_blueprint_multicast_delegate"))
    {
        return HandleBindBlueprintMulticastDelegate(Params);
    }
    else if (CommandType == TEXT("find_blueprint_nodes"))
    {
        return HandleFindBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("clear_blueprint_event_graph"))
    {
        return HandleClearBlueprintEventGraph(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint node command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
    }

    FString TargetNodeId;
    if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
    }

    FString SourcePinName;
    if (!Params->TryGetStringField(TEXT("source_pin"), SourcePinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_pin' parameter"));
    }

    FString TargetPinName;
    if (!Params->TryGetStringField(TEXT("target_pin"), TargetPinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == SourceNodeId)
        {
            SourceNode = Node;
        }
        else if (Node->NodeGuid.ToString() == TargetNodeId)
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Source or target node not found"));
    }

    // Connect the nodes
    if (FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName))
    {
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("source_node_id"), SourceNodeId);
        ResultObj->SetStringField(TEXT("target_node_id"), TargetNodeId);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect nodes"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }
    
    // We'll skip component verification since the GetAllNodes API may have changed in UE5.5
    
    // Create the variable get node directly
    UK2Node_VariableGet* GetComponentNode = NewObject<UK2Node_VariableGet>(EventGraph);
    if (!GetComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create get component node"));
    }
    
    // Set up the variable reference properly for UE5.5
    FMemberReference& VarRef = GetComponentNode->VariableReference;
    VarRef.SetSelfMember(FName(*ComponentName));
    
    // Set node position
    GetComponentNode->NodePosX = NodePosition.X;
    GetComponentNode->NodePosY = NodePosition.Y;
    
    // Add to graph
    EventGraph->AddNode(GetComponentNode);
    GetComponentNode->CreateNewGuid();
    GetComponentNode->PostPlacedNewNode();
    GetComponentNode->AllocateDefaultPins();
    
    // Explicitly reconstruct node for UE5.5
    GetComponentNode->ReconstructNode();
    
    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), GetComponentNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the event node
    UK2Node_Event* EventNode = FUnrealMCPCommonUtils::CreateEventNode(EventGraph, EventName, NodePosition);
    if (!EventNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Check for target parameter (optional)
    FString Target;
    Params->TryGetStringField(TEXT("target"), Target);

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the function
    UFunction* Function = nullptr;
    UK2Node_CallFunction* FunctionNode = nullptr;
    
    // Add extensive logging for debugging
    UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in target '%s'"), 
           *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target);
    
    // Check if we have a target class specified
    if (!Target.IsEmpty())
    {
        // Try to find the target class
        UClass* TargetClass = nullptr;
        
        // First try without a prefix
        TargetClass = FindObject<UClass>(ANY_PACKAGE, *Target);
        UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
               *Target, TargetClass ? TEXT("Found") : TEXT("Not found"));
        
        // If not found, try with U prefix (common convention for UE classes)
        if (!TargetClass && !Target.StartsWith(TEXT("U")))
        {
            FString TargetWithPrefix = FString(TEXT("U")) + Target;
            TargetClass = FindObject<UClass>(ANY_PACKAGE, *TargetWithPrefix);
            UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
                   *TargetWithPrefix, TargetClass ? TEXT("Found") : TEXT("Not found"));
        }
        
        // If still not found, try with common component names
        if (!TargetClass)
        {
            // Try some common component class names
            TArray<FString> PossibleClassNames;
            PossibleClassNames.Add(FString(TEXT("U")) + Target + TEXT("Component"));
            PossibleClassNames.Add(Target + TEXT("Component"));
            
            for (const FString& ClassName : PossibleClassNames)
            {
                TargetClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
                if (TargetClass)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found class using alternative name '%s'"), *ClassName);
                    break;
                }
            }
        }

        // Try script path if caller passed fully qualified object path.
        if (!TargetClass && Target.StartsWith(TEXT("/Script/")))
        {
            TargetClass = LoadObject<UClass>(nullptr, *Target);
            UE_LOG(LogTemp, Display, TEXT("Tried to load class by script path '%s': %s"),
                   *Target, TargetClass ? TEXT("Found") : TEXT("Not found"));
        }

        // Fallback: iterate loaded classes by short name.
        if (!TargetClass)
        {
            const FString TargetNoPrefix = Target.StartsWith(TEXT("U")) ? Target.RightChop(1) : Target;
            for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
            {
                UClass* CandidateClass = *ClassIt;
                if (!IsValid(CandidateClass))
                {
                    continue;
                }

                const FString CandidateName = CandidateClass->GetName();
                const FString CandidateNoPrefix = CandidateName.StartsWith(TEXT("U")) ? CandidateName.RightChop(1) : CandidateName;
                if (CandidateName.Equals(Target, ESearchCase::IgnoreCase) ||
                    CandidateNoPrefix.Equals(TargetNoPrefix, ESearchCase::IgnoreCase))
                {
                    TargetClass = CandidateClass;
                    UE_LOG(LogTemp, Display, TEXT("Found class by iterator fallback: %s"), *CandidateClass->GetPathName());
                    break;
                }
            }
        }
        
        // Special case handling for common classes like UGameplayStatics
        if (!TargetClass && Target == TEXT("UGameplayStatics"))
        {
            // For UGameplayStatics, use a direct reference to known class
            TargetClass = FindObject<UClass>(ANY_PACKAGE, TEXT("UGameplayStatics"));
            if (!TargetClass)
            {
                // Try loading it from its known package
                TargetClass = LoadObject<UClass>(nullptr, TEXT("/Script/Engine.GameplayStatics"));
                UE_LOG(LogTemp, Display, TEXT("Explicitly loading GameplayStatics: %s"), 
                       TargetClass ? TEXT("Success") : TEXT("Failed"));
            }
        }
        
        // If we found a target class, look for the function there
        if (TargetClass)
        {
            UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in class '%s'"), 
                   *FunctionName, *TargetClass->GetName());
                   
            // First try exact name
            Function = TargetClass->FindFunctionByName(*FunctionName);
            
            // If not found, try class hierarchy
            UClass* CurrentClass = TargetClass;
            while (!Function && CurrentClass)
            {
                UE_LOG(LogTemp, Display, TEXT("Searching in class: %s"), *CurrentClass->GetName());
                
                // Try exact match
                Function = CurrentClass->FindFunctionByName(*FunctionName);
                
                // Try case-insensitive match
                if (!Function)
                {
                    for (TFieldIterator<UFunction> FuncIt(CurrentClass); FuncIt; ++FuncIt)
                    {
                        UFunction* AvailableFunc = *FuncIt;
                        UE_LOG(LogTemp, Display, TEXT("  - Available function: %s"), *AvailableFunc->GetName());
                        
                        if (AvailableFunc->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive match: %s"), *AvailableFunc->GetName());
                            Function = AvailableFunc;
                            break;
                        }
                    }
                }
                
                // Move to parent class
                CurrentClass = CurrentClass->GetSuperClass();
            }
            
            // Special handling for known functions
            if (!Function)
            {
                if (TargetClass->GetName() == TEXT("GameplayStatics") && 
                    (FunctionName == TEXT("GetActorOfClass") || FunctionName.Equals(TEXT("GetActorOfClass"), ESearchCase::IgnoreCase)))
                {
                    UE_LOG(LogTemp, Display, TEXT("Using special case handling for GameplayStatics::GetActorOfClass"));
                    
                    // Create the function node directly
                    FunctionNode = NewObject<UK2Node_CallFunction>(EventGraph);
                    if (FunctionNode)
                    {
                        // Direct setup for known function
                        FunctionNode->FunctionReference.SetExternalMember(
                            FName(TEXT("GetActorOfClass")), 
                            TargetClass
                        );
                        
                        FunctionNode->NodePosX = NodePosition.X;
                        FunctionNode->NodePosY = NodePosition.Y;
                        EventGraph->AddNode(FunctionNode);
                        FunctionNode->CreateNewGuid();
                        FunctionNode->PostPlacedNewNode();
                        FunctionNode->AllocateDefaultPins();
                        
                        UE_LOG(LogTemp, Display, TEXT("Created GetActorOfClass node directly"));
                        
                        // List all pins
                        for (UEdGraphPin* Pin : FunctionNode->Pins)
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Pin: %s, Direction: %d, Category: %s"), 
                                   *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
                        }
                    }
                }
            }
        }
    }
    
    // If we still haven't found the function, try in the blueprint's class
    if (!Function && !FunctionNode)
    {
        UE_LOG(LogTemp, Display, TEXT("Trying to find function in blueprint class"));
        Function = Blueprint->GeneratedClass->FindFunctionByName(*FunctionName);
    }
    
    // Create the function call node if we found the function
    if (Function && !FunctionNode)
    {
        FunctionNode = FUnrealMCPCommonUtils::CreateFunctionCallNode(EventGraph, Function, NodePosition);
    }
    
    if (!FunctionNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s in target %s"), *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target));
    }

    // Reconstruct before assigning params so defaults are not reset afterwards.
    FunctionNode->ReconstructNode();

    // Set parameters if provided
    if (Params->HasField(TEXT("params")))
    {
        const TSharedPtr<FJsonObject>* ParamsObj;
        if (Params->TryGetObjectField(TEXT("params"), ParamsObj))
        {
            // Process parameters
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Param : (*ParamsObj)->Values)
            {
                const FString& ParamName = Param.Key;
                const TSharedPtr<FJsonValue>& ParamValue = Param.Value;
                
                // Find the parameter pin
                UEdGraphPin* ParamPin = FUnrealMCPCommonUtils::FindPin(FunctionNode, ParamName, EGPD_Input);
                if (ParamPin)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found parameter pin '%s' of category '%s'"), 
                           *ParamName, *ParamPin->PinType.PinCategory.ToString());
                    UE_LOG(LogTemp, Display, TEXT("  Current default value: '%s'"), *ParamPin->DefaultValue);
                    if (ParamPin->PinType.PinSubCategoryObject.IsValid())
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Pin subcategory: '%s'"), 
                               *ParamPin->PinType.PinSubCategoryObject->GetName());
                    }
                    
                    // Set parameter based on type
                    if (ParamValue->Type == EJson::String)
                    {
                        FString StringVal = ParamValue->AsString();
                        UE_LOG(LogTemp, Display, TEXT("  Setting string parameter '%s' to: '%s'"), 
                               *ParamName, *StringVal);
                        
                        // Handle class reference parameters (e.g., ActorClass in GetActorOfClass)
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
                        {
                            // For class references, we require the exact class name with proper prefix
                            // - Actor classes must start with 'A' (e.g., ACameraActor)
                            // - Non-actor classes must start with 'U' (e.g., UObject)
                            const FString& ClassName = StringVal;
                            
                            // TODO: This likely won't work in UE5.5+, so don't rely on it.
                            UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);

                            if (!Class)
                            {
                                Class = LoadObject<UClass>(nullptr, *ClassName);
                                UE_LOG(LogUnrealMCP, Display, TEXT("FindObject<UClass> failed. Assuming soft path  path: %s"), *ClassName);
                            }
                            
                            // If not found, try with Engine module path
                            if (!Class)
                            {
                                FString EngineClassName = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
                                Class = LoadObject<UClass>(nullptr, *EngineClassName);
                                UE_LOG(LogUnrealMCP, Display, TEXT("Trying Engine module path: %s"), *EngineClassName);
                            }
                            
                            if (!Class)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to find class '%s'. Make sure to use the exact class name with proper prefix (A for actors, U for non-actors)"), *ClassName);
                                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find class '%s'"), *ClassName));
                            }

                            const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema());
                            if (!K2Schema)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to get K2Schema"));
                                return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get K2Schema"));
                            }

                            K2Schema->TrySetDefaultObject(*ParamPin, Class);
                            if (ParamPin->DefaultObject != Class)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to set class reference for pin '%s'"), *ParamPin->PinName.ToString()));
                            }

                            UE_LOG(LogUnrealMCP, Log, TEXT("Successfully set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                            continue;
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
                        {
                            bool BoolValue = ParamValue->AsBool();
                            ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                            UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                                   *ParamName, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
                        {
                            // Handle array parameters - like Vector parameters
                            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                            if (ParamValue->TryGetArray(ArrayValue))
                            {
                                // Check if this could be a vector (array of 3 numbers)
                                if (ArrayValue->Num() == 3)
                                {
                                    // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                    float X = (*ArrayValue)[0]->AsNumber();
                                    float Y = (*ArrayValue)[1]->AsNumber();
                                    float Z = (*ArrayValue)[2]->AsNumber();
                                    
                                    FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                    ParamPin->DefaultValue = VectorString;
                                    
                                    UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                           *ParamName, *VectorString);
                                    UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                           *ParamPin->DefaultValue);
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                                }
                            }
                        }
                        else
                        {
                            if (const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>())
                            {
                                K2Schema->TrySetDefaultValue(*ParamPin, StringVal);
                            }
                            else
                            {
                                ParamPin->DefaultValue = StringVal;
                            }
                            UE_LOG(LogTemp, Display, TEXT("  Set generic string parameter '%s' to: '%s'"), 
                                   *ParamName, *ParamPin->DefaultValue);
                        }
                    }
                    else if (ParamValue->Type == EJson::Number)
                    {
                        // Handle integer vs float parameters correctly
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                    }
                    else if (ParamValue->Type == EJson::Boolean)
                    {
                        bool BoolValue = ParamValue->AsBool();
                        ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                        UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                               *ParamName, *ParamPin->DefaultValue);
                    }
                    else if (ParamValue->Type == EJson::Array)
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Processing array parameter '%s'"), *ParamName);
                        // Handle array parameters - like Vector parameters
                        const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                        if (ParamValue->TryGetArray(ArrayValue))
                        {
                            // Check if this could be a vector (array of 3 numbers)
                            if (ArrayValue->Num() == 3 && 
                                (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) &&
                                (ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get()))
                            {
                                // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                float X = (*ArrayValue)[0]->AsNumber();
                                float Y = (*ArrayValue)[1]->AsNumber();
                                float Z = (*ArrayValue)[2]->AsNumber();
                                
                                FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                ParamPin->DefaultValue = VectorString;
                                
                                UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                       *ParamName, *VectorString);
                                UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                       *ParamPin->DefaultValue);
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                            }
                        }
                    }
                    // Add handling for other types as needed
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Parameter pin '%s' not found"), *ParamName);
                }
            }
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_type' parameter"));
    }

    // Get optional parameters
    bool IsExposed = false;
    if (Params->HasField(TEXT("is_exposed")))
    {
        IsExposed = Params->GetBoolField(TEXT("is_exposed"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create variable based on type
    FEdGraphPinType PinType;
    
    // Set up pin type based on variable_type string
    if (VariableType == TEXT("Boolean"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (VariableType == TEXT("Integer") || VariableType == TEXT("Int"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType == TEXT("Float"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType == TEXT("String"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType == TEXT("Vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported variable type: %s"), *VariableType));
    }

    // Create the variable
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);

    // Set variable properties
    FBPVariableDescription* NewVar = nullptr;
    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == FName(*VariableName))
        {
            NewVar = &Variable;
            break;
        }
    }

    if (NewVar)
    {
        // Set exposure in editor
        if (IsExposed)
        {
            NewVar->PropertyFlags |= CPF_Edit;
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetStringField(TEXT("variable_type"), VariableType);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the input action node
    UK2Node_InputAction* InputActionNode = FUnrealMCPCommonUtils::CreateInputActionNode(EventGraph, ActionName, NodePosition);
    if (!InputActionNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create input action node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), InputActionNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the self node
    UK2Node_Self* SelfNode = FUnrealMCPCommonUtils::CreateSelfReferenceNode(EventGraph, NodePosition);
    if (!SelfNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create self node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), SelfNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintDynamicCastNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString TargetClassName;
    if (!Params->TryGetStringField(TEXT("target_class"), TargetClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_class' parameter"));
    }

    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    UClass* TargetClass = ResolveClassByName(TargetClassName);
    if (!TargetClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to resolve target class: %s"), *TargetClassName));
    }

    UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(EventGraph);
    if (!CastNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic cast node"));
    }

    CastNode->TargetType = TargetClass;
    CastNode->NodePosX = NodePosition.X;
    CastNode->NodePosY = NodePosition.Y;
    EventGraph->AddNode(CastNode);
    CastNode->CreateNewGuid();
    CastNode->PostPlacedNewNode();
    CastNode->AllocateDefaultPins();
    CastNode->ReconstructNode();

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), CastNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintSubsystemGetterNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SubsystemClassName;
    if (!Params->TryGetStringField(TEXT("subsystem_class"), SubsystemClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'subsystem_class' parameter"));
    }

    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    UClass* SubsystemClass = ResolveClassByName(SubsystemClassName);
    if (!SubsystemClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to resolve subsystem class: %s"), *SubsystemClassName));
    }

    if (!SubsystemClass->IsChildOf(USubsystem::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Class is not a subsystem: %s"), *SubsystemClass->GetPathName()));
    }

    UK2Node_GetSubsystem* GetSubsystemNode = NewObject<UK2Node_GetSubsystem>(EventGraph);
    if (!GetSubsystemNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create get subsystem node"));
    }

    GetSubsystemNode->Initialize(SubsystemClass);
    GetSubsystemNode->NodePosX = static_cast<int32>(NodePosition.X);
    GetSubsystemNode->NodePosY = static_cast<int32>(NodePosition.Y);
    EventGraph->AddNode(GetSubsystemNode);
    GetSubsystemNode->CreateNewGuid();
    GetSubsystemNode->PostPlacedNewNode();
    GetSubsystemNode->AllocateDefaultPins();
    GetSubsystemNode->ReconstructNode();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), GetSubsystemNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("subsystem_class"), SubsystemClass->GetPathName());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleAddBlueprintMakeStructNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString StructTypeName;
    if (!Params->TryGetStringField(TEXT("struct_type"), StructTypeName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'struct_type' parameter"));
    }

    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    UScriptStruct* StructType = FindObject<UScriptStruct>(ANY_PACKAGE, *StructTypeName);
    if (!StructType && StructTypeName.StartsWith(TEXT("/Script/")))
    {
        StructType = LoadObject<UScriptStruct>(nullptr, *StructTypeName);
    }

    if (!StructType)
    {
        for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
        {
            UScriptStruct* Candidate = *StructIt;
            if (!IsValid(Candidate))
            {
                continue;
            }

            const FString CandidateName = Candidate->GetName();
            if (CandidateName.Equals(StructTypeName, ESearchCase::IgnoreCase) ||
                Candidate->GetPathName().Equals(StructTypeName, ESearchCase::IgnoreCase))
            {
                StructType = Candidate;
                break;
            }
        }
    }

    if (!StructType)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to resolve struct type: %s"), *StructTypeName));
    }

    UK2Node_MakeStruct* MakeStructNode = NewObject<UK2Node_MakeStruct>(EventGraph);
    if (!MakeStructNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create make-struct node"));
    }

    MakeStructNode->StructType = StructType;
    MakeStructNode->NodePosX = static_cast<int32>(NodePosition.X);
    MakeStructNode->NodePosY = static_cast<int32>(NodePosition.Y);
    EventGraph->AddNode(MakeStructNode);
    MakeStructNode->CreateNewGuid();
    MakeStructNode->PostPlacedNewNode();
    MakeStructNode->AllocateDefaultPins();
    MakeStructNode->ReconstructNode();

    if (Params->HasField(TEXT("values")))
    {
        const TSharedPtr<FJsonObject>* ValuesObj = nullptr;
        if (Params->TryGetObjectField(TEXT("values"), ValuesObj))
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& FieldPair : (*ValuesObj)->Values)
            {
                const FString& FieldName = FieldPair.Key;
                UEdGraphPin* InputPin = FUnrealMCPCommonUtils::FindPin(MakeStructNode, FieldName, EGPD_Input);
                if (!InputPin)
                {
                    continue;
                }

                const TSharedPtr<FJsonValue>& FieldValue = FieldPair.Value;
                switch (FieldValue->Type)
                {
                case EJson::String:
                    InputPin->DefaultValue = FieldValue->AsString();
                    break;
                case EJson::Number:
                    if (InputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                    {
                        InputPin->DefaultValue = FString::FromInt(FMath::RoundToInt(FieldValue->AsNumber()));
                    }
                    else
                    {
                        InputPin->DefaultValue = FString::SanitizeFloat(FieldValue->AsNumber());
                    }
                    break;
                case EJson::Boolean:
                    InputPin->DefaultValue = FieldValue->AsBool() ? TEXT("true") : TEXT("false");
                    break;
                default:
                    break;
                }
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    FString OutputPinName = TEXT("ReturnValue");
    for (UEdGraphPin* Pin : MakeStructNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output)
        {
            OutputPinName = Pin->PinName.ToString();
            break;
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), MakeStructNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("struct_type"), StructType->GetPathName());
    ResultObj->SetStringField(TEXT("output_pin"), OutputPinName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleBreakBlueprintNodePinLinks(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    FString PinName;
    if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    UEdGraphNode* TargetNode = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (IsValid(Node) && Node->NodeGuid.ToString() == NodeId)
        {
            TargetNode = Node;
            break;
        }
    }

    if (!TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeId));
    }

    UEdGraphPin* TargetPin = nullptr;
    for (UEdGraphPin* Pin : TargetNode->Pins)
    {
        if (Pin && Pin->PinName.ToString() == PinName)
        {
            TargetPin = Pin;
            break;
        }
    }

    if (!TargetPin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Pin '%s' not found on node '%s'"), *PinName, *NodeId));
    }

    const int32 PreviousLinkCount = TargetPin->LinkedTo.Num();
    TargetPin->BreakAllPinLinks();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), NodeId);
    ResultObj->SetStringField(TEXT("pin_name"), PinName);
    ResultObj->SetNumberField(TEXT("removed_links"), PreviousLinkCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleClearBlueprintEventExecChain(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventNodeId;
    if (!Params->TryGetStringField(TEXT("event_node_id"), EventNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_node_id' parameter"));
    }

    FString EventOutputPinName = TEXT("Then");
    Params->TryGetStringField(TEXT("event_output_pin"), EventOutputPinName);

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    UEdGraphNode* EventNode = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (IsValid(Node) && Node->NodeGuid.ToString() == EventNodeId)
        {
            EventNode = Node;
            break;
        }
    }

    if (!EventNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Event node not found: %s"), *EventNodeId));
    }

    if (!FindNodePinByName(EventNode, EventOutputPinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Pin '%s' not found on event node '%s'"), *EventOutputPinName, *EventNodeId));
    }

    TSet<UEdGraphNode*> NodesToRemove = CollectExecChainNodes(EventNode, EventOutputPinName);

    int32 RemovedCount = 0;
    for (UEdGraphNode* NodeToRemove : NodesToRemove)
    {
        if (!IsValid(NodeToRemove))
        {
            continue;
        }
        FBlueprintEditorUtils::RemoveNode(Blueprint, NodeToRemove, true);
        ++RemovedCount;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("event_node_id"), EventNodeId);
    ResultObj->SetStringField(TEXT("event_output_pin"), EventOutputPinName);
    ResultObj->SetNumberField(TEXT("removed_nodes"), RemovedCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleDedupeBlueprintComponentBoundEvents(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString WidgetName;
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    FString KeepNodeId;
    Params->TryGetStringField(TEXT("keep_node_id"), KeepNodeId);

    FString EventOutputPinName = TEXT("Then");
    Params->TryGetStringField(TEXT("event_output_pin"), EventOutputPinName);

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    const FName WidgetFName(*WidgetName);
    const FName EventFName(*EventName);
    TArray<UK2Node_ComponentBoundEvent*> MatchedEvents;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        UK2Node_ComponentBoundEvent* BoundEventNode = Cast<UK2Node_ComponentBoundEvent>(Node);
        if (!BoundEventNode)
        {
            continue;
        }

        if (BoundEventNode->ComponentPropertyName != WidgetFName)
        {
            continue;
        }

        if (BoundEventNode->DelegatePropertyName != EventFName)
        {
            continue;
        }

        MatchedEvents.Add(BoundEventNode);
    }

    if (MatchedEvents.Num() <= 1)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
        ResultObj->SetStringField(TEXT("event_name"), EventName);
        ResultObj->SetNumberField(TEXT("matched_events"), MatchedEvents.Num());
        ResultObj->SetNumberField(TEXT("removed_event_nodes"), 0);
        ResultObj->SetNumberField(TEXT("removed_chain_nodes"), 0);
        if (MatchedEvents.Num() == 1)
        {
            ResultObj->SetStringField(TEXT("kept_node_id"), MatchedEvents[0]->NodeGuid.ToString());
        }
        return ResultObj;
    }

    UK2Node_ComponentBoundEvent* KeptEventNode = nullptr;
    if (!KeepNodeId.IsEmpty())
    {
        for (UK2Node_ComponentBoundEvent* Candidate : MatchedEvents)
        {
            if (Candidate && Candidate->NodeGuid.ToString() == KeepNodeId)
            {
                KeptEventNode = Candidate;
                break;
            }
        }
    }

    if (!KeptEventNode)
    {
        MatchedEvents.Sort([](const UK2Node_ComponentBoundEvent* A, const UK2Node_ComponentBoundEvent* B)
        {
            if (!A || !B)
            {
                return A != nullptr;
            }
            if (A->NodePosY == B->NodePosY)
            {
                return A->NodePosX < B->NodePosX;
            }
            return A->NodePosY < B->NodePosY;
        });
        KeptEventNode = MatchedEvents[0];
    }

    int32 RemovedEventNodes = 0;
    int32 RemovedChainNodes = 0;
    TSet<UEdGraphNode*> ChainNodesToRemove;
    for (UK2Node_ComponentBoundEvent* Candidate : MatchedEvents)
    {
        if (!Candidate || Candidate == KeptEventNode)
        {
            continue;
        }

        const TSet<UEdGraphNode*> CandidateChainNodes = CollectExecChainNodes(Candidate, EventOutputPinName);
        for (UEdGraphNode* ChainNode : CandidateChainNodes)
        {
            if (IsValid(ChainNode))
            {
                ChainNodesToRemove.Add(ChainNode);
            }
        }

        FBlueprintEditorUtils::RemoveNode(Blueprint, Candidate, true);
        ++RemovedEventNodes;
    }

    for (UEdGraphNode* NodeToRemove : ChainNodesToRemove)
    {
        if (!IsValid(NodeToRemove))
        {
            continue;
        }
        FBlueprintEditorUtils::RemoveNode(Blueprint, NodeToRemove, true);
        ++RemovedChainNodes;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
    ResultObj->SetStringField(TEXT("event_name"), EventName);
    ResultObj->SetNumberField(TEXT("matched_events"), MatchedEvents.Num());
    ResultObj->SetStringField(TEXT("kept_node_id"), KeptEventNode ? KeptEventNode->NodeGuid.ToString() : TEXT(""));
    ResultObj->SetNumberField(TEXT("removed_event_nodes"), RemovedEventNodes);
    ResultObj->SetNumberField(TEXT("removed_chain_nodes"), RemovedChainNodes);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleBindBlueprintMulticastDelegate(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString TargetClassName;
    if (!Params->TryGetStringField(TEXT("target_class"), TargetClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_class' parameter"));
    }

    FString DelegateName;
    if (!Params->TryGetStringField(TEXT("delegate_name"), DelegateName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'delegate_name' parameter"));
    }

    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    FVector2D CustomEventPosition(0.0f, 0.0f);
    bool bHasCustomEventPosition = false;
    if (Params->HasField(TEXT("custom_event_position")))
    {
        CustomEventPosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("custom_event_position"));
        bHasCustomEventPosition = true;
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    UClass* TargetClass = ResolveClassByName(TargetClassName);
    if (!TargetClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to resolve target class: %s"), *TargetClassName));
    }

    const FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(TargetClass, *DelegateName);
    if (!DelegateProperty)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Delegate '%s' not found on class '%s'"), *DelegateName, *TargetClass->GetName()));
    }

    UK2Node_AssignDelegate* AssignNode = NewObject<UK2Node_AssignDelegate>(EventGraph);
    if (!AssignNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create assign delegate node"));
    }

    AssignNode->SetFromProperty(DelegateProperty, false, TargetClass);
    AssignNode->NodePosX = static_cast<int32>(NodePosition.X);
    AssignNode->NodePosY = static_cast<int32>(NodePosition.Y);
    EventGraph->AddNode(AssignNode);
    AssignNode->CreateNewGuid();
    AssignNode->AllocateDefaultPins();
    AssignNode->ReconstructNode();

    UFunction* SignatureFunction = DelegateProperty->SignatureFunction;
    if (!SignatureFunction)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Delegate '%s' on class '%s' has no signature function"),
                *DelegateName, *TargetClass->GetName()));
    }

    FVector2D ResolvedCustomEventPosition = bHasCustomEventPosition
        ? CustomEventPosition
        : FVector2D(NodePosition.X + 300.0f, NodePosition.Y + 120.0f);
    const FString DesiredEventName = FString::Printf(TEXT("%s_Event"), *DelegateName);
    const FName UniqueEventName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, DesiredEventName);
    UK2Node_CustomEvent* CreatedCustomEventNode = UK2Node_CustomEvent::CreateFromFunction(
        ResolvedCustomEventPosition,
        EventGraph,
        UniqueEventName.ToString(),
        SignatureFunction,
        false);
    if (!CreatedCustomEventNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create custom event for delegate binding"));
    }

    UEdGraphPin* CustomEventDelegatePin = CreatedCustomEventNode->FindPin(UK2Node_CustomEvent::DelegateOutputName);
    UEdGraphPin* AssignDelegatePin = AssignNode->GetDelegatePin();
    if (!CustomEventDelegatePin || !AssignDelegatePin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to resolve delegate pins for custom-event binding"));
    }

    if (const UEdGraphSchema* Schema = EventGraph->GetSchema())
    {
        if (!Schema->TryCreateConnection(CustomEventDelegatePin, AssignDelegatePin))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect custom event delegate output to assign delegate pin"));
        }
    }
    else
    {
        CustomEventDelegatePin->MakeLinkTo(AssignDelegatePin);
    }

    FString TargetNodeId;
    Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId);
    if (!TargetNodeId.IsEmpty())
    {
        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (IsValid(Node) && Node->NodeGuid.ToString() == TargetNodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (!TargetNode)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("target_node_id not found: %s"), *TargetNodeId));
        }

        FString TargetOutputPinName = TEXT("ReturnValue");
        Params->TryGetStringField(TEXT("target_output_pin"), TargetOutputPinName);

        FString AssignTargetPinName = TEXT("self");
        Params->TryGetStringField(TEXT("assign_target_pin"), AssignTargetPinName);

        bool bConnected = FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, TargetNode, TargetOutputPinName, AssignNode, AssignTargetPinName);
        if (!bConnected && AssignTargetPinName.Equals(TEXT("self"), ESearchCase::IgnoreCase))
        {
            bConnected = FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, TargetNode, TargetOutputPinName, AssignNode, TEXT("Target"));
            if (bConnected)
            {
                AssignTargetPinName = TEXT("Target");
            }
        }

        if (!bConnected)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to connect target node '%s' pin '%s' to assign node pin '%s'"),
                    *TargetNodeId, *TargetOutputPinName, *AssignTargetPinName));
        }
    }

    FString ExecSourceNodeId;
    Params->TryGetStringField(TEXT("exec_source_node_id"), ExecSourceNodeId);
    if (!ExecSourceNodeId.IsEmpty())
    {
        UEdGraphNode* ExecSourceNode = nullptr;
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (IsValid(Node) && Node->NodeGuid.ToString() == ExecSourceNodeId)
            {
                ExecSourceNode = Node;
                break;
            }
        }

        if (!ExecSourceNode)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("exec_source_node_id not found: %s"), *ExecSourceNodeId));
        }

        FString ExecSourcePinName = TEXT("Then");
        Params->TryGetStringField(TEXT("exec_source_pin"), ExecSourcePinName);

        FString AssignExecPinName = TEXT("Execute");
        Params->TryGetStringField(TEXT("assign_exec_pin"), AssignExecPinName);

        if (!FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, ExecSourceNode, ExecSourcePinName, AssignNode, AssignExecPinName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to connect exec node '%s' pin '%s' to assign node pin '%s'"),
                    *ExecSourceNodeId, *ExecSourcePinName, *AssignExecPinName));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("assign_node_id"), AssignNode->NodeGuid.ToString());
    ResultObj->SetStringField(TEXT("delegate_name"), DelegateName);
    ResultObj->SetStringField(TEXT("target_class"), TargetClass->GetPathName());
    if (CreatedCustomEventNode)
    {
        ResultObj->SetStringField(TEXT("custom_event_node_id"), CreatedCustomEventNode->NodeGuid.ToString());
        ResultObj->SetStringField(TEXT("custom_event_name"), CreatedCustomEventNode->CustomFunctionName.ToString());
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create a JSON array for the node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;
    
    // Filter nodes by the exact requested type
    if (NodeType == TEXT("Event"))
    {
        FString EventName;
        if (!Params->TryGetStringField(TEXT("event_name"), EventName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter for Event node search"));
        }
        
        // Look for nodes with exact event name (e.g., ReceiveBeginPlay)
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
            if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
            {
                UE_LOG(LogTemp, Display, TEXT("Found event node with name %s: %s"), *EventName, *EventNode->NodeGuid.ToString());
                NodeGuidArray.Add(MakeShared<FJsonValueString>(EventNode->NodeGuid.ToString()));
            }
        }
    }
    // Add other node types as needed (InputAction, etc.)
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("node_guids"), NodeGuidArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintNodeCommands::HandleClearBlueprintEventGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    bool bKeepBoundEvents = false;
    if (Params->HasField(TEXT("keep_bound_events")))
    {
        bKeepBoundEvents = Params->GetBoolField(TEXT("keep_bound_events"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    int32 RemovedCount = 0;
    int32 KeptCount = 0;
    for (int32 NodeIndex = EventGraph->Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
    {
        UEdGraphNode* Node = EventGraph->Nodes[NodeIndex];
        if (!IsValid(Node))
        {
            continue;
        }

        if (bKeepBoundEvents && Cast<UK2Node_ComponentBoundEvent>(Node))
        {
            ++KeptCount;
            continue;
        }

        FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
        ++RemovedCount;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetNumberField(TEXT("removed_count"), RemovedCount);
    ResultObj->SetNumberField(TEXT("kept_count"), KeptCount);
    return ResultObj;
}
