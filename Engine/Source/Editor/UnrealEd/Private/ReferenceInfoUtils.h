// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ReferenceInfoUtils
{
	/**
	 * Outputs reference info to a log file
	 */
	void GenerateOutput(UWorld* InWorld, int32 Depth, bool bShowDefault, bool bShowScript);
}