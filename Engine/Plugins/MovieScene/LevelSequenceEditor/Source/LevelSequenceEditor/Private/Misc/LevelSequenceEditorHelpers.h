// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class LevelSequenceEditorHelpers
{
public:

	/** Open dialog for creating a master sequence */
	static void OpenMasterSequenceDialog(const TSharedRef<FTabManager>& TabManager);
	
	/** Create a level sequence asset given an asset name and package path */
	static UObject* CreateLevelSequenceAsset(const FString& AssetName, const FString& PackagePath, UObject* AssetToDuplicate = nullptr);
};
