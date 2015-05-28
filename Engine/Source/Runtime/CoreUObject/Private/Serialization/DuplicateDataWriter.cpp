// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

/*----------------------------------------------------------------------------
	FDuplicateDataWriter.
----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	InDuplicatedObjects		will contain the original object -> copy mappings
 * @param	InObjectData			will store the serialized data
 * @param	SourceObject			the object to copy
 * @param	DestObject				the object to copy to
 * @param	InFlagMask				the flags that should be copied when the object is duplicated
 * @param	InApplyFlags			the flags that should always be set on the duplicated objects (regardless of whether they're set on the source)
 * @param	InInstanceGraph			the instancing graph to use when creating the duplicate objects.
 */
FDuplicateDataWriter::FDuplicateDataWriter( FUObjectAnnotationSparse<FDuplicatedObject,false>& InDuplicatedObjects ,TArray<uint8>& InObjectData,UObject* SourceObject,
										   UObject* DestObject,EObjectFlags InFlagMask, EObjectFlags InApplyFlags, FObjectInstancingGraph* InInstanceGraph, uint32 InPortFlags )
: DuplicatedObjectAnnotation(InDuplicatedObjects)
, ObjectData(InObjectData)
, Offset(0)
, FlagMask(InFlagMask)
, ApplyFlags(InApplyFlags)
, InstanceGraph(InInstanceGraph)
{
	ArIsSaving			= true;
	ArIsPersistent		= true;
	ArAllowLazyLoading	= false;
	ArPortFlags |= PPF_Duplicate | InPortFlags;

	AddDuplicate(SourceObject,DestObject);
}

/**
 * I/O function
 */
FArchive& FDuplicateDataWriter::operator<<(FName& N)
{
	NAME_INDEX ComparisonIndex = N.GetComparisonIndex();
	NAME_INDEX DisplayIndex = N.GetDisplayIndex();
	int32 Number = N.GetNumber();
	ByteOrderSerialize(&ComparisonIndex, sizeof(ComparisonIndex));
	ByteOrderSerialize(&DisplayIndex, sizeof(DisplayIndex));
	ByteOrderSerialize(&Number, sizeof(Number));
	return *this;
}

FArchive& FDuplicateDataWriter::operator<<(UObject*& Object)
{
	GetDuplicatedObject(Object);

	// store the pointer to this object
	Serialize(&Object,sizeof(UObject*));
	return *this;
}

FArchive& FDuplicateDataWriter::operator<<(FLazyObjectPtr& LazyObjectPtr)
{
	if (UObject* DuplicatedObject = GetDuplicatedObject(LazyObjectPtr.Get(), false))
	{
		FLazyObjectPtr Ptr(DuplicatedObject);
		return *this << Ptr.GetUniqueID();
	}

	FUniqueObjectGuid ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

FArchive& FDuplicateDataWriter::operator<<(FAssetPtr& AssetPtr)
{
	FStringAssetReference ID = AssetPtr.GetUniqueID();
	ID.Serialize(*this);
	return *this;
}

/**
 * Places a new duplicate in the DuplicatedObjects map as well as the UnserializedObjects list
 *
 * @param	SourceObject		the original version of the object
 * @param	DuplicateObject		the copy of the object
 *
 * @return	a pointer to the copy of the object
 */
UObject* FDuplicateDataWriter::AddDuplicate(UObject* SourceObject,UObject* DupObject)
{
	if ( DupObject && !DupObject->IsTemplate() )
	{
		// Make sure the duplicated object is prepared to postload
		DupObject->SetFlags(RF_NeedPostLoad|RF_NeedPostLoadSubobjects);
	}

	// Check for an existing duplicate of the object; if found, use that one instead of creating a new one.
	FDuplicatedObject Info = DuplicatedObjectAnnotation.GetAnnotation( SourceObject );
	if ( Info.IsDefault() )
	{
		DuplicatedObjectAnnotation.AddAnnotation( SourceObject, FDuplicatedObject( DupObject ) );
	}
	else
	{
		Info.DuplicatedObject = DupObject;
	}


	UnserializedObjects.Add(SourceObject);
	return DupObject;
}

/**
 * Returns a pointer to the duplicate of a given object, creating the duplicate object if necessary.
 *
 * @param	Object	the object to find a duplicate for
 *
 * @return	a pointer to the duplicate of the specified object
 */
UObject* FDuplicateDataWriter::GetDuplicatedObject(UObject* Object, bool bCreateIfMissing)
{
	UObject* Result = NULL;
	if (IsValid(Object))
	{
		// Check for an existing duplicate of the object.
		FDuplicatedObject DupObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( Object );
		if( !DupObjectInfo.IsDefault() )
		{
			Result = DupObjectInfo.DuplicatedObject;
		}
		else if (bCreateIfMissing)
		{
			// Check to see if the object's outer is being duplicated.
			UObject* DupOuter = GetDuplicatedObject(Object->GetOuter());
			if(DupOuter != NULL)
			{
				// The object's outer is being duplicated, create a duplicate of this object.
				UObject* NewEmptyDuplicate = StaticConstructObject_Internal(Object->GetClass(), DupOuter, Object->GetFName(), ApplyFlags|Object->GetMaskedFlags(FlagMask),
					Object->GetArchetype(), true, InstanceGraph);

				Result = AddDuplicate(Object, NewEmptyDuplicate);
			}
		}
	}

	return Result;
}