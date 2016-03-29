// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FObjectBaseArchiveProxy
{
	FObjectBaseArchiveProxy() { }
	FObjectBaseArchiveProxy(FUHTMakefile& UHTMakefile, const UObjectBase* ObjectBase);

	UObjectBase* CreateObjectBase(const FUHTMakefile& UHTMakefile) const;
	void Resolve(UObjectBase* ObjectBase, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FObjectBaseArchiveProxy& ObjectBaseArchiveProxy);
	
	static void AddReferencedNames(const UObjectBase* ObjectBase, FUHTMakefile& UHTMakefile);
	void PostConstruct(UObjectBase* ObjectBase) const;
	uint32 ObjectFlagsUint32;
	FSerializeIndex ClassIndex;
	FNameArchiveProxy Name;
	FSerializeIndex OuterIndex;
};
