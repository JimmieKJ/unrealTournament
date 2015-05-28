// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Delegate used for registering a widget for tick services. */
DECLARE_DELEGATE_RetVal_TwoParams(EActiveTimerReturnType, FWidgetActiveTimerDelegate, double /*InCurrentTime*/, float /*InDeltaTime*/);