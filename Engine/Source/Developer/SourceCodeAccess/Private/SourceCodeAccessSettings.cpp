// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SourceCodeAccessPrivatePCH.h"
#include "SourceCodeAccessSettings.h"

USourceCodeAccessSettings::USourceCodeAccessSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if PLATFORM_WINDOWS
	PreferredAccessor = TEXT("VisualStudioSourceCodeAccessor");
#elif PLATFORM_MAC
	PreferredAccessor = TEXT("XCodeSourceCodeAccessor");
#elif PLATFORM_LINUX
	GConfig->GetString(TEXT("/Script/SourceCodeAccess.SourceCodeAccessSettings"), TEXT("PreferredAccessor"), PreferredAccessor, GEngineIni);
	UE_LOG(LogHAL, Log, TEXT("Linux SourceCodeAccessSettings: %s"), *PreferredAccessor);
#endif
}