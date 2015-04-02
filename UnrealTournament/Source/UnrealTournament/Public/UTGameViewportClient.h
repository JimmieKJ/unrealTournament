// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "UTGameViewportClient.generated.h"

UCLASS()
class UUTGameViewportClient : public UGameViewportClient
{
	GENERATED_UCLASS_BODY()

	virtual void AddViewportWidgetContent(TSharedRef<class SWidget> ViewportContent, const int32 ZOrder = 0) override;
	virtual void RemoveViewportWidgetContent(TSharedRef<class SWidget> ViewportContent) override;

	virtual void PeekTravelFailureMessages(UWorld* World, enum ETravelFailure::Type FailureType, const FString& ErrorString) override;
	virtual void PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, enum ENetworkFailure::Type FailureType, const FString& ErrorString) override;

	virtual void PostRender(UCanvas* Canvas) override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	TWeakPtr<class SUTGameLayerManager> LayerManagerPtr;

	TSharedPtr<class SUWDialog> ReconnectDialog;
	TSharedPtr<class SUWRedirectDialog> RedirectDialog;

	// Holds the IP/Port of the last connect so we can try to reconnect
	FURL LastAttemptedURL;
	float ReconnectAfterDownloadingMapDelay;
	float VerifyFilesToDownloadAndReconnectDelay;

	virtual void RankDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void NetworkFailureDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void LoginFailureDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void ConnectPasswordResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void RedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void CloudRedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	virtual void VerifyFilesToDownloadAndReconnect();
	virtual void ReconnectAfterDownloadingMap();
};

