// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CurveViewerCommands.h"

#define LOCTEXT_NAMESPACE "CurveViewerCommands"

void FCurveViewerCommands::RegisterCommands()
{
	UI_COMMAND( AddCurve, "Add Curve", "Add a curve to the Skeleton", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
