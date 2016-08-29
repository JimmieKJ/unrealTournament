// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemModule.h"
#include "ModuleManager.h"

#include "OnlineIdentityInterface.h"
#include "OnlineFriendsInterface.h"
#include "OnlineEventsInterface.h"
#include "OnlineSessionInterface.h"
#include "OnlinePartyInterface.h"
#include "OnlineExternalUIInterface.h"
#include "VoiceInterface.h"
#include "OnlineTitleFileInterface.h"
#include "OnlineAchievementsInterface.h"
#include "OnlinePresenceInterface.h"
#include "OnlineUserCloudInterface.h"
#include "OnlineUserInterface.h"
#include "OnlineFriendsInterface.h" //Needed for online friend functionality

/** Macro to handle the boilerplate of accessing the proper online subsystem and getting the requested interface */
#define IMPLEMENT_GET_INTERFACE(InterfaceType) \
	static IOnline##InterfaceType##Ptr Get##InterfaceType##Interface(const FName SubsystemName = NAME_None) \
{ \
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(SubsystemName); \
	return (OSS == NULL) ? NULL : OSS->Get##InterfaceType##Interface(); \
}

/** Helpers for accessing all the online features available in the online subsystem */
namespace Online
{
	/** 
	 * Get the interface for accessing the session services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate session service
	 */
	IMPLEMENT_GET_INTERFACE(Session);

	/**
	 * Get the interface for accessing the party services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate party service
	 */
	IMPLEMENT_GET_INTERFACE(Party);

	/**
	 * Get the interface for accessing the chat services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate party service
	 */
	IMPLEMENT_GET_INTERFACE(Chat);

	/** 
	 * Get the interface for accessing the player friends services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate friend service
	 */
	IMPLEMENT_GET_INTERFACE(Friends);

	/** 
	 * Get the interface for accessing user information by uniqueid
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate user service
	 */
	IMPLEMENT_GET_INTERFACE(User);

	/** 
	 * Get the interface for sharing user files in the cloud
	 * @return Interface pointer for the appropriate cloud service
	 */
	IMPLEMENT_GET_INTERFACE(SharedCloud);

	/** 
	 * Get the interface for accessing user files in the cloud
	 * @return Interface pointer for the appropriate cloud service
	 */
	IMPLEMENT_GET_INTERFACE(UserCloud);

	/** 
	 * Get the interface for accessing voice services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate voice service
	 */
	IMPLEMENT_GET_INTERFACE(Voice);

	/** 
	 * Get the interface for accessing the external UIs of a service
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate external UI service
	 */
	IMPLEMENT_GET_INTERFACE(ExternalUI);

	/** 
	 * Get the interface for accessing the server time from an online service
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate server time service
	 */
	IMPLEMENT_GET_INTERFACE(Time);

	/** 
	 * Get the interface for accessing identity online services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate identity service
	 */
	IMPLEMENT_GET_INTERFACE(Identity);
	
	/** 
	 * Get the interface for accessing title file online services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate service
	 */
	IMPLEMENT_GET_INTERFACE(TitleFile);

	/** 
	 * Get the interface for accessing entitlements online services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate service
	 */
	IMPLEMENT_GET_INTERFACE(Entitlements);

	/** 
	 * Get the interface for accessing platform leaderboards
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate leaderboard service
	 */
	IMPLEMENT_GET_INTERFACE(Leaderboards);

	/** 
	 * Get the interface for accessing entitlements online services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate service
	 */
	IMPLEMENT_GET_INTERFACE(Achievements);

	/** 
	 * Get the interface for accessing online events
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate service
	 */
	IMPLEMENT_GET_INTERFACE(Events);

	/** 
	 * Get the interface for accessing rich presence online services
	 * @param SubsystemName - Name of the requested online service
	 * @return Interface pointer for the appropriate service
	 */
	IMPLEMENT_GET_INTERFACE(Presence);
};

#undef IMPLEMENT_GET_INTERFACE