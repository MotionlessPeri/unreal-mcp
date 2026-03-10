#pragma once

#include "Modules/ModuleInterface.h"

class IUnrealMCPCommandHandler;

class FUnrealMCPLogicDriverModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IUnrealMCPCommandHandler> LogicDriverHandler;
};
