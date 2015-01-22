// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ProfilerModule.cpp: Implements the FProfilerModule class.
=============================================================================*/

#include "ProfilerPrivatePCH.h"
#include "SDockTab.h"

/**
 * Implements the FProfilerModule module.
 */
class FProfilerModule
	: public IProfilerModule
{
public:
	virtual TSharedRef<SWidget> CreateProfilerWindow( const ISessionManagerRef& InSessionManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab ) override
	{
		ProfilerManager = FProfilerManager::Initialize( InSessionManager );
		TSharedRef<SProfilerWindow> ProfilerWindow = SNew(SProfilerWindow);
		FProfilerManager::Get()->AssignProfilerWindow( ProfilerWindow );
		// Register OnTabClosed to handle profiler manager shutdown.
		ConstructUnderMajorTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateRaw(this, &FProfilerModule::Shutdown) );

		return ProfilerWindow;
	}

	virtual void StartupModule() override
	{}

	virtual void ShutdownModule() override
	{
		if( FProfilerManager::Get().IsValid() )
		{
			FProfilerManager::Get()->Shutdown();
		}
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual TWeakPtr<class IProfilerManager> GetProfilerManager() override
	{
		return ProfilerManager;
	}

protected:
	/** Shutdowns the profiler manager. */
	void Shutdown( TSharedRef<SDockTab> TabBeingClosed )
	{
		FProfilerManager::Get()->Shutdown();
		TabBeingClosed->SetOnTabClosed( SDockTab::FOnTabClosedCallback() );
	}

	/** A weak pointer to the global instance of the profiler manager. Will last as long as profiler window is visible. */
	TWeakPtr<class IProfilerManager> ProfilerManager;
};

IMPLEMENT_MODULE(FProfilerModule, Profiler);