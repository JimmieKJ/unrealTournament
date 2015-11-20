// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// Global editor common commands
// Note: There is no real global command concept, so these must still be registered in each editor
class UNREALED_API FGlobalEditorCommonCommands : public TCommands< FGlobalEditorCommonCommands >
{
public:
	FGlobalEditorCommonCommands();

	virtual void RegisterCommands() override;

	static void MapActions(TSharedRef<FUICommandList>& ToolkitCommands);

protected:
	static void OnPressedCtrlTab(TSharedPtr<FUICommandInfo> TriggeringCommand);
	static void OnSummonedAssetPicker();
	static void OnSummonedConsoleCommandBox();

	DEPRECATED(4.9, "OpenPopup() returning a window is deprecated. Use OpenPopupMenu() that returns an IMenu.")
	static TSharedPtr<SWindow> OpenPopup(TSharedRef<SWidget> WindowContents, const FVector2D& PopupDesiredSize);

	static TSharedPtr<IMenu> OpenPopupMenu(TSharedRef<SWidget> WindowContents, const FVector2D& PopupDesiredSize);
public:
	TSharedPtr<FUICommandInfo> FindInContentBrowser;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigation;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigationAlternate;
	TSharedPtr<FUICommandInfo> SummonOpenAssetDialog;
	TSharedPtr<FUICommandInfo> SummonOpenAssetDialogAlternate;
	TSharedPtr<FUICommandInfo> OpenDocumentation;
	TSharedPtr<FUICommandInfo> ViewReferences;
	TSharedPtr<FUICommandInfo> ViewSizeMap;
	TSharedPtr<FUICommandInfo> OpenConsoleCommandBox;
};

