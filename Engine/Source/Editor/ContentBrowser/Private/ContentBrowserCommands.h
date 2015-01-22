// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FContentBrowserCommands : public TCommands<FContentBrowserCommands>
{
public:
	FContentBrowserCommands()
		: TCommands<FContentBrowserCommands>(TEXT("ContentBrowser"), NSLOCTEXT( "ContentBrowser", "ContentBrowser", "Content Browser" ), NAME_None, FEditorStyle::GetStyleSetName() )
	{
	}

	virtual void RegisterCommands() override;

public:

	TSharedPtr< FUICommandInfo > OpenAssetsOrFolders;
	TSharedPtr< FUICommandInfo > PreviewAssets;
	TSharedPtr< FUICommandInfo > DirectoryUp;
};