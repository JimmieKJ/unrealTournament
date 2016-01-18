// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RuntimeAssetCacheInterface.h"
#include "RuntimeAssetCacheBPHooks.generated.h"


/** Forward declarations */
class IRuntimeAssetCacheBuilder;
class FOnRuntimeAssetCacheAsyncComplete;

UCLASS()
class URuntimeAssetCacheBPHooks : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = RuntimeAssetCache)
	static FVoidPtrParam GetSynchronous(TScriptInterface<IRuntimeAssetCacheBuilder> CacheBuilder);

	UFUNCTION(BlueprintCallable, Category = RuntimeAssetCache)
	static int32 GetAsynchronous(TScriptInterface<IRuntimeAssetCacheBuilder> CacheBuilder, const FOnRuntimeAssetCacheAsyncComplete& CompletionDelegate);

	UFUNCTION(BlueprintCallable, Category = RuntimeAssetCache)
	static int32 GetCacheSize(FName Bucket);

	UFUNCTION(BlueprintCallable, Category = RuntimeAssetCache)
	static bool ClearCache(FName Bucket);

	UFUNCTION(BlueprintCallable, Category = RuntimeAssetCache)
	static void WaitAsynchronousCompletion(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = RuntimeAssetCache)
	static FVoidPtrParam GetAsynchronousResults(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = RuntimeAssetCache)
	static bool PollAsynchronousCompletion(int32 Handle);
};