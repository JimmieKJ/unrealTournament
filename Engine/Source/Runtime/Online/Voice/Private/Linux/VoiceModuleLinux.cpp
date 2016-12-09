// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceCodec.h"

bool InitVoiceCapture()
{
	return false;
}

void ShutdownVoiceCapture()
{
}

IVoiceCapture* CreateVoiceCaptureObject()
{
	return NULL; 
}

IVoiceEncoder* CreateVoiceEncoderObject()
{
	return NULL; 
}

IVoiceDecoder* CreateVoiceDecoderObject()
{
	return NULL; 
}
