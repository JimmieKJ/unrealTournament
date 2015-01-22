// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IProfilerManager.h: Declares the IProfilerManager interface.
=============================================================================*/

#pragma once

/** Type definition for shared pointers to instances of IProfilerManager. */
typedef TSharedPtr<class IProfilerManager> IProfilerManagerPtr;

/** Type definition for shared references to instances of IProfilerManager. */
typedef TSharedRef<class IProfilerManager> IProfilerManagerRef;

/**
 * Interface for profiler manager.
 */
class IProfilerManager
{
public:
	/** Virtual destructor. */
	virtual ~IProfilerManager()
	{}
};