// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UWidgetTree;
class UWidgetBlueprint;
class UWidgetSlotPair;
struct FWidgetReference;

//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintEditorUtils

class FWidgetBlueprintEditorUtils
{
public:
	static bool VerifyWidgetRename(TSharedRef<class FWidgetBlueprintEditor> BlueprintEditor, FWidgetReference Widget, const FText& NewName, FText& OutErrorMessage);

	static bool RenameWidget(TSharedRef<class FWidgetBlueprintEditor> BlueprintEditor, const FName& OldName, const FName& NewName);

	static void CreateWidgetContextMenu(FMenuBuilder& MenuBuilder, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, FVector2D TargetLocation);

	static void CopyWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void PasteWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference ParentWidget, FVector2D PasteLocation);

	static void DeleteWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void CutWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static bool IsUsableWidgetClass(UClass* WidgetClass);

public:
	static void ExportWidgetsToText(TSet<UWidget*> WidgetsToExport, /*out*/ FString& ExportedText);

	static void ImportWidgetsFromText(UWidgetBlueprint* BP, const FString& TextToImport, /*out*/ TSet<UWidget*>& ImportedWidgetSet, /*out*/ TMap<FName, UWidgetSlotPair*>& PastedExtraSlotData);

	/** Exports the individual properties of an object to text and stores them in a map. */
	static void ExportPropertiesToText(UObject* Object, TMap<FName, FString>& ExportedProperties);

	/** Attempts to import any property in the map and apply it to a property with the same name on the object. */
	static void ImportPropertiesFromText(UObject* Object, const TMap<FName, FString>& ExportedProperties);

private:

	static void ExecuteOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets );

	static bool CanOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets );

	static bool FindAndRemoveNamedSlotContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree);

	static void BuildWrapWithMenu(FMenuBuilder& Menu, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void WrapWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass);

	static void BuildReplaceWithMenu(FMenuBuilder& Menu, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void ReplaceWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass);
};
