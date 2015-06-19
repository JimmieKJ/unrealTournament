// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IClanInfo
{
public:
	virtual ~IClanInfo() {}

	virtual FText GetTitle() const = 0;
	virtual FText GetDescription() const = 0;
	virtual FText GetLongDescription() const = 0;
	virtual TArray<TSharedPtr<class IFriendItem>> GetMemberList() const = 0;
	virtual FText GetClanBrushName() const = 0;
	virtual int32 GetClanMembersCount() const = 0;

	DECLARE_EVENT(IClanInfo, FChangedEvent)
	virtual FChangedEvent& OnChanged() = 0;
};
