// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IUserInfo
{
public:
	virtual const EOnlinePresenceState::Type GetOnlineStatus() const = 0;
	virtual const FString GetClientId() const = 0;
	virtual const FString GetName() const = 0;
};