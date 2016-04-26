// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/MetadataArchiveProxy.h"

FMetadataArchiveProxy::FMetadataArchiveProxy(FUHTMakefile& UHTMakefile, UMetaData* Metadata)
	: FObjectBaseArchiveProxy(UHTMakefile, Metadata)
{
	for (auto& Kvp : UMetaData::KeyRedirectMap)
	{
		KeyRedirectMap.Add(TPairInitializer<FNameArchiveProxy, FNameArchiveProxy>(FNameArchiveProxy(UHTMakefile, Kvp.Key), FNameArchiveProxy(UHTMakefile, Kvp.Value)));
	}
	
	for (auto& Kvp : Metadata->ObjectMetaDataMap)
	{
		FSerializeIndex Index = UHTMakefile.GetObjectIndex(Kvp.Key.Get());
		TArray<TPair<FNameArchiveProxy, FString>> Value;
		Value.Empty(Kvp.Value.Num());
		for (const auto& KvpInner : Kvp.Value)
		{
			Value.Add(TPairInitializer<FNameArchiveProxy, FString>(FNameArchiveProxy(UHTMakefile, KvpInner.Key), KvpInner.Value));
		}
		ObjectMetaDataMapProxy.Add(TPairInitializer<FSerializeIndex, TArray<TPair<FNameArchiveProxy, FString>>>(Index, Value));
	}
}

UMetaData* FMetadataArchiveProxy::CreateMetadata(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UMetaData* Metadata = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UMetaData(FObjectInitializer());
	for (auto& Kvp : KeyRedirectMap)
	{
		UMetaData::KeyRedirectMap.Add(Kvp.Key.CreateName(UHTMakefile), Kvp.Value.CreateName(UHTMakefile));
	}

	return Metadata;
}

void FMetadataArchiveProxy::Resolve(UMetaData* Metadata, const FUHTMakefile& UHTMakefile) const
{
	for (auto& Kvp : ObjectMetaDataMapProxy)
	{
		UObject* Object = UHTMakefile.GetObjectByIndex(Kvp.Key);
		TMap<FName, FString>& AddedObj = Metadata->ObjectMetaDataMap.Add(Object);
		AddedObj.Reserve(AddedObj.Num() + Kvp.Value.Num());
		for (const auto& KvpInner : Kvp.Value)
		{
			AddedObj.Add(KvpInner.Key.CreateName(UHTMakefile), KvpInner.Value);
		}
	}
}

void FMetadataArchiveProxy::AddReferencedNames(UMetaData* Metadata, FUHTMakefile& UHTMakefile)
{
	FObjectBaseArchiveProxy::AddReferencedNames(Metadata, UHTMakefile);
	for (auto& Kvp : Metadata->ObjectMetaDataMap)
	{
		for (const auto& KvpInner : Kvp.Value)
		{
			UHTMakefile.AddName(KvpInner.Key);
		}
	}
}

void FMetadataArchiveProxy::AddStaticallyReferencedNames(FUHTMakefile& UHTMakefile)
{
	for (auto& Kvp : UMetaData::KeyRedirectMap)
	{
		UHTMakefile.AddName(Kvp.Key);
		UHTMakefile.AddName(Kvp.Value);
	}
}

FArchive& operator<<(FArchive& Ar, FMetadataArchiveProxy& MetadataArchiveProxy)
{
	Ar << static_cast<FObjectBaseArchiveProxy&>(MetadataArchiveProxy);
	Ar << MetadataArchiveProxy.ObjectMetaDataMapProxy;
	Ar << MetadataArchiveProxy.KeyRedirectMap;

	return Ar;
}

