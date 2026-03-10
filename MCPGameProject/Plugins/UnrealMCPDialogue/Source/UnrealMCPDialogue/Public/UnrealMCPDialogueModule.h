#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IUnrealMCPCommandHandler;

class FUnrealMCPDialogueModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IUnrealMCPCommandHandler> DialogueHandler;
};
