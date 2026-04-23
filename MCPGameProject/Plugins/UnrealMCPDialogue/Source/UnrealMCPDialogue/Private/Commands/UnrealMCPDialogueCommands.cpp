#include "Commands/UnrealMCPDialogueCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "DialogueAsset.h"
#include "DialogueAssetFactory.h"
#include "DialogueLineIdConvention.h"
#include "DialogueLineTypes.h"
#include "DialogueNode.h"
#include "DialogueSettings.h"
#include "LineRegistry.h"
#include "StateGraphNode.h"
#include "DialogueEdgeTypes.h"
#include "DialogueGraphNode.h"
#include "DialogueGraphSchema.h"
#include "DialogueChoiceHelpers.h"
#include "StateGraphEdGraphNode.h"
#include "DialogueSpeakerAsset.h"
#include "DialogueConditionEvaluator.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

FUnrealMCPDialogueCommands::FUnrealMCPDialogueCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("get_dialogue_graph"))
		return HandleGetDialogueGraph(Params);
	if (CommandType == TEXT("get_dialogue_connections"))
		return HandleGetDialogueConnections(Params);
	if (CommandType == TEXT("create_dialogue_asset"))
		return HandleCreateDialogueAsset(Params);
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
	if (CommandType == TEXT("set_transition_condition"))
		return HandleSetTransitionCondition(Params);
	if (CommandType == TEXT("bind_dialogue_node_line"))
		return HandleBindDialogueNodeLine(Params);
	if (CommandType == TEXT("unbind_dialogue_node_line"))
		return HandleUnbindDialogueNodeLine(Params);
	if (CommandType == TEXT("query_dialogue_line"))
		return HandleQueryDialogueLine(Params);
	if (CommandType == TEXT("list_dialogue_lines"))
		return HandleListDialogueLines(Params);
	if (CommandType == TEXT("dialogue_registry_info"))
		return HandleDialogueRegistryInfo(Params);

	return FUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown dialogue command: %s"), *CommandType));
}

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

UStateGraphNode* FUnrealMCPDialogueCommands::FindNodeByGuid(
	UDialogueAsset* Asset, const FString& NodeIdStr)
{
	FGuid SearchId;
	if (!FGuid::Parse(NodeIdStr, SearchId))
	{
		return nullptr;
	}

	for (UStateGraphNode* Node : Asset->AllNodes)
	{
		if (Node && Node->NodeId == SearchId)
		{
			return Node;
		}
	}

	for (UStateGraphNode* Node : Asset->AllNodes)
	{
		if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
		{
			for (UDialogueChoiceItemNode* Item : Choice->Items)
			{
				if (Item && Item->NodeId == SearchId)
				{
					return Item;
				}
			}
		}
	}
	return nullptr;
}

UEdGraphPin* FUnrealMCPDialogueCommands::FindOutputPin(
	UDialogueAsset* Asset, UStateGraphNode* Node, const FString& PinName)
{
#if WITH_EDITORONLY_DATA
	if (!Asset->EditorGraph)
	{
		return nullptr;
	}

	UStateGraphNode* GraphOwnerRT = Node;
	if (UDialogueChoiceItemNode* Item = Cast<UDialogueChoiceItemNode>(Node))
	{
		for (UStateGraphNode* N : Asset->AllNodes)
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
		if (!DGN || DGN->RuntimeNode != GraphOwnerRT)
		{
			continue;
		}

		FName SearchName(*PinName);
		for (UEdGraphPin* Pin : DGN->Pins)
		{
			if (Pin->Direction == EGPD_Output && Pin->PinName == SearchName)
			{
				return Pin;
			}
		}
	}
#endif
	return nullptr;
}

UEdGraphPin* FUnrealMCPDialogueCommands::FindInputPin(
	UDialogueAsset* Asset, UStateGraphNode* Node)
{
#if WITH_EDITORONLY_DATA
	if (!Asset->EditorGraph)
	{
		return nullptr;
	}

	for (UEdGraphNode* GN : Asset->EditorGraph->Nodes)
	{
		UDialogueGraphNode* DGN = Cast<UDialogueGraphNode>(GN);
		if (!DGN || DGN->RuntimeNode != Node)
		{
			continue;
		}

		for (UEdGraphPin* Pin : DGN->Pins)
		{
			if (Pin->Direction == EGPD_Input)
			{
				return Pin;
			}
		}
	}
#endif
	return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleGetDialogueGraph(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'asset_path' parameter (e.g. \"/Game/Dialogues/DA_Test\")"));
	}

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {TEXT("asset_path")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	TArray<TSharedPtr<FJsonValue>> NodesArr;
	for (UStateGraphNode* Node : Asset->AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("node_id"), Node->NodeId.ToString());

		FString NodeType;
		if (Cast<UDialogueEntryNode>(Node))
		{
			NodeType = TEXT("Entry");
		}
		else if (Cast<UDialogueSpeechNode>(Node))
		{
			NodeType = TEXT("Speech");
		}
		else if (Cast<UDialogueChoiceNode>(Node))
		{
			NodeType = TEXT("Choice");
		}
		else if (Cast<UDialogueExitNode>(Node))
		{
			NodeType = TEXT("Exit");
		}
		else if (Cast<UDialogueConduitNode>(Node))
		{
			NodeType = TEXT("Conduit");
		}
		else if (Cast<UDialogueRerouteNode>(Node))
		{
			NodeType = TEXT("Reroute");
		}
		else
		{
			NodeType = TEXT("Unknown");
		}
		NodeObj->SetStringField(TEXT("node_type"), NodeType);

#if WITH_EDITORONLY_DATA
		TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
		PosObj->SetNumberField(TEXT("x"), Node->NodePosition.X);
		PosObj->SetNumberField(TEXT("y"), Node->NodePosition.Y);
		NodeObj->SetObjectField(TEXT("position"), PosObj);
#endif

		if (UDialogueSpeechNode* Speech = Cast<UDialogueSpeechNode>(Node))
		{
			NodeObj->SetStringField(TEXT("speaker_name"), Speech->GetDisplaySpeakerName().ToString());
			NodeObj->SetStringField(TEXT("speaker_asset_path"), Speech->Speaker ? Speech->Speaker->GetPathName() : TEXT(""));
			NodeObj->SetStringField(TEXT("dialogue_text"), Speech->DialogueText.ToString());
		}
		else if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
		{
			NodeObj->SetStringField(TEXT("speaker_name"), Choice->GetDisplaySpeakerName().ToString());
			NodeObj->SetStringField(TEXT("speaker_asset_path"), Choice->Speaker ? Choice->Speaker->GetPathName() : TEXT(""));
			NodeObj->SetStringField(TEXT("dialogue_text"), Choice->DialogueText.ToString());
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

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {TEXT("asset_path")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	TArray<TSharedPtr<FJsonValue>> ConnArr;

	for (UStateGraphNode* Node : Asset->AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		auto MakeConn = [&](const FString& FromPin, UStateGraphNode* Target)
		{
			if (!Target)
			{
				return;
			}
			TSharedPtr<FJsonObject> Conn = MakeShared<FJsonObject>();
			Conn->SetStringField(TEXT("from_node_id"), Node->NodeId.ToString());
			Conn->SetStringField(TEXT("from_pin"), FromPin);
			Conn->SetStringField(TEXT("to_node_id"), Target->NodeId.ToString());
			Conn->SetStringField(TEXT("to_pin"), TEXT("In"));
			ConnArr.Add(MakeShared<FJsonValueObject>(Conn));
		};

		if (UDialogueNodeWithTransitions* WithTrans = Cast<UDialogueNodeWithTransitions>(Node))
		{
			const FString PinName = TEXT("Out");
			for (const FDialogueTransition& Trans : WithTrans->OutTransitions)
			{
				MakeConn(PinName, Trans.TargetNode);
			}
		}
		else if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
		{
			for (int32 i = 0; i < Choice->Items.Num(); ++i)
			{
				if (!Choice->Items[i])
				{
					continue;
				}
				const FString PinName = FString::Printf(TEXT("Item_%d"), i);
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

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleCreateDialogueAsset(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {TEXT("asset_path")}))
		return Err;

	if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

#if WITH_EDITORONLY_DATA
	FString PackagePath;
	FString AssetName;
	int32 LastSlash;
	if (AssetPath.FindLastChar('/', LastSlash))
	{
		PackagePath = AssetPath.Left(LastSlash);
		AssetName = AssetPath.Mid(LastSlash + 1);
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid asset_path format"));
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UDialogueAssetFactory* Factory = NewObject<UDialogueAssetFactory>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDialogueAsset::StaticClass(), Factory);
	if (!NewAsset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create DialogueAsset"));
	}

	UDialogueAsset* Asset = Cast<UDialogueAsset>(NewAsset);
	if (!Asset)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Created asset is not a DialogueAsset"));
	}

	Asset->Modify();
	Asset->EditorGraph = NewObject<UEdGraph>(Asset, NAME_None, RF_Transactional);
	Asset->EditorGraph->Schema = UDialogueGraphSchema::StaticClass();

	UDialogueEntryNode* EntryRuntime = NewObject<UDialogueEntryNode>(Asset, NAME_None, RF_Transactional);
	EntryRuntime->NodeId = FGuid::NewGuid();
	EntryRuntime->NodePosition = FVector2D(100.f, 100.f);
	Asset->AllNodes.Add(EntryRuntime);
	Asset->EntryNode = EntryRuntime;

	UDialogueGraphNode* EntryGraphNode = NewObject<UDialogueGraphNode>(Asset->EditorGraph, NAME_None, RF_Transactional);
	EntryGraphNode->RuntimeNode = EntryRuntime;
	EntryGraphNode->CreateNewGuid();
	EntryGraphNode->PostPlacedNewNode();
	EntryGraphNode->NodePosX = 100.f;
	EntryGraphNode->NodePosY = 100.f;
	Asset->EditorGraph->AddNode(EntryGraphNode);
	EntryGraphNode->AllocateDefaultPins();

	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("entry_node_id"), EntryRuntime->NodeId.ToString());
	return Result;
#else
	return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Create requires editor build"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleAddDialogueNode(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString NodeType;
	if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' (Speech, Choice, Exit, Reroute)"));
	}

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {TEXT("asset_path"), TEXT("node_type"), TEXT("pos_x"), TEXT("pos_y")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

#if WITH_EDITORONLY_DATA
	if (!Asset->EditorGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset has no EditorGraph — open it in the editor first"));
	}

	TSubclassOf<UStateGraphNode> NodeClass;
	if (NodeType == TEXT("Speech"))
	{
		NodeClass = UDialogueSpeechNode::StaticClass();
	}
	else if (NodeType == TEXT("Choice"))
	{
		NodeClass = UDialogueChoiceNode::StaticClass();
	}
	else if (NodeType == TEXT("Exit"))
	{
		NodeClass = UDialogueExitNode::StaticClass();
	}
	else if (NodeType == TEXT("Reroute"))
	{
		NodeClass = UDialogueRerouteNode::StaticClass();
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid node_type '%s'. Must be Speech, Choice, Exit, or Reroute."), *NodeType));
	}

	double PosX = 0.0;
	double PosY = 0.0;
	Params->TryGetNumberField(TEXT("pos_x"), PosX);
	Params->TryGetNumberField(TEXT("pos_y"), PosY);

	UStateGraphNode* RuntimeNode = NewObject<UStateGraphNode>(Asset, NodeClass, NAME_None, RF_Transactional);
	RuntimeNode->NodeId = FGuid::NewGuid();
	RuntimeNode->NodePosition = FVector2D(PosX, PosY);

	if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(RuntimeNode))
	{
		UDialogueChoiceItemNode* Item = NewObject<UDialogueChoiceItemNode>(RuntimeNode, NAME_None, RF_Transactional);
		Item->NodeId = FGuid::NewGuid();
		Choice->Items.Add(Item);
	}

	Asset->AllNodes.Add(RuntimeNode);

	UDialogueGraphNode* GraphNode = NewObject<UDialogueGraphNode>(Asset->EditorGraph, NAME_None, RF_Transactional);
	GraphNode->RuntimeNode = RuntimeNode;
	GraphNode->CreateNewGuid();
	GraphNode->PostPlacedNewNode();
	GraphNode->NodePosX = PosX;
	GraphNode->NodePosY = PosY;
	Asset->EditorGraph->AddNode(GraphNode, true, false);
	GraphNode->AllocateDefaultPins();

	Asset->EditorGraph->NotifyGraphChanged();
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), RuntimeNode->NodeId.ToString());
	Result->SetStringField(TEXT("node_type"), NodeType);

	if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(RuntimeNode))
	{
		TArray<TSharedPtr<FJsonValue>> ItemIds;
		for (const TObjectPtr<UDialogueChoiceItemNode>& Item : Choice->Items)
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
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString NodeIdStr;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));
	}

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params,
		{TEXT("asset_path"), TEXT("node_id"), TEXT("speaker_name"), TEXT("speaker_asset_path"),
		 TEXT("dialogue_text"), TEXT("choice_text"), TEXT("pos_x"), TEXT("pos_y")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	UStateGraphNode* Node = FindNodeByGuid(Asset, NodeIdStr);
	if (!Node)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));
	}

	Node->Modify();

	// --- Speaker asset (Speech and Choice nodes) ---
	FString SpeakerPath;
	if (Params->TryGetStringField(TEXT("speaker_asset_path"), SpeakerPath))
	{
		UObject* SpeakerObj = UEditorAssetLibrary::LoadAsset(SpeakerPath);
		if (!SpeakerObj)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Could not load asset at '%s'"), *SpeakerPath));
		}
		UDialogueSpeakerAsset* SpeakerAsset = Cast<UDialogueSpeakerAsset>(SpeakerObj);
		if (!SpeakerAsset)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Asset at '%s' is not a UDialogueSpeakerAsset (class: %s)"),
					*SpeakerPath, *SpeakerObj->GetClass()->GetName()));
		}
		if (UDialogueSpeechNode* Speech = Cast<UDialogueSpeechNode>(Node))
		{
			Speech->Speaker = SpeakerAsset;
		}
		else if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
		{
			Choice->Speaker = SpeakerAsset;
		}
	}

	// --- dialogue_text (Speech and Choice nodes) ---
	FString DialogueTextVal;
	if (Params->TryGetStringField(TEXT("dialogue_text"), DialogueTextVal))
	{
		if (UDialogueSpeechNode* Speech = Cast<UDialogueSpeechNode>(Node))
		{
			Speech->DialogueText = FText::FromString(DialogueTextVal);
		}
		else if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
		{
			Choice->DialogueText = FText::FromString(DialogueTextVal);
		}
	}

	// --- choice_text (ChoiceItem only) ---
	if (UDialogueChoiceItemNode* Item = Cast<UDialogueChoiceItemNode>(Node))
	{
		FString ChoiceTextVal;
		if (Params->TryGetStringField(TEXT("choice_text"), ChoiceTextVal))
		{
			Item->ChoiceText = FText::FromString(ChoiceTextVal);
		}
	}

	double PosX;
	double PosY;
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
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString FromNodeId;
	FString ToNodeId;
	if (!Params->TryGetStringField(TEXT("from_node_id"), FromNodeId))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_node_id'"));
	}
	if (!Params->TryGetStringField(TEXT("to_node_id"), ToNodeId))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_node_id'"));
	}

	FString FromPin = TEXT("Out");
	Params->TryGetStringField(TEXT("from_pin"), FromPin);

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params,
		{TEXT("asset_path"), TEXT("from_node_id"), TEXT("to_node_id"), TEXT("from_pin")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	UStateGraphNode* FromNode = FindNodeByGuid(Asset, FromNodeId);
	if (!FromNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("from_node '%s' not found"), *FromNodeId));
	}

	UStateGraphNode* ToNode = FindNodeByGuid(Asset, ToNodeId);
	if (!ToNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("to_node '%s' not found"), *ToNodeId));
	}

#if WITH_EDITORONLY_DATA
	UEdGraphPin* OutPin = FindOutputPin(Asset, FromNode, FromPin);
	if (!OutPin)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Output pin '%s' not found on from_node"), *FromPin));
	}

	UEdGraphPin* InPin = FindInputPin(Asset, ToNode);
	if (!InPin)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Input pin not found on to_node"));
	}

	const UEdGraphSchema* Schema = Asset->EditorGraph->GetSchema();
	const bool bConnected = Schema->TryCreateConnection(OutPin, InPin);
	if (!bConnected)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("TryCreateConnection failed"));
	}

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
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString FromNodeId;
	FString ToNodeId;
	if (!Params->TryGetStringField(TEXT("from_node_id"), FromNodeId))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_node_id'"));
	}
	if (!Params->TryGetStringField(TEXT("to_node_id"), ToNodeId))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_node_id'"));
	}

	FString FromPin = TEXT("Out");
	Params->TryGetStringField(TEXT("from_pin"), FromPin);

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params,
		{TEXT("asset_path"), TEXT("from_node_id"), TEXT("to_node_id"), TEXT("from_pin")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	UStateGraphNode* FromNode = FindNodeByGuid(Asset, FromNodeId);
	if (!FromNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("from_node '%s' not found"), *FromNodeId));
	}

	UStateGraphNode* ToNode = FindNodeByGuid(Asset, ToNodeId);
	if (!ToNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("to_node '%s' not found"), *ToNodeId));
	}

#if WITH_EDITORONLY_DATA
	UEdGraphPin* OutPin = FindOutputPin(Asset, FromNode, FromPin);
	if (!OutPin)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Output pin '%s' not found on from_node"), *FromPin));
	}

	UEdGraphPin* InPin = FindInputPin(Asset, ToNode);
	if (!InPin)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Input pin not found on to_node"));
	}

	if (!OutPin->LinkedTo.Contains(InPin))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("These pins are not connected"));
	}

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
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString NodeIdStr;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));
	}

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {TEXT("asset_path"), TEXT("node_id")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	UStateGraphNode* Node = FindNodeByGuid(Asset, NodeIdStr);
	if (!Node)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));
	}

	if (Cast<UDialogueEntryNode>(Node))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Cannot delete Entry node"));
	}

	if (Cast<UDialogueChoiceItemNode>(Node))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Cannot delete ChoiceItem directly — delete the parent Choice node instead"));
	}

#if WITH_EDITORONLY_DATA
	if (!Asset->EditorGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset has no EditorGraph"));
	}

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
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Graph node not found for this runtime node"));
	}

	const UEdGraphSchema* Schema = Asset->EditorGraph->GetSchema();
	for (UEdGraphPin* Pin : GraphNodeToRemove->Pins)
	{
		Schema->BreakPinLinks(*Pin, true);
	}

	TSet<UStateGraphNode*> ValidNodes;
	for (UStateGraphNode* N : Asset->AllNodes)
	{
		if (N && N != Node)
		{
			ValidNodes.Add(N);
			if (UDialogueChoiceNode* C = Cast<UDialogueChoiceNode>(N))
			{
				for (auto& Item : C->Items)
				{
					if (Item)
					{
						ValidNodes.Add(Item.Get());
					}
				}
			}
		}
	}

	auto IsDangling = [&ValidNodes](const FDialogueTransition& T)
	{
		return !T.TargetNode || !ValidNodes.Contains(T.TargetNode.Get());
	};

	for (UStateGraphNode* OtherNode : Asset->AllNodes)
	{
		if (!OtherNode || OtherNode == Node)
		{
			continue;
		}
		if (UDialogueNodeWithTransitions* WithTrans = Cast<UDialogueNodeWithTransitions>(OtherNode))
		{
			const int32 Removed = WithTrans->OutTransitions.RemoveAll(IsDangling);
			if (Removed > 0)
			{
				WithTrans->Modify();
			}
		}
		if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(OtherNode))
		{
			for (UDialogueChoiceItemNode* Item : Choice->Items)
			{
				if (!Item)
				{
					continue;
				}
				const int32 Removed = Item->OutTransitions.RemoveAll(IsDangling);
				if (Removed > 0)
				{
					Item->Modify();
				}
			}
		}
	}

	Asset->EditorGraph->RemoveNode(GraphNodeToRemove);
	Asset->AllNodes.Remove(Node);

	if (Asset->EntryNode == Node)
	{
		Asset->EntryNode = nullptr;
	}

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
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString NodeIdStr;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' (Choice node GUID)"));
	}

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params,
		{TEXT("asset_path"), TEXT("node_id"), TEXT("choice_text")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	UStateGraphNode* Node = FindNodeByGuid(Asset, NodeIdStr);
	if (!Node)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));
	}

	UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node);
	if (!Choice)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Node is not a Choice node"));
	}

#if WITH_EDITORONLY_DATA
	if (!Asset->EditorGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset has no EditorGraph"));
	}

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
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Graph node not found for Choice"));
	}

	DialogueChoiceHelpers::AddChoiceItem(GraphNode);
	UDialogueChoiceItemNode* NewItem = Choice->Items.Last();

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

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleSetTransitionCondition(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));
	}

	FString FromNodeIdStr;
	if (!Params->TryGetStringField(TEXT("from_node_id"), FromNodeIdStr))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_node_id'"));
	}

	FString ToNodeIdStr;
	if (!Params->TryGetStringField(TEXT("to_node_id"), ToNodeIdStr))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'to_node_id'"));
	}

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params,
		{TEXT("asset_path"), TEXT("from_node_id"), TEXT("to_node_id"), TEXT("condition_class_path")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset)
	{
		return Error;
	}

	UStateGraphNode* FromNode = FindNodeByGuid(Asset, FromNodeIdStr);
	if (!FromNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Source node '%s' not found"), *FromNodeIdStr));
	}

	UDialogueNodeWithTransitions* TransNode = Cast<UDialogueNodeWithTransitions>(FromNode);
	if (!TransNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' does not support transitions"), *FromNodeIdStr));
	}

	UStateGraphNode* ToNode = FindNodeByGuid(Asset, ToNodeIdStr);
	if (!ToNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Target node '%s' not found"), *ToNodeIdStr));
	}

	// Find the transition matching TargetNode
	FDialogueTransition* FoundTransition = nullptr;
	for (FDialogueTransition& Trans : TransNode->OutTransitions)
	{
		if (Trans.TargetNode == ToNode)
		{
			FoundTransition = &Trans;
			break;
		}
	}

	if (!FoundTransition)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("No transition from '%s' to '%s'"), *FromNodeIdStr, *ToNodeIdStr));
	}

	TransNode->Modify();

	// Load condition class or clear it
	FString ConditionPath;
	if (Params->TryGetStringField(TEXT("condition_class_path"), ConditionPath) && !ConditionPath.IsEmpty())
	{
		UClass* CondClass = LoadClass<UDialogueConditionEvaluator>(nullptr, *ConditionPath);
		if (!CondClass)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Could not load condition class at '%s'"), *ConditionPath));
		}
		FoundTransition->ConditionClass = CondClass;
	}
	else
	{
		FoundTransition->ConditionClass = nullptr;
	}

	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("from_node_id"), FromNodeIdStr);
	Result->SetStringField(TEXT("to_node_id"), ToNodeIdStr);
	Result->SetStringField(TEXT("condition_class"),
		FoundTransition->ConditionClass ? FoundTransition->ConditionClass->GetPathName() : TEXT(""));
	return Result;
}

// ============================================================================
// MCP-3: Line ID commands
// Mirror the editor GUI "Pick Line..." / "Unbind" flow through TCP so automation
// and live smoke can exercise Line binding without clicking Details panels.
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleBindDialogueNodeLine(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

	FString NodeIdStr;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

	FString LineIdStr;
	if (!Params->TryGetStringField(TEXT("line_id"), LineIdStr))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'line_id'"));

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params,
		{TEXT("asset_path"), TEXT("node_id"), TEXT("line_id")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset) return Error;

	UStateGraphNode* Node = FindNodeByGuid(Asset, NodeIdStr);
	if (!Node)
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));

	// ChoiceItem cannot be directly bound — mirrors the GUI rule: ChoiceItem
	// LineIds are always set via their parent Choice's batch fill.
	if (Cast<UDialogueChoiceItemNode>(Node))
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("ChoiceItem nodes cannot be bound directly; bind the parent Choice instead."));

	const FName LineId(*LineIdStr);

	// Speech: simple LineId assignment.
	if (UDialogueSpeechNode* Speech = Cast<UDialogueSpeechNode>(Node))
	{
		Speech->Modify();
		Speech->LineId = LineId;
		Asset->MarkPackageDirty();

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("node_id"), NodeIdStr);
		Result->SetStringField(TEXT("line_id"), LineIdStr);
		Result->SetStringField(TEXT("node_type"), TEXT("Speech"));
		Result->SetNumberField(TEXT("items_filled"), 0);
		return Result;
	}

	// Choice: set LineId + batch-fill ChoiceItems under that Choice (matches
	// DialogueNodeLineIdCustomization's Pick callback for the Choice kind).
	if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
	{
		// Resolve the child ChoiceItem LineIds from the Registry using the
		// project-wide convention helper.
		ULineRegistry* Reg = ULineRegistry::Get();
		if (!Reg)
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("ULineRegistry unavailable (engine subsystem not yet initialized?)"));

		TMap<FName, FDialogueLineRow> AllLines;
		Reg->GetAllLines(AllLines);

		TArray<FName> ItemIds;
		for (const TPair<FName, FDialogueLineRow>& Pair : AllLines)
		{
			if (Pair.Value.LineType != FName(TEXT("ChoiceItem"))) continue;
			if (!FDialogueLineIdConvention::IsChoiceItemOf(Pair.Key, LineId)) continue;
			ItemIds.Add(Pair.Key);
		}
		ItemIds.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

		// Find the Editor GraphNode so ReplaceChoiceItemsWithLineIds can rebuild pins.
#if WITH_EDITORONLY_DATA
		UStateGraphEdGraphNode* GraphNode = nullptr;
		if (Asset->EditorGraph)
		{
			for (UEdGraphNode* GN : Asset->EditorGraph->Nodes)
			{
				if (UStateGraphEdGraphNode* SG = Cast<UStateGraphEdGraphNode>(GN))
				{
					if (SG->RuntimeNode == Choice) { GraphNode = SG; break; }
				}
			}
		}
		if (!GraphNode)
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Could not find EdGraphNode for this Choice; cannot rebuild pins"));

		Choice->Modify();
		Choice->LineId = LineId;
		const int32 Filled =
			DialogueChoiceHelpers::ReplaceChoiceItemsWithLineIds(GraphNode, ItemIds);

		Asset->MarkPackageDirty();

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("node_id"), NodeIdStr);
		Result->SetStringField(TEXT("line_id"), LineIdStr);
		Result->SetStringField(TEXT("node_type"), TEXT("Choice"));
		Result->SetNumberField(TEXT("items_filled"), Filled);

		TArray<TSharedPtr<FJsonValue>> ItemIdArr;
		for (const FName& Id : ItemIds)
		{
			ItemIdArr.Add(MakeShared<FJsonValueString>(Id.ToString()));
		}
		Result->SetArrayField(TEXT("item_line_ids"), ItemIdArr);
		return Result;
#else
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Choice batch-fill requires editor build"));
#endif
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Node type '%s' does not carry a LineId"),
			*Node->GetClass()->GetName()));
#else
	return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Line commands require editor build"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleUnbindDialogueNodeLine(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path'"));

	FString NodeIdStr;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params,
		{TEXT("asset_path"), TEXT("node_id")}))
		return Err;

	TSharedPtr<FJsonObject> Error;
	UDialogueAsset* Asset = LoadDialogueAsset(AssetPath, Error);
	if (!Asset) return Error;

	UStateGraphNode* Node = FindNodeByGuid(Asset, NodeIdStr);
	if (!Node)
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found"), *NodeIdStr));

	// ChoiceItem cannot be directly unbound — same rule as bind.
	if (Cast<UDialogueChoiceItemNode>(Node))
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("ChoiceItem nodes cannot be unbound directly; unbind the parent Choice."));

	int32 CascadeCleared = 0;

	if (UDialogueSpeechNode* Speech = Cast<UDialogueSpeechNode>(Node))
	{
		Speech->Modify();
		Speech->LineId = NAME_None;
	}
	else if (UDialogueChoiceNode* Choice = Cast<UDialogueChoiceNode>(Node))
	{
		// Cascade: clear LineId on every ChoiceItem (pins/Condition/Callback preserved).
		// Mirrors the GUI Unbind button's cascade behavior for Choice nodes.
		Choice->Modify();
		Choice->LineId = NAME_None;
		for (UDialogueChoiceItemNode* Item : Choice->Items)
		{
			if (!Item || Item->LineId.IsNone()) continue;
			Item->Modify();
			Item->LineId = NAME_None;
			++CascadeCleared;
		}
	}
	else
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node type '%s' does not carry a LineId"),
				*Node->GetClass()->GetName()));
	}

	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("node_id"), NodeIdStr);
	Result->SetNumberField(TEXT("choice_items_cleared"), CascadeCleared);
	return Result;
#else
	return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Line commands require editor build"));
#endif
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleQueryDialogueLine(
	const TSharedPtr<FJsonObject>& Params)
{
	FString LineIdStr;
	if (!Params->TryGetStringField(TEXT("line_id"), LineIdStr))
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'line_id'"));

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {TEXT("line_id")}))
		return Err;

	ULineRegistry* Reg = ULineRegistry::Get();
	if (!Reg)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ULineRegistry unavailable"));

	FDialogueLineRow Row;
	const bool bHit = Reg->GetLineRow(FName(*LineIdStr), Row);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("found"), bHit);
	Result->SetStringField(TEXT("line_id"), LineIdStr);
	if (bHit)
	{
		Result->SetStringField(TEXT("line_type"), Row.LineType.ToString());
		Result->SetStringField(TEXT("dialogue_id"), Row.DialogueID.ToString());
		Result->SetStringField(TEXT("speaker_id"), Row.SpeakerID.ToString());
		Result->SetStringField(TEXT("text"), Row.Text.ToString());
		Result->SetStringField(TEXT("notes"), Row.Notes);
		TArray<TSharedPtr<FJsonValue>> NextArr;
		for (const FName& N : Row.NextNodes)
			NextArr.Add(MakeShared<FJsonValueString>(N.ToString()));
		Result->SetArrayField(TEXT("next_nodes"), NextArr);

		// Include Speaker display name (joined lookup) so callers don't have to
		// make a second round trip for the common case.
		const FText SpName = Reg->GetSpeakerDisplayName(Row.SpeakerID);
		Result->SetStringField(TEXT("speaker_display_name"), SpName.ToString());
	}
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleListDialogueLines(
	const TSharedPtr<FJsonObject>& Params)
{
	FString DialogueIdFilter;
	const bool bHasFilter = Params->TryGetStringField(TEXT("dialogue_id"), DialogueIdFilter);

	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {TEXT("dialogue_id")}))
		return Err;

	ULineRegistry* Reg = ULineRegistry::Get();
	if (!Reg)
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ULineRegistry unavailable"));

	TMap<FName, FDialogueLineRow> AllLines;
	Reg->GetAllLines(AllLines);

	// Sort for stable output.
	TArray<FName> Keys;
	AllLines.GetKeys(Keys);
	Keys.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

	TArray<TSharedPtr<FJsonValue>> LinesArr;
	for (const FName& K : Keys)
	{
		const FDialogueLineRow& Row = AllLines[K];
		if (bHasFilter && Row.DialogueID != FName(*DialogueIdFilter)) continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("line_id"), K.ToString());
		Obj->SetStringField(TEXT("line_type"), Row.LineType.ToString());
		Obj->SetStringField(TEXT("dialogue_id"), Row.DialogueID.ToString());
		Obj->SetStringField(TEXT("speaker_id"), Row.SpeakerID.ToString());
		Obj->SetStringField(TEXT("text"), Row.Text.ToString());
		LinesArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	if (bHasFilter) Result->SetStringField(TEXT("dialogue_id_filter"), DialogueIdFilter);
	Result->SetNumberField(TEXT("count"), LinesArr.Num());
	Result->SetArrayField(TEXT("lines"), LinesArr);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleDialogueRegistryInfo(
	const TSharedPtr<FJsonObject>& Params)
{
	if (auto Err = FUnrealMCPCommonUtils::CheckUnknownParams(Params, {}))
		return Err;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	ULineRegistry* Reg = ULineRegistry::Get();
	Result->SetBoolField(TEXT("registry_available"), Reg != nullptr);

	const UDialogueSettings* Settings = GetDefault<UDialogueSettings>();
	Result->SetStringField(TEXT("line_database_path"),
		(Settings && !Settings->LineDatabase.IsNull())
			? Settings->LineDatabase.ToString()
			: FString());

	if (Reg)
	{
		// Triggers EnsureFullCache on first call; subsequent calls are O(1).
		Result->SetNumberField(TEXT("line_count"), Reg->GetLineCount());
		Result->SetNumberField(TEXT("speaker_count"), Reg->GetSpeakerCount());
	}
	return Result;
}
