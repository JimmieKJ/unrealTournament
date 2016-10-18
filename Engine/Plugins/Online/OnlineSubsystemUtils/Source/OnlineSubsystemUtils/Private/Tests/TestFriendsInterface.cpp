// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "ModuleManager.h"
#include "TestFriendsInterface.h"
#include "OnlinePresenceInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

void FTestFriendsInterface::Test(UWorld* InWorld, const TArray<FString>& Invites)
{
	OnlineSub = Online::GetSubsystem(InWorld, SubsystemName.Len() ? FName(*SubsystemName, FNAME_Find) : NAME_None);
	if (OnlineSub != NULL &&
		OnlineSub->GetIdentityInterface().IsValid() &&
		OnlineSub->GetFriendsInterface().IsValid())
	{
		// don't affect default friends list for MCP
		FString McpSubsystemString;
		MCP_SUBSYSTEM.ToString(McpSubsystemString);
		if (SubsystemName.Equals(McpSubsystemString, ESearchCase::IgnoreCase))
		{
			FriendsListName = TEXT("TestFriends");
		}

		// Add our delegate for the async call
		OnDeleteFriendCompleteDelegate = FOnDeleteFriendCompleteDelegate::CreateRaw(this, &FTestFriendsInterface::OnDeleteFriendComplete);
		OnQueryRecentPlayersCompleteDelegate = FOnQueryRecentPlayersCompleteDelegate::CreateRaw(this, &FTestFriendsInterface::OnQueryRecentPlayersComplete);

		OnDeleteFriendCompleteDelegateHandle       = OnlineSub->GetFriendsInterface()->AddOnDeleteFriendCompleteDelegate_Handle      (0, OnDeleteFriendCompleteDelegate);
		OnQueryRecentPlayersCompleteDelegateHandle = OnlineSub->GetFriendsInterface()->AddOnQueryRecentPlayersCompleteDelegate_Handle(OnQueryRecentPlayersCompleteDelegate);

		// list of pending users to send invites to
		for (int32 Idx=0; Idx < Invites.Num(); Idx++)
		{
			TSharedPtr<const FUniqueNetId> FriendId = OnlineSub->GetIdentityInterface()->CreateUniquePlayerId(Invites[Idx]);
			if (FriendId.IsValid())
			{
				InvitesToSend.Add(FriendId);
			}
		}

		// kick off next test
		StartNextTest();
	}
	else
	{
		UE_LOG(LogOnline, Warning,
			TEXT("Failed to get friends interface for %s"), *SubsystemName);
		
		FinishTest();
	}
}

void FTestFriendsInterface::StartNextTest()
{
	if (bReadFriendsList)
	{
		FOnReadFriendsListComplete Delegate = FOnReadFriendsListComplete::CreateRaw(this, &FTestFriendsInterface::OnReadFriendsComplete);
		OnlineSub->GetFriendsInterface()->ReadFriendsList(0, FriendsListName, Delegate);
	}
	else if (bQueryRecentPlayers)
	{
		if (OnlineSub->GetIdentityInterface().IsValid() &&
			OnlineSub->GetIdentityInterface()->GetUniquePlayerId(0).IsValid())
		{
			OnlineSub->GetFriendsInterface()->QueryRecentPlayers(*OnlineSub->GetIdentityInterface()->GetUniquePlayerId(0), RecentPlayersNamespace);
		}
		bQueryRecentPlayers = false;
	}
	else if (bAcceptInvites && InvitesToAccept.Num() > 0)
	{
		FOnAcceptInviteComplete Delegate = FOnAcceptInviteComplete::CreateRaw(this, &FTestFriendsInterface::OnAcceptInviteComplete);
		OnlineSub->GetFriendsInterface()->AcceptInvite(0, *InvitesToAccept[0], FriendsListName, Delegate);
	}
	else if (bSendInvites && InvitesToSend.Num() > 0)
	{
		FOnSendInviteComplete OnSendInviteCompleteDelegate = FOnSendInviteComplete::CreateRaw(this, &FTestFriendsInterface::OnSendInviteComplete);
		OnlineSub->GetFriendsInterface()->SendInvite(0, *InvitesToSend[0], FriendsListName, OnSendInviteCompleteDelegate);
	}
	else if (bDeleteFriends && FriendsToDelete.Num() > 0)
	{
		OnlineSub->GetFriendsInterface()->DeleteFriend(0, *FriendsToDelete[0], FriendsListName);
	}
	else if (bDeleteFriendsList)
	{
		FOnDeleteFriendsListComplete Delegate = FOnDeleteFriendsListComplete::CreateRaw(this, &FTestFriendsInterface::OnDeleteFriendsListComplete);
		OnlineSub->GetFriendsInterface()->DeleteFriendsList(0, FriendsListName, Delegate);
	}
	else
	{
		FinishTest();
	}

}

void FTestFriendsInterface::FinishTest()
{
	if (OnlineSub != NULL &&
		OnlineSub->GetFriendsInterface().IsValid())
	{
		// Clear delegates for the various async calls
		OnlineSub->GetFriendsInterface()->ClearOnDeleteFriendCompleteDelegate_Handle      (0, OnDeleteFriendCompleteDelegateHandle);
		OnlineSub->GetFriendsInterface()->ClearOnQueryRecentPlayersCompleteDelegate_Handle(OnQueryRecentPlayersCompleteDelegateHandle);
	}
	delete this;
}

void FTestFriendsInterface::OnReadFriendsComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log,
		TEXT("ReadFriendsList() for player (%d) was success=%d error=%s"), LocalPlayer, bWasSuccessful, *ErrorStr);

	if (bWasSuccessful)
	{
		TArray< TSharedRef<FOnlineFriend> > Friends;
		// Grab the friends data so we can print it out
		if (OnlineSub->GetFriendsInterface()->GetFriendsList(LocalPlayer, ListName, Friends))
		{
			UE_LOG(LogOnline, Log,
				TEXT("GetFriendsList(%d) returned %d friends"), LocalPlayer, Friends.Num());

			// Clear old entries
			InvitesToAccept.Empty();
			FriendsToDelete.Empty();
			// Log each friend's data out
			for (int32 Index = 0; Index < Friends.Num(); Index++)
			{
				const FOnlineFriend& Friend = *Friends[Index];
				const FOnlineUserPresence& Presence = Friend.GetPresence();
				UE_LOG(LogOnline, Log,
					TEXT("\t%s has unique id (%s)"), *Friend.GetDisplayName(), *Friend.GetUserId()->ToDebugString());
				UE_LOG(LogOnline, Log,
					TEXT("\t\t Invite status (%s)"), EInviteStatus::ToString(Friend.GetInviteStatus()));
				UE_LOG(LogOnline, Log,
					TEXT("\t\t Presence: %s"), *Presence.Status.StatusStr);
				UE_LOG(LogOnline, Log,
					TEXT("\t\t State: %s"), EOnlinePresenceState::ToString(Presence.Status.State));
				UE_LOG(LogOnline, Log,
					TEXT("\t\t bIsOnline (%s)"), Presence.bIsOnline ? TEXT("true") : TEXT("false"));
				UE_LOG(LogOnline, Log,
					TEXT("\t\t bIsPlaying (%s)"), Presence.bIsPlaying ? TEXT("true") : TEXT("false"));
				UE_LOG(LogOnline, Log,
					TEXT("\t\t bIsPlayingThisGame (%s)"), Presence.bIsPlayingThisGame ? TEXT("true") : TEXT("false"));
				UE_LOG(LogOnline, Log,
					TEXT("\t\t bIsJoinable (%s)"), Presence.bIsJoinable ? TEXT("true") : TEXT("false"));
				UE_LOG(LogOnline, Log,
					TEXT("\t\t bHasVoiceSupport (%s)"), Presence.bHasVoiceSupport ? TEXT("true") : TEXT("false"));

				// keep track of pending invites from the list and accept them
				if (Friend.GetInviteStatus() == EInviteStatus::PendingInbound)
				{
					InvitesToAccept.AddUnique(Friend.GetUserId());
				}
				// keep track of list of friends to delete
				FriendsToDelete.AddUnique(Friend.GetUserId());
			}
		}	
		else
		{
			UE_LOG(LogOnline, Log,
				TEXT("GetFriendsList(%d) failed"), LocalPlayer);
		}
	}
	
	// done with this part of the test
	bReadFriendsList = false;
	// kick off next test
	StartNextTest();
}

void FTestFriendsInterface::OnQueryRecentPlayersComplete(const FUniqueNetId& UserId, const FString& Namespace, bool bWasSuccessful, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log,
		TEXT("QueryRecentPlayers() for player (%s) was success=%d error=%s"), *UserId.ToDebugString(), bWasSuccessful, *ErrorStr);

	if (bWasSuccessful)
	{
		TArray< TSharedRef<FOnlineRecentPlayer> > Players;
		// Grab the friends data so we can print it out
		if (OnlineSub->GetFriendsInterface()->GetRecentPlayers(UserId, Namespace, Players))
		{
			UE_LOG(LogOnline, Log,
				TEXT("GetRecentPlayers returned %d players"), Players.Num());

			// Log each friend's data out
			for (auto RecentPlayer : Players)
			{
				UE_LOG(LogOnline, Log,
					TEXT("\t%s has unique id (%s)"), *RecentPlayer->GetDisplayName(), *RecentPlayer->GetUserId()->ToDebugString());
				UE_LOG(LogOnline, Log,
					TEXT("\t LastSeen (%s)"), *RecentPlayer->GetLastSeen().ToString());
			}
		}
	}

	// done with this part of the test
	bQueryRecentPlayers = false;
	// kick off next test
	StartNextTest();
}

void FTestFriendsInterface::OnAcceptInviteComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log, TEXT("AcceptInvite() for player (%d) from friend (%s) was success=%d. %s"), 
		LocalPlayer, *FriendId.ToDebugString(), bWasSuccessful, *ErrorStr);

	// done with this part of the test if no more invites to accept
	InvitesToAccept.RemoveAt(0);
	if (InvitesToAccept.Num() == 0)
	{
		bAcceptInvites = false;
		bReadFriendsList = true;
	}
	// kick off next test
	StartNextTest();
}

void FTestFriendsInterface::OnSendInviteComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log, TEXT("SendInvite() for player (%d) to friend (%s) was success=%d. %s"), 
		LocalPlayer, *FriendId.ToDebugString(), bWasSuccessful, *ErrorStr);

	// done with this part of the test if no more invites to send
	InvitesToSend.RemoveAt(0);
	if (InvitesToSend.Num() == 0)
	{
		bSendInvites = false;
		bReadFriendsList = true;
	}
	// kick off next test
	StartNextTest();
}

void FTestFriendsInterface::OnDeleteFriendComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log, TEXT("DeleteFriend() for player (%d) to friend (%s) was success=%d. %s"), 
		LocalPlayer, *FriendId.ToDebugString(), bWasSuccessful, *ErrorStr);

	// done with this part of the test if no more friends to delete
	FriendsToDelete.RemoveAt(0);
	if (bWasSuccessful && FriendsToDelete.Num() == 0)
	{
		bDeleteFriends = false;
		bReadFriendsList = true;
	}
	// kick off next test
	StartNextTest();
}

void FTestFriendsInterface::OnDeleteFriendsListComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	UE_LOG(LogOnline, Log,
		TEXT("DeleteFriendsList() for player (%d) was success=%d"), LocalPlayer, bWasSuccessful);

	// done with this part of the test
	bDeleteFriendsList = false;
	// kick off next test
	StartNextTest();
}

#endif //WITH_DEV_AUTOMATION_TESTS