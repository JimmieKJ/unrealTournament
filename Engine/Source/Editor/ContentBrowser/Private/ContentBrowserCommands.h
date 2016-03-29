// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class FContentBrowserCommands
	: public TCommands<FContentBrowserCommands>
{
public:

	/** Default constructor. */
	FContentBrowserCommands()
		: TCommands<FContentBrowserCommands>(TEXT("ContentBrowser"), NSLOCTEXT( "ContentBrowser", "ContentBrowser", "Content Browser" ), NAME_None, FEditorStyle::GetStyleSetName() )
	{ }

public:

	//~ TCommands interface

	virtual void RegisterCommands() override;

public:

	TSharedPtr<FUICommandInfo> CreateNewFolder;
	TSharedPtr<FUICommandInfo> DirectoryUp;
	TSharedPtr<FUICommandInfo> OpenAssetsOrFolders;
	TSharedPtr<FUICommandInfo> PreviewAssets;
};
