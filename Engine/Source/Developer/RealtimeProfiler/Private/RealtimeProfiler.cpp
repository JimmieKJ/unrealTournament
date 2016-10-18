// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RealtimeProfilerPrivatePCH.h"
#include "SlateBasics.h"
#include "RealtimeProfiler.h"

#include "VisualizerEvents.h"
#include "SRealtimeProfilerFrame.h"
#include "STaskGraph.h"

void FRealtimeProfiler::MakeWindow()
{
	Window = FSlateApplication::Get().AddWindow(
		SNew(SWindow)
		.Title( NSLOCTEXT("RealtimeProfiler", "WindowTitle", "Realtime Profiler" ) )
		.ClientSize( FVector2D(1024, 768) )
	);
}


FRealtimeProfiler::FRealtimeProfiler()
{
	MakeWindow();	
	StatsMasterEnableAdd();
}

FRealtimeProfiler::~FRealtimeProfiler()
{
	StatsMasterEnableSubtract();
}

bool FRealtimeProfiler::IsProfiling()
{
	return false;
}

void FRealtimeProfiler::Update(TSharedPtr< FVisualizerEvent > InProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame)
{

}

class FRealtimeProfilerModule : public IModuleInterface
{
public:
	virtual void ShutdownModule() override
	{
	}
};
IMPLEMENT_MODULE(FRealtimeProfilerModule, RealtimeProfiler);

