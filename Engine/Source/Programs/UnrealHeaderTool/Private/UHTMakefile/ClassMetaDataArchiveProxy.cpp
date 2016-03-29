// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/ClassMetaDataArchiveProxy.h"

FClassMetaDataArchiveProxy::FClassMetaDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FClassMetaData* ClassMetaData)
{
	GlobalPropertyData = FPropertyDataArchiveProxy(UHTMakefile, &ClassMetaData->GlobalPropertyData);

	for (FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass : ClassMetaData->MultipleInheritanceParents)
	{
		MultipleInheritanceParents.Add(FMultipleInheritanceBaseClassArchiveProxy(UHTMakefile, MultipleInheritanceBaseClass));
	}

	bContainsDelegates = ClassMetaData->bContainsDelegates;
	PrologLine = ClassMetaData->PrologLine;
	GeneratedBodyLine = ClassMetaData->GeneratedBodyLine;
	InterfaceGeneratedBodyLine = ClassMetaData->InterfaceGeneratedBodyLine;
	bConstructorDeclared = ClassMetaData->bConstructorDeclared;
	bDefaultConstructorDeclared = ClassMetaData->bDefaultConstructorDeclared;
	bObjectInitializerConstructorDeclared = ClassMetaData->bObjectInitializerConstructorDeclared;
	bCustomVTableHelperConstructorDeclared = ClassMetaData->bCustomVTableHelperConstructorDeclared;
	GeneratedBodyMacroAccessSpecifier = ClassMetaData->GeneratedBodyMacroAccessSpecifier;
}

void FClassMetaDataArchiveProxy::AddReferencedNames(const FClassMetaData* ClassMetaData, FUHTMakefile& UHTMakefile)
{
	FPropertyDataArchiveProxy::AddReferencedNames(&ClassMetaData->GlobalPropertyData, UHTMakefile);

	for (FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass : ClassMetaData->MultipleInheritanceParents)
	{
		FMultipleInheritanceBaseClassArchiveProxy::AddReferencedNames(MultipleInheritanceBaseClass, UHTMakefile);
	}
}

TScopedPointer<FClassMetaData> FClassMetaDataArchiveProxy::CreateClassMetaData() const
{
	FClassMetaData* ClassMetaData = new FClassMetaData();
	PostConstruct(ClassMetaData);
	return TScopedPointer<FClassMetaData>(ClassMetaData);
}

void FClassMetaDataArchiveProxy::PostConstruct(FClassMetaData* ClassMetaData) const
{
	ClassMetaData->bContainsDelegates = bContainsDelegates;
	ClassMetaData->PrologLine = PrologLine;
	ClassMetaData->GeneratedBodyLine = GeneratedBodyLine;
	ClassMetaData->InterfaceGeneratedBodyLine = InterfaceGeneratedBodyLine;
	ClassMetaData->bConstructorDeclared = bConstructorDeclared;
	ClassMetaData->bDefaultConstructorDeclared = bDefaultConstructorDeclared;
	ClassMetaData->bObjectInitializerConstructorDeclared = bObjectInitializerConstructorDeclared;
	ClassMetaData->bCustomVTableHelperConstructorDeclared = bCustomVTableHelperConstructorDeclared;
	ClassMetaData->GeneratedBodyMacroAccessSpecifier = GeneratedBodyMacroAccessSpecifier;
}

void FClassMetaDataArchiveProxy::Resolve(FClassMetaData* ClassMetaData, FUHTMakefile& UHTMakefile)
{
	GlobalPropertyData.Resolve(&ClassMetaData->GlobalPropertyData, UHTMakefile);

	for (const FMultipleInheritanceBaseClassArchiveProxy& MultipleInheritanceBaseClassArchiveProxy : MultipleInheritanceParents)
	{
		FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass = MultipleInheritanceBaseClassArchiveProxy.CreateMultipleInheritanceBaseClass();
		MultipleInheritanceBaseClassArchiveProxy.Resolve(MultipleInheritanceBaseClass, UHTMakefile);
		ClassMetaData->MultipleInheritanceParents.Add(MultipleInheritanceBaseClass);
	}
}

FArchive& operator<<(FArchive& Ar, FClassMetaDataArchiveProxy& ClassMetaDataArchiveProxy)
{
	Ar << ClassMetaDataArchiveProxy.GlobalPropertyData;
	Ar << ClassMetaDataArchiveProxy.MultipleInheritanceParents;
	Ar << ClassMetaDataArchiveProxy.bContainsDelegates;
	Ar << ClassMetaDataArchiveProxy.PrologLine;
	Ar << ClassMetaDataArchiveProxy.GeneratedBodyLine;
	Ar << ClassMetaDataArchiveProxy.InterfaceGeneratedBodyLine;
	Ar << ClassMetaDataArchiveProxy.bConstructorDeclared;
	Ar << ClassMetaDataArchiveProxy.bDefaultConstructorDeclared;
	Ar << ClassMetaDataArchiveProxy.bObjectInitializerConstructorDeclared;
	Ar << ClassMetaDataArchiveProxy.bCustomVTableHelperConstructorDeclared;
	Ar << ClassMetaDataArchiveProxy.GeneratedBodyMacroAccessSpecifier;

	return Ar;
}
