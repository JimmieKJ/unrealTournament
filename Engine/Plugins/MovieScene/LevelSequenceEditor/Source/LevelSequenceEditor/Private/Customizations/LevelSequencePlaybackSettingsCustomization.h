// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Details view customization for the FLevelSequencePlaybackSettings struct.
 */
class FLevelSequencePlaybackSettingsCustomization
	: public IPropertyTypeCustomization
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

public:

	// IDetailCustomization interface

	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;

protected:

	/** Respond to a selection change event from the combo box. */
	void UpdateProperty();
	
private:

	/** Get the suffix to display after the custom entry box. */
	FText GetCustomSuffix() const;

	/** Array of loop modes. */
	struct FLoopMode
	{
		FText DisplayName;
		int32 Value;
	};

	TArray<TSharedPtr<FLoopMode>> LoopModes;

	/** The loop mode we're currently displaying. */
	TSharedPtr<FLoopMode> CurrentMode;
	/** The text of the current selection. */
	TSharedPtr<STextBlock> CurrentText;
	/** The loop number entry to be hidden and shown based on combo box selection. */
	TSharedPtr<SWidget> LoopEntry;

	/** Property handles of the properties we're editing */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> LoopCountProperty;
};
