// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerWidgetsPrivatePCH.h"
#include "STimeSlider.h"
#include "STimeRange.h"
#include "ModuleManager.h"


IMPLEMENT_MODULE( FSequencerWidgetsModule, SequencerWidgets );

void FSequencerWidgetsModule::StartupModule()
{

}

void FSequencerWidgetsModule::ShutdownModule()
{

}

TSharedRef<ITimeSlider> FSequencerWidgetsModule::CreateTimeSlider( const TSharedRef<ITimeSliderController>& InController, bool bMirrorLabels )
{
	return
		SNew( STimeSlider, InController )
		.MirrorLabels( bMirrorLabels );
}

TSharedRef<ITimeSlider> FSequencerWidgetsModule::CreateTimeSlider( const TSharedRef<ITimeSliderController>& InController, const TAttribute<EVisibility>& VisibilityDelegate, bool bMirrorLabels )
{
	return
		SNew( STimeSlider, InController )
		.Visibility(VisibilityDelegate)
		.MirrorLabels( bMirrorLabels );
}

TSharedRef<ITimeSlider> FSequencerWidgetsModule::CreateTimeRange( const TSharedRef<ITimeSliderController>& InController, const TAttribute<EVisibility>& VisibilityDelegate, const TAttribute<bool>& ShowFrameNumbersDelegate, const TAttribute<float>& TimeSnapIntervalDelegate )
{
	return 
		SNew( STimeRange, InController)
		.Visibility(VisibilityDelegate)
		.ShowFrameNumbers(ShowFrameNumbersDelegate)
		.TimeSnapInterval(TimeSnapIntervalDelegate);
}

