// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
 

// Module includes
#include "OnlineSharingInterface.h"
#include "OnlineSubsystemFacebookPackage.h"

/**
 * Facebook implementation of the Online Sharing Interface
 */
class FOnlineSharingFacebook : public IOnlineSharing
{

public:

	//~ Begin IOnlineSharing Interface
	virtual bool ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead) override;
	virtual bool RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingReadCategory::Type NewPermissions) override;
	virtual bool ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate) override;
	virtual bool RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingPublishingCategory::Type NewPermissions, EOnlineStatusUpdatePrivacy::Type Privacy) override;
	virtual EOnlineCachedResult::Type GetCachedNewsFeed(int32 LocalUserNum, int32 NewsFeedIdx, FOnlineStatusUpdate& OutNewsFeed) override;
	virtual EOnlineCachedResult::Type GetCachedNewsFeeds(int32 LocalUserNum, TArray<FOnlineStatusUpdate>& OutNewsFeeds) override;
	//~ End IOnlineSharing Interface

	
public:

	/**
	 * Constructor used to indicate which OSS we are a part of
	 */
	FOnlineSharingFacebook(class FOnlineSubsystemFacebook* InSubsystem);
	
	/**
	 * Default destructor
	 */
	virtual ~FOnlineSharingFacebook();


private:
	
	/**
	 * Setup the permission categories with the relevant Facebook entries
	 */
	void SetupPermissionMaps();

	// The read permissions map which sets up the facebook permissions in their correct category
	typedef TMap< EOnlineSharingReadCategory::Type, NSArray* > FReadPermissionsMap;
	FReadPermissionsMap ReadPermissionsMap;

	// The publish permissions map which sets up the facebook permissions in their correct category
	typedef TMap< EOnlineSharingPublishingCategory::Type, NSArray* > FPublishPermissionsMap;
	FPublishPermissionsMap PublishPermissionsMap;


private:

	/** Reference to the main Facebook identity */
	IOnlineIdentityPtr IdentityInterface;
};


typedef TSharedPtr<FOnlineSharingFacebook, ESPMode::ThreadSafe> FOnlineSharingFacebookPtr;