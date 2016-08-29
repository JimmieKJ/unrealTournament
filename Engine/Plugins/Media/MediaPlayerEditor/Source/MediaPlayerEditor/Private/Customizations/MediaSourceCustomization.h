// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"


class IPropertyHandle;
class SWidget;


/**
 * Implements a details view customization for the UMediaSource class.
 */
class FMediaSourceCustomization
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
		return MakeShareable(new FMediaSourceCustomization());
	}

protected:

	/**
	 * Makes a widget for the DefaultPlayers property value.
	 *
	 * @return The widget.
	 */
	TSharedRef<SWidget> MakeDefaultPlayersValueWidget() const;

private:

	/** Pointer to the DefaultPlayers property handle. */
	TSharedPtr<IPropertyHandle> DefaultPlayersProperty;
};
