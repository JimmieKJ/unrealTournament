// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObject/UObjectThreadContext.h"

/*----------------------------------------------------------------------------
	FDuplicateDataReader.
----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	InDuplicatedObjects		map of original object to copy of that object
 * @param	InObjectData			object data to read from
 */
FDuplicateDataReader::FDuplicateDataReader( class FUObjectAnnotationSparse<FDuplicatedObject,false>& InDuplicatedObjects ,const TArray<uint8>& InObjectData, uint32 InPortFlags )
: DuplicatedObjectAnnotation(InDuplicatedObjects)
, ObjectData(InObjectData)
, Offset(0)
{
	ArIsLoading			= true;
	ArIsPersistent		= true;
	ArPortFlags |= PPF_Duplicate | InPortFlags;
}

void FDuplicateDataReader::SerializeFail()
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	UE_LOG(LogObj, Fatal, TEXT("FDuplicateDataReader Overread. SerializedObject = %s SerializedProperty = %s"), *GetFullNameSafe(ThreadContext.SerializedObject), *GetFullNameSafe(GetSerializedProperty()));
}

FArchive& FDuplicateDataReader::operator<<(FName& N)
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

FArchive& FDuplicateDataReader::operator<<( UObject*& Object )
{
	UObject*	SourceObject = Object;
	Serialize(&SourceObject,sizeof(UObject*));

	FDuplicatedObject ObjectInfo = SourceObject ? DuplicatedObjectAnnotation.GetAnnotation(SourceObject) : FDuplicatedObject();
	if( !ObjectInfo.IsDefault() )
	{
		Object = ObjectInfo.DuplicatedObject;
	}
	else
	{
		Object = SourceObject;
	}

	return *this;
}

FArchive& FDuplicateDataReader::operator<<( FLazyObjectPtr& LazyObjectPtr)
{
	FArchive& Ar = *this;
	FUniqueObjectGuid ID;
	Ar << ID;

	if (Ar.GetPortFlags()&PPF_DuplicateForPIE)
	{
		// Remap unique ID if necessary
		ID = ID.FixupForPIE();
	}
	LazyObjectPtr = ID;
	return Ar;
}

FArchive& FDuplicateDataReader::operator<<( FAssetPtr& AssetPtr)
{
	FArchive& Ar = *this;
	FStringAssetReference ID;
	ID.Serialize(Ar);

	AssetPtr = ID;
	return Ar;
}
