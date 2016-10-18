// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef MALLOC_BINNED_VERSION_OVERRIDE
	#define MALLOC_BINNED_VERSION_OVERRIDE 1
#endif

#if MALLOC_BINNED_VERSION_OVERRIDE != 1 && MALLOC_BINNED_VERSION_OVERRIDE != 2
	#error MALLOC_BINNED_VERSION_OVERRIDE should be set to 1 or 2
#endif

#if MALLOC_BINNED_VERSION_OVERRIDE == 1
	#include "MallocBinned.h"
	typedef FMallocBinned FMallocBinnedRedirect;
#else
	#include "MallocBinned2.h"
	typedef FMallocBinned2 FMallocBinnedRedirect;
#endif
