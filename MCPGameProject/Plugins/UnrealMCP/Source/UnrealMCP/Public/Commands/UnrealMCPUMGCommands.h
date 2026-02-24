#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handles UMG (Widget Blueprint) related MCP commands
 * Responsible for creating and modifying UMG Widget Blueprints,
 * adding widget components, and managing widget instances in the viewport.
 */
class UNREALMCP_API FUnrealMCPUMGCommands
{
public:
    FUnrealMCPUMGCommands();

    /**
     * Handle UMG-related commands
     * @param CommandType - The type of command to handle
     * @param Params - JSON parameters for the command
     * @return JSON response with results or error
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Create a new UMG Widget Blueprint
     * @param Params - Must include "widget_name" (or legacy "name") for the blueprint name
     * @return JSON response with the created blueprint details
     */
    TSharedPtr<FJsonObject> HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * Read widget hierarchy from a UMG Widget Blueprint
     * @param Params - Must include:
     *                "blueprint_name" - Name/path of the target Widget Blueprint
     * @return JSON response with root widget tree and metadata
     */
    TSharedPtr<FJsonObject> HandleGetWidgetTree(const TSharedPtr<FJsonObject>& Params);

    /**
     * Ensure a widget blueprint has a root widget (create/reuse, optional replace)
     * @param Params - Must include:
     *                "blueprint_name" - Name/path of the target Widget Blueprint
     *                "widget_class" - Root widget class (e.g. CanvasPanel)
     *                "widget_name" - Root widget instance name
     *                Optional: "replace_existing" (bool)
     * @return JSON response with root widget details
     */
    TSharedPtr<FJsonObject> HandleEnsureWidgetRoot(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add a generic widget as a child of a panel widget in the widget tree
     * @param Params - Must include:
     *                "blueprint_name" - Name/path of the target Widget Blueprint
     *                "parent_widget_name" - Parent panel widget name (optional if using root)
     *                "widget_class" - Child widget class (e.g. VerticalBox, TextBlock)
     *                "widget_name" - Child widget instance name
     * @return JSON response with child widget and slot details
     */
    TSharedPtr<FJsonObject> HandleAddWidgetChild(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set layout properties for a widget in a CanvasPanelSlot
     * @param Params - Must include:
     *                "blueprint_name", "widget_name"
     *                Optional: "position", "size", "alignment", "anchors", "auto_size", "z_order"
     * @return JSON response with readback slot layout values
     */
    TSharedPtr<FJsonObject> HandleSetCanvasSlotLayout(const TSharedPtr<FJsonObject>& Params);

    /**
     * Batch-set layout properties for multiple widgets hosted in CanvasPanelSlot
     * @param Params - Must include:
     *                "blueprint_name"
     *                "items" - array of objects; each object must include "widget_name"
     *                          and may include same optional fields as set_canvas_slot_layout
     * @return JSON response with per-item readback layout values
     */
    TSharedPtr<FJsonObject> HandleSetCanvasSlotLayoutBatch(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set layout properties for a widget in a UniformGridSlot
     * @param Params - Must include:
     *                "blueprint_name", "widget_name"
     *                Optional: "row", "column", "horizontal_alignment", "vertical_alignment", "padding"
     * @return JSON response with readback slot layout values
     */
    TSharedPtr<FJsonObject> HandleSetUniformGridSlot(const TSharedPtr<FJsonObject>& Params);

    /**
     * Batch-set layout properties for multiple widgets hosted in UniformGridSlot
     * @param Params - Must include:
     *                "blueprint_name"
     *                "items" - array of objects; each object must include "widget_name"
     *                          and may include same optional fields as set_uniform_grid_slot
     * @return JSON response with per-item readback slot values
     */
    TSharedPtr<FJsonObject> HandleSetUniformGridSlotBatch(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set common UWidget properties useful for debug UI automation
     * @param Params - Must include "blueprint_name", "widget_name"
     *                Optional: "visibility", "is_enabled"
     * @return JSON response with readback values
     */
    TSharedPtr<FJsonObject> HandleSetWidgetCommonProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * Batch-set common UWidget properties useful for debug UI automation
     * @param Params - Must include:
     *                "blueprint_name"
     *                "items" - array of objects; each must include "widget_name"
     *                          and may include "visibility", "is_enabled"
     * @return JSON response with per-item readback values
     */
    TSharedPtr<FJsonObject> HandleSetWidgetCommonPropertiesBatch(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set common TextBlock properties useful for debug UI automation
     * @param Params - Must include "blueprint_name", "widget_name"
     *                Optional: "text", "color"
     * @return JSON response with readback values
     */
    TSharedPtr<FJsonObject> HandleSetTextBlockProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * Remove all direct children from a panel widget (optionally root) and their subtrees
     * @param Params - Must include "blueprint_name"
     *                Optional: "widget_name" (defaults to root widget)
     * @return JSON response with removal counts and readback tree
     */
    TSharedPtr<FJsonObject> HandleClearWidgetChildren(const TSharedPtr<FJsonObject>& Params);

    /**
     * Remove a widget from a WidgetBlueprint widget tree (removes subtree)
     * @param Params - Must include "blueprint_name", "widget_name"
     * @return JSON response with removal counts and readback tree
     */
    TSharedPtr<FJsonObject> HandleRemoveWidgetFromBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * Delete WidgetBlueprint assets under a path filtered by name prefix
     * @param Params - Must include:
     *                "path" - Content path (e.g. /Game/UI)
     *                "name_prefix" - Asset name prefix filter
     *                Optional: "recursive" (bool, default true), "dry_run" (bool, default false)
     * @return JSON response with matched/deleted asset paths
     */
    TSharedPtr<FJsonObject> HandleDeleteWidgetBlueprintsByPrefix(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add a Text Block widget to a UMG Widget Blueprint
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name for the new Text Block
     *                "text" - Initial text content (optional)
     *                "position" - [X, Y] position in the canvas (optional)
     * @return JSON response with the added widget details
     */
    TSharedPtr<FJsonObject> HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add a widget instance to the game viewport
     * @param Params - Must include:
     *                "blueprint_name" - Name of the Widget Blueprint to instantiate
     *                "z_order" - Z-order for widget display (optional)
     * @return JSON response with the widget instance details
     */
    TSharedPtr<FJsonObject> HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params);

    /**
     * Add a Button widget to a UMG Widget Blueprint
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name for the new Button
     *                "text" - Button text
     *                "position" - [X, Y] position in the canvas
     * @return JSON response with the added widget details
     */
    TSharedPtr<FJsonObject> HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * Bind an event to a widget (e.g. button click)
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" - Name of the widget to bind
     *                "event_name" - Name of the event to bind
     * @return JSON response with the binding details
     */
    TSharedPtr<FJsonObject> HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set up text block binding for dynamic updates
     * @param Params - Must include:
     *                "blueprint_name" - Name of the target Widget Blueprint
     *                "widget_name" (or legacy "text_block_name") - Name of the TextBlock to bind
     *                "binding_name" (or legacy "binding_property") - Name of the binding variable to set up
     * @return JSON response with the binding details
     */
    TSharedPtr<FJsonObject> HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params);
}; 
