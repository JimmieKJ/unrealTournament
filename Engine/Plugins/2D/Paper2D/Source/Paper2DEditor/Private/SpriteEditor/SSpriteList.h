// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// SSpriteList

class SSpriteList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSpriteList){}
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, TSharedPtr<class FSpriteEditor> InSpriteEditor);

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

protected:
	void RebuildWidget(UTexture2D* NewTextureFilter);

	void OnSpriteSelected(const class FAssetData& AssetData);
	void OnSpriteDoubleClicked(const class FAssetData& AssetData);
	bool CanShowColumnForAssetRegistryTag(FName AssetType, FName TagName) const;

protected:
	// Last source texture we saw (used to discern when the texture has changed)
	TWeakObjectPtr<UTexture2D> SourceTexturePtr;

	// Pointer back to owning sprite editor instance (the keeper of state)
	TWeakPtr<class FSpriteEditor> SpriteEditorPtr;

	// Set of tags to prevent creating details view columns for (infrequently used)
	TSet<FName> AssetRegistryTagsToIgnore;
};