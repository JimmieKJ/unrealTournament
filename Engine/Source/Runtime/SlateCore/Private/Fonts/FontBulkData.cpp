// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "FontBulkData.h"

UFontBulkData::UFontBulkData()
{
	BulkData.SetBulkDataFlags(BULKDATA_SerializeCompressed);
}

void UFontBulkData::Initialize(const FString& InFontFilename)
{
	BulkData.SetBulkDataFlags(BULKDATA_SerializeCompressed);

	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*InFontFilename, 0));
	if(Reader)
	{
		const int32 FontDataSizeBytes = Reader->TotalSize();

		BulkData.Lock(LOCK_READ_WRITE);
		void* const LockedFontData = BulkData.Realloc(FontDataSizeBytes);
		Reader->Serialize(LockedFontData, FontDataSizeBytes);
		BulkData.Unlock();
	}
	else
	{
		UE_LOG(LogSlate, Warning, TEXT("Failed to load font data from '%s'"), *InFontFilename);
	}
}

void UFontBulkData::Initialize(const void* const InFontData, const int32 InFontDataSizeBytes)
{
	BulkData.SetBulkDataFlags(BULKDATA_SerializeCompressed);

	BulkData.Lock(LOCK_READ_WRITE);
	void* const LockedFontData = BulkData.Realloc(InFontDataSizeBytes);
	FMemory::Memcpy(LockedFontData, InFontData, InFontDataSizeBytes);
	BulkData.Unlock();
}

const void* UFontBulkData::Lock(int32& OutFontDataSizeBytes) const
{
	CriticalSection.Lock();
	OutFontDataSizeBytes = BulkData.GetBulkDataSize();
	return BulkData.LockReadOnly();
}

void UFontBulkData::Unlock() const
{
	BulkData.Unlock();
	CriticalSection.Unlock();
}

void UFontBulkData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	BulkData.Serialize(Ar, this);
}
