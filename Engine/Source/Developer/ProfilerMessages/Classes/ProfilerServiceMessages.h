// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProfilerServiceMessages.generated.h"


/**
 */
USTRUCT()
struct FProfilerServiceAuthorize
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid SessionId;

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;

	/**
	 * Default constructor.
	 */
	FProfilerServiceAuthorize( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceAuthorize( const FGuid& InSessionId, const FGuid& InInstanceId )
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{
	}

};

template<>
struct TStructOpsTypeTraits<FProfilerServiceAuthorize> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceAuthorize2
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid SessionId;

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;

	/**
	 */
	UPROPERTY()
	TArray<uint8> Data;

	/**
	 * Default constructor.
	 */
	FProfilerServiceAuthorize2( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceAuthorize2( const FGuid& InSessionId, const FGuid& InInstanceId, const TArray<uint8>& InData )
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{
		Data.Append(InData);
	}

};

template<>
struct TStructOpsTypeTraits<FProfilerServiceAuthorize2> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceData
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;

	/**
	 */
	UPROPERTY()
	TArray<uint8> Data;


	/**
	 * Default constructor.
	 */
	FProfilerServiceData( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceData( const FGuid& InInstance, const TArray<uint8>& InData )
		: InstanceId(InInstance)
	{
		Data.Append(InData);
	}
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceData> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceData2
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	int64 Frame;

	/**
	 */
	UPROPERTY()
	TArray<uint8> Data;


	/**
	 * Default constructor.
	 */
	FProfilerServiceData2( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceData2( const FGuid& InInstance, int64 InFrame, const TArray<uint8>& InData )
		: InstanceId(InInstance)
		, Frame(InFrame)
	{
		Data.Append(InData);
	}
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceData2> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServicePreviewAck
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	int64 Frame;


	/**
	 * Default constructor.
	 */
	FProfilerServicePreviewAck( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServicePreviewAck( const FGuid& InInstance, int64 InFrame )
		: InstanceId(InInstance)
		, Frame(InFrame)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FProfilerServicePreviewAck> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceMetaData
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;

	/**
	 */
	UPROPERTY()
	TArray<uint8> Data;


	/**
	 * Default constructor.
	 */
	FProfilerServiceMetaData( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceMetaData( const FGuid& InInstance, const TArray<uint8>& InData )
		: InstanceId(InInstance)
		, Data( InData )
	{}
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceMetaData> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message for copying a file through the network, as well as for synchronization.
 * Unfortunately assumes that InstanceId and Filename are transfered without errors.
 */
USTRUCT()
struct FProfilerServiceFileChunk
{
	GENERATED_USTRUCT_BODY()

	/** The ID of the instance where this message should be sent. */
	UPROPERTY()
	FGuid InstanceId;

	/** The file containing this file chunk. */
	UPROPERTY()
	FString Filename;

	/** Data to be sent through message bus. */
	UPROPERTY()
	TArray<uint8> Data;

	/** FProfilerFileChunkHeader stored in the array. */
	UPROPERTY()
	TArray<uint8> Header;

	/** Hash of this data and header. */
	UPROPERTY()
	TArray<uint8> ChunkHash;

	/**
	 * Default constructor.
	 */
	FProfilerServiceFileChunk() 
	{}

	/**
	 * Constructor for the new file chunk.
	 */
	FProfilerServiceFileChunk
	( 
		const FGuid& InInstanceID, 
		const FString& InFilename, 
		const TArray<uint8>& InHeader
	)
		: InstanceId( InInstanceID )
		, Filename( InFilename )
		, Header( InHeader )
	{}

	struct FNullTag{};

	/**
	 * Copy constructor, copies all properties, but not data.
	 */
	FProfilerServiceFileChunk( const FProfilerServiceFileChunk& ProfilerServiceFileChunk, FNullTag )
		: InstanceId( ProfilerServiceFileChunk.InstanceId )
		, Filename( ProfilerServiceFileChunk.Filename )
		, Header( ProfilerServiceFileChunk.Header )
	{}
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceFileChunk> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};

/**
 */
USTRUCT()
struct FProfilerServicePing
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FProfilerServicePing> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServicePong
{
	GENERATED_USTRUCT_BODY()

};

template<>
struct TStructOpsTypeTraits<FProfilerServicePong> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceSubscribe
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid SessionId;

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;

	/**
	 * Default constructor.
	 */
	FProfilerServiceSubscribe( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceSubscribe( const FGuid& InSessionId, const FGuid& InInstanceId )
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{ }
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceSubscribe> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceUnsubscribe
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	FGuid SessionId;

	/**
	 */
	UPROPERTY()
	FGuid InstanceId;


	/**
	 * Default constructor.
	 */
	FProfilerServiceUnsubscribe( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceUnsubscribe( const FGuid& InSessionId, const FGuid& InInstanceId )
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{ }
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceUnsubscribe> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceCapture
{
	GENERATED_USTRUCT_BODY()

	/**
	 * The data capture state that should be set.
	 */
	UPROPERTY()
	bool bRequestedCaptureState;

	/**
	 * Default constructor.
	 */
	FProfilerServiceCapture( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceCapture( const bool bInRequestedCaptureState )
		: bRequestedCaptureState( bInRequestedCaptureState )
	{ }
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceCapture> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServicePreview
{
	GENERATED_USTRUCT_BODY()

	/**
	 * The data preview state that should be set.
	 */
	UPROPERTY()
	bool bRequestedPreviewState;

	/**
	 * Default constructor.
	 */
	FProfilerServicePreview( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServicePreview( const bool bInRequestedPreviewState )
		: bRequestedPreviewState( bInRequestedPreviewState )
	{ }
};

template<>
struct TStructOpsTypeTraits<FProfilerServicePreview> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FProfilerServiceRequest
{
	GENERATED_USTRUCT_BODY()

	/** Request @see EProfilerRequestType. */
	UPROPERTY()
	uint32 Request;

	/**
	 * Default constructor.
	 */
	FProfilerServiceRequest( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FProfilerServiceRequest( uint32 InRequest )
		: Request(InRequest)
	{ }
};

template<>
struct TStructOpsTypeTraits<FProfilerServiceRequest> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};
