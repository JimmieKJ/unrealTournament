// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetSnapshotMessages.generated.h"


/**
 * Implements a message that is used to request a widget snapshot from a remote target.
 */
USTRUCT()
struct FWidgetSnapshotRequest
{
	GENERATED_USTRUCT_BODY()

	/** The instance ID of the remote target we want to receive a snapshot from */
	UPROPERTY()
	FGuid TargetInstanceId;

	/** The request ID of this snapshot (used to identify the correct response from the target) */
	UPROPERTY()
	FGuid SnapshotRequestId;
};


/**
 * Implements a message that is used to receive a widget snapshot from a remote target.
 */
USTRUCT()
struct FWidgetSnapshotResponse
{
	GENERATED_USTRUCT_BODY()

	/** The request ID of this snapshot (used to identify the correct response from the target) */
	UPROPERTY()
	FGuid SnapshotRequestId;

	/** The snapshot data, to be used by FWidgetSnapshotData::LoadSnapshotFromBuffer */
	UPROPERTY()
	TArray<uint8> SnapshotData;
};
