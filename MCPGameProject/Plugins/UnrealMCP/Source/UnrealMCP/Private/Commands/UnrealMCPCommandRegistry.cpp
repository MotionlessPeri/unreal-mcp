#include "Commands/UnrealMCPCommandRegistry.h"

FUnrealMCPCommandRegistry& FUnrealMCPCommandRegistry::Get()
{
	static FUnrealMCPCommandRegistry Registry;
	return Registry;
}

void FUnrealMCPCommandRegistry::RegisterHandler(
	const FName HandlerName,
	TSharedRef<IUnrealMCPCommandHandler> Handler)
{
	FScopeLock Lock(&HandlersMutex);
	Handlers.Add(HandlerName, Handler);
}

void FUnrealMCPCommandRegistry::UnregisterHandler(const FName HandlerName)
{
	FScopeLock Lock(&HandlersMutex);
	Handlers.Remove(HandlerName);
}

TSharedPtr<IUnrealMCPCommandHandler> FUnrealMCPCommandRegistry::FindHandlerForCommand(
	const FString& CommandType) const
{
	FScopeLock Lock(&HandlersMutex);

	for (const TPair<FName, TSharedPtr<IUnrealMCPCommandHandler>>& Entry : Handlers)
	{
		if (Entry.Value.IsValid() && Entry.Value->CanHandleCommand(CommandType))
		{
			return Entry.Value;
		}
	}

	return nullptr;
}

TArray<FMCPCommandMeta> FUnrealMCPCommandRegistry::GetAllExtensionMetadata() const
{
	FScopeLock Lock(&HandlersMutex);

	TArray<FMCPCommandMeta> AllMeta;
	for (const TPair<FName, TSharedPtr<IUnrealMCPCommandHandler>>& Entry : Handlers)
	{
		if (Entry.Value.IsValid())
		{
			AllMeta.Append(Entry.Value->GetCommandMetadata());
		}
	}
	return AllMeta;
}
