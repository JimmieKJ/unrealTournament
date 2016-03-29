// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

	FString Path = ToString();

	bool bReturn = SerializeFromMismatchedTagTemplate<UClassTypePolicy>(Path, Tag, Ar);

	if (Ar.IsLoading())
	{
		SetPath(MoveTemp(Path));
	}

	return bReturn;
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
