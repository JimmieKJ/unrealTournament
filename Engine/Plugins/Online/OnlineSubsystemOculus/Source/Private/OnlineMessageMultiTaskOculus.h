// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemOculusPackage.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "OnlineDelegateMacros.h"
#include "OnlineMessageTaskManagerOculus.h"
#include "OnlineSubsystemOculusPackage.h"

/**
 *
 */
class FOnlineMessageMultiTaskOculus
{
private:
	/** Requests that are waiting to be completed */
	TArray<ovrRequest> InProgressRequests;

protected:
	bool bDidAllRequestsFinishedSuccessfully = true;

	DECLARE_DELEGATE(FFinalizeDelegate);

	FOnlineMessageMultiTaskOculus::FFinalizeDelegate Delegate;

PACKAGE_SCOPE:
	FOnlineSubsystemOculus& OculusSubsystem;

	FOnlineMessageMultiTaskOculus(
		FOnlineSubsystemOculus& InOculusSubsystem,
		const FOnlineMessageMultiTaskOculus::FFinalizeDelegate& InDelegate) :
		OculusSubsystem(InOculusSubsystem),
		Delegate(InDelegate) {}

	void AddNewRequest(ovrRequest RequestId);
};
