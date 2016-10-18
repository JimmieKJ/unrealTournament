// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "FontBulkData.h"

// The total amount of memory we are using to store raw font bytes in bulk data
DECLARE_MEMORY_STAT(TEXT("Font BulkData Memory"), STAT_SlateBulkFontDataMemory, STATGROUP_SlateMemory);

UFontBulkData::UFontBulkData()
{
	BulkData.SetBulkDataFlags(BULKDATA_SerializeCompressed|BULKDATA_SerializeCompressedBitWindow);
}

void UFontBulkData::Initialize(const FString& InFontFilename)
{
	// The bulk data cannot be removed if we are loading from source file
	BulkData.ClearBulkDataFlags( BULKDATA_SingleUse );
	
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*InFontFilename, 0));
	if(Reader)
	{
		const int32 FontDataSizeBytes = Reader->TotalSize();

		BulkData.Lock(LOCK_READ_WRITE);
		void* const LockedFontData = BulkData.Realloc(FontDataSizeBytes);
		Reader->Serialize(LockedFontData, FontDataSizeBytes);
		BulkData.Unlock();
		INC_DWORD_STAT_BY( STAT_SlateBulkFontDataMemory, BulkData.GetBulkDataSize() );
	}
	else
	{
		UE_LOG(LogSlate, Warning, TEXT("Failed to load font data from '%s'"), *InFontFilename);
	}
}

void UFontBulkData::Initialize(const void* const InFontData, const int32 InFontDataSizeBytes)
{
	// The bulk data cannot be removed if we are loading a memory location since we 
	// have no knowledge of this memory later
	BulkData.ClearBulkDataFlags( BULKDATA_SingleUse );

	BulkData.Lock(LOCK_READ_WRITE);
	void* const LockedFontData = BulkData.Realloc(InFontDataSizeBytes);
	FMemory::Memcpy(LockedFontData, InFontData, InFontDataSizeBytes);
	BulkData.Unlock();
	
	INC_DWORD_STAT_BY( STAT_SlateBulkFontDataMemory, BulkData.GetBulkDataSize() );
}

const void* UFontBulkData::Lock(int32& OutFontDataSizeBytes) const
{
	CriticalSection.Lock();

#if STATS
	bool bWasLoaded = BulkData.IsBulkDataLoaded();
#endif

	OutFontDataSizeBytes = BulkData.GetBulkDataSize();

	const void* Data = BulkData.LockReadOnly();

#if STATS
	if( !bWasLoaded && BulkData.IsBulkDataLoaded() )
	{
		INC_DWORD_STAT_BY( STAT_SlateBulkFontDataMemory, BulkData.GetBulkDataSize() );
	}
#endif

	return Data;

}

void UFontBulkData::Unlock() const
{
	bool bWasLoaded = BulkData.IsBulkDataLoaded();
	int32 BulkDataSize = BulkData.GetBulkDataSize(); 
	BulkData.Unlock();

#if STATS
	if( bWasLoaded && !BulkData.IsBulkDataLoaded() )
	{
		DEC_DWORD_STAT_BY( STAT_SlateBulkFontDataMemory, BulkDataSize );
	}
#endif
	CriticalSection.Unlock();
}

void UFontBulkData::ForceLoadBulkData()
{
	FScopeLock Lock(&CriticalSection);

	// Keep the bulk data resident once it's been loaded
	BulkData.ClearBulkDataFlags(BULKDATA_SingleUse);

#if STATS
	const bool bWasLoaded = BulkData.IsBulkDataLoaded();
#endif

	// Trigger the load (if needed)
	BulkData.LockReadOnly();
	BulkData.Unlock();

#if STATS
	if (!bWasLoaded && BulkData.IsBulkDataLoaded())
	{
		INC_DWORD_STAT_BY(STAT_SlateBulkFontDataMemory, BulkData.GetBulkDataSize());
	}
#endif
}

int32 UFontBulkData::GetBulkDataSize() const
{
	return BulkData.GetBulkDataSize();
}

int32 UFontBulkData::GetBulkDataSizeOnDisk() const
{
	return BulkData.GetBulkDataSizeOnDisk();
}

void UFontBulkData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	BulkData.Serialize(Ar, this);

	if( !GIsEditor && Ar.IsLoading() )
	{
		BulkData.SetBulkDataFlags( BULKDATA_SingleUse );
	}

#if STATS
	if( Ar.IsLoading() && BulkData.IsBulkDataLoaded() )
	{
		INC_DWORD_STAT_BY( STAT_SlateBulkFontDataMemory, BulkData.GetBulkDataSize() );
	}
#endif
}
