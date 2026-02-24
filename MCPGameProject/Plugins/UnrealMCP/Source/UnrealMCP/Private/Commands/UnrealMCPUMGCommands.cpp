#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
// We'll create widgets using regular Factory classes
#include "Factories/Factory.h"
#include "WidgetBlueprintFactory.h"
#include "WidgetBlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/UniformGridSlot.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"
#include "K2Node_ComponentBoundEvent.h"
#include "UObject/UnrealType.h"
#include "Styling/SlateColor.h"
#include "Misc/PackageName.h"

namespace
{
UWidgetBlueprint* ResolveWidgetBlueprint(const FString& BlueprintNameOrPath)
{
	return Cast<UWidgetBlueprint>(FUnrealMCPCommonUtils::FindBlueprintByName(BlueprintNameOrPath));
}

FString GetWidgetBlueprintSavePath(const UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return FString();
	}

	FString SavePath = WidgetBlueprint->GetPathName();
	const int32 DotIndex = SavePath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (DotIndex != INDEX_NONE)
	{
		SavePath = SavePath.Left(DotIndex);
	}
	return SavePath;
}

TSharedPtr<FJsonObject> BuildWidgetTreeNodeJson(const UWidget* Widget, const FString& ParentWidgetName)
{
	TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
	if (!Widget)
	{
		NodeObj->SetStringField(TEXT("name"), TEXT(""));
		NodeObj->SetStringField(TEXT("class"), TEXT("None"));
		NodeObj->SetStringField(TEXT("parent_name"), ParentWidgetName);
		NodeObj->SetBoolField(TEXT("is_variable"), false);
		NodeObj->SetArrayField(TEXT("children"), {});
		return NodeObj;
	}

	NodeObj->SetStringField(TEXT("name"), Widget->GetName());
	NodeObj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
	NodeObj->SetStringField(TEXT("parent_name"), ParentWidgetName);
	NodeObj->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
	NodeObj->SetBoolField(TEXT("is_root"), ParentWidgetName.IsEmpty());

	if (const UPanelSlot* Slot = Widget->Slot)
	{
		NodeObj->SetStringField(TEXT("slot_class"), Slot->GetClass()->GetName());
	}
	else
	{
		NodeObj->SetStringField(TEXT("slot_class"), TEXT(""));
	}

	TArray<TSharedPtr<FJsonValue>> ChildrenJson;
	if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		const int32 ChildCount = Panel->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			if (const UWidget* Child = Panel->GetChildAt(ChildIndex))
			{
				TSharedPtr<FJsonObject> ChildObj = BuildWidgetTreeNodeJson(Child, Widget->GetName());
				ChildrenJson.Add(MakeShared<FJsonValueObject>(ChildObj));
			}
		}
	}
	NodeObj->SetArrayField(TEXT("children"), ChildrenJson);

	return NodeObj;
}

int32 CountWidgetSubtreeNodes(const UWidget* Widget)
{
	if (!Widget)
	{
		return 0;
	}

	int32 Count = 1;
	if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		const int32 ChildCount = Panel->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			Count += CountWidgetSubtreeNodes(Panel->GetChildAt(ChildIndex));
		}
	}
	return Count;
}

bool TryGetJsonVector2(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, FVector2D& OutValue)
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
	if (!Params->TryGetArrayField(FieldName, ArrayPtr) || !ArrayPtr || ArrayPtr->Num() < 2)
	{
		return false;
	}

	OutValue = FVector2D((*ArrayPtr)[0]->AsNumber(), (*ArrayPtr)[1]->AsNumber());
	return true;
}

bool TryGetJsonMargin(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, FMargin& OutMargin)
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
	if (!Params->TryGetArrayField(FieldName, ArrayPtr) || !ArrayPtr || ArrayPtr->Num() < 4)
	{
		return false;
	}

	OutMargin = FMargin(
		(*ArrayPtr)[0]->AsNumber(),
		(*ArrayPtr)[1]->AsNumber(),
		(*ArrayPtr)[2]->AsNumber(),
		(*ArrayPtr)[3]->AsNumber());
	return true;
}

bool TryGetJsonAnchors(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, FAnchors& OutAnchors)
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
	if (!Params->TryGetArrayField(FieldName, ArrayPtr) || !ArrayPtr)
	{
		return false;
	}

	if (ArrayPtr->Num() >= 4)
	{
		OutAnchors = FAnchors(
			(*ArrayPtr)[0]->AsNumber(),
			(*ArrayPtr)[1]->AsNumber(),
			(*ArrayPtr)[2]->AsNumber(),
			(*ArrayPtr)[3]->AsNumber());
		return true;
	}
	if (ArrayPtr->Num() >= 2)
	{
		const float X = (*ArrayPtr)[0]->AsNumber();
		const float Y = (*ArrayPtr)[1]->AsNumber();
		OutAnchors = FAnchors(X, Y, X, Y);
		return true;
	}
	return false;
}

TArray<TSharedPtr<FJsonValue>> MakeJsonArrayFromVector2(const FVector2D& Value)
{
	return {
		MakeShared<FJsonValueNumber>(Value.X),
		MakeShared<FJsonValueNumber>(Value.Y)
	};
}

TArray<TSharedPtr<FJsonValue>> MakeJsonArrayFromAnchors(const FAnchors& Anchors)
{
	return {
		MakeShared<FJsonValueNumber>(Anchors.Minimum.X),
		MakeShared<FJsonValueNumber>(Anchors.Minimum.Y),
		MakeShared<FJsonValueNumber>(Anchors.Maximum.X),
		MakeShared<FJsonValueNumber>(Anchors.Maximum.Y)
	};
}

TArray<TSharedPtr<FJsonValue>> MakeJsonArrayFromMargin(const FMargin& Margin)
{
	return {
		MakeShared<FJsonValueNumber>(Margin.Left),
		MakeShared<FJsonValueNumber>(Margin.Top),
		MakeShared<FJsonValueNumber>(Margin.Right),
		MakeShared<FJsonValueNumber>(Margin.Bottom)
	};
}

void FillCanvasSlotLayoutReadback(TSharedPtr<FJsonObject> ResultObj, const UCanvasPanelSlot* CanvasSlot)
{
	if (!ResultObj || !CanvasSlot)
	{
		return;
	}

	ResultObj->SetStringField(TEXT("slot_class"), CanvasSlot->GetClass()->GetName());
	ResultObj->SetArrayField(TEXT("position"), MakeJsonArrayFromVector2(CanvasSlot->GetPosition()));
	ResultObj->SetArrayField(TEXT("size"), MakeJsonArrayFromVector2(CanvasSlot->GetSize()));
	ResultObj->SetArrayField(TEXT("alignment"), MakeJsonArrayFromVector2(CanvasSlot->GetAlignment()));
	ResultObj->SetArrayField(TEXT("anchors"), MakeJsonArrayFromAnchors(CanvasSlot->GetAnchors()));
	ResultObj->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
	ResultObj->SetNumberField(TEXT("z_order"), CanvasSlot->GetZOrder());
}

bool ApplyCanvasSlotLayoutFields(const TSharedPtr<FJsonObject>& Params, UCanvasPanelSlot* CanvasSlot, FString& OutError)
{
	if (!Params || !CanvasSlot)
	{
		OutError = TEXT("Invalid Params or CanvasSlot");
		return false;
	}

	FVector2D Vec2Value;
	if (TryGetJsonVector2(Params, TEXT("position"), Vec2Value))
	{
		CanvasSlot->SetPosition(Vec2Value);
	}
	if (TryGetJsonVector2(Params, TEXT("size"), Vec2Value))
	{
		CanvasSlot->SetSize(Vec2Value);
	}
	if (TryGetJsonVector2(Params, TEXT("alignment"), Vec2Value))
	{
		CanvasSlot->SetAlignment(Vec2Value);
	}

	FAnchors Anchors;
	if (TryGetJsonAnchors(Params, TEXT("anchors"), Anchors))
	{
		CanvasSlot->SetAnchors(Anchors);
	}

	bool bAutoSize = false;
	if (Params->TryGetBoolField(TEXT("auto_size"), bAutoSize))
	{
		CanvasSlot->SetAutoSize(bAutoSize);
	}

	int32 ZOrder = 0;
	if (Params->TryGetNumberField(TEXT("z_order"), ZOrder))
	{
		CanvasSlot->SetZOrder(ZOrder);
	}

	return true;
}

bool TryGetJsonLinearColor(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, FLinearColor& OutColor)
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
	if (!Params->TryGetArrayField(FieldName, ArrayPtr) || !ArrayPtr || ArrayPtr->Num() < 4)
	{
		return false;
	}

	OutColor = FLinearColor(
		(*ArrayPtr)[0]->AsNumber(),
		(*ArrayPtr)[1]->AsNumber(),
		(*ArrayPtr)[2]->AsNumber(),
		(*ArrayPtr)[3]->AsNumber());
	return true;
}

TArray<TSharedPtr<FJsonValue>> MakeJsonArrayFromLinearColor(const FLinearColor& Color)
{
	return {
		MakeShared<FJsonValueNumber>(Color.R),
		MakeShared<FJsonValueNumber>(Color.G),
		MakeShared<FJsonValueNumber>(Color.B),
		MakeShared<FJsonValueNumber>(Color.A)
	};
}

bool TryParseVisibility(const FString& InValue, ESlateVisibility& OutValue)
{
	const FString Value = InValue.TrimStartAndEnd();
	if (Value.IsEmpty())
	{
		return false;
	}

	if (Value.Equals(TEXT("Visible"), ESearchCase::IgnoreCase)) { OutValue = ESlateVisibility::Visible; return true; }
	if (Value.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase)) { OutValue = ESlateVisibility::Collapsed; return true; }
	if (Value.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase)) { OutValue = ESlateVisibility::Hidden; return true; }
	if (Value.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("NotHitTestableAll"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("Not-Hit-Testable (Self & All Children)"), ESearchCase::IgnoreCase))
	{
		OutValue = ESlateVisibility::HitTestInvisible; return true;
	}
	if (Value.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("NotHitTestableSelf"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("Not-Hit-Testable (Self Only)"), ESearchCase::IgnoreCase))
	{
		OutValue = ESlateVisibility::SelfHitTestInvisible; return true;
	}
	return false;
}

FString VisibilityToString(const ESlateVisibility Visibility)
{
	switch (Visibility)
	{
	case ESlateVisibility::Visible: return TEXT("Visible");
	case ESlateVisibility::Collapsed: return TEXT("Collapsed");
	case ESlateVisibility::Hidden: return TEXT("Hidden");
	case ESlateVisibility::HitTestInvisible: return TEXT("HitTestInvisible");
	case ESlateVisibility::SelfHitTestInvisible: return TEXT("SelfHitTestInvisible");
	default: return TEXT("Unknown");
	}
}

bool TryParseHorizontalAlignment(const FString& InValue, EHorizontalAlignment& OutValue);
bool TryParseVerticalAlignment(const FString& InValue, EVerticalAlignment& OutValue);

void FillWidgetCommonPropertiesReadback(TSharedPtr<FJsonObject> ResultObj, const UWidget* Widget)
{
	if (!ResultObj || !Widget)
	{
		return;
	}

	ResultObj->SetStringField(TEXT("widget_class"), Widget->GetClass()->GetName());
	ResultObj->SetStringField(TEXT("visibility"), VisibilityToString(Widget->GetVisibility()));
	ResultObj->SetBoolField(TEXT("is_enabled"), Widget->GetIsEnabled());
	ResultObj->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
}

bool ApplyWidgetCommonPropertiesFields(const TSharedPtr<FJsonObject>& Params, UWidget* Widget, FString& OutError)
{
	if (!Params || !Widget)
	{
		OutError = TEXT("Invalid Params or Widget");
		return false;
	}

	FString VisibilityStr;
	if (Params->TryGetStringField(TEXT("visibility"), VisibilityStr))
	{
		ESlateVisibility ParsedVisibility = ESlateVisibility::Visible;
		if (!TryParseVisibility(VisibilityStr, ParsedVisibility))
		{
			OutError = FString::Printf(TEXT("Invalid visibility: %s"), *VisibilityStr);
			return false;
		}
		Widget->SetVisibility(ParsedVisibility);
	}

	bool bIsEnabled = false;
	if (Params->TryGetBoolField(TEXT("is_enabled"), bIsEnabled))
	{
		Widget->SetIsEnabled(bIsEnabled);
	}

	return true;
}

void FillUniformGridSlotReadback(TSharedPtr<FJsonObject> ResultObj, const UUniformGridSlot* GridSlot)
{
	if (!ResultObj || !GridSlot)
	{
		return;
	}

	ResultObj->SetStringField(TEXT("slot_class"), GridSlot->GetClass()->GetName());
	ResultObj->SetNumberField(TEXT("row"), GridSlot->GetRow());
	ResultObj->SetNumberField(TEXT("column"), GridSlot->GetColumn());
	ResultObj->SetNumberField(TEXT("horizontal_alignment"), static_cast<int32>(GridSlot->GetHorizontalAlignment()));
	ResultObj->SetNumberField(TEXT("vertical_alignment"), static_cast<int32>(GridSlot->GetVerticalAlignment()));
}

bool ApplyUniformGridSlotFields(const TSharedPtr<FJsonObject>& Params, UUniformGridSlot* GridSlot, FString& OutError)
{
	if (!Params || !GridSlot)
	{
		OutError = TEXT("Invalid Params or GridSlot");
		return false;
	}

	int32 IntValue = 0;
	if (Params->TryGetNumberField(TEXT("row"), IntValue))
	{
		GridSlot->SetRow(IntValue);
	}
	if (Params->TryGetNumberField(TEXT("column"), IntValue))
	{
		GridSlot->SetColumn(IntValue);
	}

	FString HorizontalAlignmentStr;
	if (Params->TryGetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentStr))
	{
		EHorizontalAlignment Alignment = HAlign_Fill;
		if (!TryParseHorizontalAlignment(HorizontalAlignmentStr, Alignment))
		{
			OutError = FString::Printf(TEXT("Invalid horizontal_alignment: %s"), *HorizontalAlignmentStr);
			return false;
		}
		GridSlot->SetHorizontalAlignment(Alignment);
	}

	FString VerticalAlignmentStr;
	if (Params->TryGetStringField(TEXT("vertical_alignment"), VerticalAlignmentStr))
	{
		EVerticalAlignment Alignment = VAlign_Fill;
		if (!TryParseVerticalAlignment(VerticalAlignmentStr, Alignment))
		{
			OutError = FString::Printf(TEXT("Invalid vertical_alignment: %s"), *VerticalAlignmentStr);
			return false;
		}
		GridSlot->SetVerticalAlignment(Alignment);
	}

	return true;
}

bool TryParseHorizontalAlignment(const FString& InValue, EHorizontalAlignment& OutValue)
{
	const FString Value = InValue.TrimStartAndEnd();
	if (Value.IsEmpty())
	{
		return false;
	}

	if (Value.Equals(TEXT("Fill"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("HAlign_Fill"), ESearchCase::IgnoreCase))
	{
		OutValue = HAlign_Fill; return true;
	}
	if (Value.Equals(TEXT("Left"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("HAlign_Left"), ESearchCase::IgnoreCase))
	{
		OutValue = HAlign_Left; return true;
	}
	if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("HAlign_Center"), ESearchCase::IgnoreCase))
	{
		OutValue = HAlign_Center; return true;
	}
	if (Value.Equals(TEXT("Right"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("HAlign_Right"), ESearchCase::IgnoreCase))
	{
		OutValue = HAlign_Right; return true;
	}
	return false;
}

bool TryParseVerticalAlignment(const FString& InValue, EVerticalAlignment& OutValue)
{
	const FString Value = InValue.TrimStartAndEnd();
	if (Value.IsEmpty())
	{
		return false;
	}

	if (Value.Equals(TEXT("Fill"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("VAlign_Fill"), ESearchCase::IgnoreCase))
	{
		OutValue = VAlign_Fill; return true;
	}
	if (Value.Equals(TEXT("Top"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("VAlign_Top"), ESearchCase::IgnoreCase))
	{
		OutValue = VAlign_Top; return true;
	}
	if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("VAlign_Center"), ESearchCase::IgnoreCase))
	{
		OutValue = VAlign_Center; return true;
	}
	if (Value.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("VAlign_Bottom"), ESearchCase::IgnoreCase))
	{
		OutValue = VAlign_Bottom; return true;
	}
	return false;
}

FString NormalizeWidgetClassKey(FString WidgetClassName)
{
	WidgetClassName.TrimStartAndEndInline();
	if (WidgetClassName.Len() > 1 &&
		WidgetClassName.StartsWith(TEXT("U")) &&
		FChar::IsUpper(WidgetClassName[1]))
	{
		WidgetClassName.RightChopInline(1, EAllowShrinking::No);
	}
	return WidgetClassName;
}

UClass* ResolveUMGWidgetClassByName(FString WidgetClassName)
{
	WidgetClassName = NormalizeWidgetClassKey(WidgetClassName);

	// Prefer direct class references for common widgets used by automation smokes.
	if (WidgetClassName.Equals(TEXT("CanvasPanel"), ESearchCase::IgnoreCase)) return UCanvasPanel::StaticClass();
	if (WidgetClassName.Equals(TEXT("UniformGridPanel"), ESearchCase::IgnoreCase)) return UUniformGridPanel::StaticClass();
	if (WidgetClassName.Equals(TEXT("TextBlock"), ESearchCase::IgnoreCase)) return UTextBlock::StaticClass();
	if (WidgetClassName.Equals(TEXT("Button"), ESearchCase::IgnoreCase)) return UButton::StaticClass();

	static const TMap<FString, FString> ClassPathMap = {
		{TEXT("CanvasPanel"), TEXT("/Script/UMG.CanvasPanel")},
		{TEXT("Border"), TEXT("/Script/UMG.Border")},
		{TEXT("VerticalBox"), TEXT("/Script/UMG.VerticalBox")},
		{TEXT("HorizontalBox"), TEXT("/Script/UMG.HorizontalBox")},
		{TEXT("UniformGridPanel"), TEXT("/Script/UMG.UniformGridPanel")},
		{TEXT("GridPanel"), TEXT("/Script/UMG.GridPanel")},
		{TEXT("Overlay"), TEXT("/Script/UMG.Overlay")},
		{TEXT("SizeBox"), TEXT("/Script/UMG.SizeBox")},
		{TEXT("ScrollBox"), TEXT("/Script/UMG.ScrollBox")},
		{TEXT("TextBlock"), TEXT("/Script/UMG.TextBlock")},
		{TEXT("Button"), TEXT("/Script/UMG.Button")},
		{TEXT("Image"), TEXT("/Script/UMG.Image")}
	};

	const FString* ClassPath = ClassPathMap.Find(WidgetClassName);
	if (!ClassPath)
	{
		return nullptr;
	}

	return StaticLoadClass(UWidget::StaticClass(), nullptr, **ClassPath);
}

void MarkCompileAndSaveWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return;
	}

	WidgetBlueprint->Modify();
	WidgetBlueprint->WidgetTree->Modify();
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	const FString BlueprintPath = GetWidgetBlueprintSavePath(WidgetBlueprint);
	if (!BlueprintPath.IsEmpty())
	{
		UEditorAssetLibrary::SaveAsset(BlueprintPath, false);
	}
}
}

FUnrealMCPUMGCommands::FUnrealMCPUMGCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandName == TEXT("create_umg_widget_blueprint"))
	{
		return HandleCreateUMGWidgetBlueprint(Params);
	}
	else if (CommandName == TEXT("add_text_block_to_widget"))
	{
		return HandleAddTextBlockToWidget(Params);
	}
	else if (CommandName == TEXT("get_widget_tree"))
	{
		return HandleGetWidgetTree(Params);
	}
	else if (CommandName == TEXT("ensure_widget_root"))
	{
		return HandleEnsureWidgetRoot(Params);
	}
	else if (CommandName == TEXT("add_widget_child"))
	{
		return HandleAddWidgetChild(Params);
	}
	else if (CommandName == TEXT("set_canvas_slot_layout"))
	{
		return HandleSetCanvasSlotLayout(Params);
	}
	else if (CommandName == TEXT("set_canvas_slot_layout_batch"))
	{
		return HandleSetCanvasSlotLayoutBatch(Params);
	}
	else if (CommandName == TEXT("set_uniform_grid_slot"))
	{
		return HandleSetUniformGridSlot(Params);
	}
	else if (CommandName == TEXT("set_uniform_grid_slot_batch"))
	{
		return HandleSetUniformGridSlotBatch(Params);
	}
	else if (CommandName == TEXT("clear_widget_children"))
	{
		return HandleClearWidgetChildren(Params);
	}
	else if (CommandName == TEXT("remove_widget_from_blueprint"))
	{
		return HandleRemoveWidgetFromBlueprint(Params);
	}
	else if (CommandName == TEXT("delete_widget_blueprints_by_prefix"))
	{
		return HandleDeleteWidgetBlueprintsByPrefix(Params);
	}
	else if (CommandName == TEXT("set_widget_common_properties"))
	{
		return HandleSetWidgetCommonProperties(Params);
	}
	else if (CommandName == TEXT("set_widget_common_properties_batch"))
	{
		return HandleSetWidgetCommonPropertiesBatch(Params);
	}
	else if (CommandName == TEXT("set_text_block_properties"))
	{
		return HandleSetTextBlockProperties(Params);
	}
	else if (CommandName == TEXT("add_widget_to_viewport"))
	{
		return HandleAddWidgetToViewport(Params);
	}
	else if (CommandName == TEXT("add_button_to_widget"))
	{
		return HandleAddButtonToWidget(Params);
	}
	else if (CommandName == TEXT("bind_widget_event"))
	{
		return HandleBindWidgetEvent(Params);
	}
	else if (CommandName == TEXT("set_text_block_binding"))
	{
		return HandleSetTextBlockBinding(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), BlueprintName) &&
		!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter (legacy: 'name')"));
	}

	// Create the full asset path
	FString PackagePath = TEXT("/Game/Widgets");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	PackagePath.TrimStartAndEndInline();
	if (PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game/Widgets");
	}
	if (!PackagePath.StartsWith(TEXT("/")))
	{
		PackagePath = TEXT("/") + PackagePath;
	}
	if (PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath.LeftChopInline(1, EAllowShrinking::No);
	}

	FString ParentClassName = TEXT("UserWidget");
	Params->TryGetStringField(TEXT("parent_class"), ParentClassName);
	if (!ParentClassName.IsEmpty() && !ParentClassName.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported parent_class '%s'. Only 'UserWidget' is currently supported."), *ParentClassName));
	}

	FString AssetName = BlueprintName;
	FString FullPath = PackagePath + TEXT("/") + AssetName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
	}

	// Create package
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	// Create Widget Blueprint using the dedicated UMG factory.
	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	if (!Factory)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create UWidgetBlueprintFactory"));
	}
	Factory->ParentClass = UUserWidget::StaticClass();
	Factory->BlueprintType = BPTYPE_Normal;

	UObject* CreatedObject = Factory->FactoryCreateNew(
		UWidgetBlueprint::StaticClass(),
		Package,
		FName(*AssetName),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn);

	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(CreatedObject);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}

	// Add a default Canvas Panel if one doesn't exist
	if (!WidgetBlueprint->WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
	}

	// Mark the package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("widget_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullPath);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleGetWidgetTree(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found by name or path: %s"), *BlueprintName));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));

	UWidget* RootWidget = WidgetBlueprint->WidgetTree ? WidgetBlueprint->WidgetTree->RootWidget : nullptr;
	ResultObj->SetBoolField(TEXT("has_root"), RootWidget != nullptr);
	if (RootWidget)
	{
		ResultObj->SetObjectField(TEXT("root"), BuildWidgetTreeNodeJson(RootWidget, TEXT("")));
	}
	else
	{
		ResultObj->SetObjectField(TEXT("root"), BuildWidgetTreeNodeJson(nullptr, TEXT("")));
	}

	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleEnsureWidgetRoot(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetClassName;
	if (!Params->TryGetStringField(TEXT("widget_class"), WidgetClassName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_class' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found by name or path: %s"), *BlueprintName));
	}
	if (!WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint has no WidgetTree"));
	}

	UClass* DesiredWidgetClass = ResolveUMGWidgetClassByName(WidgetClassName);
	if (!DesiredWidgetClass || !DesiredWidgetClass->IsChildOf(UWidget::StaticClass()))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported widget_class '%s'"), *WidgetClassName));
	}

	bool bReplaceExisting = false;
	Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

	UWidget* ExistingRoot = WidgetBlueprint->WidgetTree->RootWidget;
	bool bCreated = false;
	bool bReplaced = false;

	if (ExistingRoot)
	{
		const bool bClassMatches = ExistingRoot->GetClass() == DesiredWidgetClass;
		const bool bNameMatches = ExistingRoot->GetName().Equals(WidgetName, ESearchCase::CaseSensitive);

		if (bClassMatches && bNameMatches)
		{
			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetBoolField(TEXT("success"), true);
			ResultObj->SetBoolField(TEXT("created"), false);
			ResultObj->SetBoolField(TEXT("replaced"), false);
			ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
			ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
			ResultObj->SetObjectField(TEXT("root"), BuildWidgetTreeNodeJson(ExistingRoot, TEXT("")));
			return ResultObj;
		}

		// Prefer renaming the existing root when only the name differs. Replacing a root widget
		// can leave stale widget variable GUID metadata and triggers UMG compiler ensure paths.
		if (bClassMatches && !bNameMatches && bReplaceExisting)
		{
			ExistingRoot->Modify();
			ExistingRoot->Rename(*WidgetName, WidgetBlueprint->WidgetTree);
			MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetBoolField(TEXT("success"), true);
			ResultObj->SetBoolField(TEXT("created"), false);
			ResultObj->SetBoolField(TEXT("replaced"), false);
			ResultObj->SetBoolField(TEXT("renamed"), true);
			ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
			ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
			ResultObj->SetObjectField(TEXT("root"), BuildWidgetTreeNodeJson(ExistingRoot, TEXT("")));
			return ResultObj;
		}

		if (!bReplaceExisting)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Root widget already exists (%s/%s). Use replace_existing=true to replace."),
					*ExistingRoot->GetName(), *ExistingRoot->GetClass()->GetName()));
		}
	}

	UWidget* NewRoot = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(DesiredWidgetClass, *WidgetName);
	if (!NewRoot)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to construct root widget"));
	}

	WidgetBlueprint->WidgetTree->RootWidget = NewRoot;
	bCreated = true;
	bReplaced = ExistingRoot != nullptr;
	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetBoolField(TEXT("created"), bCreated);
	ResultObj->SetBoolField(TEXT("replaced"), bReplaced);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
	ResultObj->SetObjectField(TEXT("root"), BuildWidgetTreeNodeJson(WidgetBlueprint->WidgetTree->RootWidget, TEXT("")));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetChild(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetClassName;
	if (!Params->TryGetStringField(TEXT("widget_class"), WidgetClassName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_class' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	FString ParentWidgetName;
	Params->TryGetStringField(TEXT("parent_widget_name"), ParentWidgetName);

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found by name or path: %s"), *BlueprintName));
	}
	if (!WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint has no WidgetTree"));
	}

	if (WidgetBlueprint->WidgetTree->FindWidget(*WidgetName) != nullptr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' already exists in WidgetTree"), *WidgetName));
	}

	UWidget* ParentWidget = nullptr;
	if (ParentWidgetName.IsEmpty())
	{
		ParentWidget = WidgetBlueprint->WidgetTree->RootWidget;
	}
	else
	{
		ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(*ParentWidgetName);
	}

	if (!ParentWidget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Parent widget not found: %s"), ParentWidgetName.IsEmpty() ? TEXT("<root>") : *ParentWidgetName));
	}

	UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
	if (!ParentPanel)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Parent widget '%s' is not a panel widget"), *ParentWidget->GetName()));
	}

	UClass* ChildWidgetClass = ResolveUMGWidgetClassByName(WidgetClassName);
	if (!ChildWidgetClass || !ChildWidgetClass->IsChildOf(UWidget::StaticClass()))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported widget_class '%s'"), *WidgetClassName));
	}

	UWidget* NewChild = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(ChildWidgetClass, *WidgetName);
	if (!NewChild)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to construct child widget"));
	}

	UPanelSlot* AddedSlot = ParentPanel->AddChild(NewChild);
	if (!AddedSlot)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to add child '%s' to parent '%s'"), *WidgetName, *ParentWidget->GetName()));
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
	ResultObj->SetStringField(TEXT("parent_widget_name"), ParentWidget->GetName());
	ResultObj->SetStringField(TEXT("widget_name"), NewChild->GetName());
	ResultObj->SetStringField(TEXT("widget_class"), NewChild->GetClass()->GetName());
	ResultObj->SetStringField(TEXT("slot_class"), AddedSlot->GetClass()->GetName());
	ResultObj->SetObjectField(TEXT("widget"), BuildWidgetTreeNodeJson(NewChild, ParentWidget->GetName()));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetCanvasSlotLayout(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' is not in a CanvasPanelSlot"), *WidgetName));
	}

	FString ApplyError;
	if (!ApplyCanvasSlotLayoutFields(Params, CanvasSlot, ApplyError))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ApplyError);
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("widget_name"), Widget->GetName());
	FillCanvasSlotLayoutReadback(ResultObj, CanvasSlot);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetCanvasSlotLayoutBatch(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ItemsPtr = nullptr;
	if (!Params->TryGetArrayField(TEXT("items"), ItemsPtr) || !ItemsPtr || ItemsPtr->Num() == 0)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'items' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	TArray<TSharedPtr<FJsonValue>> Results;
	Results.Reserve(ItemsPtr->Num());

	for (int32 ItemIndex = 0; ItemIndex < ItemsPtr->Num(); ++ItemIndex)
	{
		const TSharedPtr<FJsonObject>* ItemObjPtr = nullptr;
		if (!(*ItemsPtr)[ItemIndex].IsValid() || !(*ItemsPtr)[ItemIndex]->TryGetObject(ItemObjPtr) || !ItemObjPtr || !ItemObjPtr->IsValid())
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] is not a valid object"), ItemIndex));
		}

		const TSharedPtr<FJsonObject>& ItemObj = *ItemObjPtr;
		FString WidgetName;
		if (!ItemObj->TryGetStringField(TEXT("widget_name"), WidgetName))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] missing 'widget_name'"), ItemIndex));
		}

		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
		if (!Widget)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
		}

		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (!CanvasSlot)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Widget '%s' is not in a CanvasPanelSlot"), *WidgetName));
		}

		FString ApplyError;
		if (!ApplyCanvasSlotLayoutFields(ItemObj, CanvasSlot, ApplyError))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] apply failed: %s"), ItemIndex, *ApplyError));
		}

		TSharedPtr<FJsonObject> ItemResult = MakeShared<FJsonObject>();
		ItemResult->SetNumberField(TEXT("index"), ItemIndex);
		ItemResult->SetStringField(TEXT("widget_name"), Widget->GetName());
		FillCanvasSlotLayoutReadback(ItemResult, CanvasSlot);
		Results.Add(MakeShared<FJsonValueObject>(ItemResult));
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
	ResultObj->SetNumberField(TEXT("updated_count"), Results.Num());
	ResultObj->SetArrayField(TEXT("results"), Results);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetUniformGridSlot(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Widget->Slot);
	if (!GridSlot)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' is not in a UniformGridSlot"), *WidgetName));
	}

	FString ApplyError;
	if (!ApplyUniformGridSlotFields(Params, GridSlot, ApplyError))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ApplyError);
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("widget_name"), Widget->GetName());
	FillUniformGridSlotReadback(ResultObj, GridSlot);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetUniformGridSlotBatch(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ItemsPtr = nullptr;
	if (!Params->TryGetArrayField(TEXT("items"), ItemsPtr) || !ItemsPtr || ItemsPtr->Num() == 0)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'items' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	TArray<TSharedPtr<FJsonValue>> Results;
	Results.Reserve(ItemsPtr->Num());

	for (int32 ItemIndex = 0; ItemIndex < ItemsPtr->Num(); ++ItemIndex)
	{
		const TSharedPtr<FJsonObject>* ItemObjPtr = nullptr;
		if (!(*ItemsPtr)[ItemIndex].IsValid() || !(*ItemsPtr)[ItemIndex]->TryGetObject(ItemObjPtr) || !ItemObjPtr || !ItemObjPtr->IsValid())
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] is not a valid object"), ItemIndex));
		}

		const TSharedPtr<FJsonObject>& ItemObj = *ItemObjPtr;
		FString WidgetName;
		if (!ItemObj->TryGetStringField(TEXT("widget_name"), WidgetName))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] missing 'widget_name'"), ItemIndex));
		}

		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
		if (!Widget)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
		}

		UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Widget->Slot);
		if (!GridSlot)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Widget '%s' is not in a UniformGridSlot"), *WidgetName));
		}

		FString ApplyError;
		if (!ApplyUniformGridSlotFields(ItemObj, GridSlot, ApplyError))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] apply failed: %s"), ItemIndex, *ApplyError));
		}

		TSharedPtr<FJsonObject> ItemResult = MakeShared<FJsonObject>();
		ItemResult->SetNumberField(TEXT("index"), ItemIndex);
		ItemResult->SetStringField(TEXT("widget_name"), Widget->GetName());
		FillUniformGridSlotReadback(ItemResult, GridSlot);
		Results.Add(MakeShared<FJsonValueObject>(ItemResult));
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
	ResultObj->SetNumberField(TEXT("updated_count"), Results.Num());
	ResultObj->SetArrayField(TEXT("results"), Results);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetCommonProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	FString ApplyError;
	if (!ApplyWidgetCommonPropertiesFields(Params, Widget, ApplyError))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ApplyError);
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("widget_name"), Widget->GetName());
	FillWidgetCommonPropertiesReadback(ResultObj, Widget);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetCommonPropertiesBatch(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ItemsPtr = nullptr;
	if (!Params->TryGetArrayField(TEXT("items"), ItemsPtr) || !ItemsPtr || ItemsPtr->Num() == 0)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'items' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	TArray<TSharedPtr<FJsonValue>> Results;
	Results.Reserve(ItemsPtr->Num());

	for (int32 ItemIndex = 0; ItemIndex < ItemsPtr->Num(); ++ItemIndex)
	{
		const TSharedPtr<FJsonObject>* ItemObjPtr = nullptr;
		if (!(*ItemsPtr)[ItemIndex].IsValid() || !(*ItemsPtr)[ItemIndex]->TryGetObject(ItemObjPtr) || !ItemObjPtr || !ItemObjPtr->IsValid())
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] is not a valid object"), ItemIndex));
		}

		const TSharedPtr<FJsonObject>& ItemObj = *ItemObjPtr;
		FString WidgetName;
		if (!ItemObj->TryGetStringField(TEXT("widget_name"), WidgetName))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] missing 'widget_name'"), ItemIndex));
		}

		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
		if (!Widget)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
		}

		FString ApplyError;
		if (!ApplyWidgetCommonPropertiesFields(ItemObj, Widget, ApplyError))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("items[%d] apply failed: %s"), ItemIndex, *ApplyError));
		}

		TSharedPtr<FJsonObject> ItemResult = MakeShared<FJsonObject>();
		ItemResult->SetNumberField(TEXT("index"), ItemIndex);
		ItemResult->SetStringField(TEXT("widget_name"), Widget->GetName());
		FillWidgetCommonPropertiesReadback(ItemResult, Widget);
		Results.Add(MakeShared<FJsonValueObject>(ItemResult));
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
	ResultObj->SetNumberField(TEXT("updated_count"), Results.Num());
	ResultObj->SetArrayField(TEXT("results"), Results);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetTextBlockProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(*WidgetName));
	if (!TextBlock)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("TextBlock not found: %s"), *WidgetName));
	}

	FString TextValue;
	if (Params->TryGetStringField(TEXT("text"), TextValue))
	{
		TextBlock->SetText(FText::FromString(TextValue));
	}

	FLinearColor Color;
	if (TryGetJsonLinearColor(Params, TEXT("color"), Color))
	{
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	const FLinearColor ReadbackColor = TextBlock->GetColorAndOpacity().GetSpecifiedColor();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("widget_name"), TextBlock->GetName());
	ResultObj->SetStringField(TEXT("widget_class"), TextBlock->GetClass()->GetName());
	ResultObj->SetStringField(TEXT("text"), TextBlock->GetText().ToString());
	ResultObj->SetArrayField(TEXT("color"), MakeJsonArrayFromLinearColor(ReadbackColor));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleClearWidgetChildren(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	Params->TryGetStringField(TEXT("widget_name"), WidgetName);

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	UWidget* TargetWidget = nullptr;
	if (WidgetName.IsEmpty())
	{
		TargetWidget = WidgetBlueprint->WidgetTree->RootWidget;
	}
	else
	{
		TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	}

	if (!TargetWidget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Target widget not found: %s"), WidgetName.IsEmpty() ? TEXT("<root>") : *WidgetName));
	}

	UPanelWidget* TargetPanel = Cast<UPanelWidget>(TargetWidget);
	if (!TargetPanel)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' is not a panel widget"), *TargetWidget->GetName()));
	}

	TArray<UWidget*> ChildrenToRemove;
	ChildrenToRemove.Reserve(TargetPanel->GetChildrenCount());
	for (int32 ChildIndex = 0; ChildIndex < TargetPanel->GetChildrenCount(); ++ChildIndex)
	{
		if (UWidget* Child = TargetPanel->GetChildAt(ChildIndex))
		{
			ChildrenToRemove.Add(Child);
		}
	}

	int32 RemovedDirectChildren = 0;
	int32 RemovedTotalWidgets = 0;
	for (UWidget* Child : ChildrenToRemove)
	{
		if (!Child)
		{
			continue;
		}

		RemovedTotalWidgets += CountWidgetSubtreeNodes(Child);
		if (WidgetBlueprint->WidgetTree->RemoveWidget(Child))
		{
			++RemovedDirectChildren;
		}
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
	ResultObj->SetStringField(TEXT("target_widget_name"), TargetWidget->GetName());
	ResultObj->SetStringField(TEXT("target_widget_class"), TargetWidget->GetClass()->GetName());
	ResultObj->SetNumberField(TEXT("removed_direct_children"), RemovedDirectChildren);
	ResultObj->SetNumberField(TEXT("removed_total_widgets"), RemovedTotalWidgets);
	ResultObj->SetNumberField(TEXT("remaining_child_count"), TargetPanel->GetChildrenCount());
	ResultObj->SetObjectField(TEXT("target_widget"), BuildWidgetTreeNodeJson(TargetWidget, TargetWidget == WidgetBlueprint->WidgetTree->RootWidget ? TEXT("") : TEXT("<parent>")));
	ResultObj->SetObjectField(TEXT("root"), BuildWidgetTreeNodeJson(WidgetBlueprint->WidgetTree->RootWidget, TEXT("")));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleRemoveWidgetFromBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found or invalid: %s"), *BlueprintName));
	}

	UWidget* TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!TargetWidget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	if (TargetWidget == WidgetBlueprint->WidgetTree->RootWidget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Removing root widget is not supported. Use ensure_widget_root/clear_widget_children instead."));
	}

	const FString ParentWidgetName = (TargetWidget->GetParent() != nullptr) ? TargetWidget->GetParent()->GetName() : FString();
	const int32 RemovedTotalWidgets = CountWidgetSubtreeNodes(TargetWidget);
	const bool bRemoved = WidgetBlueprint->WidgetTree->RemoveWidget(TargetWidget);
	if (!bRemoved)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to remove widget: %s"), *WidgetName));
	}

	MarkCompileAndSaveWidgetBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("asset_path"), GetWidgetBlueprintSavePath(WidgetBlueprint));
	ResultObj->SetStringField(TEXT("removed_widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("parent_widget_name"), ParentWidgetName);
	ResultObj->SetNumberField(TEXT("removed_total_widgets"), RemovedTotalWidgets);
	ResultObj->SetObjectField(TEXT("root"), BuildWidgetTreeNodeJson(WidgetBlueprint->WidgetTree->RootWidget, TEXT("")));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleDeleteWidgetBlueprintsByPrefix(const TSharedPtr<FJsonObject>& Params)
{
	FString ContentPath;
	if (!Params->TryGetStringField(TEXT("path"), ContentPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
	}

	FString NamePrefix;
	if (!Params->TryGetStringField(TEXT("name_prefix"), NamePrefix))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name_prefix' parameter"));
	}

	bool bRecursive = true;
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	bool bDryRun = false;
	Params->TryGetBoolField(TEXT("dry_run"), bDryRun);

	TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(ContentPath, bRecursive, false);
	TArray<TSharedPtr<FJsonValue>> MatchedAssets;
	TArray<TSharedPtr<FJsonValue>> DeletedAssets;
	TArray<TSharedPtr<FJsonValue>> FailedDeletes;

	for (const FString& AssetPath : AssetPaths)
	{
		FString PackagePath = AssetPath;
		const int32 DotIndex = PackagePath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (DotIndex != INDEX_NONE)
		{
			PackagePath = PackagePath.Left(DotIndex);
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
		if (!AssetName.StartsWith(NamePrefix, ESearchCase::CaseSensitive))
		{
			continue;
		}

		UObject* AssetObj = UEditorAssetLibrary::LoadAsset(PackagePath);
		if (!AssetObj || !AssetObj->IsA(UWidgetBlueprint::StaticClass()))
		{
			continue;
		}

		MatchedAssets.Add(MakeShared<FJsonValueString>(PackagePath));

		if (!bDryRun)
		{
			if (UEditorAssetLibrary::DeleteAsset(PackagePath))
			{
				DeletedAssets.Add(MakeShared<FJsonValueString>(PackagePath));
			}
			else
			{
				FailedDeletes.Add(MakeShared<FJsonValueString>(PackagePath));
			}
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("path"), ContentPath);
	ResultObj->SetStringField(TEXT("name_prefix"), NamePrefix);
	ResultObj->SetBoolField(TEXT("recursive"), bRecursive);
	ResultObj->SetBoolField(TEXT("dry_run"), bDryRun);
	ResultObj->SetNumberField(TEXT("matched_count"), MatchedAssets.Num());
	ResultObj->SetArrayField(TEXT("matched_assets"), MatchedAssets);
	if (!bDryRun)
	{
		ResultObj->SetNumberField(TEXT("deleted_count"), DeletedAssets.Num());
		ResultObj->SetArrayField(TEXT("deleted_assets"), DeletedAssets);
		ResultObj->SetNumberField(TEXT("failed_delete_count"), FailedDeletes.Num());
		ResultObj->SetArrayField(TEXT("failed_delete_assets"), FailedDeletes);
	}
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	// Find the Widget Blueprint by full path or short name
	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found by name or path: %s"), *BlueprintName));
	}

	// Get optional parameters
	FString InitialText = TEXT("New Text Block");
	Params->TryGetStringField(TEXT("text"), InitialText);

	FVector2D Position(0.0f, 0.0f);
	if (Params->HasField(TEXT("position")))
	{
		const TArray<TSharedPtr<FJsonValue>>* PosArray;
		if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
		{
			Position.X = (*PosArray)[0]->AsNumber();
			Position.Y = (*PosArray)[1]->AsNumber();
		}
	}

	// Create Text Block widget
	UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
	if (!TextBlock)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
	}

	// Set initial text
	TextBlock->SetText(FText::FromString(InitialText));

	// Add to canvas panel
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(TextBlock);
	PanelSlot->SetPosition(Position);

	// Mark the package dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("text"), InitialText);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	// Find the Widget Blueprint by full path or short name
	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget Blueprint not found by name or path: %s"), *BlueprintName));
	}

	// Get optional Z-order parameter
	int32 ZOrder = 0;
	Params->TryGetNumberField(TEXT("z_order"), ZOrder);

	// Create widget instance
	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get widget class"));
	}

	// Note: This creates the widget but doesn't add it to viewport
	// The actual addition to viewport should be done through Blueprint nodes
	// as it requires a game context

	// Create success response with instructions
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
	ResultObj->SetNumberField(TEXT("z_order"), ZOrder);
	ResultObj->SetStringField(TEXT("note"), TEXT("Widget class ready. Use CreateWidget and AddToViewport nodes in Blueprint to display in game."));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString ButtonText;
	if (!Params->TryGetStringField(TEXT("text"), ButtonText))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing text parameter"));
		return Response;
	}

	// Load the Widget Blueprint by full path or short name
	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
		return Response;
	}

	const FString BlueprintPath = GetWidgetBlueprintSavePath(WidgetBlueprint);

	// Create Button widget
	UButton* Button = NewObject<UButton>(WidgetBlueprint->GeneratedClass->GetDefaultObject(), UButton::StaticClass(), *WidgetName);
	if (!Button)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create Button widget"));
		return Response;
	}

	// Set button text
	UTextBlock* ButtonTextBlock = NewObject<UTextBlock>(Button, UTextBlock::StaticClass(), *(WidgetName + TEXT("_Text")));
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(FText::FromString(ButtonText));
		Button->AddChild(ButtonTextBlock);
	}

	// Get canvas panel and add button
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		Response->SetStringField(TEXT("error"), TEXT("Root widget is not a Canvas Panel"));
		return Response;
	}

	// Add to canvas and set position
	UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(Button);
	if (ButtonSlot)
	{
		const TArray<TSharedPtr<FJsonValue>>* Position;
		if (Params->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
		{
			FVector2D Pos(
				(*Position)[0]->AsNumber(),
				(*Position)[1]->AsNumber()
			);
			ButtonSlot->SetPosition(Pos);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Support both parameter shapes:
	// 1) New: blueprint_name + widget_name
	// 2) Legacy python tool: widget_name(blueprint) + widget_component_name(component)
	FString BlueprintName;
	FString WidgetName;
	Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
	Params->TryGetStringField(TEXT("widget_name"), WidgetName);

	if (BlueprintName.IsEmpty())
	{
		FString LegacyBlueprintName;
		FString LegacyWidgetComponentName;
		if (Params->TryGetStringField(TEXT("widget_name"), LegacyBlueprintName) &&
			Params->TryGetStringField(TEXT("widget_component_name"), LegacyWidgetComponentName))
		{
			BlueprintName = LegacyBlueprintName;
			WidgetName = LegacyWidgetComponentName;
		}
	}

	if (BlueprintName.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing blueprint name. Use 'blueprint_name' or legacy 'widget_name'."));
	}

	if (WidgetName.IsEmpty())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing widget component name. Use 'widget_name' or legacy 'widget_component_name'."));
	}

	FString EventName;
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing event_name parameter"));
	}

	// Load the Widget Blueprint by full path or short name
	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
	}

	const FString BlueprintPath = GetWidgetBlueprintSavePath(WidgetBlueprint);

	// Create the event graph if it doesn't exist
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
	if (!EventGraph)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
	}

	// Find the widget in the blueprint
	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find widget: %s"), *WidgetName));
	}

	const FName EventFName(*EventName);
	const FName WidgetPropertyName(*WidgetName);

	const FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), EventFName);
	if (DelegateProperty == nullptr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' does not expose multicast delegate '%s'."), *WidgetName, *EventName));
	}

	FObjectProperty* ComponentProperty = nullptr;
	if (WidgetBlueprint->SkeletonGeneratedClass != nullptr)
	{
		ComponentProperty = FindFProperty<FObjectProperty>(WidgetBlueprint->SkeletonGeneratedClass, WidgetPropertyName);
	}
	if (ComponentProperty == nullptr && WidgetBlueprint->GeneratedClass != nullptr)
	{
		ComponentProperty = FindFProperty<FObjectProperty>(WidgetBlueprint->GeneratedClass, WidgetPropertyName);
	}
	if (ComponentProperty == nullptr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget component '%s' is not a blueprint variable. Enable IsVariable in designer first."), *WidgetName));
	}

	const UK2Node_ComponentBoundEvent* ExistingBoundEvent =
		FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, EventFName, WidgetPropertyName);

	if (ExistingBoundEvent == nullptr)
	{
		FKismetEditorUtilities::CreateNewBoundEventForComponent(
			Widget,
			EventFName,
			WidgetBlueprint,
			ComponentProperty);

		ExistingBoundEvent = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, EventFName, WidgetPropertyName);
	}

	if (ExistingBoundEvent == nullptr)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create bound event '%s' for widget '%s'."), *EventName, *WidgetName));
	}

	UK2Node_ComponentBoundEvent* MutableBoundEvent = const_cast<UK2Node_ComponentBoundEvent*>(ExistingBoundEvent);
	if (MutableBoundEvent != nullptr)
	{
		FVector2D RequestedPosition(0.0f, 0.0f);
		bool bHasRequestedPosition = false;
		if (Params->HasField(TEXT("node_position")))
		{
			RequestedPosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
			bHasRequestedPosition = true;
		}

		if (bHasRequestedPosition)
		{
			MutableBoundEvent->NodePosX = static_cast<int32>(RequestedPosition.X);
			MutableBoundEvent->NodePosY = static_cast<int32>(RequestedPosition.Y);
		}
		else
		{
			float MaxHeight = 0.0f;
			for (UEdGraphNode* Node : EventGraph->Nodes)
			{
				MaxHeight = FMath::Max(MaxHeight, static_cast<float>(Node->NodePosY));
			}

			MutableBoundEvent->NodePosX = 200;
			MutableBoundEvent->NodePosY = static_cast<int32>(MaxHeight + 200.0f);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("event_name"), EventName);
	Response->SetStringField(TEXT("node_id"), ExistingBoundEvent->NodeGuid.ToString());
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) &&
		!Params->TryGetStringField(TEXT("text_block_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter (legacy: text_block_name)"));
		return Response;
	}

	FString BindingName;
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName) &&
		!Params->TryGetStringField(TEXT("binding_property"), BindingName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing binding_name parameter (legacy: binding_property)"));
		return Response;
	}

	// Load the Widget Blueprint by full path or short name
	UWidgetBlueprint* WidgetBlueprint = ResolveWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
		return Response;
	}

	const FString BlueprintPath = GetWidgetBlueprintSavePath(WidgetBlueprint);

	// Create a variable for binding if it doesn't exist
	FBlueprintEditorUtils::AddMemberVariable(
		WidgetBlueprint,
		FName(*BindingName),
		FEdGraphPinType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
	);

	// Find the TextBlock widget
	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)));
	if (!TextBlock)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find TextBlock widget: %s"), *WidgetName));
		return Response;
	}

	// Create binding function
	const FString FunctionName = FString::Printf(TEXT("Get%s"), *BindingName);
	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		WidgetBlueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (FuncGraph)
	{
		// Add the function to the blueprint with proper template parameter
		// Template requires null for last parameter when not using a signature-source
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(WidgetBlueprint, FuncGraph, false, nullptr);

		// Create entry node
		UK2Node_FunctionEntry* EntryNode = nullptr;
		
		// Create entry node - use the API that exists in UE 5.5
		EntryNode = NewObject<UK2Node_FunctionEntry>(FuncGraph);
		FuncGraph->AddNode(EntryNode, false, false);
		EntryNode->NodePosX = 0;
		EntryNode->NodePosY = 0;
		EntryNode->FunctionReference.SetExternalMember(FName(*FunctionName), WidgetBlueprint->GeneratedClass);
		EntryNode->AllocateDefaultPins();

		// Create get variable node
		UK2Node_VariableGet* GetVarNode = NewObject<UK2Node_VariableGet>(FuncGraph);
		GetVarNode->VariableReference.SetSelfMember(FName(*BindingName));
		FuncGraph->AddNode(GetVarNode, false, false);
		GetVarNode->NodePosX = 200;
		GetVarNode->NodePosY = 0;
		GetVarNode->AllocateDefaultPins();

		// Connect nodes
		UEdGraphPin* EntryThenPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* GetVarOutPin = GetVarNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
		if (EntryThenPin && GetVarOutPin)
		{
			EntryThenPin->MakeLinkTo(GetVarOutPin);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("binding_name"), BindingName);
	return Response;
} 
