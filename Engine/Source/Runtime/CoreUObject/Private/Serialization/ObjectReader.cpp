// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

///////////////////////////////////////////////////////
// FObjectReader

FArchive& FObjectReader::operator<<( class FName& N )
{
	NAME_INDEX ComparisonIndex;
	NAME_INDEX DisplayIndex;
	int32 Number;
	ByteOrderSerialize(&ComparisonIndex, sizeof(ComparisonIndex));
	ByteOrderSerialize(&DisplayIndex, sizeof(DisplayIndex));
	ByteOrderSerialize(&Number, sizeof(Number));
	// copy over the name with a name made from the name index and number
	N = FName(ComparisonIndex, DisplayIndex, Number);
	return *this;
}

FArchive& FObjectReader::operator<<( class UObject*& Res )
{
	ByteOrderSerialize(&Res, sizeof(Res));
	return *this;
}

FArchive& FObjectReader::operator<<( class FLazyObjectPtr& LazyObjectPtr )
{
	FArchive& Ar = *this;
	FUniqueObjectGuid ID;
	Ar << ID;

	LazyObjectPtr = ID;
	return Ar;
}

FArchive& FObjectReader::operator<<( class FAssetPtr& AssetPtr )
{
	FArchive& Ar = *this;
	FStringAssetReference ID;
	ID.Serialize(Ar);

	AssetPtr = ID;
	return Ar;
}

FArchive& FObjectReader::operator<<(FStringAssetReference& Value)
{
	return *this << Value.AssetLongPathname;
}

FString FObjectReader::GetArchiveName() const
{
	return TEXT("FObjectReader");
}
