#include "UnrealMCPDialogueModule.h"

#include "Commands/UnrealMCPCommandRegistry.h"
#include "Commands/UnrealMCPDialogueCommands.h"

namespace
{
	static const FName DialogueHandlerName(TEXT("DialogueSystem"));

	class FUnrealMCPDialogueCommandHandler : public IUnrealMCPCommandHandler
	{
	public:
		FUnrealMCPDialogueCommandHandler()
			: Commands(MakeShared<FUnrealMCPDialogueCommands>())
		{
		}

		virtual bool CanHandleCommand(const FString& CommandType) const override
		{
			return CommandType == TEXT("create_dialogue_asset") ||
				CommandType == TEXT("get_dialogue_graph") ||
				CommandType == TEXT("get_dialogue_connections") ||
				CommandType == TEXT("add_dialogue_node") ||
				CommandType == TEXT("set_dialogue_node_properties") ||
				CommandType == TEXT("connect_dialogue_nodes") ||
				CommandType == TEXT("disconnect_dialogue_nodes") ||
				CommandType == TEXT("delete_dialogue_node") ||
				CommandType == TEXT("add_dialogue_choice_item");
		}

		virtual TSharedPtr<FJsonObject> HandleCommand(
			const FString& CommandType,
			const TSharedPtr<FJsonObject>& Params) override
		{
			return Commands->HandleCommand(CommandType, Params);
		}

	private:
		TSharedPtr<FUnrealMCPDialogueCommands> Commands;
	};
}

void FUnrealMCPDialogueModule::StartupModule()
{
	DialogueHandler = MakeShared<FUnrealMCPDialogueCommandHandler>();
	FUnrealMCPCommandRegistry::Get().RegisterHandler(DialogueHandlerName, DialogueHandler.ToSharedRef());
}

void FUnrealMCPDialogueModule::ShutdownModule()
{
	FUnrealMCPCommandRegistry::Get().UnregisterHandler(DialogueHandlerName);
	DialogueHandler.Reset();
}

IMPLEMENT_MODULE(FUnrealMCPDialogueModule, UnrealMCPDialogue)
