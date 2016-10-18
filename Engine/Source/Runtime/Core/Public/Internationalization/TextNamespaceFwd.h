// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// USE_STABLE_LOCALIZATION_KEYS controls whether we attempt to keep localization keys stable when editing text properties.
// This requires WITH_EDITORONLY_DATA, but can also be optionally disabled in editor builds by changing its value to 0.
#if WITH_EDITORONLY_DATA
	#define USE_STABLE_LOCALIZATION_KEYS (1)
#else	// WITH_EDITORONLY_DATA
	#define USE_STABLE_LOCALIZATION_KEYS (0)
#endif	// WITH_EDITORONLY_DATA
