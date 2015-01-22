// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Defines commands for the SCurveEditor.
 */
class UNREALED_API FRichCurveEditorCommands : public TCommands<FRichCurveEditorCommands>
{
public:
	FRichCurveEditorCommands()
		: TCommands<FRichCurveEditorCommands>
		(
			TEXT("RichCurveEditor"),
			NSLOCTEXT("Contexts", "RichCurveEditor", "Curve Editor"),
			NAME_None,
			FEditorStyle::GetStyleSetName()
		)
	{
	}

	TSharedPtr<FUICommandInfo> ZoomToFitHorizontal;
	TSharedPtr<FUICommandInfo> ZoomToFitVertical;
	TSharedPtr<FUICommandInfo> ZoomToFitAll;
	TSharedPtr<FUICommandInfo> ZoomToFitSelected;
	TSharedPtr<FUICommandInfo> ToggleSnapping;
	TSharedPtr<FUICommandInfo> InterpolationConstant;
	TSharedPtr<FUICommandInfo> InterpolationLinear;
	TSharedPtr<FUICommandInfo> InterpolationCubicAuto;
	TSharedPtr<FUICommandInfo> InterpolationCubicUser;
	TSharedPtr<FUICommandInfo> InterpolationCubicBreak;

public:
	virtual void RegisterCommands() override;
};