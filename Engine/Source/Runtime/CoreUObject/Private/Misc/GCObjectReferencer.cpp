// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GCObjectReferencer.cpp: Implementation of UGCObjectReferencer
=============================================================================*/

#include "CoreUObjectPrivate.h"

void UGCObjectReferencer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UGCObjectReferencer* This = CastChecked<UGCObjectReferencer>(InThis);
	// Let each registered object handle its AddReferencedObjects call
	for (int32 i = 0; i < This->ReferencedObjects.Num(); i++)
	{
		FGCObject* Object = This->ReferencedObjects[i];
		check(Object);
		Object->AddReferencedObjects(Collector);
	}
	Super::AddReferencedObjects( This, Collector );
}

void UGCObjectReferencer::AddObject(FGCObject* Object)
{
	check(Object);
	// Make sure there are no duplicates. Should be impossible...
	ReferencedObjects.AddUnique(Object);
}

void UGCObjectReferencer::RemoveObject(FGCObject* Object)
{
	check(Object);
	ReferencedObjects.Remove(Object);
}

void UGCObjectReferencer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Make sure FGCObjects that are around after exit purge don't
		// reference this object.
		check( FGCObject::GGCObjectReferencer == this );
		FGCObject::GGCObjectReferencer = NULL;
	}

	Super::FinishDestroy();
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UGCObjectReferencer, UObject, 
	{
		Class->ClassAddReferencedObjects = &UGCObjectReferencer::AddReferencedObjects;
	}
);

/** Static used for calling AddReferencedObjects on non-UObject objects */
UGCObjectReferencer* FGCObject::GGCObjectReferencer = NULL;


