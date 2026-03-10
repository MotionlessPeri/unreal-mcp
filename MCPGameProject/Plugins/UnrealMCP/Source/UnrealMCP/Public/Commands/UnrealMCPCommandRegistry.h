#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Json.h"

/**
 * Extension point for project-specific MCP command groups.
 * Base UnrealMCP stays consumer-agnostic; extensions register handlers here.
 */
class UNREALMCP_API IUnrealMCPCommandHandler
{
public:
	virtual ~IUnrealMCPCommandHandler() = default;

	virtual bool CanHandleCommand(const FString& CommandType) const = 0;
	virtual TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params) = 0;
};

class UNREALMCP_API FUnrealMCPCommandRegistry
{
public:
	static FUnrealMCPCommandRegistry& Get();

	void RegisterHandler(const FName HandlerName, TSharedRef<IUnrealMCPCommandHandler> Handler);
	void UnregisterHandler(const FName HandlerName);
	TSharedPtr<IUnrealMCPCommandHandler> FindHandlerForCommand(const FString& CommandType) const;

private:
	mutable FCriticalSection HandlersMutex;
	TMap<FName, TSharedPtr<IUnrealMCPCommandHandler>> Handlers;
};
