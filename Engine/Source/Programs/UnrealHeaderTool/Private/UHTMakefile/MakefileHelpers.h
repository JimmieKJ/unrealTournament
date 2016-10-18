// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UHTMakefile/NameArchiveProxy.h"

static const int32 IndexOfNativeClass = -2;

struct FSerializeIndex
{
	FSerializeIndex()
		: Index(INDEX_NONE)
		, NameProxy(FNameArchiveProxy())
		, Name(NAME_None)
	{ }

	FSerializeIndex(const FUHTMakefile& UHTMakefile, FName InName)
		: Index(IndexOfNativeClass)
		, NameProxy(FNameArchiveProxy(UHTMakefile, InName))
		, Name(InName)
	{ }

	FSerializeIndex(FName InName, int32 InIndex)
		: Index(InIndex)
		, NameProxy(FNameArchiveProxy())
		, Name(InName)
	{ }

	explicit FSerializeIndex(int32 InIndex)
		: Index(InIndex)
		, NameProxy(FNameArchiveProxy())
		, Name(NAME_None)
	{ }

	bool IsNone() const
	{
		return Name == NAME_None && Index == INDEX_NONE;
	}

	bool HasName()
	{
		return Index == IndexOfNativeClass;
	}

	int32 Index;
	FNameArchiveProxy NameProxy;
	FName Name;
};

inline FSerializeIndex operator+(FSerializeIndex Lhs, FSerializeIndex Rhs)
{
	check(Lhs.Name == NAME_None);
	check(Rhs.Name == NAME_None);
	return FSerializeIndex(NAME_None, Lhs.Index + Rhs.Index);
}

inline FSerializeIndex operator+(FSerializeIndex Lhs, int Rhs)
{
	check(Lhs.Name == NAME_None);
	return FSerializeIndex(NAME_None, Lhs.Index + Rhs);
}

inline FSerializeIndex& operator+=(FSerializeIndex& Lhs, int32 Rhs)
{
	check(Lhs.Name == NAME_None);
	Lhs.Index += Rhs;
	return Lhs;
}

inline FSerializeIndex& operator-=(FSerializeIndex& Lhs, int32 Rhs)
{
	check(Lhs.Name == NAME_None);
	Lhs.Index -= Rhs;
	return Lhs;
}

inline bool operator==(FSerializeIndex& Lhs, int32 Rhs)
{
	check(Lhs.Name == NAME_None || Rhs == IndexOfNativeClass || Rhs == INDEX_NONE);
	return Lhs.Index == Rhs;
}

inline bool operator!=(FSerializeIndex& Lhs, int32 Rhs)
{
	return !(Lhs == Rhs);
}

inline FArchive& operator<<(FArchive& Ar, FSerializeIndex& SerializeIndex)
{
	Ar << SerializeIndex.NameProxy;
	Ar << SerializeIndex.Index;

	return Ar;
}


enum class ESerializedObjectType : uint32
{
	EPackage,
	EClass,
	EField,
	EEnum,
	EStruct,
	EScriptStruct,
	EProperty,
	EByteProperty,
	EInt8Property,
	EInt16Property,
	EIntProperty,
	EInt64Property,
	EUInt16Property,
	EUInt32Property,
	EUInt64Property,
	EFloatProperty,
	EDoubleProperty,
	EBoolProperty,
	ENameProperty,
	EStrProperty,
	ETextProperty,
	EDelegateProperty,
	EMulticastDelegateProperty,
	EObjectPropertyBase,
	EClassProperty,
	EObjectProperty,
	EWeakObjectProperty,
	ELazyObjectProperty,
	EAssetObjectProperty,
	EAssetClassProperty,
	EInterfaceProperty,
	EStructProperty,
	EMapProperty,
	ESetProperty,
	EArrayProperty,
	EUnrealSourceFile,
	EFileScope,
	EStructScope,
	EScope,
	EFunction,
	EDelegateFunction,
	EUnrealTypeDefinitionInfo,
	ETypeDefinitionInfoMapEntry,
	EGeneratedCodeCRC,
	EPropertyBase,
	EGScriptHelperEntry,
	EClassMetaData,
	EPropertyDataEntry,
	EPublicClassSetEntry,
	EPublicSourceFileSetEntry,
	EUnrealSourceFilesMapEntry,
	EMultipleInheritanceBaseClass,
	EToken,
	EEnumUnderlyingType,
	EStructNameMapEntry,
	EInterfaceAllocation,
};

enum class EUHTMakefileLoadingPhase : uint8
{
	Preload,
	Load,
	Export,
	Max,
};

inline uint8 operator+(EUHTMakefileLoadingPhase UHTMakefileLoadingPhase)
{
	return static_cast<uint8>(UHTMakefileLoadingPhase);
}

inline EUHTMakefileLoadingPhase& operator++(EUHTMakefileLoadingPhase& UHTMakefileLoadingPhase)
{
	return UHTMakefileLoadingPhase = static_cast<EUHTMakefileLoadingPhase>(+UHTMakefileLoadingPhase + 1);
}