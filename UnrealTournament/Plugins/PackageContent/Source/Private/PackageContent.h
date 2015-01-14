// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LevelEditor.h"

class FPackageContent : public TSharedFromThis< FPackageContent >
{
public:
	~FPackageContent();

	static TSharedRef< FPackageContent > Create();

private:
	FPackageContent(const TSharedRef< FUICommandList >& InActionList);
	void Initialize();


	void OpenPackageContentWindow();

	void CreatePackageContentMenu(FToolBarBuilder& Builder);
	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);
	
	TSharedRef< FUICommandList > ActionList;

	TSharedPtr< FExtender > MenuExtender;

};