// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#if	STATS

class FAsyncStatsWrite;

/**
* Magic numbers for stats streams, this is for the first version.
*/
namespace EStatMagicNoHeader
{
	enum Type
	{
		MAGIC = 0x7E1B83C1,
		MAGIC_SWAPPED = 0xC1831B7E,
		NO_VERSION = 0,
	};
}

/**
* Magic numbers for stats streams, this is for the second and later versions.
* Allows more advanced options.
*/
namespace EStatMagicWithHeader
{
	enum Type
	{
		MAGIC = 0x10293847,
		MAGIC_SWAPPED = 0x47382910,
		VERSION_2 = 2,
		VERSION_3 = 3,

		/**
		 *	Added support for compressing the stats data, now each frame is compressed.
		 *	!!CAUTION!! Deprecated.
		 */
		VERSION_4 = 4,
		HAS_COMPRESSED_DATA_VER = VERSION_4,

		/**
		 *	New low-level raw stats with memory profiler functionality.
		 *  !!CAUTION!! Not backward compatible with version 4.
		 */
		VERSION_5 = 5
	};
}

struct EStatsFileConstants
{
	enum
	{
		/**
		 *	Maximum size of the data that can be compressed.
		 */
		MAX_COMPRESSED_SIZE = 1024 * 1024,

		/** Header reserved for the compression internals. */
		DUMMY_HEADER_SIZE = 1024,

		/** Indicates the end of the compressed data. */
		END_OF_COMPRESSED_DATA = 0xE0F0DA4A,

		/** Indicates that the compression is disabled for the data. */
		NO_COMPRESSION = 0,
	};
};

/*-----------------------------------------------------------------------------
	FCompressedStatsData
-----------------------------------------------------------------------------*/

/** Helper struct used to operate on the compressed data. */
struct CORE_API FCompressedStatsData
{
	/**
	 * Initialization constructor 
	 *	
	 * @param SrcData - uncompressed data if saving, compressed data if loading
	 * @param DestData - compressed data if saving, uncompressed data if loading
	 *
	 */
	FCompressedStatsData( TArray<uint8>& InSrcData, TArray<uint8>& InDestData )
		: SrcData( InSrcData )
		, DestData( InDestData )
		, bEndOfCompressedData( false )
	{}

	/**
	 * Writes a special data to mark the end of the compressed data.
	 */
	static void WriteEndOfCompressedData( FArchive& Writer )
	{
		int32 Marker = EStatsFileConstants::END_OF_COMPRESSED_DATA;
		check( Writer.IsSaving() );
		Writer << Marker << Marker;
	}

protected:
	/** Serialization operator. */
	friend FArchive& operator << (FArchive& Ar, FCompressedStatsData& Data)
	{
		if( Ar.IsSaving() )
		{
			Data.WriteCompressed( Ar );
		}
		else if( Ar.IsLoading() )
		{
			Data.ReadCompressed( Ar );
		}
		else
		{
			check( 0 );
		}
		return Ar;
	}

	/** Compress the data and writes to the archive. */
	void WriteCompressed( FArchive& Writer )
	{
		int32 UncompressedSize = SrcData.Num();
		if( UncompressedSize > EStatsFileConstants::MAX_COMPRESSED_SIZE - EStatsFileConstants::DUMMY_HEADER_SIZE )
		{
			int32 DisabledCompressionSize = EStatsFileConstants::NO_COMPRESSION;
			Writer << DisabledCompressionSize << UncompressedSize;
			Writer.Serialize( SrcData.GetData(), UncompressedSize );
		}
		else
		{
			DestData.Reserve( EStatsFileConstants::MAX_COMPRESSED_SIZE );
			int32 CompressedSize = DestData.GetAllocatedSize();

			const bool bResult = FCompression::CompressMemory( COMPRESS_ZLIB, DestData.GetData(), CompressedSize, SrcData.GetData(), UncompressedSize );
			check( bResult );
			Writer << CompressedSize << UncompressedSize;
			Writer.Serialize( DestData.GetData(), CompressedSize );
		}
	}

	/** Reads the data and decompresses it. */
	void ReadCompressed( FArchive& Reader )
	{
		int32 CompressedSize = 0;
		int32 UncompressedSize = 0;
		Reader << CompressedSize << UncompressedSize;

		if( CompressedSize == EStatsFileConstants::END_OF_COMPRESSED_DATA && UncompressedSize == EStatsFileConstants::END_OF_COMPRESSED_DATA )
		{
			bEndOfCompressedData = true;
		}
		// This chunk is not compressed.
		else if( CompressedSize == 0 )
		{
			DestData.Reset( UncompressedSize );
			DestData.AddUninitialized( UncompressedSize );
			Reader.Serialize( DestData.GetData(), UncompressedSize );
		}
		else
		{
			SrcData.Reset( CompressedSize );
			DestData.Reset( UncompressedSize );
			SrcData.AddUninitialized( CompressedSize );
			DestData.AddUninitialized( UncompressedSize );

			Reader.Serialize( SrcData.GetData(), CompressedSize );
			const bool bResult = FCompression::UncompressMemory( COMPRESS_ZLIB, DestData.GetData(), UncompressedSize, SrcData.GetData(), CompressedSize );
			check( bResult );
		}
	}

public:
	/**
	 * @return true if we reached the end of the compressed data.
	 */
	const bool HasReachedEndOfCompressedData() const
	{
		return bEndOfCompressedData;
	}
	
protected:
	/** Reference to the source data. */
	TArray<uint8>& SrcData;

	/** Reference to the destination data. */
	TArray<uint8>& DestData;

	/** Set to true if we reached the end of the compressed data. */
	bool bEndOfCompressedData;
};

/*-----------------------------------------------------------------------------
	Stats file writing functionality
-----------------------------------------------------------------------------*/

/** Header for a stats file. */
struct FStatsStreamHeader
{
	/** Default constructor. */
	FStatsStreamHeader()
		: Version( 0 )
		, FrameTableOffset( 0 )
		, FNameTableOffset( 0 )
		, NumFNames( 0 )
		, MetadataMessagesOffset( 0 )
		, NumMetadataMessages( 0 )
		, bRawStatsFile( false )
	{}

	/**
	 *	Version number to detect version mismatches
	 *	1 - Initial version for supporting raw ue4 stats files
	 */
	uint32	Version;

	/** Platform that this file was captured on. */
	FString	PlatformName;

	/** 
	 *  Offset in the file for the frame table. Serialized as TArray<int64>
	 *  For raw stats contains only one element which indicates the begin of the data. */
	uint64	FrameTableOffset;

	/** Offset in the file for the FName table. Serialized with WriteFName/ReadFName. */
	uint64	FNameTableOffset;

	/** Number of the FNames. */
	uint64	NumFNames;

	/** Offset in the file for the metadata messages. Serialized with WriteMessage/ReadMessage. */
	uint64	MetadataMessagesOffset;

	/** Number of the metadata messages. */
	uint64	NumMetadataMessages;

	/** Whether this stats file uses raw data, required for thread view/memory profiling/advanced profiling. */
	bool bRawStatsFile;

	/** Whether this stats file has all names stored at the end of file. */
	bool IsFinalized() const
	{
		return NumMetadataMessages > 0 && MetadataMessagesOffset > 0 && FrameTableOffset > 0;
	}

	/** Whether this stats file contains compressed data and needs the special serializer to read that data. */
	bool HasCompressedData() const
	{
		return Version >= EStatMagicWithHeader::HAS_COMPRESSED_DATA_VER;
	}

	/** Serialization operator. */
	friend FArchive& operator << (FArchive& Ar, FStatsStreamHeader& Header)
	{
		Ar << Header.Version;

		if( Ar.IsSaving() )
		{
			Header.PlatformName.SerializeAsANSICharArray( Ar, 255 );
		}
		else if( Ar.IsLoading() )
		{
			Ar << Header.PlatformName;
		}

		Ar << Header.FrameTableOffset;

		Ar << Header.FNameTableOffset
			<< Header.NumFNames;

		Ar << Header.MetadataMessagesOffset
			<< Header.NumMetadataMessages;

		Ar << Header.bRawStatsFile;

		return Ar;
	}
};

/**
 * Contains basic information about one frame of the stats.
 * This data is used to generate ultra-fast preview of the stats history without the requirement of reading the whole file.
 */
struct FStatsFrameInfo
{
	/** Empty constructor. */
	FStatsFrameInfo()
		: FrameFileOffset(0)
	{}

	/** Initialization constructor. */
	FStatsFrameInfo( int64 InFrameFileOffset )
		: FrameFileOffset(InFrameFileOffset)
	{}

	/** Initialization constructor. */
	FStatsFrameInfo( int64 InFrameFileOffset, TMap<uint32, int64>& InThreadCycles )
		: FrameFileOffset(InFrameFileOffset)
		, ThreadCycles(InThreadCycles)
	{}

	/** Serialization operator. */
	friend FArchive& operator << (FArchive& Ar, FStatsFrameInfo& Data)
	{
		Ar << Data.ThreadCycles << Data.FrameFileOffset;
		return Ar;
	}

	/** Frame offset in the stats file. */
	int64 FrameFileOffset;

	/** Thread cycles for this frame. */
	TMap<uint32, int64> ThreadCycles;
};

/** Interface for writing stats data. Can be only used in the stats thread. */
struct CORE_API IStatsWriteFile
{
	friend class FAsyncStatsWrite;

protected:
	/** Stats file archive. */
	FArchive* File;

	/** Filename of the archive that we are writing to. */
	FString ArchiveFilename;

	/** Stats stream header. */
	FStatsStreamHeader Header;

	/** Set of names already sent. */
	TSet<int32> FNamesSent;

	/** Async task used to offload saving the capture data. */
	FAsyncTask<FAsyncStatsWrite>* AsyncTask;

	/** Data to write through the async task. **/
	TArray<uint8> OutData;

	/** Buffer to store the compressed data, used by the FAsyncStatsWrite. */
	TArray<uint8> CompressedData;

	/**
	 *  Array of stats frames info already captured.
	 *  !!CAUTION!!
	 *  Only modified in the async write thread through FinalizeSavingData method.
	 */
	TArray<FStatsFrameInfo> FramesInfo;

protected:
	/** Default constructor. */
	IStatsWriteFile();

public:
	/** Destructor. */
	virtual ~IStatsWriteFile()
	{}

protected:
	/** NewFrame delegate handle  */
	FDelegateHandle DataDelegateHandle;

public:
	/** Creates a file writer and registers for the data delegate. */
	void Start( const FString& InFilename );

	/** Finalizes writing the stats data and unregisters the data delegate. */
	void Stop();

protected:
	/** Sets the data delegate used to receive a stats data. */
	virtual void SetDataDelegate( bool bSet ) = 0;

	/** Finalization code called after the compressed data has been saved. */
	virtual void FinalizeSavingData( int64 FrameFileOffset )
	{};

	bool IsValid() const
	{
		return !!File;
	}

public:
	const TArray<uint8>& GetOutData() const
	{
		return OutData;
	}

	void ResetData()
	{
		OutData.Reset();
	}

	/** Writes magic value, dummy header and initial metadata. */
	void WriteHeader();

protected:
	/** Writes metadata messages into the stream. */
	void WriteMetadata( FArchive& Ar );

	/**	Finalizes writing to the file. */
	void Finalize();

	/** Sends the data to the file via async task. */
	void SendTask();

	/** Sends an FName, and the string it represents if we have not sent that string before. **/
	FORCEINLINE_STATS void WriteFName( FArchive& Ar, FStatNameAndInfo NameAndInfo )
	{
		FName RawName = NameAndInfo.GetRawName();
		bool bSendFName = !FNamesSent.Contains( RawName.GetComparisonIndex() );
		int32 Index = RawName.GetComparisonIndex();
		Ar << Index;
		int32 Number = NameAndInfo.GetRawNumber();
		if( bSendFName )
		{
			FNamesSent.Add( RawName.GetComparisonIndex() );
			Number |= EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift);
		}
		Ar << Number;
		if( bSendFName )
		{
			FString Name = RawName.ToString();
			Ar << Name;
		}
	}

	/** Write a stat message. **/
	FORCEINLINE_STATS void WriteMessage( FArchive& Ar, FStatMessage const& Item )
	{
		WriteFName( Ar, Item.NameAndInfo );
		switch( Item.NameAndInfo.GetField<EStatDataType>() )
		{
			case EStatDataType::ST_int64:
			{
				int64 Payload = Item.GetValue_int64();
				Ar << Payload;
				break;
			}
				
			case EStatDataType::ST_double:
			{
				double Payload = Item.GetValue_double();
				Ar << Payload;
				break;
			}

			case EStatDataType::ST_FName:
			{
				WriteFName( Ar, FStatNameAndInfo( Item.GetValue_FName(), false ) );
				break;
			}

			case EStatDataType::ST_Ptr:
			{
				uint64 Payload = Item.GetValue_Ptr();
				Ar << Payload;
				break;
			}
		}
	}
};

/** Helper struct used to write regular stats to the file. */
struct CORE_API FStatsWriteFile : public IStatsWriteFile
{
	friend class FAsyncStatsWrite;

protected:
	/** Thread cycles for the last frame. */
	TMap<uint32, int64> ThreadCycles;

public:
	/** Default constructor, set bRawStatsFile to false. */
	FStatsWriteFile()
	{
		Header.bRawStatsFile = false;
	}

protected:
	virtual void SetDataDelegate( bool bSet ) override;

	virtual void FinalizeSavingData( int64 FrameFileOffset ) override;

public:
	/**
	 *	Grabs a frame from the local FStatsThreadState and writes it to the array.
	 *	Only used by the ProfilerServiceManager.
	 */
	void WriteFrame( int64 TargetFrame, bool bNeedFullMetadata );

protected:
	/**
	 *	Grabs a frame from the local FStatsThreadState and adds it to the output.
	 *	Called from the stats thread, but the data is saved using the the FAsyncStatsWrite. 
	 */
	void WriteFrame( int64 TargetFrame )
	{
		WriteFrame( TargetFrame, false );
		SendTask();
	}
};

/** Helper struct used to write raw stats to the file. */
struct CORE_API FRawStatsWriteFile : public IStatsWriteFile
{
	bool bWrittenOffsetToData;

public:
	/** Default constructor, set bRawStatsFile to true. */
	FRawStatsWriteFile()
		: bWrittenOffsetToData(false)
	{
		Header.bRawStatsFile = true;
	}

protected:
	virtual void SetDataDelegate( bool bSet ) override;

	void WriteRawStatPacket( const FStatPacket* StatPacket );

	/** Write a stat packed into the specified archive. */
	void WriteStatPacket( FArchive& Ar, FStatPacket& StatPacket );
};

/*-----------------------------------------------------------------------------
	Stats file reading functionality
-----------------------------------------------------------------------------*/

/**
 * Class for maintaining state of receiving a stream of stat messages
 */
struct CORE_API FStatsReadStream
{
public:
	/** Stats stream header. */
	FStatsStreamHeader Header;

	/** FNames have a different index on each machine, so we translate via this map. **/
	TMap<int32, int32> FNamesIndexMap;

	/** Array of stats frame info. Empty for the raw stats. */
	TArray<FStatsFrameInfo> FramesInfo;

	/** Reads a stats stream header, returns true if the header is valid and we can continue reading. */
	bool ReadHeader( FArchive& Ar )
	{
		bool bStatWithHeader = false;

		uint32 Magic = 0;
		Ar << Magic;
		if( Magic == EStatMagicNoHeader::MAGIC )
		{

		}
		else if( Magic == EStatMagicNoHeader::MAGIC_SWAPPED )
		{
			Ar.SetByteSwapping( true );
		}
		else if( Magic == EStatMagicWithHeader::MAGIC )
		{
			bStatWithHeader = true;
		}
		else if( Magic == EStatMagicWithHeader::MAGIC_SWAPPED )
		{
			bStatWithHeader = true;
			Ar.SetByteSwapping( true );
		}
		else
		{
			return false;
		}

		// We detected a header for a stats file, read it.
		if( bStatWithHeader )
		{
			Ar << Header;
		}

		return true;
	}

	// @TODO yrx 2014-12-02 Add a better way to read the file.

	/** Reads a stat packed from the specified archive. */
	void ReadStatPacket( FArchive& Ar, FStatPacket& StatPacked )
	{
		Ar << StatPacked.Frame;
		Ar << StatPacked.ThreadId;
		int32 MyThreadType = 0;
		Ar << MyThreadType;
		StatPacked.ThreadType = (EThreadType::Type)MyThreadType;

		Ar << StatPacked.bBrokenCallstacks;
		// We must handle stat messages in a different way.
		int32 NumMessages = 0;
		Ar << NumMessages;
		StatPacked.StatMessages.Reserve( NumMessages );
		for( int32 MessageIndex = 0; MessageIndex < NumMessages; ++MessageIndex )
		{
			new(StatPacked.StatMessages) FStatMessage( ReadMessage( Ar, true ) );
		}
	}

	/** Read and translate or create an FName. **/
	FORCEINLINE_STATS FStatNameAndInfo ReadFName( FArchive& Ar, bool bHasFNameMap )
	{
		// If we read the whole FNames translation map, we don't want to add the FName again.
		// This is a bit tricky, even if we have the FName translation map, we still need to read the FString.
		// CAUTION!! This is considered to be thread safe in this case.
		int32 Index = 0;
		Ar << Index;
		int32 Number = 0;
		Ar << Number;
		FName TheFName;
		
		if( !bHasFNameMap )
		{
			if( Number & (EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift)) )
			{
				FString Name;
				Ar << Name;

				TheFName = FName( *Name );
				FNamesIndexMap.Add( Index, TheFName.GetComparisonIndex() );
				Number &= ~(EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift));
			}
			else
			{
				if( FNamesIndexMap.Contains( Index ) )
				{
					int32 MyIndex = FNamesIndexMap.FindChecked( Index );
					TheFName = FName( MyIndex, MyIndex, 0 );
				}
				else
				{
					TheFName = FName( TEXT( "Unknown FName" ) );
					Number = 0;
					UE_LOG( LogTemp, Warning, TEXT( "Missing FName Indexed: %d, %d" ), Index, Number );
				}
			}
		}
		else
		{
			if( Number & (EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift)) )
			{
				FString Name;
				Ar << Name;
				Number &= ~(EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift));
			}
			if( FNamesIndexMap.Contains( Index ) )
			{
				int32 MyIndex = FNamesIndexMap.FindChecked( Index );
				TheFName = FName( MyIndex, MyIndex, 0 );
			}
			else
			{
				TheFName = FName( TEXT( "Unknown FName" ) );
				Number = 0;
				UE_LOG( LogTemp, Warning, TEXT( "Missing FName Indexed: %d, %d" ), Index, Number );
			}
		}

		FStatNameAndInfo Result( TheFName, false );
		Result.SetNumberDirect( Number );
		return Result;
	}

	/** Read a stat message. **/
	FORCEINLINE_STATS FStatMessage ReadMessage( FArchive& Ar, bool bHasFNameMap = false )
	{
		FStatMessage Result( ReadFName( Ar, bHasFNameMap ) );
		Result.Clear();
		switch( Result.NameAndInfo.GetField<EStatDataType>() )
		{
			case EStatDataType::ST_int64:
			{
				int64 Payload = 0;
				Ar << Payload;
				Result.GetValue_int64() = Payload;
				break;
			}
				
			case EStatDataType::ST_double:
			{
				double Payload = 0;
				Ar << Payload;
				Result.GetValue_double() = Payload;
				break;
			}
				
			case EStatDataType::ST_FName:
			{
				FStatNameAndInfo Payload( ReadFName( Ar, bHasFNameMap ) );
				Result.GetValue_FName() = Payload.GetRawName();
				break;
			}

			case EStatDataType::ST_Ptr:
			{
				uint64 Payload = 0;
				Ar << Payload;
				Result.GetValue_Ptr() = Payload;
				break;
			}
		}
		return Result;
	}

	/**
	 *	Reads stats frames info from the specified archive, only valid for finalized stats files.
	 *	Allows unordered file access and whole data mini-view.
	 */
	void ReadFramesOffsets( FArchive& Ar )
	{
		Ar.Seek( Header.FrameTableOffset );
		Ar << FramesInfo;
	}

	/**
	 *	Reads FNames and metadata messages from the specified archive, only valid for finalized stats files.
	 *	Allow unordered file access.
	 */
	void ReadFNamesAndMetadataMessages( FArchive& Ar, TArray<FStatMessage>& out_MetadataMessages )
	{
		// Read FNames.
		Ar.Seek( Header.FNameTableOffset );
		out_MetadataMessages.Reserve( Header.NumFNames );
		for( int32 Index = 0; Index < Header.NumFNames; Index++ )
		{
			ReadFName( Ar, false );
		}

		// Read metadata messages.
		Ar.Seek( Header.MetadataMessagesOffset );
		out_MetadataMessages.Reserve( Header.NumMetadataMessages );
		for( int32 Index = 0; Index < Header.NumMetadataMessages; Index++ )
		{
			new(out_MetadataMessages)FStatMessage( ReadMessage( Ar, false ) );
		}
	}
};


/*-----------------------------------------------------------------------------
	Commands functionality
-----------------------------------------------------------------------------*/

/** Implements 'Stat Start/StopFile' functionality. */
struct FCommandStatsFile
{
	/** Access to the singleton. */
	static CORE_API FCommandStatsFile& Get();

	/** Default constructor. */
	FCommandStatsFile()
		: FirstFrame(-1)
		, CurrentStatsFile(nullptr)
	{}

	/** Stat StartFile. */
	void Start( const FString& File );

	/** Stat StartFileRaw. */
	void StartRaw( const FString& File );

	/** Stat StopFile. */
	void Stop();

	bool IsStatFileActive()
	{
		return StatFileActiveCounter.GetValue() > 0;
	}

	/** Filename of the last saved stats file. */
	FString LastFileSaved;

protected:
	/** First frame. */
	int64 FirstFrame;

	/** Whether the 'stat startfile' command is active, can be accessed by other thread. */
	FThreadSafeCounter StatFileActiveCounter;

	/** Stats file that is currently being saved. */
	IStatsWriteFile* CurrentStatsFile;
};


#endif // STATS