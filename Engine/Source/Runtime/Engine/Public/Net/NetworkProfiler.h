// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkProfiler.h: network profiling support.
=============================================================================*/

#ifndef UNREAL_NETWORK_PROFILER_H
#define UNREAL_NETWORK_PROFILER_H

#if USE_NETWORK_PROFILER 

#define NETWORK_PROFILER( x ) if ( GNetworkProfiler.IsTrackingEnabled() ) { x; }

/*=============================================================================
	FNetworkProfiler
=============================================================================*/

/**
 * Network profiler, using serialized token emission like e.g. script and malloc profiler.
 */
class FNetworkProfiler
{
private:
	/** File writer used to serialize data.															*/
	FArchive*								FileWriter;

	/** Critical section to sequence tracking.														*/
	FCriticalSection						CriticalSection;

	/** Mapping from name to index in name array.													*/
	TMap<FString,int32>						NameToNameTableIndexMap;
	/** Array of unique names.																		*/
	TArray<FString>							NameArray;

	/**	Temp file used for sessions in progress.													*/
	FString								TempFileName;

	/** Whether noticeable network traffic has occured in this session. Used to discard it.			*/
	bool									bHasNoticeableNetworkTrafficOccured;
	/** Whether tracking is enabled.																*/
	bool									bIsTrackingEnabled;	

	/** URL used for current tracking session.														*/
	FURL									CurrentURL;

	/**
	 * Returns index of passed in name into name array. If not found, adds it.
	 *
	 * @param	Name	Name to find index for
	 * @return	Index of passed in name
	 */
	int32 GetNameTableIndex( const FString& Name );

public:
	/**
	 * Constructor, initializing members.
	 */
	FNetworkProfiler();

	/**
	 * Enables/ disables tracking. Emits a session changes if disabled.
	 *
	 * @param	bShouldEnableTracking	Whether tracking should be enabled or not
	 */
	void EnableTracking( bool bShouldEnableTracking );

	/**
	 * Marks the beginning of a frame.
	 */
	void TrackFrameBegin();
	
	/**
	 * Tracks and RPC being sent.
	 * 
	 * @param	Actor		Actor RPC is being called on
	 * @param	Function	Function being called
	 * @param	NumBits		Number of bits serialized into bunch for this RPC
	 */
	void TrackSendRPC( const AActor* Actor, const UFunction* Function, uint16 NumBits );
	
	/**
	 * Low level FSocket::Send information.
	 *
	 * @param	SocketDesc				Description of socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 */
	ENGINE_API void TrackSocketSend( const FString& SocketDesc, const void* Data, uint16 BytesSent );

	/**
	 * Low level FSocket::SendTo information.
	 *
 	 * @param	SocketDesc				Description of socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 * @param	Destination				Destination address
	 */
	ENGINE_API void TrackSocketSendTo( const FString& SocketDesc, const void* Data, uint16 BytesSent, const FInternetAddr& Destination );

	/**
	 * Low level FSocket::SendTo information.
	 *
 	 * @param	SocketDesc				Description of socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 * @param	IpAddr					Destination address
	 */
	void TrackSocketSendToCore( const FString& SocketDesc, const void* Data, uint16 BytesSent, uint32 IpAddr );

	
	/**
	 * Mid level UChannel::SendBunch information.
	 * 
	 * @param	OutBunch	FOutBunch being sent
	 * @param	NumBits		Num bits to serialize for this bunch (not including merging)
	 */
	void TrackSendBunch( FOutBunch* OutBunch, uint16 NumBits );
	
	/**
	 * Track actor being replicated.
	 *
	 * @param	Actor		Actor being replicated
	 */
	void TrackReplicateActor( const AActor* Actor, FReplicationFlags RepFlags, uint32 Cycles );
	
	/**
	 * Track property being replicated.
	 *
	 * @param	Property	Property being replicated
	 * @param	NumBits		Number of bits used to replicate this property
	 */
	void TrackReplicateProperty( const UProperty* Property, uint16 NumBits );

	/**
	 * Track event occuring, like e.g. client join/ leave
	 *
	 * @param	EventName			Name of the event
	 * @param	EventDescription	Additional description/ information for event
	 */
	void TrackEvent( const FString& EventName, const FString& EventDescription );

	/**
	 * Called when the server first starts listening and on round changes or other
	 * similar game events. We write to a dummy file that is renamed when the current
	 * session ends.
	 *
	 * @param	bShouldContinueTracking		Whether to continue tracking
	 * @param	InURL						URL used for new session
	 */
	void TrackSessionChange( bool bShouldContinueTracking, const FURL& InURL );

	/**
	 * Processes any network profiler specific exec commands
	 *
	 * @param InWorld	The world in this context
	 * @param Cmd		The command to parse
	 * @param Ar		The output device to write data to
	 *
	 * @return			True if processed, false otherwise
	 */
	bool Exec( UWorld * InWorld, const TCHAR* Cmd, FOutputDevice & Ar );

	bool FORCEINLINE IsTrackingEnabled() const { return bIsTrackingEnabled; }
};

/** Global network profiler instance. */
extern ENGINE_API FNetworkProfiler GNetworkProfiler;

#else	// USE_NETWORK_PROFILER

#define NETWORK_PROFILER(x)

#endif

#endif	//#ifndef UNREAL_NETWORK_PROFILER_H
