// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#ifndef __ASSETNOTIFICATIONS_H__
#define __ASSETNOTIFICATIONS_H__

#include "CoreMinimal.h"

class FAssetNotifications
{
public:
	/** Inform about change in skeleton asset that requires saving*/
	UNREALED_API static void SkeletonNeedsToBeSaved(const class USkeleton* Skeleton);
};

#endif

// EOF

