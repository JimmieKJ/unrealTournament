// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "DeviceProfileEditorSingleProfileView"


/**
 * Slate widget to allow users to select device profiles
 */
class  SDeviceProfileEditorSingleProfileView
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceProfileEditorSingleProfileView) {}
		SLATE_DEFAULT_SLOT( FArguments, Content )
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, TWeakObjectPtr< UDeviceProfile > InDeviceProfile );

	/** Destructor */
	~SDeviceProfileEditorSingleProfileView(){}

private:

	/** The profile selected from the current list. */
	TWeakObjectPtr< UDeviceProfile > EditingProfile;

	/** Holds the details view. */
	TSharedPtr<IDetailsView> SettingsView;
};


#undef LOCTEXT_NAMESPACE
