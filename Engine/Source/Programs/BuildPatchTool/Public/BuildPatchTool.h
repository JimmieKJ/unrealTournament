// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogBuildPatchTool, Log, All);

namespace BuildPatchTool
{
	enum class EReturnCode : int32
	{
		OK = 0,
		UnknownError,
		ArgumentProcessingError,
		UnknownToolMode,
		FileNotFound,
		ToolFailure,
	};
}