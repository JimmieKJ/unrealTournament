// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectArchetype.cpp: Unreal object archetype relationship management
=============================================================================*/

#include "CoreUObjectPrivate.h"


UObject* UObject::GetArchetypeFromRequiredInfo(UClass* Class, UObject* Outer, FName Name, EObjectFlags ObjectFlags)
{
	UObject* Result = NULL;
	const bool bIsCDO = !!(ObjectFlags&RF_ClassDefaultObject);
	if (bIsCDO)
	{
		Result = Class->GetArchetypeForCDO();
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
			else if (!!(ObjectFlags&RF_InheritableComponentTemplate) && Outer->IsA<UClass>())
			{
				for (auto SuperClassArchetype = static_cast<UClass*>(Outer)->GetSuperClass();
					SuperClassArchetype && SuperClassArchetype->HasAllClassFlags(CLASS_CompiledFromBlueprint);
					SuperClassArchetype = SuperClassArchetype->GetSuperClass())
				{
					Result = static_cast<UObject*>(FindObjectWithOuter(SuperClassArchetype, Class, Name));
					if (Result)
					{
						break;
					}
				}
			}
			else
			{
				Result = ArchetypeToSearch->GetClass()->FindArchetype(Class, Name);
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


