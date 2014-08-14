// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameViewportClient.generated.h"

UCLASS()
class UUTGameViewportClient : public UGameViewportClient
{
	GENERATED_UCLASS_BODY()


	virtual void PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, enum ENetworkFailure::Type FailureType, const FString& ErrorString);

protected:

	TSharedPtr<class SUWDialog> ReconnectDialog;

	// Holds the IP/Port of the last connect so we can try to reconnect
	FURL LastAttemptedURL;

	UFUNCTION()
	virtual void NetworkFailureDialogResult(uint16 ButtonID);

	UFUNCTION()
	virtual void ConnectPasswordResult(const FString& InputText, bool bCancelled);
};

