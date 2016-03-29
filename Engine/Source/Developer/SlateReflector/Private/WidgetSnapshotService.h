// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetSnapshotMessages.h"
#include "MessageEndpoint.h"

/**
 * Implements the service for handling remote widget snapshots.
 */
class FWidgetSnapshotService
{
public:
	DECLARE_DELEGATE_OneParam(FOnWidgetSnapshotResponse, const TArray<uint8>& /*InSnapshotData*/);

	FWidgetSnapshotService();

	/** Request a snapshot from the given instance. The given delegate will be called when the response comes in */
	FGuid RequestSnapshot(const FGuid& InRemoteInstanceId, const FOnWidgetSnapshotResponse& OnResponse);

	/** Abort a request using the GUID previously obtained via a call to RequestSnapshot */
	void AbortSnapshotRequest(const FGuid& InSnapshotRequestId);

private:
	/** Handles FWidgetSnapshotRequest messages */
	void HandleWidgetSnapshotRequestMessage(const FWidgetSnapshotRequest& Message, const IMessageContextRef& Context);

	/** Handles FWidgetSnapshotResponse messages */
	void HandleWidgetSnapshotResponseMessage(const FWidgetSnapshotResponse& Message, const IMessageContextRef& Context);

	/** Holds the message endpoint */
	FMessageEndpointPtr MessageEndpoint;

	/** Handlers awaiting their resultant snapshot data */
	TMap<FGuid, FOnWidgetSnapshotResponse> PendingSnapshotResponseHandlers;
};
