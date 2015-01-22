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
	static void OnPressedCtrlTab();
	static void OnSummonedAssetPicker();
	static void OnSummonedConsoleCommandBox();
public:
	TSharedPtr<FUICommandInfo> FindInContentBrowser;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigation;
	TSharedPtr<FUICommandInfo> SummonOpenAssetDialog;
	TSharedPtr<FUICommandInfo> OpenDocumentation;
	TSharedPtr<FUICommandInfo> ViewReferences;
	TSharedPtr<FUICommandInfo> OpenConsoleCommandBox;
};

