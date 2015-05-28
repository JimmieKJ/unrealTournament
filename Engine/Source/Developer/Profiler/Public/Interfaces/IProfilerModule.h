// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// forward declarations
class IProfilerManager;
class ISessionManager;


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
	 * @param InSessionManager The session manager to use.
	 * @param ConstructUnderMajorTab The tab which will contain the profiler tabs.
	 *
	 */
	virtual TSharedRef<SWidget> CreateProfilerWindow( const TSharedRef<ISessionManager>& InSessionManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab ) = 0;

	/**
	 * @return a weak pointer to the global instance of the profiler manager.
	 */
	virtual TWeakPtr<IProfilerManager> GetProfilerManager() = 0;
};