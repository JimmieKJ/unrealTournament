// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __ASSETNOTIFICATIONS_H__
#define __ASSETNOTIFICATIONS_H__

#include "Core.h"

class FAssetNotifications
{
public:
	/** Inform about change in skeleton asset that requires saving*/
	UNREALED_API static void SkeletonNeedsToBeSaved(class USkeleton* Skeleton);
};

#endif

// EOF

