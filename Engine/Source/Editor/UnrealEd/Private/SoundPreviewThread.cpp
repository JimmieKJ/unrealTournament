// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "SoundPreviewThread.h"
#include "SoundDefinitions.h"
#include "Sound/SoundWave.h"

/**
 *
 * @param PreviewCount	number of sounds to preview
 * @param Node			sound node to perform compression/decompression
 * @param Info			preview info class
 */
FSoundPreviewThread::FSoundPreviewThread( int32 PreviewCount, USoundWave *Wave, FPreviewInfo *Info ) : 
	Count( PreviewCount ),
	SoundWave( Wave ),
	PreviewInfo( Info ),
	TaskFinished( false ),
	CancelCalculations( false )
{
}

bool FSoundPreviewThread::Init( void )
{
	return true;
}

uint32 FSoundPreviewThread::Run( void )
{
	// Get the original wave
	SoundWave->RemoveAudioResource();
	SoundWave->InitAudioResource( SoundWave->RawData );

	for( Index = 0; Index < Count && !CancelCalculations; Index++ )
	{
		SoundWaveQualityPreview( SoundWave, PreviewInfo + Index );
	}

	SoundWave->RemoveAudioResource();
	TaskFinished = true;
	return 0;
}

void FSoundPreviewThread::Stop( void )
{
	CancelCalculations = true;
}

void FSoundPreviewThread::Exit( void )
{
}

