#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UNREALMCP_API FUnrealMCPAnimationCommands
{
public:
    FUnrealMCPAnimationCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetMontageInfo(const TSharedPtr<FJsonObject>& Params);
};
