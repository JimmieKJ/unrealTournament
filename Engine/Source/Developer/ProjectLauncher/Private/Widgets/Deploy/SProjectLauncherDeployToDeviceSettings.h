// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the deploy-to-device settings panel.
 */
class SProjectLauncherDeployToDeviceSettings
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherDeployToDeviceSettings) { }
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs - The Slate argument list.
	 * @param InModel - The data model.
	 */
	void Construct(	const FArguments& InArgs, const FProjectLauncherModelRef& InModel, EVisibility InShowAdvanced = EVisibility::Visible );

protected:

private:

	/** Handles check state changes of the 'Incremental' check box. */
	void HandleIncrementalCheckBoxCheckStateChanged( ECheckBoxState NewState );

	/** Handles determining the checked state of the 'Incremental' check box. */
	ECheckBoxState HandleIncrementalCheckBoxIsChecked( ) const;

	// Holds a pointer to the data model.
	FProjectLauncherModelPtr Model;
};
