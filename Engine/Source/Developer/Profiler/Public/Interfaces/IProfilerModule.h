// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IProfilerModule.h: Declares the IProfilerModule interface.
=============================================================================*/

#pragma once

/**
 * Interface for the profiler module.
 */
class IProfilerModule
	: public IModuleInterface
{
public:

	/**
	 * Creates the main window for the profiler.
	 *
	 * @param InSessionManager			- the session manager to use
	 * @param ConstructUnderMajorTab	- the tab which will contain the profiler tabs
	 *
	 */
	virtual TSharedRef<SWidget> CreateProfilerWindow( const ISessionManagerRef& InSessionManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab ) = 0;


	/**
	 * @return a weak pointer to the global instance of the profiler manager.
	 */
	virtual TWeakPtr<class IProfilerManager> GetProfilerManager() = 0;
};