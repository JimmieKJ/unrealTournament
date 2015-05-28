// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHttpRequest.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "AsyncTaskDownloadImage.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDownloadImageDelegate, UTexture2D*, Texture);

UCLASS()
class UMG_API UAsyncTaskDownloadImage : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=( BlueprintInternalUseOnly="true" ))
	static UAsyncTaskDownloadImage* DownloadImage(FString URL);

public:

	UPROPERTY(BlueprintAssignable)
	FDownloadImageDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FDownloadImageDelegate OnFail;

public:

	void Start(FString URL);

private:

	/** Handles image requests coming from the web */
	void HandleImageRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
};
