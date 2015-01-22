// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "GameLiveStreamingModule.h"
#include "QueryLiveStreamsCallbackProxy.h"
#include "Runtime/Engine/Public/Features/ILiveStreamingService.h"
#include "IGameLiveStreaming.h"

UQueryLiveStreamsCallbackProxy::UQueryLiveStreamsCallbackProxy( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}


UQueryLiveStreamsCallbackProxy* UQueryLiveStreamsCallbackProxy::QueryLiveStreams( const FString& GameName )
{
	UQueryLiveStreamsCallbackProxy* Proxy = NewObject<UQueryLiveStreamsCallbackProxy>();
	Proxy->GameName = GameName;

	return Proxy;
}


void UQueryLiveStreamsCallbackProxy::Activate()
{
	// Query the live streaming system to find out about which live streams are available for the game that was requested.  This
	// will call back later on with results.
	IGameLiveStreaming::Get().GetLiveStreamingService()->QueryLiveStreams(
		this->GameName,
		ILiveStreamingService::FQueryLiveStreamsCallback::CreateStatic( []( const TArray< FLiveStreamInfo >& LiveStreams, bool bQueryWasSuccessful, TWeakObjectPtr< UQueryLiveStreamsCallbackProxy > WeakThis )
			{
				// Make sure our UObject hasn't been destroyed at the time that the live streaming system calls back with results
				UQueryLiveStreamsCallbackProxy* This = WeakThis.Get();
				if( This != nullptr )
				{
					TArray< FBlueprintLiveStreamInfo > BlueprintLiveStreams;
					for( const auto& LiveStream : LiveStreams )
					{
						FBlueprintLiveStreamInfo& BlueprintLiveStream = *new( BlueprintLiveStreams )FBlueprintLiveStreamInfo;
						BlueprintLiveStream.GameName = LiveStream.GameName;
						BlueprintLiveStream.StreamName = LiveStream.StreamName;
						BlueprintLiveStream.URL = LiveStream.URL;
					}

					// We have results!
					This->OnQueriedLiveStreams.Broadcast( BlueprintLiveStreams, bQueryWasSuccessful );
				}
			},
			TWeakObjectPtr< UQueryLiveStreamsCallbackProxy>( this ) )
		);
}
