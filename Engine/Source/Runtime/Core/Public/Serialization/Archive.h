// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Core utility archive classes.  Must be separate from ArchiveBase.h since 
	UnrealTemplate.h references FArchive implementation.
				
=============================================================================*/

#pragma once

#include "EnumClassFlags.h"

/**
 * Base class for serializing arbitrary data in memory.
 */
class FMemoryArchive : public FArchive
{
public:
	/**
  	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FMemoryArchive"); }

	void Seek( int64 InPos ) final
	{
		Offset = InPos;
	}
	int64 Tell() final
	{
		return Offset;
	}

	using FArchive::operator<<; // For visibility of the overloads we don't override

	virtual FArchive& operator<<( class FName& N ) override
	{
		// Serialize the FName as a string
		if (IsLoading())
		{
			FString StringName;
			*this << StringName;
			N = FName(*StringName);
		}
		else
		{
			FString StringName = N.ToString();
			*this << StringName;
		}
		return *this;
	}

	virtual FArchive& operator<<( class UObject*& Res ) override
	{
		// Not supported through this archive
		check(0);
		return *this;
	}

protected:

	/** Marked as protected to avoid instantiating this class directly */
	FMemoryArchive()
		: FArchive(), Offset(0)
	{
	}

	int64				Offset;
};

/**
 * Archive for storing arbitrary data to the specified memory location
 */
class FMemoryWriter : public FMemoryArchive
{
public:
	FMemoryWriter( TArray<uint8>& InBytes, bool bIsPersistent = false, bool bSetOffset = false, const FName InArchiveName = NAME_None )
	: FMemoryArchive()
	, Bytes(InBytes)
	, ArchiveName(InArchiveName)
	{
		ArIsSaving		= true;
		ArIsPersistent	= bIsPersistent;
		if (bSetOffset)
		{
			Offset = InBytes.Num();
		}
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		const int64 NumBytesToAdd = Offset + Num - Bytes.Num();
		if( NumBytesToAdd > 0 )
		{
			const int64 NewArrayCount = Bytes.Num() + NumBytesToAdd;
			if( NewArrayCount >= MAX_int32 )
			{
				UE_LOG( LogSerialization, Fatal, TEXT( "FMemoryWriter does not support data larger than 2GB. Archive name: %s." ), *ArchiveName.ToString() );
			}

			Bytes.AddUninitialized( (int32)NumBytesToAdd );
		}

		check((Offset + Num) <= Bytes.Num());
		
		if( Num )
		{
			FMemory::Memcpy( &Bytes[Offset], Data, Num );
			Offset+=Num;
		}
	}
	/**
  	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const override { return TEXT("FMemoryWriter"); }

	int64 TotalSize() override
	{
		return Bytes.Num();
	}

protected:

	TArray<uint8>&	Bytes;

	/** Archive name, used to debugging, by default set to NAME_None. */
	const FName ArchiveName;
};

/**
* Archive for storing a large amount of arbitrary data to memory
*/
class CORE_API FLargeMemoryWriter : public FMemoryArchive
{
public:
	
	FLargeMemoryWriter(const int64 PreAllocateBytes = 0, bool bIsPersistent = false, const FName InArchiveName = NAME_None);

	virtual void Serialize(void* InData, int64 Num) override;

	/**
	* Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	* is in when a loading error occurs.
	*
	* This is overridden for the specific Archive Types
	**/
	virtual FString GetArchiveName() const override;

	/**
	 * Gets the total size of the data written
	 */
	virtual int64 TotalSize() override;
	
	/**
	 * Returns the written data. To release this archive's ownership of the data, call ReleaseOwnership()
	 */
	uint8* GetData() const;

	/** 
	 * Releases ownership of the written data
	 */
	void ReleaseOwnership();
	
	/**
	 * Destructor
	 */
	~FLargeMemoryWriter();

private:

	/** Non-copyable */
	FLargeMemoryWriter(const FLargeMemoryWriter&) = delete;
	FLargeMemoryWriter& operator=(const FLargeMemoryWriter&) = delete;

	/** Memory owned by this archive. Ownership can be released by calling ReleaseOwnership() */
	uint8* Data;

	/** Number of bytes currently written to our data buffer */
	int64 NumBytes;

	/** Number of bytes currently allocated for our data buffer */
	int64 MaxBytes;

	/** Resizes the data buffer to at least the desired new size with some slack */
	void GrowBuffer(const int64 DesiredBytes);

	/** Archive name, used for debugging, by default set to NAME_None. */
	const FName ArchiveName;
};

enum class ELargeMemoryReaderFlags : uint8
{
	None =				0x0,	// No Flags
	TakeOwnership =		0x1,	// Archive will take ownership of the passed-in memory and free it on destruction
	Persistent =		0x2,	// Archive will be set as persistent when constructed
};

ENUM_CLASS_FLAGS(ELargeMemoryReaderFlags);

/**
* Archive for reading a large amount of arbitrary data from memory
*/
class CORE_API FLargeMemoryReader : public FMemoryArchive
{
public:

	FLargeMemoryReader(const uint8* InData, const int64 Num, ELargeMemoryReaderFlags InFlags = ELargeMemoryReaderFlags::None, const FName InArchiveName = NAME_None);

	virtual void Serialize(void* OutData, int64 Num) override;

	/**
	* Gets the total size of the data buffer
	*/
	virtual int64 TotalSize() override;

	/**
	* Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	* is in when a loading error occurs.
	*
	* This is overridden for the specific Archive Types
	**/
	virtual FString GetArchiveName() const override;

	/**
	* Destructor
	*/
	~FLargeMemoryReader();

private:

	/** Non-copyable */
	FLargeMemoryReader(const FLargeMemoryReader&) = delete;
	FLargeMemoryReader& operator=(const FLargeMemoryReader&) = delete;

	/** Whether the data buffer should be freed when this archive is closed */
	const uint8 bFreeOnClose : 1;
	
	/** Data buffer we are reading from */
	const uint8* Data;

	/** Total size of the data buffer */
	const int64 NumBytes;

	/** Name of the archive this buffer is for */
	const FName ArchiveName;
};


/**
 * Buffer archiver.
 */
class FBufferArchive : public FMemoryWriter, public TArray<uint8>
{
public:
	FBufferArchive( bool bIsPersistent = false, const FName InArchiveName = NAME_None )
	: FMemoryWriter( (TArray<uint8>&)*this, bIsPersistent, false, InArchiveName )
	{}
	/**
  	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return *FString::Printf( TEXT("FBufferArchive %s"), *ArchiveName.ToString()); }
};

/**
 * Archive for reading arbitrary data from the specified memory location
 */
class FMemoryReader final : public FMemoryArchive
{
public:
	/**
  	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FMemoryReader"); }

	int64 TotalSize()
	{
		return FMath::Min((int64)Bytes.Num(), LimitSize);
	}

	void Serialize( void* Data, int64 Num )
	{
		if (Num && !ArIsError)
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Num <= TotalSize())
			{
				FMemory::Memcpy( Data, &Bytes[Offset], Num );
				Offset += Num;
			}
			else
			{
				ArIsError = true;
			}
		}
	}

	FMemoryReader( const TArray<uint8>& InBytes, bool bIsPersistent = false )
	: FMemoryArchive()
	, Bytes(InBytes)
	, LimitSize(INT64_MAX)
	{
		ArIsLoading		= true;
		ArIsPersistent	= bIsPersistent;
	}

	/** With this method it's possible to attach data behind some serialized data. */
	void SetLimitSize(int64 NewLimitSize)
	{
		LimitSize = NewLimitSize;
	}

protected:

	const TArray<uint8>& Bytes;
	int64 LimitSize;
};

/**
 * Archive objects that are also a TArray. Since FBufferArchive is already the 
 * writer version, we just typedef to the old, outdated name
 */
typedef FBufferArchive FArrayWriter;

class FArrayReader final : public FMemoryArchive, public TArray<uint8>
{
public:
	FArrayReader( bool bIsPersistent=false )
	{
		ArIsLoading = true;
		ArIsPersistent = bIsPersistent;
	}

	/**
	* Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	* is in when a loading error occurs.
	*
	* This is overridden for the specific Archive Types
	**/
	virtual FString GetArchiveName() const { return TEXT("FArrayReader"); }

	int64 TotalSize()
	{
		return (int64)Num();
	}

	void Serialize(void* Data, int64 Count)
	{
		if (Count && !ArIsError)
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Count <= Num())
			{
				FMemory::Memcpy(Data, &((*this)[Offset]), Count);
				Offset += Count;
			}
			else
			{
				ArIsError = true;
			}
		}
	}
};

/**
* Similar to FMemoryReader, but able to internally
* manage the memory for the buffer.
*/
class FBufferReaderBase : public FArchive
{
public:
	/**
	* Constructor
	*
	* @param Data Buffer to use as the source data to read from
	* @param Size Size of Data
	* @param bInFreeOnClose If true, Data will be FMemory::Free'd when this archive is closed
	* @param bIsPersistent Uses this value for ArIsPersistent
	* @param bInSHAVerifyOnClose It true, an async SHA verification will be done on the Data buffer (bInFreeOnClose will be passed on to the async task)
	*/
	FBufferReaderBase(void* Data, int64 Size, bool bInFreeOnClose, bool bIsPersistent = false)
		: ReaderData(Data)
		, ReaderPos(0)
		, ReaderSize(Size)
		, bFreeOnClose(bInFreeOnClose)
	{
		ArIsLoading = true;
		ArIsPersistent = bIsPersistent;
	}

	~FBufferReaderBase()
	{
		Close();
	}
	bool Close()
	{
		if (bFreeOnClose)
		{
			FMemory::Free(ReaderData);
			ReaderData = nullptr;
		}
		return !ArIsError;
	}
	void Serialize(void* Data, int64 Num) final
	{
		check(ReaderPos >= 0);
		check(ReaderPos + Num <= ReaderSize);
		FMemory::Memcpy(Data, (uint8*)ReaderData + ReaderPos, Num);
		ReaderPos += Num;
	}
	int64 Tell() final
	{
		return ReaderPos;
	}
	int64 TotalSize() final
	{
		return ReaderSize;
	}
	void Seek(int64 InPos) final 
	{
		check(InPos >= 0);
		check(InPos <= ReaderSize);
		ReaderPos = InPos;
	}
	bool AtEnd() final 
	{
		return ReaderPos >= ReaderSize;
	}
	/**
	* Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	* is in when a loading error occurs.
	*
	* This is overridden for the specific Archive Types
	**/
	virtual FString GetArchiveName() const { return TEXT("FBufferReaderBase"); }
protected:
	void*		ReaderData;
	int64		ReaderPos;
	int64		ReaderSize;
	bool	bFreeOnClose;
};

/**
 * Similar to FMemoryReader, but able to internally
 * manage the memory for the buffer.
 */
class FBufferReader final : public FBufferReaderBase
{
public:
	/**
	 * Constructor
	 * 
	 * @param Data Buffer to use as the source data to read from
	 * @param Size Size of Data
	 * @param bInFreeOnClose If true, Data will be FMemory::Free'd when this archive is closed
	 * @param bIsPersistent Uses this value for ArIsPersistent
	 * @param bInSHAVerifyOnClose It true, an async SHA verification will be done on the Data buffer (bInFreeOnClose will be passed on to the async task)
	 */
	FBufferReader( void* Data, int64 Size, bool bInFreeOnClose, bool bIsPersistent = false )
		: FBufferReaderBase(Data, Size, bInFreeOnClose, bIsPersistent)
	{
	}

	virtual FString GetArchiveName() const { return TEXT("FBufferReader"); }
};



/**
* Similar to FMemoryWriter, but able to internally
* manage the memory for the buffer.
*/
class FBufferWriter final : public FArchive
{
public:
	/**
	* Constructor
	*
	* @param Data Buffer to use as the source data to read from
	* @param Size Size of Data
	* @param bInFreeOnClose If true, Data will be FMemory::Free'd when this archive is closed
	* @param bIsPersistent Uses this value for ArIsPersistent
	*/
	FBufferWriter(void* Data, int64 Size, bool bInFreeOnClose, bool bIsPersistent = false)
		: WriterData(Data)
		, WriterPos(0)
		, WriterSize(Size)
		, bFreeOnClose(bInFreeOnClose)
	{
		ArIsSaving = true;
		ArIsPersistent = bIsPersistent;
	}

	~FBufferWriter()
	{
		Close();
	}
	bool Close()
	{
		if (bFreeOnClose)
		{
			FMemory::Free(WriterData);
			WriterData = nullptr;
		}
		return !ArIsError;
	}
	void Serialize(void* Data, int64 Num)
	{
		const int64 NumBytesToAdd = WriterPos + Num - WriterSize;
		if (NumBytesToAdd > 0)
		{
			const int64 NewArrayCount = WriterSize + NumBytesToAdd;
			if (NewArrayCount >= MAX_int32)
			{
				UE_LOG(LogSerialization, Fatal, TEXT("FBufferWriter does not support data larger than 2GB. Archive name: %s."), *GetArchiveName());
			}

			FMemory::Realloc(WriterData, NewArrayCount);
			WriterSize = NewArrayCount;
		}

		check(WriterPos >= 0);
		check((WriterPos + Num) <= WriterSize);
		FMemory::Memcpy((uint8*)WriterData + WriterPos, Data, Num);
		WriterPos += Num;
	}
	int64 Tell()
	{
		return WriterPos;
	}
	int64 TotalSize()
	{
		return WriterSize;
	}
	void Seek(int64 InPos)
	{
		check(InPos >= 0);
		check(InPos <= WriterSize);
		WriterPos = InPos;
	}
	bool AtEnd()
	{
		return WriterPos >= WriterSize;
	}
	/**
	* Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	* is in when a loading error occurs.
	*
	* This is overridden for the specific Archive Types
	**/
	virtual FString GetArchiveName() const { return TEXT("FBufferWriter"); }

	void* GetWriterData()
	{
		return WriterData;
	}
protected:
	void* WriterData;
	int64 WriterPos;
	int64 WriterSize;
	bool bFreeOnClose;
};
/*----------------------------------------------------------------------------
	FArchiveSaveCompressedProxy.
----------------------------------------------------------------------------*/

/**
 * FArchive Proxy to transparently write out compressed data to an array.
 */
class CORE_API FArchiveSaveCompressedProxy : public FArchive
{
public:
	/** 
	 * Constructor, initializing all member variables and allocating temp memory.
	 *
	 * @param	InCompressedData [ref]	Array of bytes that is going to hold compressed data
	 * @param	InCompressionFlags		Compression flags to use for compressing data
	 */
	FArchiveSaveCompressedProxy( TArray<uint8>& InCompressedData, ECompressionFlags InCompressionFlags );

	/** Destructor, flushing array if needed. Also frees temporary memory. */
	virtual ~FArchiveSaveCompressedProxy();

	/**
	 * Flushes tmp data to array.
	 */
	virtual void Flush();

	/**
	 * Serializes data to archive. This function is called recursively and determines where to serialize
	 * to and how to do so based on internal state.
	 *
	 * @param	Data	Pointer to serialize to
	 * @param	Count	Number of bytes to read
	 */
	virtual void Serialize( void* Data, int64 Count );

	/**
	 * Seeking is only implemented internally for writing out compressed data and asserts otherwise.
	 * 
	 * @param	InPos	Position to seek to
	 */
	virtual void Seek( int64 InPos );

	/**
	 * @return current position in uncompressed stream in bytes.
	 */
	virtual int64 Tell();

private:
	/** Array to write compressed data to.					*/
	TArray<uint8>&	CompressedData;
	/** Current index in array.								*/
	int32				CurrentIndex;
	/** Pointer to start of temporary buffer.				*/
	uint8*			TmpDataStart;
	/** Pointer to end of temporary buffer.					*/
	uint8*			TmpDataEnd;
	/** Pointer to current position in temporary buffer.	*/
	uint8*			TmpData;
	/** Whether to serialize to temporary buffer of array.	*/
	bool			bShouldSerializeToArray;
	/** Number of raw (uncompressed) bytes serialized.		*/
	int64		RawBytesSerialized;
	/** Flags to use for compression.						*/
	ECompressionFlags CompressionFlags;
};

/*----------------------------------------------------------------------------
	FArchiveLoadCompressedProxy.
----------------------------------------------------------------------------*/

/**
 * FArchive Proxy to transparently load compressed data from an array.
 */
class CORE_API FArchiveLoadCompressedProxy : public FArchive
{
public:
	/** 
	 * Constructor, initializing all member variables and allocating temp memory.
	 *
	 * @param	InCompressedData	Array of bytes that is holding compressed data
	 * @param	InCompressionFlags	Compression flags that were used to compress data
	 */
	FArchiveLoadCompressedProxy( const TArray<uint8>& InCompressedData, ECompressionFlags InCompressionFlags );

	/** Destructor, freeing temporary memory. */
	virtual ~FArchiveLoadCompressedProxy();

	/**
	 * Serializes data from archive. This function is called recursively and determines where to serialize
	 * from and how to do so based on internal state.
	 *
	 * @param	Data	Pointer to serialize to
	 * @param	Count	Number of bytes to read
	 */
	virtual void Serialize( void* Data, int64 Count );

	/**
	 * Seeks to the passed in position in the stream. This archive only supports forward seeking
	 * and implements it by serializing data till it reaches the position.
	 */
	virtual void Seek( int64 InPos );

	/**
	 * @return current position in uncompressed stream in bytes.
	 */
	virtual int64 Tell();

private:
	/**
	 * Flushes tmp data to array.
	 */
	void DecompressMoreData();

	/** Array to write compressed data to.						*/
	const TArray<uint8>&	CompressedData;
	/** Current index into compressed data array.				*/
	int32				CurrentIndex;
	/** Pointer to start of temporary buffer.					*/
	uint8*			TmpDataStart;
	/** Pointer to end of temporary buffer.						*/
	uint8*			TmpDataEnd;
	/** Pointer to current position in temporary buffer.		*/
	uint8*			TmpData;
	/** Whether to serialize from temporary buffer of array.	*/
	bool			bShouldSerializeFromArray;
	/** Number of raw (uncompressed) bytes serialized.			*/
	int64		RawBytesSerialized;
	/** Flags used for compression.								*/
	ECompressionFlags CompressionFlags;
};

