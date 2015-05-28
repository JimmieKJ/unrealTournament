// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Templates/SharedPointer.h"

struct FNavigationDirtyArea;

class ENGINE_API FNavDataGenerator : public TSharedFromThis<FNavDataGenerator, ESPMode::ThreadSafe>
{
public:
	virtual ~FNavDataGenerator() {}

	/** Rebuilds all known navigation data */
	virtual bool RebuildAll() { return false; };

	/** Blocks until build is complete */
	virtual void EnsureBuildCompletion() {};

	/** Cancels build, may block until current running async tasks are finished */
	virtual void CancelBuild() {};

	/**  */
	virtual void TickAsyncBuild(float DeltaSeconds) {};
	
	/**  */
	virtual void OnNavigationBoundsChanged() {};

	/** Asks generator to update navigation affected by DirtyAreas */
	virtual void RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& DirtyAreas) {}

	/** determines whether this generator is performing navigation building actions at the moment*/
	virtual bool IsBuildInProgress(bool bCheckDirtyToo = false) const { return false; }	

	/** Called when world is loaded and all actors are initialized - to be used for post-load steps like data validating */
	virtual void OnWorldInitDone(bool bAllowedToRebuild) {}	

	/** Returns number of remaining tasks till build is complete
	 */
	virtual int32 GetNumRemaningBuildTasks() const { return 0; };

	/** Returns number of currently running tasks
	 */
	virtual int32 GetNumRunningBuildTasks() const { return 0; };

	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	virtual uint32 LogMemUsed() const { return 0; }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual void ExportNavigationData(const FString& FileName) const {}
#endif

};