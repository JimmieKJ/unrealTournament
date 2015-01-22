// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Delegate type for SProjectLauncher Profile running.
 *
 * The first parameter is the SProjectLauncher Profile that you want to run.
 */
DECLARE_DELEGATE_OneParam(FOnProfileRun, const ILauncherProfileRef&)