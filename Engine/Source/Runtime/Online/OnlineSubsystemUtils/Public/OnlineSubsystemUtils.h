// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleManager.h"
#include "OnlineSubsystemUtilsModule.h"
#include "Online.h"

/** @return an initialized audio component specifically for use with VoIP */
#ifdef ONLINESUBSYSTEMUTILS_API
ONLINESUBSYSTEMUTILS_API class UAudioComponent* CreateVoiceAudioComponent(uint32 SampleRate);
ONLINESUBSYSTEMUTILS_API UWorld* GetWorldForOnline(FName InstanceName);

/**
 * Try to retrieve the active listen port for a server session
 */
ONLINESUBSYSTEMUTILS_API int32 GetPortFromNetDriver(FName InstanceName);
#endif

/** Macro to handle the boilerplate of accessing the proper online subsystem and getting the requested interface (UWorld version) */
#define IMPLEMENT_GET_INTERFACE(InterfaceType) \
static IOnline##InterfaceType##Ptr Get##InterfaceType##Interface(class UWorld* World, const FName SubsystemName = NAME_None) \
{ \
	IOnlineSubsystem* OSS = Online::GetSubsystem(World, SubsystemName); \
	return (OSS == NULL) ? NULL : OSS->Get##InterfaceType##Interface(); \
}

namespace Online
{
	static IOnlineSubsystem* GetSubsystem(UWorld* World, const FName& SubsystemName = NAME_None)
	{
#if UE_EDITOR // at present, multiple worlds are only possible in the editor
		FName Identifier = SubsystemName; 
		if (World != NULL)
		{
			FWorldContext& CurrentContext = GEngine->GetWorldContextFromWorldChecked(World);
			if (CurrentContext.WorldType == EWorldType::PIE)
			{ 
				Identifier = FName(*FString::Printf(TEXT("%s:%s"), SubsystemName != NAME_None ? *SubsystemName.ToString() : TEXT(""), *CurrentContext.ContextHandle.ToString()));
			} 
		}

		return IOnlineSubsystem::Get(Identifier); 
#else
		return IOnlineSubsystem::Get(SubsystemName); 
#endif
	}

	/** Reimplement all the interfaces of Online.h with support for UWorld accessors */
	IMPLEMENT_GET_INTERFACE(Session);
	IMPLEMENT_GET_INTERFACE(Party);
	IMPLEMENT_GET_INTERFACE(Friends);
	IMPLEMENT_GET_INTERFACE(User);
	IMPLEMENT_GET_INTERFACE(SharedCloud);
	IMPLEMENT_GET_INTERFACE(UserCloud);
	IMPLEMENT_GET_INTERFACE(Voice);
	IMPLEMENT_GET_INTERFACE(ExternalUI);
	IMPLEMENT_GET_INTERFACE(Time);
	IMPLEMENT_GET_INTERFACE(Identity);
	IMPLEMENT_GET_INTERFACE(TitleFile);
	IMPLEMENT_GET_INTERFACE(Entitlements);
	IMPLEMENT_GET_INTERFACE(Leaderboards);
	IMPLEMENT_GET_INTERFACE(Achievements);
}

#undef IMPLEMENT_GET_INTERFACE