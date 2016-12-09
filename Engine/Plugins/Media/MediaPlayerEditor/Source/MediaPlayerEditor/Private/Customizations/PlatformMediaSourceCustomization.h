// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class FAssetData;
class IDetailLayoutBuilder;
class IPropertyHandle;
class SWidget;
class UMediaSource;

/**
 * Implements a details view customization for the UPlatformMediaSource class.
 */
class FPlatformMediaSourceCustomization
	: public IDetailCustomization
{
public:

	//~ IDetailCustomization interface

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:

	/**
	 * Creates an instance of this class.
	 *
	 * @return The new instance.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FPlatformMediaSourceCustomization());
	}

protected:

	/**
	 * Makes a widget for the DefaultPlayers property value.
	 *
	 * @return The widget.
	 */
	TSharedRef<SWidget> MakePlatformMediaSourcesValueWidget();

	/**
	 * Set the value of the PlatformMediaSources property.
	 *
	 * @param MediaSource The media source to set.
	 */
	void SetPlatformMediaSourcesValue(FString PlatformName, UMediaSource* MediaSource);

private:

	/** Callback for when a per-platform media source property changed. */
	void HandleMediaSourcePropertyEntryBoxChanged(const FAssetData& AssetData, FString PlatformName);

	/** Callback for getting the currently selected object in a per-platform media source property. */
	FString HandleMediaSourcePropertyEntryObjectPath(FString PlatformName) const;

private:

	/** Pointer to the DefaultPlayers property handle. */
	TSharedPtr<IPropertyHandle> PlatformMediaSourcesProperty;
};
