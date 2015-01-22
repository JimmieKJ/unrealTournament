// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for ProfilerService modules.
 */
class IProfilerServiceModule
	: public IModuleInterface
{
public:

	/** 
	 * Creates a profiler service.
	 *
	 * @param Capture Specifies whether to start capturing immediately.
	 * @return A new profiler service.
	 */
	virtual IProfilerServiceManagerPtr CreateProfilerServiceManager( ) = 0;

protected:

	/** Virtual destructor */
	virtual ~IProfilerServiceModule( ) { }
};
