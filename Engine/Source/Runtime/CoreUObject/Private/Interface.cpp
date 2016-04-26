// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "Interface.h"

#if WITH_EDITOR
IMPLEMENT_CORE_INTRINSIC_CLASS(UInterface, UObject,
	{
		Class->ClassAddReferencedObjects = &UInterface::AddReferencedObjects;
		Class->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
		Class->SetMetaData(TEXT("CannotImplementInterfaceInBlueprint"), TEXT(""));
	}
);

#else

IMPLEMENT_CORE_INTRINSIC_CLASS(UInterface, UObject,
{
	Class->ClassAddReferencedObjects = &UInterface::AddReferencedObjects;
}
);

#endif