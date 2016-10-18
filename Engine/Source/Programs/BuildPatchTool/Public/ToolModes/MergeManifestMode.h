// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ToolMode.h"

namespace BuildPatchTool
{
	class FMergeManifestToolModeFactory
	{
	public:
		static IToolModeRef Create(const TSharedRef<IBuildPatchServicesModule>& BpsInterface);
	};
}
