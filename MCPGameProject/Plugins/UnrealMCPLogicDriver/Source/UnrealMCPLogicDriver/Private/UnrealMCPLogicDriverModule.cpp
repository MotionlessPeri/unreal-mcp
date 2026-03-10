#include "UnrealMCPLogicDriverModule.h"

#include "Commands/UnrealMCPCommandRegistry.h"
#include "Commands/UnrealMCPLogicDriverCommands.h"

namespace
{
	static const FName LogicDriverHandlerName(TEXT("LogicDriver"));

	class FUnrealMCPLogicDriverCommandHandler : public IUnrealMCPCommandHandler
	{
	public:
		FUnrealMCPLogicDriverCommandHandler()
			: Commands(MakeShared<FUnrealMCPLogicDriverCommands>())
		{
		}

		virtual bool CanHandleCommand(const FString& CommandType) const override
		{
			return CommandType == TEXT("get_logicdriver_state_machine_graph");
		}

		virtual TSharedPtr<FJsonObject> HandleCommand(
			const FString& CommandType,
			const TSharedPtr<FJsonObject>& Params) override
		{
			return Commands->HandleCommand(CommandType, Params);
		}

	private:
		TSharedPtr<FUnrealMCPLogicDriverCommands> Commands;
	};
}

void FUnrealMCPLogicDriverModule::StartupModule()
{
	LogicDriverHandler = MakeShared<FUnrealMCPLogicDriverCommandHandler>();
	FUnrealMCPCommandRegistry::Get().RegisterHandler(LogicDriverHandlerName, LogicDriverHandler.ToSharedRef());
}

void FUnrealMCPLogicDriverModule::ShutdownModule()
{
	FUnrealMCPCommandRegistry::Get().UnregisterHandler(LogicDriverHandlerName);
	LogicDriverHandler.Reset();
}

IMPLEMENT_MODULE(FUnrealMCPLogicDriverModule, UnrealMCPLogicDriver)
