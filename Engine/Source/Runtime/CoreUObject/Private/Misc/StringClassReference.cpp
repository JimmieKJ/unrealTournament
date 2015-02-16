// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

#include "StringClassReference.h"
#include "PropertyTag.h"

#include "StringReferenceTemplates.h"

bool FStringClassReference::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar)
{
	struct UClassTypePolicy
	{
		typedef UClass Type;
		static const FName FORCEINLINE GetTypeName() { return NAME_ClassProperty; }
	};

	return SerializeFromMismatchedTagTemplate<UClassTypePolicy>(AssetLongPathname, Tag, Ar);
}

UClass* FStringClassReference::ResolveClass() const
{
	return dynamic_cast<UClass*>(ResolveObject());
}

FStringClassReference FStringClassReference::GetOrCreateIDForClass(const UClass *InClass)
{
	check(InClass);
	return FStringClassReference(InClass);
}
