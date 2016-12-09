// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

//////////////////////////////////////////////////////////////////////////
// SGlobalOpenAssetDialog

class SGlobalOpenAssetDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGlobalOpenAssetDialog){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FVector2D InSize);

	// SWidget interface
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	// End of SWidget interface

protected:
	void OnAssetSelectedFromPicker(const class FAssetData& AssetData);
	void OnPressedEnterOnAssetsInPicker(const TArray<class FAssetData>& SelectedAssets);
};
