// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImplementedInterfaceArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UHTMakefile.h"

FImplementedInterfaceArchiveProxy::FImplementedInterfaceArchiveProxy(FUHTMakefile& UHTMakefile, const FImplementedInterface& ImplementedInterface)
{
	ClassIndex = UHTMakefile.GetClassIndex(ImplementedInterface.Class);
	PointerOffset = ImplementedInterface.PointerOffset;
	bImplementedByK2 = ImplementedInterface.bImplementedByK2;
}

FImplementedInterface FImplementedInterfaceArchiveProxy::CreateImplementedInterface(const FUHTMakefile& UHTMakefile) const
{
	return FImplementedInterface(nullptr, PointerOffset, bImplementedByK2);
}

void FImplementedInterfaceArchiveProxy::Resolve(FImplementedInterface& ImplementedInterface, const FUHTMakefile& UHTMakefile) const
{
	ImplementedInterface.Class = UHTMakefile.GetClassByIndex(ClassIndex);
}

FArchive& operator<<(FArchive& Ar, FImplementedInterfaceArchiveProxy& ImplementedInterfaceArchiveProxy)
{
	Ar << ImplementedInterfaceArchiveProxy.ClassIndex;
	Ar << ImplementedInterfaceArchiveProxy.PointerOffset;
	Ar << ImplementedInterfaceArchiveProxy.bImplementedByK2;

	return Ar;
}
