// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusPrivatePCH.h"
#include "OnlineMessageTaskManagerOculus.h"

void FOnlineMessageTaskManagerOculus::OnReceiveMessage(ovrMessageHandle Message)
{
	auto RequestId = ovr_Message_GetRequestID(Message);
	bool bIsError = ovr_Message_IsError(Message);

	if (RequestDelegates.Contains(RequestId))
	{
		RequestDelegates[RequestId].ExecuteIfBound(Message, bIsError);

		// Remove the delegate
		RequestDelegates[RequestId].Unbind();
		RequestDelegates.Remove(RequestId);
	}
	else
	{
		auto MessageType = ovr_Message_GetType(Message);
		if (NotifDelegates.Contains(MessageType))
		{
			if (!bIsError)
			{
				NotifDelegates[MessageType].Broadcast(Message, bIsError);
			}
		}
		else
		{
			UE_LOG_ONLINE(Verbose, TEXT("Unhandled request id: %llu"), RequestId);
		}
	}
	ovr_FreeMessage(Message);
}

void FOnlineMessageTaskManagerOculus::AddRequestDelegate(ovrRequest RequestId, FOculusMessageOnCompleteDelegate&& Delegate)
{
	RequestDelegates.Emplace(RequestId, Delegate);
}

FOculusMulticastMessageOnCompleteDelegate& FOnlineMessageTaskManagerOculus::GetNotifDelegate(ovrMessageType MessageType)
{
	return NotifDelegates.FindOrAdd(MessageType);
}

void FOnlineMessageTaskManagerOculus::RemoveNotifDelegate(ovrMessageType MessageType, const FDelegateHandle& Delegate)
{
	NotifDelegates.FindOrAdd(MessageType).Remove(Delegate);
}

bool FOnlineMessageTaskManagerOculus::Tick(float DeltaTime)
{
	for (;;)
	{
		auto Message = ovr_PopMessage();
		if (!Message)
		{
			break;
		}
		OnReceiveMessage(Message);
	}
	return true;
}
