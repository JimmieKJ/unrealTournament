// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_CEF3
namespace CEF3Utils
{
	/**
	 * Load the required modules for CEF3
	 */
	CEF3UTILS_API void LoadCEF3Modules();

	/**
	 * Unload the required modules for CEF3
	 */
	CEF3UTILS_API void UnloadCEF3Modules();
};
#endif //WITH_CEF3
