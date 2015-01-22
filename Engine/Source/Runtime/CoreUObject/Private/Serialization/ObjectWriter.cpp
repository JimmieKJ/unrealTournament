// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

///////////////////////////////////////////////////////
// FObjectWriter

FArchive& FObjectWriter::operator<<( class FName& N )
{
	NAME_INDEX ComparisonIndex = N.GetComparisonIndex();
	NAME_INDEX DisplayIndex = N.GetDisplayIndex();
	int32 Number = N.GetNumber();
	ByteOrderSerialize(&ComparisonIndex, sizeof(ComparisonIndex));
	ByteOrderSerialize(&DisplayIndex, sizeof(DisplayIndex));
	ByteOrderSerialize(&Number, sizeof(Number));
	return *this;
}

FArchive& FObjectWriter::operator<<( class UObject*& Res )
{
	ByteOrderSerialize(&Res, sizeof(Res));
	return *this;
}

FArchive& FObjectWriter::operator<<( class FLazyObjectPtr& LazyObjectPtr )
{
	FUniqueObjectGuid ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

FArchive& FObjectWriter::operator<<( class FAssetPtr& AssetPtr )
{
	FStringAssetReference ID = AssetPtr.GetUniqueID();
	ID.Serialize(*this);
	return *this;
}

FArchive& FObjectWriter::operator<<(FStringAssetReference& Value)
{
	return *this << Value.AssetLongPathname;
}

FString FObjectWriter::GetArchiveName() const
{
	return TEXT("FObjectWriter");
}