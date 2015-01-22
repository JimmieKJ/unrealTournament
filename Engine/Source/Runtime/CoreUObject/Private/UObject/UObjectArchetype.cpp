// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectArchetype.cpp: Unreal object archetype relationship management
=============================================================================*/

#include "CoreUObjectPrivate.h"


UObject* UObject::GetArchetypeFromRequiredInfo(UClass* Class, UObject* Outer, FName Name, bool bIsCDO)
{
	UObject* Result = NULL;
	if (bIsCDO)
	{
		static UClass* UObjectStaticClass(UObject::StaticClass());
		if (Class != UObjectStaticClass)
		{
			Result = Class->GetSuperClass()->GetDefaultObject(); // the archetype of any CDO (other than the UObject CDO) is the CDO of the super class
		}
		// else the archetype of the UObject CDO is NULL
	}
	else
	{
		if (Outer 
			&& Outer->GetClass() != UPackage::StaticClass()) // packages cannot have subobjects
		{
			UObject* ArchetypeToSearch = Outer->GetArchetype();
			UObject* MyArchetype = static_cast<UObject*>(FindObjectWithOuter(ArchetypeToSearch, Class, Name));
			if (MyArchetype)
			{
				Result = MyArchetype; // found that my outers archetype had a matching component, that must be my archetype
			}
		}
		if (!Result)
		{
			// nothing found, I am not a CDO, so this is just the class CDO
			Result = Class->GetDefaultObject();
		}
	}
	return Result;
}


