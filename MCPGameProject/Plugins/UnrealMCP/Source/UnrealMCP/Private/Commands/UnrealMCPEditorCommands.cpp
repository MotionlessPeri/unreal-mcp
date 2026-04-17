#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Misc/PackageName.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "JsonObjectConverter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"

namespace
{
struct FDirtyPackageStats
{
    int32 DirtyMapPackages = 0;
    int32 DirtyContentPackages = 0;
};

FDirtyPackageStats CollectDirtyPackageStats()
{
    FDirtyPackageStats Stats;

    for (TObjectIterator<UPackage> It; It; ++It)
    {
        UPackage* Package = *It;
        if (!Package || !Package->IsDirty())
        {
            continue;
        }

        const FString PackageName = Package->GetName();
        if (PackageName.IsEmpty() || PackageName.StartsWith(TEXT("/Temp")) || PackageName.StartsWith(TEXT("/Engine/Transient")))
        {
            continue;
        }

        if (Package->ContainsMap())
        {
            ++Stats.DirtyMapPackages;
        }
        else
        {
            ++Stats.DirtyContentPackages;
        }
    }

    return Stats;
}

void ScheduleEditorExit(const bool bForceExit, const float DelaySeconds)
{
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([bForceExit](float /*DeltaTime*/)
        {
            if (GEditor && GEditor->PlayWorld)
            {
                GEditor->RequestEndPlayMap();
            }

            FPlatformMisc::RequestExit(bForceExit);
            return false;
        }),
        FMath::Max(0.0f, DelaySeconds));
}
}

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    else if (CommandType == TEXT("save_dirty_assets"))
    {
        return HandleSaveDirtyAssets(Params);
    }
    else if (CommandType == TEXT("request_editor_exit"))
    {
        return HandleRequestEditorExit(Params);
    }
    else if (CommandType == TEXT("save_and_exit_editor"))
    {
        return HandleSaveAndExitEditor(Params);
    }
    // UObject function call commands
    else if (CommandType == TEXT("call_subsystem_function"))
    {
        return HandleCallSubsystemFunction(Params);
    }
    else if (CommandType == TEXT("add_to_actor_array_property"))
    {
        return HandleAddToActorArrayProperty(Params);
    }
    // Asset introspection
    else if (CommandType == TEXT("get_data_asset"))
    {
        return HandleGetDataAsset(Params);
    }
    else if (CommandType == TEXT("find_assets"))
    {
        return HandleFindAssets(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    UClass* ActorClass = nullptr;

    // First try to resolve common built-in types (backward compatibility)
    if (ActorType == TEXT("StaticMeshActor"))
    {
        ActorClass = AStaticMeshActor::StaticClass();
    }
    else if (ActorType == TEXT("PointLight"))
    {
        ActorClass = APointLight::StaticClass();
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        ActorClass = ASpotLight::StaticClass();
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        ActorClass = ADirectionalLight::StaticClass();
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        ActorClass = ACameraActor::StaticClass();
    }
    else
    {
        // Try to load as a class path (e.g., "/Script/AIPoint.AIPointInstance")
        ActorClass = FindObject<UClass>(nullptr, *ActorType);

        if (!ActorClass)
        {
            // Try StaticLoadClass as fallback
            ActorClass = StaticLoadClass(AActor::StaticClass(), nullptr, *ActorType);
        }

        if (!ActorClass)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Unable to find or load actor class: %s"), *ActorType));
        }

        if (!ActorClass->IsChildOf(AActor::StaticClass()))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Class '%s' is not a valid Actor class"), *ActorType));
        }
    }

    // Spawn the actor using the resolved class
    NewActor = World->SpawnActor(ActorClass, &Location, &Rotation, SpawnParams);

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Always return detailed properties for this command
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }

    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));

    // Special handling for ActorLabel (not a UPROPERTY, needs special handling)
    if (PropertyName.Equals(TEXT("ActorLabel"), ESearchCase::IgnoreCase))
    {
        if (PropertyValue->Type == EJson::String)
        {
            FString NewLabel = PropertyValue->AsString();

            // Set the actor label
            TargetActor->SetActorLabel(NewLabel, true); // true = mark package dirty

            UE_LOG(LogTemp, Display, TEXT("Set ActorLabel for %s to: %s"), *ActorName, *NewLabel);

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetStringField(TEXT("actor"), ActorName);
            ResultObj->SetStringField(TEXT("property"), PropertyName);
            ResultObj->SetStringField(TEXT("new_label"), NewLabel);
            ResultObj->SetBoolField(TEXT("success"), true);
            ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
            return ResultObj;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ActorLabel value must be a string"));
        }
    }

    // Special handling for PointColor on AIPointRoute
    // When PointColor is set, we need to sync the color to all AIPointInstance actors
    if (PropertyName.Equals(TEXT("PointColor"), ESearchCase::IgnoreCase))
    {
        // First, set the property value normally
        FString ErrorMessage;
        if (!FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }

        // Then trigger PostEditChangeProperty to sync colors and mark dirty
        // This simulates what happens when the property is changed in the Editor UI
        #if WITH_EDITOR
        FPropertyChangedEvent PropertyChangedEvent(
            TargetActor->GetClass()->FindPropertyByName(FName(*PropertyName))
        );
        TargetActor->PostEditChangeProperty(PropertyChangedEvent);
        #endif

        UE_LOG(LogTemp, Display, TEXT("Set PointColor for %s and synced to AIPointInstance actors"), *ActorName);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }

    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);

        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint not found by name or path: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Get the active viewport
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
        
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray<uint8> CompressedBitmap;
            FImageUtils::CompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);
            
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSaveDirtyAssets(const TSharedPtr<FJsonObject>& Params)
{
    bool bSaveMaps = true;
    bool bSaveContent = true;
    Params->TryGetBoolField(TEXT("save_maps"), bSaveMaps);
    Params->TryGetBoolField(TEXT("save_content"), bSaveContent);

    const FDirtyPackageStats BeforeStats = CollectDirtyPackageStats();
    const bool bSaveResult = UEditorLoadingAndSavingUtils::SaveDirtyPackages(bSaveMaps, bSaveContent);
    const FDirtyPackageStats AfterStats = CollectDirtyPackageStats();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSaveResult);
    ResultObj->SetBoolField(TEXT("save_maps"), bSaveMaps);
    ResultObj->SetBoolField(TEXT("save_content"), bSaveContent);
    ResultObj->SetNumberField(TEXT("dirty_maps_before"), BeforeStats.DirtyMapPackages);
    ResultObj->SetNumberField(TEXT("dirty_content_before"), BeforeStats.DirtyContentPackages);
    ResultObj->SetNumberField(TEXT("dirty_maps_after"), AfterStats.DirtyMapPackages);
    ResultObj->SetNumberField(TEXT("dirty_content_after"), AfterStats.DirtyContentPackages);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleRequestEditorExit(const TSharedPtr<FJsonObject>& Params)
{
    bool bForceExit = false;
    Params->TryGetBoolField(TEXT("force"), bForceExit);

    double DelaySeconds = 0.25;
    Params->TryGetNumberField(TEXT("delay_seconds"), DelaySeconds);

    ScheduleEditorExit(bForceExit, static_cast<float>(DelaySeconds));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("force"), bForceExit);
    ResultObj->SetNumberField(TEXT("delay_seconds"), DelaySeconds);
    ResultObj->SetStringField(TEXT("note"), TEXT("Editor exit has been scheduled asynchronously."));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSaveAndExitEditor(const TSharedPtr<FJsonObject>& Params)
{
    // Reuse save behavior first, then schedule exit so the MCP response can flush.
    TSharedPtr<FJsonObject> SaveResult = HandleSaveDirtyAssets(Params);
    if (!SaveResult.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Save step failed to produce a response"));
    }

    bool bForceExit = false;
    Params->TryGetBoolField(TEXT("force"), bForceExit);

    double DelaySeconds = 0.35;
    Params->TryGetNumberField(TEXT("delay_seconds"), DelaySeconds);
    ScheduleEditorExit(bForceExit, static_cast<float>(DelaySeconds));

    SaveResult->SetBoolField(TEXT("exit_scheduled"), true);
    SaveResult->SetBoolField(TEXT("force"), bForceExit);
    SaveResult->SetNumberField(TEXT("delay_seconds"), DelaySeconds);
    SaveResult->SetStringField(TEXT("note"), TEXT("Dirty assets saved (per requested flags) and editor exit scheduled."));
    return SaveResult;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCallSubsystemFunction(const TSharedPtr<FJsonObject>& Params)
{
    // Get subsystem class name
    FString SubsystemClassName;
    if (!Params->TryGetStringField(TEXT("subsystem_class"), SubsystemClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'subsystem_class' parameter"));
    }

    // Get function name
    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    // Get world
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Find subsystem class
    UClass* SubsystemClass = FindObject<UClass>(nullptr, *SubsystemClassName);
    if (!SubsystemClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Subsystem class not found: %s"), *SubsystemClassName));
    }

    // Get subsystem instance
    UWorldSubsystem* Subsystem = World->GetSubsystemBase(SubsystemClass);
    if (!Subsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to get subsystem instance: %s"), *SubsystemClassName));
    }

    // Find function
    UFunction* Function = Subsystem->FindFunction(*FunctionName);
    if (!Function)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function not found: %s::%s"), *SubsystemClassName, *FunctionName));
    }

    // Prepare function parameters
    uint8* ParamBuffer = (uint8*)FMemory_Alloca(Function->ParmsSize);
    FMemory::Memzero(ParamBuffer, Function->ParmsSize);

    // Set parameters from JSON
    if (Params->HasField(TEXT("parameters")))
    {
        const TSharedPtr<FJsonObject>* ParamsObj;
        if (Params->TryGetObjectField(TEXT("parameters"), ParamsObj))
        {
            for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
            {
                FProperty* Property = *It;
                if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
                {
                    continue;
                }

                FString PropertyName = Property->GetName();
                if ((*ParamsObj)->HasField(PropertyName))
                {
                    const TSharedPtr<FJsonValue>* JsonValue = (*ParamsObj)->Values.Find(PropertyName);
                    if (JsonValue && JsonValue->IsValid())
                    {
                        // Handle different property types
                        if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
                        {
                            FString StrValue = (*JsonValue)->AsString();
                            StrProp->SetPropertyValue_InContainer(ParamBuffer, StrValue);
                        }
                        else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
                        {
                            int32 IntValue = static_cast<int32>((*JsonValue)->AsNumber());
                            IntProp->SetPropertyValue_InContainer(ParamBuffer, IntValue);
                        }
                        else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
                        {
                            float FloatValue = static_cast<float>((*JsonValue)->AsNumber());
                            FloatProp->SetPropertyValue_InContainer(ParamBuffer, FloatValue);
                        }
                        else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
                        {
                            bool BoolValue = (*JsonValue)->AsBool();
                            BoolProp->SetPropertyValue_InContainer(ParamBuffer, BoolValue);
                        }
                        // Add more type handlers as needed
                    }
                }
            }
        }
    }

    // Call function
    Subsystem->ProcessEvent(Function, ParamBuffer);

    // Extract return value(s)
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);

    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FProperty* Property = *It;
        if (Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
        {
            FString PropertyName = Property->GetName();

            if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
            {
                FString StrValue = StrProp->GetPropertyValue_InContainer(ParamBuffer);
                ResultObj->SetStringField(PropertyName, StrValue);
            }
            else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
            {
                int32 IntValue = IntProp->GetPropertyValue_InContainer(ParamBuffer);
                ResultObj->SetNumberField(PropertyName, IntValue);
            }
            else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
            {
                float FloatValue = FloatProp->GetPropertyValue_InContainer(ParamBuffer);
                ResultObj->SetNumberField(PropertyName, FloatValue);
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
            {
                bool BoolValue = BoolProp->GetPropertyValue_InContainer(ParamBuffer);
                ResultObj->SetBoolField(PropertyName, BoolValue);
            }
            else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
            {
                UObject* ObjectValue = ObjectProp->GetPropertyValue_InContainer(ParamBuffer);
                if (ObjectValue)
                {
                    ResultObj->SetStringField(PropertyName, ObjectValue->GetName());
                    if (AActor* Actor = Cast<AActor>(ObjectValue))
                    {
                        ResultObj->SetObjectField(PropertyName + TEXT("_details"),
                                                FUnrealMCPCommonUtils::ActorToJsonObject(Actor));
                    }
                }
            }
        }
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleAddToActorArrayProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get element to add (actor name or array of actor names)
    TArray<FString> ElementNames;
    if (Params->HasField(TEXT("element_name")))
    {
        FString ElementName;
        if (Params->TryGetStringField(TEXT("element_name"), ElementName))
        {
            ElementNames.Add(ElementName);
        }
    }
    else if (Params->HasField(TEXT("element_names")))
    {
        const TArray<TSharedPtr<FJsonValue>>* JsonArray;
        if (Params->TryGetArrayField(TEXT("element_names"), JsonArray))
        {
            for (const TSharedPtr<FJsonValue>& JsonValue : *JsonArray)
            {
                if (JsonValue->Type == EJson::String)
                {
                    ElementNames.Add(JsonValue->AsString());
                }
            }
        }
    }

    if (ElementNames.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'element_name' or 'element_names' parameter"));
    }

    // Find the target actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Find the property
    FProperty* Property = TargetActor->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Property '%s' not found on actor '%s'"), *PropertyName, *ActorName));
    }

    // Check if it's an array property
    FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
    if (!ArrayProperty)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Property '%s' is not an array"), *PropertyName));
    }

    // Check if inner property is object
    FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);
    if (!InnerObjectProperty)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Property '%s' is not an object array"), *PropertyName));
    }

    // Find the element actors
    TArray<AActor*> ElementActors;
    for (const FString& ElementName : ElementNames)
    {
        AActor* ElementActor = nullptr;
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == ElementName)
            {
                ElementActor = Actor;
                break;
            }
        }

        if (!ElementActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Element actor not found: %s"), *ElementName));
        }

        ElementActors.Add(ElementActor);
    }

    // Modify actor for undo/redo
    TargetActor->Modify();

    // Get array helper
    FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(TargetActor));

    // Add elements to array
    for (AActor* ElementActor : ElementActors)
    {
        int32 NewIndex = ArrayHelper.AddValue();
        InnerObjectProperty->SetObjectPropertyValue(ArrayHelper.GetRawPtr(NewIndex), ElementActor);
    }

    // Mark package dirty
    if (TargetActor->GetLevel())
    {
        TargetActor->GetLevel()->Modify();
        TargetActor->GetLevel()->GetOutermost()->MarkPackageDirty();
    }

    // Return result
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("property"), PropertyName);
    ResultObj->SetNumberField(TEXT("added_count"), ElementActors.Num());
    ResultObj->SetNumberField(TEXT("new_array_size"), ArrayHelper.Num());

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetDataAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    // StaticLoadObject accepts both content-browser paths ("/Game/X/Y") and full object
    // paths ("/Game/X/Y.Y"); pick whichever the caller provided.
    UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
    if (!Object)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load asset at path: %s"), *Path));
    }

    bool bAllowAnyObject = false;
    Params->TryGetBoolField(TEXT("allow_any_object"), bAllowAnyObject);
    if (!bAllowAnyObject && !Object->IsA(UDataAsset::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(
                TEXT("Asset is not a DataAsset (class: %s). Pass allow_any_object=true to read arbitrary UObject assets."),
                *Object->GetClass()->GetName()));
    }

    FString PropertyPath;
    Params->TryGetStringField(TEXT("property_path"), PropertyPath);

    const int64 CheckFlags = 0;
    const int64 SkipFlags = CPF_Transient;

    TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
    if (PropertyPath.IsEmpty())
    {
        FJsonObjectConverter::UStructToJsonObject(
            Object->GetClass(), Object, PropsObj.ToSharedRef(), CheckFlags, SkipFlags);
    }
    else
    {
        FProperty* Property = Object->GetClass()->FindPropertyByName(FName(*PropertyPath));
        if (!Property)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Property not found on asset class '%s': %s"),
                    *Object->GetClass()->GetName(), *PropertyPath));
        }
        TSharedPtr<FJsonValue> Value = FJsonObjectConverter::UPropertyToJsonValue(
            Property, Property->ContainerPtrToValuePtr<void>(Object), CheckFlags, SkipFlags);
        if (Value.IsValid())
        {
            PropsObj->SetField(PropertyPath, Value);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"), Object->GetPathName());
    Result->SetStringField(TEXT("asset_name"), Object->GetName());
    Result->SetStringField(TEXT("asset_class"), Object->GetClass()->GetPathName());
    Result->SetStringField(TEXT("asset_class_short"), Object->GetClass()->GetName());
    if (!PropertyPath.IsEmpty())
    {
        Result->SetStringField(TEXT("property_path"), PropertyPath);
    }
    Result->SetObjectField(TEXT("properties"), PropsObj);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = TEXT("/Game");
    Params->TryGetStringField(TEXT("path"), Path);

    FString ClassName;
    Params->TryGetStringField(TEXT("class_name"), ClassName);

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FString NamePattern;
    Params->TryGetStringField(TEXT("name_pattern"), NamePattern);

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*Path));
    Filter.bRecursivePaths = bRecursive;

    FString ClassResolution = TEXT("none");
    if (!ClassName.IsEmpty())
    {
        // Accept either short name ("Blueprint") or full path ("/Script/Engine.Blueprint").
        UClass* TargetClass = nullptr;
        if (ClassName.Contains(TEXT(".")))
        {
            TargetClass = FindObject<UClass>(nullptr, *ClassName);
            ClassResolution = TEXT("full_path");
        }
        else
        {
            TargetClass = UClass::TryFindTypeSlow<UClass>(ClassName);
            ClassResolution = TEXT("short_name");
        }

        if (TargetClass)
        {
            Filter.ClassPaths.Add(TargetClass->GetClassPathName());
            Filter.bRecursiveClasses = true;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Class not found: %s"), *ClassName));
        }
    }

    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);

    TArray<TSharedPtr<FJsonValue>> AssetsArr;
    for (const FAssetData& Asset : AssetDataList)
    {
        if (!NamePattern.IsEmpty()
            && !Asset.AssetName.ToString().MatchesWildcard(NamePattern, ESearchCase::IgnoreCase))
        {
            continue;
        }

        TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
        AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
        AssetObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
        AssetObj->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
        AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());

        // Pull BP parent class from asset tags without loading the asset. Keys differ
        // by UE version; try both common names.
        FString ParentClassTag;
        if (Asset.GetTagValue(FName(TEXT("ParentClass")), ParentClassTag)
            || Asset.GetTagValue(FName(TEXT("NativeParentClass")), ParentClassTag))
        {
            AssetObj->SetStringField(TEXT("parent_class"), ParentClassTag);
        }

        AssetsArr.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), AssetsArr);
    Result->SetNumberField(TEXT("total_count"), AssetsArr.Num());
    Result->SetStringField(TEXT("search_path"), Path);
    Result->SetBoolField(TEXT("recursive"), bRecursive);
    if (!ClassName.IsEmpty())
    {
        Result->SetStringField(TEXT("class_filter"), ClassName);
        Result->SetStringField(TEXT("class_resolution"), ClassResolution);
    }
    if (!NamePattern.IsEmpty())
    {
        Result->SetStringField(TEXT("name_pattern"), NamePattern);
    }
    return Result;
}

TArray<FMCPCommandMeta> FUnrealMCPEditorCommands::GetCommandMetadata()
{
	return {
		{TEXT("get_actors_in_level"), TEXT("editor"), TEXT("List all actors in the current level"), {}},
		{TEXT("find_actors_by_name"), TEXT("editor"), TEXT("Find actors matching a name pattern"), {
			{TEXT("pattern"), TEXT("string"), true, TEXT("Pattern to match against actor names")}
		}},
		{TEXT("spawn_actor"), TEXT("editor"), TEXT("Spawn an actor in the level"), {
			{TEXT("type"), TEXT("string"), true, TEXT("Actor class or built-in shortcut (StaticMeshActor, PointLight, etc.) or full class path")},
			{TEXT("name"), TEXT("string"), true, TEXT("Unique actor instance name")},
			{TEXT("location"), TEXT("object"), false, TEXT("{x, y, z}")},
			{TEXT("rotation"), TEXT("object"), false, TEXT("{pitch, yaw, roll}")},
			{TEXT("scale"), TEXT("object"), false, TEXT("{x, y, z}")}
		}},
		{TEXT("delete_actor"), TEXT("editor"), TEXT("Delete an actor from the level"), {
			{TEXT("name"), TEXT("string"), true, TEXT("Actor name")}
		}},
		{TEXT("set_actor_transform"), TEXT("editor"), TEXT("Set an actor's transform"), {
			{TEXT("name"), TEXT("string"), true, TEXT("Actor name")},
			{TEXT("location"), TEXT("object"), false, TEXT("{x, y, z}")},
			{TEXT("rotation"), TEXT("object"), false, TEXT("{pitch, yaw, roll}")},
			{TEXT("scale"), TEXT("object"), false, TEXT("{x, y, z}")}
		}},
		{TEXT("get_actor_properties"), TEXT("editor"), TEXT("Get detailed properties of an actor"), {
			{TEXT("name"), TEXT("string"), true, TEXT("Actor name")}
		}},
		{TEXT("set_actor_property"), TEXT("editor"), TEXT("Set a single property on an actor"), {
			{TEXT("name"), TEXT("string"), true, TEXT("Actor name")},
			{TEXT("property_name"), TEXT("string"), true, TEXT("Property name")},
			{TEXT("property_value"), TEXT("any"), true, TEXT("Value to set")}
		}},
		{TEXT("spawn_blueprint_actor"), TEXT("editor"), TEXT("Spawn an instance of a Blueprint class"), {
			{TEXT("blueprint_name"), TEXT("string"), true, TEXT("Blueprint name or path")},
			{TEXT("actor_name"), TEXT("string"), true, TEXT("Instance label name")},
			{TEXT("location"), TEXT("object"), false, TEXT("{x, y, z}")},
			{TEXT("rotation"), TEXT("object"), false, TEXT("{pitch, yaw, roll}")},
			{TEXT("scale"), TEXT("object"), false, TEXT("{x, y, z}")}
		}},
		{TEXT("focus_viewport"), TEXT("editor"), TEXT("Move the editor viewport to a target"), {
			{TEXT("target"), TEXT("string"), false, TEXT("Actor name to focus on")},
			{TEXT("location"), TEXT("object"), false, TEXT("{x, y, z} to focus on")},
			{TEXT("distance"), TEXT("number"), false, TEXT("View distance (default: 1000)")},
			{TEXT("orientation"), TEXT("object"), false, TEXT("{pitch, yaw, roll}")}
		}},
		{TEXT("take_screenshot"), TEXT("editor"), TEXT("Capture a viewport screenshot"), {
			{TEXT("filepath"), TEXT("string"), true, TEXT("Output file path")}
		}},
		{TEXT("save_dirty_assets"), TEXT("editor"), TEXT("Save all dirty assets"), {
			{TEXT("save_maps"), TEXT("bool"), false, TEXT("Save map packages (default: true)")},
			{TEXT("save_content"), TEXT("bool"), false, TEXT("Save content packages (default: true)")}
		}},
		{TEXT("request_editor_exit"), TEXT("editor"), TEXT("Schedule an asynchronous editor exit"), {
			{TEXT("force"), TEXT("bool"), false, TEXT("Force exit without prompt (default: false)")},
			{TEXT("delay_seconds"), TEXT("number"), false, TEXT("Delay before exit (default: 0.25)")}
		}},
		{TEXT("save_and_exit_editor"), TEXT("editor"), TEXT("Save dirty assets then schedule editor exit"), {
			{TEXT("save_maps"), TEXT("bool"), false, TEXT("Save map packages (default: true)")},
			{TEXT("save_content"), TEXT("bool"), false, TEXT("Save content packages (default: true)")},
			{TEXT("force"), TEXT("bool"), false, TEXT("Force exit (default: false)")},
			{TEXT("delay_seconds"), TEXT("number"), false, TEXT("Delay before exit (default: 0.35)")}
		}},
		{TEXT("call_subsystem_function"), TEXT("editor"), TEXT("Call a BlueprintCallable function on a WorldSubsystem"), {
			{TEXT("subsystem_class"), TEXT("string"), true, TEXT("Full class path")},
			{TEXT("function_name"), TEXT("string"), true, TEXT("Function to call")},
			{TEXT("parameters"), TEXT("object"), false, TEXT("Function parameters as key-value pairs")}
		}},
		{TEXT("add_to_actor_array_property"), TEXT("editor"), TEXT("Add actor references to an array property"), {
			{TEXT("actor_name"), TEXT("string"), true, TEXT("Actor containing the array")},
			{TEXT("property_name"), TEXT("string"), true, TEXT("Array property name")},
			{TEXT("element_name"), TEXT("string"), false, TEXT("Single actor name to add")},
			{TEXT("element_names"), TEXT("array"), false, TEXT("Array of actor names to add")}
		}},
		{TEXT("get_data_asset"), TEXT("editor"), TEXT("Read a DataAsset (or any UObject asset with allow_any_object=true) as JSON properties"), {
			{TEXT("path"), TEXT("string"), true, TEXT("Content-browser or object path (/Game/.../Asset or /Game/.../Asset.Asset)")},
			{TEXT("property_path"), TEXT("string"), false, TEXT("Optional top-level property name to narrow the dump")},
			{TEXT("allow_any_object"), TEXT("bool"), false, TEXT("Read non-DataAsset UObject assets too (default: false)")}
		}},
		{TEXT("find_assets"), TEXT("editor"), TEXT("List assets under a path, optionally filtered by class or name pattern (uses AssetRegistry, no asset loading)"), {
			{TEXT("path"), TEXT("string"), false, TEXT("Package path (default: /Game)")},
			{TEXT("class_name"), TEXT("string"), false, TEXT("Filter by asset class (short name or full path; recursive by subclass)")},
			{TEXT("recursive"), TEXT("bool"), false, TEXT("Recurse into subfolders (default: true)")},
			{TEXT("name_pattern"), TEXT("string"), false, TEXT("Wildcard glob on asset name (case-insensitive)")}
		}}
	};
}
