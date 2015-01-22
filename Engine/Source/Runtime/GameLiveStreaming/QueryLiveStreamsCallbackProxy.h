// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "OnlineBlueprintCallProxyBase.h"
#include "QueryLiveStreamsCallbackProxy.generated.h"

USTRUCT( BlueprintType )
struct FBlueprintLiveStreamInfo 
{
	GENERATED_USTRUCT_BODY()

	/** Name of the game that is streaming */
	UPROPERTY( EditAnywhere, Category="LiveStreaming" )
	FString GameName;

	/** The title of the stream itself */
	UPROPERTY( EditAnywhere, Category="LiveStreaming" )
	FString StreamName;

	/** URL for the stream */
	UPROPERTY( EditAnywhere, Category="LiveStreaming" )
	FString URL;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FOnQueryLiveStreamsCompleted, const TArray<FBlueprintLiveStreamInfo>&, LiveStreams, bool, bWasSuccessful );

UCLASS()
class UQueryLiveStreamsCallbackProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_UCLASS_BODY()

	// Called when the asynchronous request for live streams completes, passing along the list of live streams currently active, along with a boolean value that indicates whether the request was successful at all
	UPROPERTY( BlueprintAssignable )
	FOnQueryLiveStreamsCompleted OnQueriedLiveStreams;

	/** Requests a list of live internet streams for the specified game name.  This will usually hit the internet so it could take a second or two. */
	UFUNCTION( BlueprintCallable, Category="LiveStreaming", meta=( BlueprintInternalUseOnly="true" ) )
	static UQueryLiveStreamsCallbackProxy* QueryLiveStreams( const FString& GameName );

	/** UOnlineBlueprintCallProxyBase interface */
	virtual void Activate() override;


protected:

	/** Name of the game that we're querying about */
	FString GameName;
};