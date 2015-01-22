// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DistCurveEditorModule.h"
#include "CurveEditorActions.h"


void FCurveEditorCommands::RegisterCommands()
{
	UI_COMMAND(RemoveCurve, "Remove Curve", "Remove Curve", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RemoveAllCurves, "Remove All Curves", "Remove All Curves", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(SetTime, "Set Time", "Set Time", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(SetValue, "Set Value", "Set Value", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(SetColor, "Set Color", "Set Color", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DeleteKeys, "Delete Key", "Delete Key", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ScaleTimes, "Scale All Times", "Scales the times of all points of all visible tracks", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ScaleValues, "Scale All Values", "Scales the values of all points of all visible tracks", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ScaleSingleCurveTimes, "Scale Curve Times", "Scales the times of all points of this curve & its sub-curves", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ScaleSingleCurveValues, "Scale Curve Values", "Scales the values of all points of this curve & its sub-curves", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ScaleSingleSubCurveValues, "Scale Sub-Curve Values", "Scales the values of all points of this curve", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(FitHorizontally, "Horizontal", "Fit Horizontally", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(FitVertically, "Vertical", "Fit Vertically", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(FitToAll, "All", "Fit to All", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(FitToSelected, "Selected", "Fit to Selected", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PanMode, "Pan", "Pan Mode", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ZoomMode, "Zoom", "Zoom Mode", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(CurveAuto, "Auto", "Curve Auto", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(CurveAutoClamped, "Auto/Clamped", "Curve Auto/Clamped", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(CurveUser, "User", "Curve User", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(CurveBreak, "Break", "Curve Break", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(Linear, "Linear", "Linear", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(Constant, "Constant", "Constant", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(FlattenTangents, "Flatten", "Flatten Tangents", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(StraightenTangents, "Straighten", "Straighten Tangents", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ShowAllTangents, "Show All", "ShowAll Tangents", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(CreateTab, "Create", "Create Tab", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DeleteTab, "Delete", "Delete Tab", EUserInterfaceActionType::Button, FInputGesture());
}
