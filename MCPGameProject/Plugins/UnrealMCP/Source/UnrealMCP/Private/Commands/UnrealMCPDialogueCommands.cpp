#include "Commands/UnrealMCPDialogueCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "EditorAssetLibrary.h"
#include "DialogueAsset.h"
#include "DialogueNode.h"
#include "DialogueEdgeTypes.h"

FUnrealMCPDialogueCommands::FUnrealMCPDialogueCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPDialogueCommands::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_dialogue_graph"))
    {
        return HandleGetDialogueGraph(Params);
    }
    if (CommandType == TEXT("get_dialogue_connections"))
    {
        return HandleGetDialogueConnections(Params);
    }
    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown dialogue command: %s"), *CommandType));
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

    UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!LoadedObj)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load asset at path: %s"), *AssetPath));
    }

    UDialogueAsset* Asset = Cast<UDialogueAsset>(LoadedObj);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset at '%s' is not a UDialogueAsset (class: %s)"),
                *AssetPath, *LoadedObj->GetClass()->GetName()));
    }

    TArray<TSharedPtr<FJsonValue>> NodesArr;
    for (UDialogueNode* Node : Asset->AllNodes)
    {
        if (!Node) continue;

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("node_id"), Node->GetName());

        // Determine node_type
        FString NodeType;
        if (Cast<UDialogueEntryNode>(Node))       NodeType = TEXT("Entry");
        else if (Cast<UDialogueSpeechNode>(Node)) NodeType = TEXT("Speech");
        else if (Cast<UDialogueChoiceNode>(Node)) NodeType = TEXT("Choice");
        else if (Cast<UDialogueExitNode>(Node))   NodeType = TEXT("Exit");
        else                                       NodeType = TEXT("Unknown");
        NodeObj->SetStringField(TEXT("node_type"), NodeType);

        // Position (editor-only, may be zero if not set)
#if WITH_EDITORONLY_DATA
        TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
        PosObj->SetNumberField(TEXT("x"), Node->NodePosition.X);
        PosObj->SetNumberField(TEXT("y"), Node->NodePosition.Y);
        NodeObj->SetObjectField(TEXT("position"), PosObj);
#endif

        // Type-specific fields
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
                ChoicesArr.Add(MakeShared<FJsonValueString>(Item ? Item->ChoiceText.ToString() : TEXT("")));
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

    UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!LoadedObj)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load asset at path: %s"), *AssetPath));
    }

    UDialogueAsset* Asset = Cast<UDialogueAsset>(LoadedObj);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset at '%s' is not a UDialogueAsset (class: %s)"),
                *AssetPath, *LoadedObj->GetClass()->GetName()));
    }

    TArray<TSharedPtr<FJsonValue>> ConnArr;

    for (UDialogueNode* Node : Asset->AllNodes)
    {
        if (!Node) continue;

        auto MakeConn = [&](const FString& FromPin, UDialogueNode* Target)
        {
            if (!Target) return;
            TSharedPtr<FJsonObject> Conn = MakeShared<FJsonObject>();
            Conn->SetStringField(TEXT("from_node_id"), Node->GetName());
            Conn->SetStringField(TEXT("from_pin"), FromPin);
            Conn->SetStringField(TEXT("to_node_id"), Target->GetName());
            Conn->SetStringField(TEXT("to_pin"), TEXT("In"));
            ConnArr.Add(MakeShared<FJsonValueObject>(Conn));
        };

        // Entry and Speech use OutTransitions (single Out pin)
        if (UDialogueNodeWithTransitions* WithTrans = Cast<UDialogueNodeWithTransitions>(Node))
        {
            // For Entry/Speech: pin name is "Out"
            // For ChoiceItem: pin name is also "Out" (but ChoiceItems are not in AllNodes)
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
        // ExitNode: no outgoing connections
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetArrayField(TEXT("connections"), ConnArr);
    return Result;
}
