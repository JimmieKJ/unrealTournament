// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FieldArchiveProxy.h"

class FUHTMakefile;
class UStruct;
class FArchive;

/* See UHTMakefile.h for overview how makefiles work. */
struct FStructArchiveProxy : public FFieldArchiveProxy
{
	FStructArchiveProxy() { }
	FStructArchiveProxy(FUHTMakefile& UHTMakefile, const UStruct* Struct);

	UStruct* CreateStruct(const FUHTMakefile& UHTMakefile) const;

	void PostConstruct(UStruct* Struct) const;

	void Resolve(UStruct* Struct, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FStructArchiveProxy& StructArchiveProxy);

	FSerializeIndex SuperStructIndex;
	FSerializeIndex ChildrenIndex;
	int32 PropertiesSize;
	int32 MinAlignment;
	TArray<uint8> Script;
	int32 PropertyLinkIndex;
	int32 RefLinkIndex;
	int32 DestructorLinkIndex;
	int32 PostConstructLinkIndex;
	TArray<FSerializeIndex> ScriptObjectReferenceIndexes;
};
