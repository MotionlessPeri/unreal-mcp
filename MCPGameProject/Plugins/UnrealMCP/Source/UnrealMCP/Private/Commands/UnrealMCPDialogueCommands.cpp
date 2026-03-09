#include "Commands/UnrealMCPDialogueCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "EditorAssetLibrary.h"
#include "DialogueAsset.h"
#include "DialogueNode.h"
#include "DialogueEdgeTypes.h"
#include "DialogueGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

FUnrealMCPDialogueCommands::FUnrealMCPDialogueCommands()
{
}

// ============================================================================
// Command dispatcher
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    // MCP-1: Read
    if (CommandType == TEXT("get_dialogue_graph"))
        return HandleGetDialogueGraph(Params);
    if (CommandType == TEXT("get_dialogue_connections"))
        return HandleGetDialogueConnections(Params);

    // MCP-2: Write
    if (CommandType == TEXT("add_dialogue_node"))
        return HandleAddDialogueNode(Params);
    if (CommandType == TEXT("set_dialogue_node_properties"))
        return HandleSetDialogueNodeProperties(Params);
    if (CommandType == TEXT("connect_dialogue_nodes"))
        return HandleConnectDialogueNodes(Params);
    if (CommandType == TEXT("disconnect_dialogue_nodes"))
        return HandleDisconnectDialogueNodes(Params);
    if (CommandType == TEXT("delete_dialogue_node"))
        return HandleDeleteDialogueNode(Params);
    if (CommandType == TEXT("add_dialogue_choice_item"))
        return HandleAddDialogueChoiceItem(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown dialogue command: %s"), *CommandType));
}

// ============================================================================
// Helpers
// ============================================================================

UDialogueAsset* FUnrealMCPDialogueCommands::LoadDialogueAsset(
    const FString& AssetPath, TSharedPtr<FJsonObject>& OutError)
{
    UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!LoadedObj)
    {
        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load asset at path: %s"), *AssetPath));
        return nullptr;
    }
    UDialogueAsset* Asset = Cast<UDialogueAsset>(LoadedObj);
    if (!Asset)
    {
        OutError = FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset at '%s' is not a UDialogueAsset (class: %s)"),
                *AssetPath, *LoadedObj->GetClass()->GetName()));
        return nullptr;
    }
    return Asset;
}

UDialogueNode* FUnrealMCPDialogueCommands::FindNodeByGuid(
    UDialogueAsset* Asset, const FString& NodeIdStr)
{
    FGuid SearchId;
    if (!FGuid::Parse(NodeIdStr, SearchId))
        return nullptr;

    for (UDialogueNode* Node : Asset->AllNodes)
    {
        if (Node && Node->NodeId == SearchId)
            return Node;
    }

    // Also search ChoiceItems (not in AllNodes)
    for (UDialogueNode* Node : Asset->AllNodes)
    {
        if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
        {
            for (UDialogueChoiceItemNode* Item : Choice->Items)
            {
                if (Item && Item->NodeId == SearchId)
                    return Item;
            }
        }
    }
    return nullptr;
}

UEdGraphPin* FUnrealMCPDialogueCommands::FindOutputPin(
    UDialogueAsset* Asset, UDialogueNode* Node, const FString& PinName)
{
#if WITH_EDITORONLY_DATA
    if (!Asset->EditorGraph) return nullptr;

    // For ChoiceItem nodes, find the parent Choice's graph node
    UDialogueNode* GraphOwnerRT = Node;
    if (UDialogueChoiceItemNode* Item = Cast<UDialogueChoiceItemNode>(Node))
    {
        // Find parent ChoiceNode
        for (UDialogueNode* N : Asset->AllNodes)
        {
            if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(N))
            {
                if (Choice->Items.Contains(Item))
                {
                    GraphOwnerRT = Choice;
                    break;
                }
            }
        }
    }

    for (UEdGraphNode* GN : Asset->EditorGraph->Nodes)
    {
        UDialogueGraphNode* DGN = Cast<UDialogueGraphNode>(GN);
        if (!DGN || DGN->RuntimeNode != GraphOwnerRT) continue;

        FName SearchName(*PinName);
        for (UEdGraphPin* Pin : DGN->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinName == SearchName)
                return Pin;
        }
    }
#endif
    return nullptr;
}

UEdGraphPin* FUnrealMCPDialogueCommands::FindInputPin(
    UDialogueAsset* Asset, UDialogueNode* Node)
{
#if WITH_EDITORONLY_DATA
    if (!Asset->EditorGraph) return nullptr;
    for (UEdGraphNode* GN : Asset->EditorGraph->Nodes)
    {
        UDialogueGraphNode* DGN = Cast<UDialogueGraphNode>(GN);
        if (!DGN || DGN->RuntimeNode != Node) continue;

        for (UEdGraphPin* Pin : DGN->Pins)
        {
            if (Pin->Direction == EGPD_Input)
                return Pin;
        }
    }
#endif
    return nullptr;
}

// ============================================================================
// MCP-1: Read commands
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleGetDialogueGraph(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'asset_path' parameter (e.g. \"/Game/Dialogues/DA_Test\")"));
    }

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

    TArray<TSharedPtr<FJsonValue>> NodesArr;
    for (UDialogueNode* Node : Asset->AllNodes)
    {
        if (!Node) continue;

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("node_id"), Node->NodeId.ToString());

        FString NodeType;
        if (Cast<UDialogueEntryNode>(Node))       NodeType = TEXT("Entry");
        else if (Cast<UDialogueSpeechNode>(Node)) NodeType = TEXT("Speech");
        else if (Cast<UDialogueChoiceNode>(Node)) NodeType = TEXT("Choice");
        else if (Cast<UDialogueExitNode>(Node))   NodeType = TEXT("Exit");
        else                                       NodeType = TEXT("Unknown");
        NodeObj->SetStringField(TEXT("node_type"), NodeType);

#if WITH_EDITORONLY_DATA
        TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
        PosObj->SetNumberField(TEXT("x"), Node->NodePosition.X);
        PosObj->SetNumberField(TEXT("y"), Node->NodePosition.Y);
        NodeObj->SetObjectField(TEXT("position"), PosObj);
#endif

        if (UDialogueSpeechNode* Speech = Cast<UDialogueSpeechNode>(Node))
        {
            NodeObj->SetStringField(TEXT("speaker_name"), Speech->SpeakerName.ToString());
            NodeObj->SetStringField(TEXT("dialogue_text"), Speech->DialogueText.ToString());
        }
        else if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
        {
            TArray<TSharedPtr<FJsonValue>> ChoicesArr;
            for (const TObjectPtr<UDialogueChoiceItemNode>& Item : Choice->Items)
            {
                TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
                ItemObj->SetStringField(TEXT("text"), Item ? Item->ChoiceText.ToString() : TEXT(""));
                ItemObj->SetStringField(TEXT("item_node_id"), Item ? Item->NodeId.ToString() : TEXT(""));
                ChoicesArr.Add(MakeShared<FJsonValueObject>(ItemObj));
            }
            NodeObj->SetArrayField(TEXT("choices"), ChoicesArr);
        }

        NodesArr.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("asset_name"), Asset->GetName());
    Result->SetArrayField(TEXT("nodes"), NodesArr);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleGetDialogueConnections(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'asset_path' parameter (e.g. \"/Game/Dialogues/DA_Test\")"));
    }

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

    TArray<TSharedPtr<FJsonValue>> ConnArr;

    for (UDialogueNode* Node : Asset->AllNodes)
    {
        if (!Node) continue;

        auto MakeConn = [&](const FString& FromPin, UDialogueNode* Target)
        {
            if (!Target) return;
            TSharedPtr<FJsonObject> Conn = MakeShared<FJsonObject>();
            Conn->SetStringField(TEXT("from_node_id"), Node->NodeId.ToString());
            Conn->SetStringField(TEXT("from_pin"), FromPin);
            Conn->SetStringField(TEXT("to_node_id"), Target->NodeId.ToString());
            Conn->SetStringField(TEXT("to_pin"), TEXT("In"));
            ConnArr.Add(MakeShared<FJsonValueObject>(Conn));
        };

        if (UDialogueNodeWithTransitions* WithTrans = Cast<UDialogueNodeWithTransitions>(Node))
        {
            FString PinName = TEXT("Out");
            for (const FDialogueTransition& Trans : WithTrans->OutTransitions)
            {
                MakeConn(PinName, Trans.TargetNode);
            }
        }
        else if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
        {
            for (int32 i = 0; i < Choice->Items.Num(); ++i)
            {
                if (!Choice->Items[i]) continue;
                FString PinName = FString::Printf(TEXT("Item_%d"), i);
                for (const FDialogueTransition& Trans : Choice->Items[i]->OutTransitions)
                {
                    MakeConn(PinName, Trans.TargetNode);
                }
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetArrayField(TEXT("connections"), ConnArr);
    return Result;
}

// ============================================================================
// MCP-2: Write commands
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleAddDialogueNode(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' (Speech, Choice, Exit)"));

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

#if WITH_EDITORONLY_DATA
    if (!Asset->EditorGraph)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset has no EditorGraph — open it in the editor first"));

    // Resolve class
    TSubclassOf<UDialogueNode> NodeClass;
    if (NodeType == TEXT("Speech"))      NodeClass = UDialogueSpeechNode::StaticClass();
    else if (NodeType == TEXT("Choice")) NodeClass = UDialogueChoiceNode::StaticClass();
    else if (NodeType == TEXT("Exit"))   NodeClass = UDialogueExitNode::StaticClass();
    else
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Invalid node_type '%s'. Must be Speech, Choice, or Exit."), *NodeType));

    // Position
    double PosX = 0, PosY = 0;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);

    // Create runtime node
    UDialogueNode* RuntimeNode = NewObject<UDialogueNode>(Asset, NodeClass);
    RuntimeNode->NodeId = FGuid::NewGuid();
    RuntimeNode->NodePosition = FVector2D(PosX, PosY);

    // ChoiceNode: create 1 default ChoiceItem
    if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(RuntimeNode))
    {
        UDialogueChoiceItemNode* Item = NewObject<UDialogueChoiceItemNode>(RuntimeNode);
        Item->NodeId = FGuid::NewGuid();
        Choice->Items.Add(Item);
    }

    Asset->AllNodes.Add(RuntimeNode);

    // Create graph node
    UDialogueGraphNode* GraphNode = NewObject<UDialogueGraphNode>(Asset->EditorGraph);
    GraphNode->RuntimeNode = RuntimeNode;
    GraphNode->CreateNewGuid();
    GraphNode->PostPlacedNewNode();
    GraphNode->NodePosX = PosX;
    GraphNode->NodePosY = PosY;
    Asset->EditorGraph->AddNode(GraphNode, true, false);
    GraphNode->AllocateDefaultPins();

    Asset->EditorGraph->NotifyGraphChanged();
    Asset->MarkPackageDirty();

    // Build result
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("node_id"), RuntimeNode->NodeId.ToString());
    Result->SetStringField(TEXT("node_type"), NodeType);

    // For Choice, also return the default item's node_id
    if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(RuntimeNode))
    {
        TArray<TSharedPtr<FJsonValue>> ItemIds;
        for (const auto& Item : Choice->Items)
        {
            ItemIds.Add(MakeShared<FJsonValueString>(Item->NodeId.ToString()));
        }
        Result->SetArrayField(TEXT("choice_item_node_ids"), ItemIds);
    }

    return Result;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Write commands require editor build"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleSetDialogueNodeProperties(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

    FString NodeIdStr;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

    UDialogueNode* Node = FindNodeByGuid(Asset, NodeIdStr);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));

    Node->Modify();

    // Speech properties
    if (UDialogueSpeechNode* Speech = Cast<UDialogueSpeechNode>(Node))
    {
        FString Val;
        if (Params->TryGetStringField(TEXT("speaker_name"), Val))
            Speech->SpeakerName = FText::FromString(Val);
        if (Params->TryGetStringField(TEXT("dialogue_text"), Val))
            Speech->DialogueText = FText::FromString(Val);
    }
    // ChoiceItem properties
    else if (UDialogueChoiceItemNode* Item = Cast<UDialogueChoiceItemNode>(Node))
    {
        FString Val;
        if (Params->TryGetStringField(TEXT("choice_text"), Val))
            Item->ChoiceText = FText::FromString(Val);
    }
    // Position (any node)
    double PosX, PosY;
    bool bPosChanged = false;
#if WITH_EDITORONLY_DATA
    if (Params->TryGetNumberField(TEXT("pos_x"), PosX))
    {
        Node->NodePosition.X = PosX;
        bPosChanged = true;
    }
    if (Params->TryGetNumberField(TEXT("pos_y"), PosY))
    {
        Node->NodePosition.Y = PosY;
        bPosChanged = true;
    }

    // Sync graph node position
    if (bPosChanged && Asset->EditorGraph)
    {
        for (UEdGraphNode* GN : Asset->EditorGraph->Nodes)
        {
            UDialogueGraphNode* DGN = Cast<UDialogueGraphNode>(GN);
            if (DGN && DGN->RuntimeNode == Node)
            {
                DGN->NodePosX = Node->NodePosition.X;
                DGN->NodePosY = Node->NodePosition.Y;
                break;
            }
        }
    }
#endif

    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("node_id"), NodeIdStr);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleConnectDialogueNodes(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

    FString FromNodeId, ToNodeId;
    if (!Params->TryGetStringField(TEXT("from_node_id"), FromNodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_node_id'"));
    if (!Params->TryGetStringField(TEXT("to_node_id"), ToNodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_node_id'"));

    // from_pin defaults to "Out" for Entry/Speech, required for Choice (e.g. "Item_0")
    FString FromPin = TEXT("Out");
    Params->TryGetStringField(TEXT("from_pin"), FromPin);

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

    UDialogueNode* FromNode = FindNodeByGuid(Asset, FromNodeId);
    if (!FromNode)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("from_node '%s' not found"), *FromNodeId));

    UDialogueNode* ToNode = FindNodeByGuid(Asset, ToNodeId);
    if (!ToNode)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("to_node '%s' not found"), *ToNodeId));

#if WITH_EDITORONLY_DATA
    UEdGraphPin* OutPin = FindOutputPin(Asset, FromNode, FromPin);
    if (!OutPin)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Output pin '%s' not found on from_node"), *FromPin));

    UEdGraphPin* InPin = FindInputPin(Asset, ToNode);
    if (!InPin)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Input pin not found on to_node"));

    // Use Schema's TryCreateConnection — keeps visual + runtime in sync
    const UEdGraphSchema* Schema = Asset->EditorGraph->GetSchema();
    bool bConnected = Schema->TryCreateConnection(OutPin, InPin);
    if (!bConnected)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("TryCreateConnection failed"));

    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("from_node_id"), FromNodeId);
    Result->SetStringField(TEXT("from_pin"), FromPin);
    Result->SetStringField(TEXT("to_node_id"), ToNodeId);
    return Result;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Write commands require editor build"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleDisconnectDialogueNodes(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

    FString FromNodeId, ToNodeId;
    if (!Params->TryGetStringField(TEXT("from_node_id"), FromNodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_node_id'"));
    if (!Params->TryGetStringField(TEXT("to_node_id"), ToNodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_node_id'"));

    FString FromPin = TEXT("Out");
    Params->TryGetStringField(TEXT("from_pin"), FromPin);

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

    UDialogueNode* FromNode = FindNodeByGuid(Asset, FromNodeId);
    if (!FromNode)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("from_node '%s' not found"), *FromNodeId));

    UDialogueNode* ToNode = FindNodeByGuid(Asset, ToNodeId);
    if (!ToNode)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("to_node '%s' not found"), *ToNodeId));

#if WITH_EDITORONLY_DATA
    UEdGraphPin* OutPin = FindOutputPin(Asset, FromNode, FromPin);
    if (!OutPin)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Output pin '%s' not found on from_node"), *FromPin));

    UEdGraphPin* InPin = FindInputPin(Asset, ToNode);
    if (!InPin)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Input pin not found on to_node"));

    // Verify they are actually linked
    if (!OutPin->LinkedTo.Contains(InPin))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("These pins are not connected"));

    // Use Schema's BreakSinglePinLink — keeps visual + runtime in sync
    const UEdGraphSchema* Schema = Asset->EditorGraph->GetSchema();
    Schema->BreakSinglePinLink(OutPin, InPin);

    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("from_node_id"), FromNodeId);
    Result->SetStringField(TEXT("to_node_id"), ToNodeId);
    return Result;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Write commands require editor build"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleDeleteDialogueNode(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

    FString NodeIdStr;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

    UDialogueNode* Node = FindNodeByGuid(Asset, NodeIdStr);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));

    // Cannot delete Entry node
    if (Cast<UDialogueEntryNode>(Node))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot delete Entry node"));

    // Cannot delete ChoiceItem directly (use set_dialogue_node_properties or delete parent Choice)
    if (Cast<UDialogueChoiceItemNode>(Node))
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Cannot delete ChoiceItem directly — delete the parent Choice node instead"));

#if WITH_EDITORONLY_DATA
    if (!Asset->EditorGraph)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset has no EditorGraph"));

    // Find and remove the graph node
    UDialogueGraphNode* GraphNodeToRemove = nullptr;
    for (UEdGraphNode* GN : Asset->EditorGraph->Nodes)
    {
        UDialogueGraphNode* DGN = Cast<UDialogueGraphNode>(GN);
        if (DGN && DGN->RuntimeNode == Node)
        {
            GraphNodeToRemove = DGN;
            break;
        }
    }

    if (!GraphNodeToRemove)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Graph node not found for this runtime node"));

    // Break all pin links (this cleans up OutTransitions via Schema)
    const UEdGraphSchema* Schema = Asset->EditorGraph->GetSchema();
    for (UEdGraphPin* Pin : GraphNodeToRemove->Pins)
    {
        Schema->BreakPinLinks(*Pin, true);
    }

    // Sweep ALL OutTransitions in the asset to remove references to the deleted node.
    // This handles stale data from pre-bugfix assets where pin links were broken
    // but OutTransitions still contain dangling references.
    for (UDialogueNode* OtherNode : Asset->AllNodes)
    {
        if (!OtherNode || OtherNode == Node) continue;
        if (UDialogueNodeWithTransitions* WithTrans = Cast<UDialogueNodeWithTransitions>(OtherNode))
        {
            int32 Removed = WithTrans->OutTransitions.RemoveAll(
                [Node](const FDialogueTransition& T){ return T.TargetNode == Node; });
            if (Removed > 0) WithTrans->Modify();
        }
        if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(OtherNode))
        {
            for (UDialogueChoiceItemNode* Item : Choice->Items)
            {
                if (!Item) continue;
                int32 Removed = Item->OutTransitions.RemoveAll(
                    [Node](const FDialogueTransition& T){ return T.TargetNode == Node; });
                if (Removed > 0) Item->Modify();
            }
        }
    }

    // Remove from graph
    Asset->EditorGraph->RemoveNode(GraphNodeToRemove);

    // Remove from AllNodes
    Asset->AllNodes.Remove(Node);

    // If this was referenced as EntryNode (shouldn't happen due to guard above, but safety)
    if (Asset->EntryNode == Node)
        Asset->EntryNode = nullptr;

    Asset->EditorGraph->NotifyGraphChanged();
    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("deleted_node_id"), NodeIdStr);
    return Result;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Write commands require editor build"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleAddDialogueChoiceItem(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

    FString NodeIdStr;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' (Choice node GUID)"));

    TSharedPtr<FJsonObject> Error;
    UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
    if (!Asset) return Error;

    UDialogueNode* Node = FindNodeByGuid(Asset, NodeIdStr);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));

    UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node);
    if (!Choice)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Node is not a Choice node"));

#if WITH_EDITORONLY_DATA
    if (!Asset->EditorGraph)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset has no EditorGraph"));

    // Find the graph node for this Choice
    UDialogueGraphNode* GraphNode = nullptr;
    for (UEdGraphNode* GN : Asset->EditorGraph->Nodes)
    {
        UDialogueGraphNode* DGN = Cast<UDialogueGraphNode>(GN);
        if (DGN && DGN->RuntimeNode == Choice)
        {
            GraphNode = DGN;
            break;
        }
    }
    if (!GraphNode)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Graph node not found for Choice"));

    // Use GraphNode::AddChoiceItem — creates ChoiceItemNode + reconstructs pins
    GraphNode->AddChoiceItem();

    // The new item is the last one
    UDialogueChoiceItemNode* NewItem = Choice->Items.Last();

    // Set optional choice_text
    FString ChoiceText;
    if (Params->TryGetStringField(TEXT("choice_text"), ChoiceText))
    {
        NewItem->ChoiceText = FText::FromString(ChoiceText);
    }

    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("item_node_id"), NewItem->NodeId.ToString());
    Result->SetNumberField(TEXT("item_index"), Choice->Items.Num() - 1);
    return Result;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Write commands require editor build"));
#endif
}
