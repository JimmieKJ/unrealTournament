// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateReflectorPrivatePCH.h"
#include "WidgetSnapshotService.h"
#include "IMessagingModule.h"
#include "MessageEndpointBuilder.h"
#include "SWidgetSnapshotVisualizer.h"

FWidgetSnapshotService::FWidgetSnapshotService()
{
	if (FPlatformMisc::SupportsMessaging() && FPlatformProcess::SupportsMultithreading())
	{
		IMessageBusPtr MessageBusPtr = IMessagingModule::Get().GetDefaultBus();

		MessageEndpoint = FMessageEndpoint::Builder("FWidgetSnapshotService", MessageBusPtr.ToSharedRef())
			.ReceivingOnThread(ENamedThreads::GameThread)
			.Handling<FWidgetSnapshotRequest>(this, &FWidgetSnapshotService::HandleWidgetSnapshotRequestMessage)
			.Handling<FWidgetSnapshotResponse>(this, &FWidgetSnapshotService::HandleWidgetSnapshotResponseMessage);

		if (MessageEndpoint.IsValid())
		{
			MessageEndpoint->Subscribe<FWidgetSnapshotRequest>();
		}
	}
}

FGuid FWidgetSnapshotService::RequestSnapshot(const FGuid& InRemoteInstanceId, const FOnWidgetSnapshotResponse& OnResponse)
{
	if (MessageEndpoint.IsValid())
	{
		FWidgetSnapshotRequest* WidgetSnapshotRequest = new FWidgetSnapshotRequest;
		WidgetSnapshotRequest->TargetInstanceId = InRemoteInstanceId;
		WidgetSnapshotRequest->SnapshotRequestId = FGuid::NewGuid();

		PendingSnapshotResponseHandlers.Add(WidgetSnapshotRequest->SnapshotRequestId, OnResponse);

		MessageEndpoint->Publish(WidgetSnapshotRequest);

		return WidgetSnapshotRequest->SnapshotRequestId;
	}

	return FGuid();
}

void FWidgetSnapshotService::AbortSnapshotRequest(const FGuid& InSnapshotRequestId)
{
	PendingSnapshotResponseHandlers.Remove(InSnapshotRequestId);
}

void FWidgetSnapshotService::HandleWidgetSnapshotRequestMessage(const FWidgetSnapshotRequest& Message, const IMessageContextRef& Context)
{
	if (MessageEndpoint.IsValid() && Message.TargetInstanceId == FApp::GetInstanceId())
	{
		FWidgetSnapshotData SnapshotData;
		SnapshotData.TakeSnapshot();

		FWidgetSnapshotResponse* WidgetSnapshotResponse = new FWidgetSnapshotResponse;
		WidgetSnapshotResponse->SnapshotRequestId = Message.SnapshotRequestId;
		SnapshotData.SaveSnapshotToBuffer(WidgetSnapshotResponse->SnapshotData);

		MessageEndpoint->Send(WidgetSnapshotResponse, Context->GetSender());
	}
}

void FWidgetSnapshotService::HandleWidgetSnapshotResponseMessage(const FWidgetSnapshotResponse& Message, const IMessageContextRef& Context)
{
	const FOnWidgetSnapshotResponse* FoundResponseHandler = PendingSnapshotResponseHandlers.Find(Message.SnapshotRequestId);
	if (FoundResponseHandler && FoundResponseHandler->IsBound())
	{
		FoundResponseHandler->Execute(Message.SnapshotData);
		PendingSnapshotResponseHandlers.Remove(Message.SnapshotRequestId);
	}
}
