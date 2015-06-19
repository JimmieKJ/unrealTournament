// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNotificationList.h"

class SLATE_API INotificationWidget
{
public:
	virtual void OnSetCompletionState(SNotificationItem::ECompletionState State) = 0;
	virtual TSharedRef< SWidget > AsWidget() = 0;
};