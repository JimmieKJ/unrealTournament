// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SImportPathDialog: public SWindow
{
public:
	SLATE_BEGIN_ARGS(SImportPathDialog)
	{
	}

	SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_END_ARGS()

	SImportPathDialog()
		: UserResponse(EAppReturnType::Cancel)
	{
		}

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

protected:
	void OnPathChange(const FString& NewPath);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	EAppReturnType::Type UserResponse;
	FText AssetPath;
	FText AssetName;
	FText Duration;

	static FText LastUsedAssetPath;
};


