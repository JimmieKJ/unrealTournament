// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IUserInfo
{
public:
	virtual const EOnlinePresenceState::Type GetOnlineStatus() const = 0;
	virtual const FString GetAppId() const = 0;
	virtual const FString GetName() const = 0;
	virtual const FSlateBrush* GetPresenceBrush() const = 0;
};