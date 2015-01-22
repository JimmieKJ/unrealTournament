// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IProfilerClientModule.h: Declares the IProfilerClientModule interface.
=============================================================================*/

#pragma once

/**
 * Interface for ProfilerClient modules.
 */
class IProfilerClientModule
	: public IModuleInterface
{
public:
	/**
	 * Creates a Profiler client.
	 */
	virtual IProfilerClientPtr CreateProfilerClient() = 0;

protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IProfilerClientModule( ) { }
};
