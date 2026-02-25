#include "Commands/UnrealMCPBehaviorTreeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "EditorAssetLibrary.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BlackboardData.h"

FUnrealMCPBehaviorTreeCommands::FUnrealMCPBehaviorTreeCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPBehaviorTreeCommands::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_behavior_tree_info"))
    {
        return HandleGetBehaviorTreeInfo(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown behavior tree command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPBehaviorTreeCommands::HandleGetBehaviorTreeInfo(
    const TSharedPtr<FJsonObject>& Params)
{
    // --- Validate parameter ---
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'asset_path' parameter (e.g. \"/Game/AIPointTree\")"));
    }

    // --- Load the asset ---
    UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!LoadedObj)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load asset at path: %s"), *AssetPath));
    }

    UBehaviorTree* BT = Cast<UBehaviorTree>(LoadedObj);
    if (!BT)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset at '%s' is not a BehaviorTree (class: %s)"),
                *AssetPath, *LoadedObj->GetClass()->GetName()));
    }

    // --- Build result object ---
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("asset_name"), BT->GetName());

    // Blackboard asset name (if any)
    if (BT->BlackboardAsset)
    {
        Result->SetStringField(TEXT("blackboard"), BT->BlackboardAsset->GetPathName());
    }
    else
    {
        Result->SetStringField(TEXT("blackboard"), TEXT(""));
    }

    // Root node
    if (BT->RootNode)
    {
        Result->SetObjectField(TEXT("root"), SerializeCompositeNode(BT->RootNode));
    }
    else
    {
        Result->SetBoolField(TEXT("root"), false);
    }

    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPBehaviorTreeCommands::SerializeCompositeNode(UBTCompositeNode* Node)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("node_type"), TEXT("Composite"));
    Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    Obj->SetStringField(TEXT("name"), Node->GetNodeName());

    // Services attached to this composite
    TArray<TSharedPtr<FJsonValue>> ServicesArr;
    for (UBTService* Svc : Node->Services)
    {
        if (Svc)
        {
            ServicesArr.Add(MakeShared<FJsonValueObject>(SerializeService(Svc)));
        }
    }
    Obj->SetArrayField(TEXT("services"), ServicesArr);

    // Children
    TArray<TSharedPtr<FJsonValue>> ChildrenArr;
    for (const FBTCompositeChild& ChildInfo : Node->Children)
    {
        TSharedPtr<FJsonObject> ChildEntry = MakeShared<FJsonObject>();

        // Decorators on this child slot
        TArray<TSharedPtr<FJsonValue>> DecsArr;
        for (UBTDecorator* Dec : ChildInfo.Decorators)
        {
            if (Dec)
            {
                DecsArr.Add(MakeShared<FJsonValueObject>(SerializeDecorator(Dec)));
            }
        }
        ChildEntry->SetArrayField(TEXT("decorators"), DecsArr);

        // The child node itself
        if (ChildInfo.ChildComposite)
        {
            ChildEntry->SetObjectField(TEXT("node"), SerializeCompositeNode(ChildInfo.ChildComposite));
        }
        else if (ChildInfo.ChildTask)
        {
            ChildEntry->SetObjectField(TEXT("node"), SerializeTaskNode(ChildInfo.ChildTask));
        }
        else
        {
            ChildEntry->SetBoolField(TEXT("node"), false);
        }

        ChildrenArr.Add(MakeShared<FJsonValueObject>(ChildEntry));
    }
    Obj->SetArrayField(TEXT("children"), ChildrenArr);

    return Obj;
}

TSharedPtr<FJsonObject> FUnrealMCPBehaviorTreeCommands::SerializeTaskNode(UBTTaskNode* Node)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("node_type"), TEXT("Task"));
    Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    Obj->SetStringField(TEXT("name"), Node->GetNodeName());
    return Obj;
}

TSharedPtr<FJsonObject> FUnrealMCPBehaviorTreeCommands::SerializeDecorator(UBTDecorator* Node)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("node_type"), TEXT("Decorator"));
    Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    Obj->SetStringField(TEXT("name"), Node->GetNodeName());
    return Obj;
}

TSharedPtr<FJsonObject> FUnrealMCPBehaviorTreeCommands::SerializeService(UBTService* Node)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("node_type"), TEXT("Service"));
    Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    Obj->SetStringField(TEXT("name"), Node->GetNodeName());
    return Obj;
}
