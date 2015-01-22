// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerWidgetsPrivatePCH.h"
#include "STimeSlider.h"
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

