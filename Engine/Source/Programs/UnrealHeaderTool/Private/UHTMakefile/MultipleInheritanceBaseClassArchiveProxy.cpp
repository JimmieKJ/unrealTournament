// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/MultipleInheritanceBaseClassArchiveProxy.h"

FMultipleInheritanceBaseClassArchiveProxy::FMultipleInheritanceBaseClassArchiveProxy(const FUHTMakefile& UHTMakefile, const FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass)
{
	ClassName = MultipleInheritanceBaseClass->ClassName;
	InterfaceClassIndex = UHTMakefile.GetClassIndex(MultipleInheritanceBaseClass->InterfaceClass);
}

void FMultipleInheritanceBaseClassArchiveProxy::AddReferencedNames(const FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass, FUHTMakefile& UHTMakefile)
{

}

FArchive& operator<<(FArchive& Ar, FMultipleInheritanceBaseClassArchiveProxy& MultipleInheritanceBaseClassArchiveProxy)
{
	Ar << MultipleInheritanceBaseClassArchiveProxy.ClassName;
	Ar << MultipleInheritanceBaseClassArchiveProxy.InterfaceClassIndex;

	return Ar;
}

void FMultipleInheritanceBaseClassArchiveProxy::Resolve(FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass, const FUHTMakefile& UHTMakefile) const
{
	MultipleInheritanceBaseClass->InterfaceClass = UHTMakefile.GetClassByIndex(InterfaceClassIndex);
}

FMultipleInheritanceBaseClass* FMultipleInheritanceBaseClassArchiveProxy::CreateMultipleInheritanceBaseClass() const
{
	return new FMultipleInheritanceBaseClass(ClassName);
}

