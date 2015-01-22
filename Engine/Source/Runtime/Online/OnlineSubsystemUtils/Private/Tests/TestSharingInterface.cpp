// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "OnlineSharingInterface.h"
#include "ModuleManager.h"
#include "ImageCore.h"
#include "TestSharingInterface.h"

FTestSharingInterface::FTestSharingInterface(const FString& InSubsystem)
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::FTestSharingInterface"));
	SubsystemName = InSubsystem;
}


FTestSharingInterface::~FTestSharingInterface()
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::~FTestSharingInterface"));
	
	if(TestStatusUpdate.Image)
	{
		delete TestStatusUpdate.Image;
		TestStatusUpdate.Image = NULL;
	}
}


void FTestSharingInterface::Test(UWorld* InWorld, bool bWithImage)
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::Test"));

	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(InWorld, FName(*SubsystemName));
	check(OnlineSub); 

	SharingInterface = OnlineSub->GetSharingInterface();
	check(SharingInterface.IsValid());

	TestStatusUpdate.Message = FString::Printf(TEXT("This is a test post for UE4 Sharing support! Date = %s"), *FDateTime::Now().ToString());
	TestStatusUpdate.PostPrivacy = EOnlineStatusUpdatePrivacy::OnlyMe;
	if( bWithImage )
	{
		TestStatusUpdate.Image = new FImage( 256, 256, ERawImageFormat::BGRA8, false );
	}

	// Kick off the first part of the test,
	RequestPermissionsToSharePosts();
}


void FTestSharingInterface::RequestPermissionsToSharePosts()
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::RequestPermissionsToSharePosts"));

	ResponsesReceived = 0;
	RequestPermissionsToPostToFeedDelegate = FOnRequestNewPublishPermissionsCompleteDelegate::CreateRaw(this, &FTestSharingInterface::OnStatusPostingPermissionsUpdated);

	// We need to be permitted to post on the users behalf to share this update
	EOnlineSharingPublishingCategory::Type PublishPermissions = EOnlineSharingPublishingCategory::Posts;

	for (int32 PlayerIndex = 0; PlayerIndex < MAX_LOCAL_PLAYERS; PlayerIndex++)
	{
		SharingInterface->AddOnRequestNewPublishPermissionsCompleteDelegate(PlayerIndex, RequestPermissionsToPostToFeedDelegate);
		SharingInterface->RequestNewPublishPermissions(PlayerIndex, PublishPermissions, TestStatusUpdate.PostPrivacy);
	}
}


void FTestSharingInterface::OnStatusPostingPermissionsUpdated(int32 LocalUserNum, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Display, TEXT("FTestSharingInterface::OnStatusPostingPermissionsUpdated() - %d"), bWasSuccessful);
	SharingInterface->ClearOnRequestNewPublishPermissionsCompleteDelegate(LocalUserNum, RequestPermissionsToPostToFeedDelegate);

	if( ++ResponsesReceived == MAX_LOCAL_PLAYERS )
	{
		SharePost();
	}
}


void FTestSharingInterface::SharePost()
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::SharePost"));

	ResponsesReceived = 0;
	OnPostSharedDelegate = FOnSharePostCompleteDelegate::CreateRaw(this, &FTestSharingInterface::OnPostShared);

	for (int32 PlayerIndex = 0; PlayerIndex < MAX_LOCAL_PLAYERS; PlayerIndex++)
	{
		SharingInterface->AddOnSharePostCompleteDelegate(PlayerIndex, OnPostSharedDelegate);
		SharingInterface->ShareStatusUpdate(PlayerIndex, TestStatusUpdate);
	}
}


void FTestSharingInterface::OnPostShared(int32 LocalPlayer, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::OnPostShared[PlayerIdx:%i - Successful:%i]"), LocalPlayer, bWasSuccessful);

	SharingInterface->ClearOnSharePostCompleteDelegate(LocalPlayer, OnPostSharedDelegate);
	if( ++ResponsesReceived == MAX_LOCAL_PLAYERS )
	{
		RequestPermissionsToReadNewsFeed();
	}
}


void FTestSharingInterface::RequestPermissionsToReadNewsFeed()
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::RequestPermissionsToReadNewsFeed"));

	ResponsesReceived = 0;
	RequestPermissionsToReadFeedDelegate = FOnRequestNewReadPermissionsCompleteDelegate::CreateRaw(this, &FTestSharingInterface::OnReadFeedPermissionsUpdated);

	// We need to be permitted to post on the users behalf to share this update
	EOnlineSharingReadCategory::Type ReadPermissions = EOnlineSharingReadCategory::Posts;

	for (int32 PlayerIndex = 0; PlayerIndex < MAX_LOCAL_PLAYERS; PlayerIndex++)
	{
		SharingInterface->AddOnRequestNewReadPermissionsCompleteDelegate(PlayerIndex, RequestPermissionsToReadFeedDelegate);
		SharingInterface->RequestNewReadPermissions(PlayerIndex, ReadPermissions);
	}
}


void FTestSharingInterface::OnReadFeedPermissionsUpdated(int32 LocalUserNum, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Display, TEXT("FTestSharingInterface::OnReadFeedPermissionsUpdated() - %d"), bWasSuccessful);
	SharingInterface->ClearOnRequestNewReadPermissionsCompleteDelegate(LocalUserNum, RequestPermissionsToReadFeedDelegate);

	if( ++ResponsesReceived == MAX_LOCAL_PLAYERS )
	{
		ReadNewsFeed();
	}
}


void FTestSharingInterface::ReadNewsFeed()
{
	UE_LOG(LogOnline, Verbose, TEXT("FTestSharingInterface::ReadNewsFeed"));

	ResponsesReceived = 0;
	OnNewsFeedReadDelegate = FOnReadNewsFeedCompleteDelegate::CreateRaw(this, &FTestSharingInterface::OnNewsFeedRead);

	for (int32 PlayerIndex = 0; PlayerIndex < MAX_LOCAL_PLAYERS; PlayerIndex++)
	{
		SharingInterface->AddOnReadNewsFeedCompleteDelegate(PlayerIndex, OnNewsFeedReadDelegate);
		SharingInterface->ReadNewsFeed(PlayerIndex);
	}
}


void FTestSharingInterface::OnNewsFeedRead(int32 LocalPlayer, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Display, TEXT("FTestSharingInterface::OnNewsFeedRead[PlayerIdx:%i - Successful:%i]"), LocalPlayer, bWasSuccessful);

	if (bWasSuccessful)
	{
		// Get the 1st cached post
		FOnlineStatusUpdate FirstReadStatusUpdate;
		SharingInterface->GetCachedNewsFeed(LocalPlayer, 0, FirstReadStatusUpdate);
		UE_LOG(LogOnline, Display, TEXT("FTestSharingInterface first read update: %s"), *FirstReadStatusUpdate.Message);

		// Get all the cached posts
		TArray<FOnlineStatusUpdate> AllReadStatusUpdates;
		SharingInterface->GetCachedNewsFeeds(LocalPlayer, AllReadStatusUpdates);
		UE_LOG(LogOnline, Display, TEXT("FTestSharingInterface number of read updates: %d"), AllReadStatusUpdates.Num());

		for (int Idx = 0; Idx < AllReadStatusUpdates.Num(); ++Idx)
		{
			const FOnlineStatusUpdate& StatusUpdate = AllReadStatusUpdates[Idx];
			UE_LOG(LogOnline, Display, TEXT("FTestSharingInterface status update [%d]: %s"), Idx, *StatusUpdate.Message);
		}
	}

	SharingInterface->ClearOnReadNewsFeedCompleteDelegate(LocalPlayer, OnNewsFeedReadDelegate);
	if( ++ResponsesReceived == MAX_LOCAL_PLAYERS )
	{
		UE_LOG(LogOnline, Display, TEXT("FTestSharingInterface TESTS COMPLETED"), LocalPlayer, bWasSuccessful);
		delete this;
	}
}