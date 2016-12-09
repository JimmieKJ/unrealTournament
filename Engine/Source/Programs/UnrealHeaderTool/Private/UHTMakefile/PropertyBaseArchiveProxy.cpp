// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyBaseArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UHTMakefile.h"

FPropertyBaseArchiveProxy::FPropertyBaseArchiveProxy(const FUHTMakefile& UHTMakefile, const FPropertyBase* PropertyBase)
{
	Type = PropertyBase->Type;
	ArrayType = PropertyBase->ArrayType;
	PropertyFlags = PropertyBase->PropertyFlags;
	ImpliedPropertyFlags = PropertyBase->ImpliedPropertyFlags;
	RefQualifier = PropertyBase->RefQualifier;
	MapKeyPropIndex = UHTMakefile.GetPropertyBaseIndex(PropertyBase->MapKeyProp.Get());
	PropertyExportFlags = PropertyBase->PropertyExportFlags;

	switch (Type)
	{
	case CPT_Byte:
		EnumIndex = UHTMakefile.GetEnumIndex(PropertyBase->Enum);
		break;
	case CPT_ObjectReference:
	case CPT_WeakObjectReference:
	case CPT_LazyObjectReference:
	case CPT_AssetObjectReference:
		PropertyClassIndex = UHTMakefile.GetClassIndex(PropertyBase->PropertyClass);
		break;
	case CPT_Delegate:
	case CPT_MulticastDelegate:
		FunctionIndex = UHTMakefile.GetFunctionIndex(PropertyBase->Function);
		break;
	case CPT_Struct:
		StructIndex = UHTMakefile.GetScriptStructIndex(PropertyBase->Struct);
		break;
	default:
		break;
	}

	MetaClassIndex = UHTMakefile.GetClassIndex(PropertyBase->MetaClass);
	DelegateName = FNameArchiveProxy(UHTMakefile, PropertyBase->DelegateName);
	RepNotifyName = FNameArchiveProxy(UHTMakefile, PropertyBase->RepNotifyName);;
	ExportInfo = PropertyBase->ExportInfo;
	MetaData.Empty(PropertyBase->MetaData.Num());
	for (auto& Kvp : PropertyBase->MetaData)
	{
		MetaData.Add(TPairInitializer<FNameArchiveProxy, FString>(FNameArchiveProxy(UHTMakefile, Kvp.Key), Kvp.Value));
	}
	PointerType = PropertyBase->PointerType;
}

void FPropertyBaseArchiveProxy::AddReferencedNames(const FPropertyBase* PropertyBase, FUHTMakefile& UHTMakefile)
{
	UHTMakefile.AddName(PropertyBase->DelegateName);
	UHTMakefile.AddName(PropertyBase->RepNotifyName);
}

FPropertyBase* FPropertyBaseArchiveProxy::CreatePropertyBase(const FUHTMakefile& UHTMakefile) const
{
	FPropertyBase* PropertyBase = new FPropertyBase(Type);
	PostConstruct(PropertyBase);
	return PropertyBase;
}

void FPropertyBaseArchiveProxy::PostConstruct(FPropertyBase* PropertyBase) const
{
	PropertyBase->Type = Type;
	PropertyBase->ArrayType = ArrayType;
	PropertyBase->PropertyFlags = PropertyFlags;
	PropertyBase->ImpliedPropertyFlags = ImpliedPropertyFlags;
	PropertyBase->RefQualifier = RefQualifier;
	PropertyBase->PropertyExportFlags = PropertyExportFlags;
	PropertyBase->ExportInfo = ExportInfo;
	PropertyBase->PointerType = PointerType;
}

void FPropertyBaseArchiveProxy::Resolve(FPropertyBase* PropertyBase, const FUHTMakefile& UHTMakefile) const
{
	FPropertyBase* MapKeyProp = UHTMakefile.GetPropertyBaseByIndex(MapKeyPropIndex);
	PropertyBase->MapKeyProp = MapKeyProp && MapKeyProp->HasBeenAlreadyMadeSharable() ? MapKeyProp->AsShared() : TSharedPtr<FPropertyBase>(MapKeyProp);

	switch (PropertyBase->Type)
	{
	case CPT_Byte:
		PropertyBase->Enum = UHTMakefile.GetEnumByIndex(EnumIndex);
		break;
	case CPT_ObjectReference:
	case CPT_WeakObjectReference:
	case CPT_LazyObjectReference:
	case CPT_AssetObjectReference:
		PropertyBase->PropertyClass = UHTMakefile.GetClassByIndex(PropertyClassIndex);
		break;
	case CPT_Delegate:
	case CPT_MulticastDelegate:
		PropertyBase->Function = UHTMakefile.GetFunctionByIndex(FunctionIndex);
		break;
	case CPT_Struct:
		PropertyBase->Struct = UHTMakefile.GetScriptStructByIndex(StructIndex);
		break;
	default:
		break;
	}

	PropertyBase->MetaClass = UHTMakefile.GetClassByIndex(MetaClassIndex);
	PropertyBase->DelegateName = DelegateName.CreateName(UHTMakefile);
	PropertyBase->RepNotifyName = RepNotifyName.CreateName(UHTMakefile);
	PropertyBase->MetaData.Empty(MetaData.Num());
	for (auto& Kvp : MetaData)
	{
		PropertyBase->MetaData.Add(Kvp.Key.CreateName(UHTMakefile), Kvp.Value);
	}
}

FArchive& operator<<(FArchive& Ar, FPropertyBaseArchiveProxy& PropertyBaseArchiveProxy)
{
	Ar << PropertyBaseArchiveProxy.Type;
	Ar << PropertyBaseArchiveProxy.ArrayType;
	Ar << PropertyBaseArchiveProxy.PropertyFlags;
	Ar << PropertyBaseArchiveProxy.ImpliedPropertyFlags;
	Ar << PropertyBaseArchiveProxy.RefQualifier;
	Ar << PropertyBaseArchiveProxy.MapKeyPropIndex;
	Ar << PropertyBaseArchiveProxy.PropertyExportFlags;
	Ar << PropertyBaseArchiveProxy.MetaClassIndex;
	Ar << PropertyBaseArchiveProxy.DelegateName;
	Ar << PropertyBaseArchiveProxy.RepNotifyName;
	Ar << PropertyBaseArchiveProxy.ExportInfo;
	Ar << PropertyBaseArchiveProxy.MetaData;
	Ar << PropertyBaseArchiveProxy.PointerType;

	switch (PropertyBaseArchiveProxy.Type)
	{
	case CPT_Byte:
		Ar << PropertyBaseArchiveProxy.EnumIndex;
		break;
	case CPT_ObjectReference:
	case CPT_WeakObjectReference:
	case CPT_LazyObjectReference:
	case CPT_AssetObjectReference:
		Ar << PropertyBaseArchiveProxy.PropertyClassIndex;
		break;
	case CPT_Delegate:
	case CPT_MulticastDelegate:
		Ar << PropertyBaseArchiveProxy.FunctionIndex;
		break;
	case CPT_Struct:
		Ar << PropertyBaseArchiveProxy.StructIndex;
		break;
	default:
		break;
	}

	return Ar;
}
