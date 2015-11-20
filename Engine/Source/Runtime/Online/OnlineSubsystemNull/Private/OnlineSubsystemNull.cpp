// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemNullPrivatePCH.h"
#include "OnlineSubsystemNull.h"
#include "OnlineAsyncTaskManagerNull.h"

#include "OnlineSessionInterfaceNull.h"
#include "OnlineLeaderboardInterfaceNull.h"
#include "OnlineIdentityNull.h"
#include "VoiceInterfaceImpl.h"
#include "OnlineAchievementsInterfaceNull.h"

IOnlineSessionPtr FOnlineSubsystemNull::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemNull::GetFriendsInterface() const
{
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemNull::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemNull::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemNull::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemNull::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemNull::GetEntitlementsInterface() const
{
	return nullptr;
};

IOnlineLeaderboardsPtr FOnlineSubsystemNull::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemNull::GetVoiceInterface() const
{
	return VoiceInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemNull::GetExternalUIInterface() const
{
	return nullptr;
}

IOnlineTimePtr FOnlineSubsystemNull::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemNull::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemNull::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemNull::GetStoreInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemNull::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemNull::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

IOnlineSharingPtr FOnlineSubsystemNull::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemNull::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemNull::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemNull::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemNull::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemNull::GetTurnBasedInterface() const
{
    return nullptr;
}

bool FOnlineSubsystemNull::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		OnlineAsyncTaskThreadRunnable->GameTick();
	}

 	if (SessionInterface.IsValid())
 	{
 		SessionInterface->Tick(DeltaTime);
 	}

	if (VoiceInterface.IsValid())
	{
		VoiceInterface->Tick(DeltaTime);
	}

	return true;
}

bool FOnlineSubsystemNull::Init()
{
	const bool bNullInit = true;
	
	if (bNullInit)
	{
		// Create the online async task thread
		OnlineAsyncTaskThreadRunnable = new FOnlineAsyncTaskManagerNull(this);
		check(OnlineAsyncTaskThreadRunnable);
		OnlineAsyncTaskThread = FRunnableThread::Create(OnlineAsyncTaskThreadRunnable, *FString::Printf(TEXT("OnlineAsyncTaskThreadNull %s"), *InstanceName.ToString()), 128 * 1024, TPri_Normal);
		check(OnlineAsyncTaskThread);
		UE_LOG_ONLINE(Verbose, TEXT("Created thread (ID:%d)."), OnlineAsyncTaskThread->GetThreadID());

 		SessionInterface = MakeShareable(new FOnlineSessionNull(this));
		LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsNull(this));
		IdentityInterface = MakeShareable(new FOnlineIdentityNull(this));
		AchievementsInterface = MakeShareable(new FOnlineAchievementsNull(this));
		VoiceInterface = MakeShareable(new FOnlineVoiceImpl(this));
		if (!VoiceInterface->Init())
		{
			VoiceInterface = nullptr;
		}
	}
	else
	{
		Shutdown();
	}

	return bNullInit;
}

bool FOnlineSubsystemNull::Shutdown()
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineSubsystemNull::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

	if (OnlineAsyncTaskThread)
	{
		// Destroy the online async task thread
		delete OnlineAsyncTaskThread;
		OnlineAsyncTaskThread = nullptr;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		delete OnlineAsyncTaskThreadRunnable;
		OnlineAsyncTaskThreadRunnable = nullptr;
	}

	if (VoiceInterface.IsValid())
	{
		VoiceInterface->Shutdown();
	}
	
 	#define DESTRUCT_INTERFACE(Interface) \
 	if (Interface.IsValid()) \
 	{ \
 		ensure(Interface.IsUnique()); \
 		Interface = nullptr; \
 	}
 
 	// Destruct the interfaces
	DESTRUCT_INTERFACE(VoiceInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
 	DESTRUCT_INTERFACE(SessionInterface);
	
	#undef DESTRUCT_INTERFACE
	
	return true;
}

FString FOnlineSubsystemNull::GetAppId() const
{
	return TEXT("");
}

bool FOnlineSubsystemNull::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

bool FOnlineSubsystemNull::IsEnabled()
{
	return true;
}
