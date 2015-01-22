// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "ArchiveFindAllRefs.h"


FArchiveFindAllRefs::FArchiveFindAllRefs( UObject* Src )
{
	// use the optimized RefLink to skip over properties which don't contain object references
	ArIsObjectReferenceCollector = true;

	ArIgnoreArchetypeRef				= false;
	ArIgnoreOuterRef					= true;
	ArIgnoreClassRef					= false;

	Src->Serialize( *this );
}

FArchive& FArchiveFindAllRefs::operator<<( class UObject*& Obj )
{
	if (Obj)
	{
		References.AddUnique(Obj);
	}
	return *this;
}
