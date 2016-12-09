// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ILauncherProfile.h"

/**
 * Delegate type for SProjectLauncher Profile running.
 *
 * The first parameter is the SProjectLauncher Profile that you want to run.
 */
DECLARE_DELEGATE_OneParam(FOnProfileRun, const ILauncherProfileRef&)
