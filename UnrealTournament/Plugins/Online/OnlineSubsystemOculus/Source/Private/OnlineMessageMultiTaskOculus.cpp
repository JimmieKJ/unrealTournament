// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemOculusPrivatePCH.h"
#include "OnlineMessageMultiTaskOculus.h"

void FOnlineMessageMultiTaskOculus::AddNewRequest(ovrRequest RequestId)
{
	OculusSubsystem.AddRequestDelegate(
		RequestId,
		FOculusMessageOnCompleteDelegate::CreateLambda([this, RequestId](ovrMessageHandle Message, bool bIsError)
	{
		InProgressRequests.Remove(RequestId);
		if (bIsError)
		{
			bDidAllRequestsFinishedSuccessfully = false;
		}

		if (InProgressRequests.Num() == 0)
		{
			Delegate.ExecuteIfBound();
		}
	}));
}
