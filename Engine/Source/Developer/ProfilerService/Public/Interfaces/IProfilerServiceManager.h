// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of IProfilerServiceManager. */
typedef TSharedPtr<class IProfilerServiceManager> IProfilerServiceManagerPtr;

/** Type definition for shared references to instances of IProfilerService. */
typedef TSharedRef<class IProfilerServiceManager> IProfilerServiceManagerRef;


/**
 * Type definition for the profiler data packet.
 */
struct FProfilerCycleCounter
{
public:
	uint32 StatId;
	uint32 GroupId;
	double FrameStart;
	uint32 ParentId;
	uint32 InstanceId;
	uint32 ParentInstanceId;
	uint32 ThreadId;
	uint32 Value;
	uint32 CallsPerFrame;
};


struct FProfilerFloatAccumulator
{
	int32 StatId;
	float Value;
};


struct FProfilerCountAccumulator
{
	int32 StatId;
	uint32 Value;
};


struct FProfilerCycleGraph
{
public:
	int32 StatId;
	uint32 FrameStart;
	uint32 ThreadId;
	uint32 Value;
	uint32 CallsPerFrame;
	TArray<FProfilerCycleGraph> Children;
};


FORCEINLINE FArchive& operator<<(FArchive& Ar, FProfilerCycleCounter& Data)
{
	Ar << Data.StatId;
	Ar << Data.GroupId;
	Ar << Data.FrameStart;
	Ar << Data.ParentId;
	Ar << Data.InstanceId;
	Ar << Data.ParentInstanceId;
	Ar << Data.ThreadId;
	Ar << Data.Value;
	Ar << Data.CallsPerFrame;

	return Ar;
}

	
FORCEINLINE FArchive& operator<<(FArchive& Ar, FProfilerFloatAccumulator& Data)
{
	Ar << Data.StatId;
	Ar << Data.Value;

	return Ar;
}


FORCEINLINE FArchive& operator<<(FArchive& Ar, FProfilerCountAccumulator& Data)
{
	Ar << Data.StatId;
	Ar << Data.Value;

	return Ar;
}


FORCEINLINE FArchive& operator<<(FArchive& Ar, FProfilerCycleGraph& Data)
{
	Ar << Data.StatId;

	Ar << Data.FrameStart;
	Ar << Data.ThreadId;
	Ar << Data.CallsPerFrame;
	Ar << Data.Value;
	Ar << Data.Children;

	return Ar;
}


/**
 * Type definition for a frame profiler data
 */
struct FProfilerDataFrame
{
	uint32 Frame;
	double FrameStart;
	TMap<uint32, TArray<FProfilerCycleCounter> > CycleCounters;
	TMap<uint32, FProfilerCycleGraph> CycleGraphs;
	TArray<FProfilerFloatAccumulator> FloatAccumulators;
	TArray<FProfilerCountAccumulator> CountAccumulators;
	bool MetaDataUpdated;
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FProfilerDataFrame& Data)
{
	Ar << Data.Frame;
	Ar << Data.FrameStart;
	Ar << Data.CycleCounters;
	Ar << Data.CycleGraphs;
	Ar << Data.FloatAccumulators;
	Ar << Data.CountAccumulators;

	return Ar;
}


struct FStatDescription
{
	/** Default constructor. */
	FStatDescription()
		: ID( 0 )
		, StatType( -1 )
		, GroupID( 0 )
	{}

	/** ID for this stat */
	int32 ID;

	/** Stat name */
	FString Name;

	/** Type of stat (counter, cycle, etc.) */
	uint32 StatType;

	/** Group this stat belongs to */
	int32 GroupID;
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FStatDescription& Data)
{
	Ar << Data.ID;
	Ar << Data.Name;
	Ar << Data.StatType;
	Ar << Data.GroupID;

	return Ar;
}
	

struct FStatGroupDescription
{
	/** Default constructor. */
	FStatGroupDescription()
		: ID( 0 )
	{}

	/** ID for this group */
	int32 ID;

	/** Group name */
	FString Name;

};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FStatGroupDescription& Data)
{
	Ar << Data.ID;
	Ar << Data.Name;

	return Ar;
}


/**
 * Structure holding the meta data describing the various stats and data associated with them
 */
struct FStatMetaData
{
	/** Stat descriptions */
	TMap<uint32, FStatDescription> StatDescriptions;

	/** Group descriptions */
	TMap<uint32, FStatGroupDescription> GroupDescriptions;

	/** Thread descriptions */
	TMap<uint32, FString> ThreadDescriptions;

	/** Seconds per cycle */
	double SecondsPerCycle;

	/** Critical section used to lock the meta data for access */
	FCriticalSection* CriticalSection;

	const uint32 GetMetaDataSize() const
	{
		return StatDescriptions.Num() + GroupDescriptions.Num() + ThreadDescriptions.Num();
	}

};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FStatMetaData& Data)
{
	Ar << Data.StatDescriptions;
	Ar << Data.GroupDescriptions;
	Ar << Data.ThreadDescriptions;
	Ar << Data.SecondsPerCycle;

	return Ar;
}


/**
 * Profiler file type
 */
namespace EProfilerFileType
{
	enum Type
	{
		PFT_Binary,
		PFT_CSV,
	};
}

/**
 * Profiler file header
 */
struct FProfilerDataHeader
{
	/** Magice value to determine it is a profiler file */
	uint32 Magic;

	/** Current version of the file */
	uint32 Version;

	/** Session id the file was generated from */
	FGuid SessionId;

	/** Instance id the file was generated from */
	FGuid InstanceId;

	/** Total amount of capture data.  Note: this is different from the file size */
	int64 TotalCaptureData;
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FProfilerDataHeader& Data)
{
	Ar << Data.Magic;
	Ar << Data.Version;
	Ar << Data.SessionId;
	Ar << Data.InstanceId;
	Ar << Data.TotalCaptureData;

	return Ar;
}

#if STATS
FORCEINLINE FArchive& operator<<(FArchive& Ar, FStatNameAndInfo& NameAndInfo)
{
	if (Ar.IsLoading())
	{
		int32 Index = 0;
		Ar << Index;
		int32 Number = 0;
		Ar << Number;
		FName TheFName;
		FString Name;
		Ar << Name;
		TheFName = FName(*Name);
		Number &= ~ (EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift));
		NameAndInfo = FStatNameAndInfo(TheFName, false);
		NameAndInfo.SetNumberDirect(Number);
	}
	else
	{
		FName RawName = NameAndInfo.GetRawName();
		int32 Index = RawName.GetComparisonIndex();
		Ar << Index;
		int32 Number = NameAndInfo.GetRawNumber();
		Number |= EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift);
		Ar << Number;
		FString Name = RawName.ToString();
		Ar << Name;
	}

	return Ar;
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FStatMessage& Data)
{
	Ar << Data.NameAndInfo;

	if (Ar.IsLoading())
	{
		Data.Clear();
		switch (Data.NameAndInfo.GetField<EStatDataType>())
		{
		case EStatDataType::ST_int64:
			{
				int64 Payload = 0;
				Ar << Payload;
				Data.GetValue_int64() = Payload;
			}
			break;
		case EStatDataType::ST_double:
			{
				double Payload = 0;
				Ar << Payload;
				Data.GetValue_double() = Payload;
			}
			break;
		case EStatDataType::ST_FName:
			FStatNameAndInfo Payload;
			Ar << Payload;
			Data.GetValue_FName() = Payload.GetRawName();
			break;
		}

	}
	else
	{
		switch (Data.NameAndInfo.GetField<EStatDataType>())
		{
		case EStatDataType::ST_int64:
			{
				int64 Payload = Data.GetValue_int64();
				Ar << Payload;
			}
			break;
		case EStatDataType::ST_double:
			{
				double Payload = Data.GetValue_double();
				Ar << Payload;
			}
			break;
		case EStatDataType::ST_FName:
			FStatNameAndInfo Payload(Data.GetValue_FName(), false);
			Ar << Payload;
			break;
		}
	}

	return Ar;
}
#endif


/**
 * Profiler service request type
 */
namespace EProfilerRequestType
{
	enum Type
	{
		/** Metadata request. */
		PRT_MetaData,
		/** Send last captured file. */
		PRT_SendLastCapturedFile,
	};
}

/** Delegate for passing profiler data to UI */
DECLARE_MULTICAST_DELEGATE_TwoParams(FProfilerDataDelegate, const FGuid&, const FProfilerDataFrame& );

#if STATS
DECLARE_MULTICAST_DELEGATE_TwoParams(FProfilerData2Delegate, const FGuid&, TArray<FStatMessage>& );
#endif


/** Enumerates profiler file chunk types. */
namespace EProfilerFileChunkType
{
	enum Type
	{
		/** Indicates that message should prepare chunks for sending. This is a helper file chunk. */
		PrepareFile,
		/** Indicates that message requires sending a file chunk. */
		SendChunk,
		/** Indicates that message requires finalization of transfered file. This is a helper file chunk. */
		FinalizeFile,
		/** Not used. */
		NotUsed,
	};
}


/** Class that describes basic information about one file chunk. */
struct FProfilerFileChunkHeader
{
	/** Size of the file chunk. */
	static const int64 DefChunkSize = 64*1024;

	/** Default constructor. */
	FProfilerFileChunkHeader()
		: ChunkOffset( 0 )
		, ChunkSize( 0 )
		, FileSize( 0 )
		, ChunkType( EProfilerFileChunkType::NotUsed )
	{}

	/** Initialization constructor. */
	FProfilerFileChunkHeader( int64 InChunkOffset, int64 InChunkSize, int64 InFileSize, EProfilerFileChunkType::Type InChunkType )
		: ChunkOffset( InChunkOffset )
		, ChunkSize( InChunkSize )
		, FileSize( InFileSize )
		, ChunkType( InChunkType )
	{}

	/** Performs sanity checks to make sure that header data is valid. */
	FORCEINLINE_DEBUGGABLE void Validate() const
	{
		check( ChunkOffset % DefChunkSize == 0 );
		check( ChunkSize <= DefChunkSize  );
		check( ChunkType == EProfilerFileChunkType::SendChunk || ChunkType == EProfilerFileChunkType::PrepareFile || ChunkType == EProfilerFileChunkType::FinalizeFile );
		check( FileSize >= 0 );
		check( ChunkOffset >= 0 );
	}

	/** Offset of this chunk. */
	int64 ChunkOffset;

	/** Size of this chunk. */
	int64 ChunkSize;

	/** The size of the file. */
	int64 FileSize;

	/** File chunk type @see EProfilerFileChunkType. */
	EProfilerFileChunkType::Type ChunkType;

	friend FORCEINLINE FArchive& operator<<(FArchive& Ar, FProfilerFileChunkHeader& Header)
	{
		Ar << Header.ChunkOffset;
		Ar << Header.ChunkSize;
		Ar << Header.FileSize;
		Ar << (uint32&)Header.ChunkType;

		return Ar;
	}

	/** Serializes this file chunk header as an array of bytes. */
	FORCEINLINE_DEBUGGABLE TArray<uint8> AsArray() const
	{
		TArray<uint8> ArrayHeader;

		// Setup header.
		FMemoryWriter Writer(ArrayHeader);
		Writer << const_cast<FProfilerFileChunkHeader&>(*this);

		return ArrayHeader;
	}
};


/**
 * Interface for the Profiler Service manager.
 */
class IProfilerServiceManager
{
public:

	/**
	 * Sends profiler cycle counter to registered clients.
	 *
	 * @param Data The profiler data to be sent.
	 */
	virtual void SendData(FProfilerCycleCounter& Data) = 0;

	/**
	 * Sends profiler float accumulator to registered clients.
	 *
	 * @param Data The profiler data to be sent.
	 */
	virtual void SendData(FProfilerFloatAccumulator& Data) = 0;

	/**
	 * Sends profiler count accumulator to registered clients.
	 *
	 * @param Data The profiler data to be sent.
	 */
	virtual void SendData(FProfilerCountAccumulator& Data) = 0;

	/**
	 * Sends profiler cycle graph to registered clients.
	 *
	 * @param Data The profiler data to be sent.
	 */
	virtual void SendData(FProfilerCycleGraph& Data) = 0;

	/**
	 * Determines if we are capturing data for clients.
	 */
	virtual bool IsCapturing() const = 0;

	/** Starts a file capture. */
	virtual void StartCapture() = 0;

	/** Stops a file capture. */
	virtual void StopCapture() = 0;

	/** Updates the meta data. */
	virtual void UpdateMetaData() = 0;

	/** Starts a new frame of data and sends the previous if it exists. */
	virtual void StartFrame(uint32 FrameNumber, double FrameStart) = 0;

	/**
	 * Gets the description for the given stat id.
	 *
	 * @param StatId IDd of the statistic description to retrieve.
	 * @return the FStatMetaData struct with the description.
	 */
	virtual FStatMetaData& GetStatMetaData() = 0;

	/**
	 * Retrieves the profiler data delegate
	 *
	 * @return profiler data delegate
	 */
	virtual FProfilerDataDelegate& OnProfilerData() = 0;

public:

	/** Virtual destructor. */
	virtual ~IProfilerServiceManager() { }
};
