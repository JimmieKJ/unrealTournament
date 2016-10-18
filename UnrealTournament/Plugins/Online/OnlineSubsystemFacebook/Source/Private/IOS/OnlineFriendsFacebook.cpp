// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "OnlineSubsystemFacebookPrivatePCH.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineSharingFacebook.h"
#include "OnlineFriendsFacebook.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>

// FOnlineFriendFacebook

TSharedRef<const FUniqueNetId> FOnlineFriendFacebook::GetUserId() const
{
	return UserId;
}

FString FOnlineFriendFacebook::GetRealName() const
{
	FString Result;
	GetAccountData(TEXT("name"), Result);
	return Result;
}

FString FOnlineFriendFacebook::GetDisplayName(const FString& Platform) const
{
	FString Result;
	GetAccountData(TEXT("username"), Result);
	return Result;
}

bool FOnlineFriendFacebook::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return GetAccountData(AttrName, OutAttrValue);
}

EInviteStatus::Type FOnlineFriendFacebook::GetInviteStatus() const
{
	return EInviteStatus::Accepted;
}

const FOnlineUserPresence& FOnlineFriendFacebook::GetPresence() const
{
	return Presence;
}

// FOnlineFriendsFacebook

FOnlineFriendsFacebook::FOnlineFriendsFacebook(FOnlineSubsystemFacebook* InSubsystem) 
{
	// Get our handle to the identity interface
	IdentityInterface = InSubsystem->GetIdentityInterface();
	check( IdentityInterface.IsValid() );
	SharingInterface = InSubsystem->GetSharingInterface();
	check( SharingInterface.IsValid() );
	GConfig->GetArray(TEXT("OnlineSubsystemFacebook.OnlineFriendsFacebook"), TEXT("FriendsFields"), FriendsFields, GEngineIni);	
	
	// always required fields
	FriendsFields.AddUnique(TEXT("name"));
}


FOnlineFriendsFacebook::FOnlineFriendsFacebook()
{
}


FOnlineFriendsFacebook::~FOnlineFriendsFacebook()
{
	IdentityInterface = NULL;
	SharingInterface = NULL;
}

bool FOnlineFriendsFacebook::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate /*= FOnReadFriendsListComplete()*/)
{
	bool bRequestTriggered = false;
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::ReadFriendsList()"));

	// To read friends, we need to be logged in and have authorization.
	if( IdentityInterface->GetLoginStatus( LocalUserNum ) == ELoginStatus::LoggedIn )
	{
		bRequestTriggered = true;

		auto RequestFriendsReadPermissionsDelegate = FOnRequestNewReadPermissionsCompleteDelegate::CreateRaw(this, &FOnlineFriendsFacebook::OnReadFriendsPermissionsUpdated);
		RequestFriendsReadPermissionsDelegateHandle = SharingInterface->AddOnRequestNewReadPermissionsCompleteDelegate_Handle(LocalUserNum, RequestFriendsReadPermissionsDelegate);
		SharingInterface->RequestNewReadPermissions(LocalUserNum, EOnlineSharingReadCategory::Friends);
		OnReadFriendsListCompleteDelegate = Delegate;
	}
	else
	{
		UE_LOG(LogOnline, Verbose, TEXT("Cannot read friends if we are not logged into facebook."));

		// Cannot read friends if we are not logged into facebook.
		Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("not logged in."));
	}

	return bRequestTriggered;
}

bool FOnlineFriendsFacebook::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate /*= FOnDeleteFriendsListComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, ListName, FString(TEXT("DeleteFriendsList() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate /*= FOnSendInviteComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("SendInvite() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate /*= FOnAcceptInviteComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("AcceptInvite() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("RejectInvite() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("DeleteFriend() is not supported")));
	return false;
}

bool FOnlineFriendsFacebook::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::GetFriendsList()"));

	for (int32 Idx=0; Idx < CachedFriends.Num(); Idx++)
	{
		OutFriends.Add(CachedFriends[Idx]);
	}

	return true;
}

TSharedPtr<FOnlineFriend> FOnlineFriendsFacebook::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedPtr<FOnlineFriend> Result;

	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::GetFriend()"));

	for (int32 Idx=0; Idx < CachedFriends.Num(); Idx++)
	{
		if (*(CachedFriends[Idx]->GetUserId()) == FriendId)
		{
			Result = CachedFriends[Idx];
			break;
		}
	}

	return Result;
}

bool FOnlineFriendsFacebook::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::IsFriend()"));

	TSharedPtr<FOnlineFriend> Friend = GetFriend(LocalUserNum,FriendId,ListName);
	if (Friend.IsValid() &&
		Friend->GetInviteStatus() == EInviteStatus::Accepted)
	{
		return true;
	}
	return false;
}

bool FOnlineFriendsFacebook::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::QueryRecentPlayers()"));

	TriggerOnQueryRecentPlayersCompleteDelegates(UserId, Namespace, false, TEXT("not implemented"));

	return false;
}

bool FOnlineFriendsFacebook::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers)
{
	return false;
}

bool FOnlineFriendsFacebook::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsFacebook::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsFacebook::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	return false;
}

bool FOnlineFriendsFacebook::GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers)
{
	return false;
}

void FOnlineFriendsFacebook::OnReadFriendsPermissionsUpdated(int32 LocalUserNum, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::OnReadPermissionsUpdated() - %d"), bWasSuccessful);
	SharingInterface->ClearOnRequestNewReadPermissionsCompleteDelegate_Handle(LocalUserNum, RequestFriendsReadPermissionsDelegateHandle);

	if( bWasSuccessful )
	{
		ReadFriendsUsingGraphPath(LocalUserNum, EFriendsLists::ToString(EFriendsLists::Default));
	}
	else
	{
		// Permissions werent applied so we cannot read friends.
		OnReadFriendsListCompleteDelegate.ExecuteIfBound(LocalUserNum, false, EFriendsLists::ToString(EFriendsLists::Default), TEXT("No read permissions"));
	}
}

void FOnlineFriendsFacebook::ReadFriendsUsingGraphPath(int32 LocalUserNum, const FString& ListName)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsFacebook::ReadFriendsUsingGraphPath()"));

	dispatch_async(dispatch_get_main_queue(),^ 
		{
			// We need to determine what the graph path we are querying is.
			// Facebook permissions only return friends who have our app installed.
			NSMutableString* GraphPath = [[NSMutableString alloc] init];
			[GraphPath appendString:@"/me/friends"];

			NSMutableDictionary *parameters = [NSMutableDictionary dictionaryWithDictionary: @{@"limit" : @"50"}];
			
			// Optional list of fields to query for each friend
			if(FriendsFields.Num() > 0)
			{
				FString FieldsStr;
				for (int32 Idx=0; Idx < FriendsFields.Num(); Idx++)
				{
					FieldsStr += FriendsFields[Idx];
					if (Idx < (FriendsFields.Num()-1))
					{
						FieldsStr += TEXT(",");
					}
				}
				
				[parameters setObject:[NSString stringWithFString:FieldsStr] forKey:@"fields"];
			}

			UE_LOG(LogOnline, Verbose, TEXT("GraphPath=%s"), *FString(GraphPath));

			[[[FBSDKGraphRequest alloc] initWithGraphPath:GraphPath parameters:parameters HTTPMethod:@"GET"]
				startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection, id result, NSError *error)
				{
					bool bSuccess = error == nil;

					if(error)
					{
			 			 UE_LOG(LogOnline, Display, TEXT("Error=%s"), *FString([error localizedDescription]));
					}
					else
					{
                        NSArray* friends = [[NSArray alloc] initWithArray:[result objectForKey:@"data"]];

						UE_LOG(LogOnline, Display, TEXT("Found %i friends"), [friends count]);

						// Clear our previosly cached friends before we repopulate the cache.
						CachedFriends.Empty();

						for( int32 FriendIdx = 0; FriendIdx < [friends count]; FriendIdx++ )
						{	
							NSDictionary* user = friends[ FriendIdx ];		
							const FString Id([user objectForKey : @"objectID"]);
							// Add new friend entry to list
							TSharedRef<FOnlineFriendFacebook> FriendEntry(
								new FOnlineFriendFacebook(*Id)
								);
							FriendEntry->AccountData.Add(
								TEXT("id"), *Id
								);
							FriendEntry->AccountData.Add(
								TEXT("name"), *FString([user objectForKey:@"name"])
								);
							FriendEntry->AccountData.Add(
								TEXT("username"), *FString([user objectForKey:@"name"])
								);
							CachedFriends.Add(FriendEntry);

							UE_LOG(LogOnline, Verbose, TEXT("GCFriend - Id:%s - NickName:%s - RealName:%s"), 
								*FriendEntry->GetUserId()->ToString(), *FriendEntry->GetDisplayName(), *FriendEntry->GetRealName() );	
						}
					}

					// Did this operation complete? Let whoever is listening know.
					OnReadFriendsListCompleteDelegate.ExecuteIfBound(LocalUserNum, bSuccess, ListName, FString());
				}
			];
		}
	);
}

void FOnlineFriendsFacebook::DumpBlockedPlayers() const
{
}