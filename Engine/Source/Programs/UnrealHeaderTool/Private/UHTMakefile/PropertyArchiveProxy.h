// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UHTMakefile/FieldArchiveProxy.h"


/* See UHTMakefile.h for overview how makefiles work. */
struct FPropertyArchiveProxy : public FFieldArchiveProxy
{
	FPropertyArchiveProxy() { }
	FPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UProperty* Property);

	UProperty* CreateProperty(const FUHTMakefile& UHTMakefile) const;
	void Resolve(UProperty* Property, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FPropertyArchiveProxy& PropertyArchiveProxy);

	static void AddReferencedNames(const UProperty* Property, FUHTMakefile& UHTMakefile);
	void PostConstruct(UProperty* Property, const FUHTMakefile& UHTMakefile) const;
	int32 ArrayDim;
	int32 ElementSize;
	uint64 PropertyFlags;
	uint16 RepIndex;
	FNameArchiveProxy RepNotifyFunc;
	int32 Offset_Internal;
	int32 PropertyLinkNextIndex;
	int32 NextRefIndex;
	int32 DestructorLinkNextIndex;
	int32 PostConstructLinkNextIndex;
};

struct FBytePropertyArchiveProxy : public FPropertyArchiveProxy
{
	FBytePropertyArchiveProxy() { }
	FBytePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UByteProperty* ByteProperty);

	UByteProperty* CreateByteProperty(const FUHTMakefile& UHTMakefile) const;

	void Resolve(UByteProperty* ByteProperty, const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UByteProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	friend FArchive& operator<<(FArchive& Ar, FBytePropertyArchiveProxy& BytePropertyArchiveProxy);

	int32 EnumIndex;
};

struct FInt8PropertyArchiveProxy : public FPropertyArchiveProxy
{
	FInt8PropertyArchiveProxy() { }
	FInt8PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInt8Property* Int8Property);

	UInt8Property* CreateInt8Property(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UInt8Property* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FInt16PropertyArchiveProxy : public FPropertyArchiveProxy
{
	FInt16PropertyArchiveProxy() { }
	FInt16PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInt16Property* Int16Property);

	UInt16Property* CreateInt16Property(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UInt16Property* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FIntPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FIntPropertyArchiveProxy() { }
	FIntPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UIntProperty* IntProperty);

	UIntProperty* CreateIntProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UIntProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FInt64PropertyArchiveProxy : public FPropertyArchiveProxy
{
	FInt64PropertyArchiveProxy() { }
	FInt64PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInt64Property* Int64Property);

	UInt64Property* CreateInt64Property(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UInt64Property* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FUInt16PropertyArchiveProxy : public FPropertyArchiveProxy
{
	FUInt16PropertyArchiveProxy() { }
	FUInt16PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UUInt16Property* UInt16Property);

	UUInt16Property* CreateUInt16Property(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UUInt16Property* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FUInt32PropertyArchiveProxy : public FPropertyArchiveProxy
{
	FUInt32PropertyArchiveProxy() { }
	FUInt32PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UUInt32Property* UInt32Property);

	UUInt32Property* CreateUInt32Property(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UUInt32Property* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FUInt64PropertyArchiveProxy : public FPropertyArchiveProxy
{
	FUInt64PropertyArchiveProxy() { }
	FUInt64PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UUInt64Property* UInt64Property);

	UUInt64Property* CreateUInt64Property(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UUInt64Property* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FFloatPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FFloatPropertyArchiveProxy() { }
	FFloatPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UFloatProperty* FloatProperty);

	UFloatProperty* CreateFloatProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UFloatProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FDoublePropertyArchiveProxy : public FPropertyArchiveProxy
{
	FDoublePropertyArchiveProxy() { }
	FDoublePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UDoubleProperty* DoubleProperty);

	UDoubleProperty* CreateDoubleProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UDoubleProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FBoolPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FBoolPropertyArchiveProxy() { }
	FBoolPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UBoolProperty* BoolProperty);

	UBoolProperty* CreateBoolProperty(const FUHTMakefile& UHTMakefile) const;

	void PostConstruct(UBoolProperty* BoolProperty, const FUHTMakefile& UHTMakefile) const;

	static void AddReferencedNames(const UBoolProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	friend FArchive& operator<<(FArchive& Ar, FBytePropertyArchiveProxy& BytePropertyArchiveProxy);

	uint8 FieldSize;
	uint8 ByteOffset;
	uint8 ByteMask;
	uint8 FieldMask;
};

struct FNamePropertyArchiveProxy : public FPropertyArchiveProxy
{
	FNamePropertyArchiveProxy() { }
	FNamePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UNameProperty* NameProperty);

	UNameProperty* CreateNameProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UNameProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FStrPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FStrPropertyArchiveProxy() { }
	FStrPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UStrProperty* StrProperty);
	static void AddReferencedNames(const UStrProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	UStrProperty* CreateStrProperty(const FUHTMakefile& UHTMakefile) const;
};

struct FTextPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FTextPropertyArchiveProxy() { }
	FTextPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UTextProperty* TextProperty);

	UTextProperty* CreateTextProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UTextProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FDelegatePropertyArchiveProxy : public FPropertyArchiveProxy
{
	FDelegatePropertyArchiveProxy() { }
	FDelegatePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UDelegateProperty* DelegateProperty);

	UDelegateProperty* CreateDelegateProperty(const FUHTMakefile& UHTMakefile) const;
	void Resolve(UDelegateProperty* DelegateProperty, const FUHTMakefile& UHTMakefile);
	static void AddReferencedNames(const UDelegateProperty* Property, FUHTMakefile& UHTMakefile);

	int32 SignatureFunctionIndex;
};

struct FMulticastDelegatePropertyArchiveProxy : public FPropertyArchiveProxy
{
	FMulticastDelegatePropertyArchiveProxy() { }
	FMulticastDelegatePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UMulticastDelegateProperty* MulticastDelegateProperty);

	UMulticastDelegateProperty* CreateMulticastDelegateProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UMulticastDelegateProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FObjectPropertyBaseArchiveProxy : public FPropertyArchiveProxy
{
	FObjectPropertyBaseArchiveProxy() { }
	FObjectPropertyBaseArchiveProxy(FUHTMakefile& UHTMakefile, const UObjectPropertyBase* ObjectPropertyBase);

	UObjectPropertyBase* CreateObjectPropertyBase(const FUHTMakefile& UHTMakefile) const;

	void Resolve(UObjectPropertyBase* ObjectPropertyBase, const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UObjectPropertyBase* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	friend FArchive& operator<<(FArchive& Ar, FObjectPropertyBaseArchiveProxy& ObjectPropertyBaseArchiveProxy);

	FSerializeIndex PropertyClassIndex;
};

struct FClassPropertyArchiveProxy : public FObjectPropertyBaseArchiveProxy
{
	FClassPropertyArchiveProxy() { }
	FClassPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UClassProperty* ClassProperty);

	UClassProperty* CreateClassProperty(const FUHTMakefile& UHTMakefile) const;

	void Resolve(UClassProperty* ClassProperty, const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UClassProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	friend FArchive& operator<<(FArchive& Ar, FClassPropertyArchiveProxy& ClassPropertyArchiveProxy);

	FSerializeIndex MetaClassIndex;
};

struct FObjectPropertyArchiveProxy : public FObjectPropertyBaseArchiveProxy
{
	FObjectPropertyArchiveProxy() { }
	FObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UObjectProperty* ObjectProperty);
	static void AddReferencedNames(const UObjectProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	UObjectProperty* CreateObjectProperty(const FUHTMakefile& UHTMakefile) const;
};

struct FWeakObjectPropertyArchiveProxy : public FObjectPropertyBaseArchiveProxy
{
	FWeakObjectPropertyArchiveProxy() { }
	FWeakObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UWeakObjectProperty* WeakObjectProperty);

	UWeakObjectProperty* CreateWeakObjectProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UWeakObjectProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FLazyObjectPropertyArchiveProxy : public FObjectPropertyBaseArchiveProxy
{
	FLazyObjectPropertyArchiveProxy() { }
	FLazyObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const ULazyObjectProperty* LazyObjectProperty);

	ULazyObjectProperty* CreateLazyObjectProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const ULazyObjectProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FAssetObjectPropertyArchiveProxy : public FObjectPropertyBaseArchiveProxy
{
	FAssetObjectPropertyArchiveProxy() { }
	FAssetObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UAssetObjectProperty* AssetObjectProperty);

	UAssetObjectProperty* CreateAssetObjectProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UAssetObjectProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}
};

struct FAssetClassPropertyArchiveProxy : public FObjectPropertyBaseArchiveProxy
{
	FAssetClassPropertyArchiveProxy() { }
	FAssetClassPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UAssetClassProperty* AssetClassProperty);

	UAssetClassProperty* CreateAssetClassProperty(const FUHTMakefile& UHTMakefile) const;

	void Resolve(UAssetClassProperty* AssetClassProperty, const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UAssetClassProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	friend FArchive& operator<<(FArchive& Ar, FAssetClassPropertyArchiveProxy& AssetClassPropertyArchiveProxy);

	FSerializeIndex MetaClassIndex;
};

struct FInterfacePropertyArchiveProxy : public FPropertyArchiveProxy
{
	FInterfacePropertyArchiveProxy() { }
	FInterfacePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInterfaceProperty* InterfaceProperty);

	UInterfaceProperty* CreateInterfaceProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UInterfaceProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	void Resolve(UInterfaceProperty* InterfaceProperty, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FInterfacePropertyArchiveProxy& InterfacePropertyArchiveProxy);

	FSerializeIndex InterfaceClassIndex;
};

struct FStructPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FStructPropertyArchiveProxy() { }
	FStructPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UStructProperty* StructProperty);

	UStructProperty* CreateStructProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UStructProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	void Resolve(UStructProperty* StructProperty, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FStructPropertyArchiveProxy& StructPropertyArchiveProxy);

	FSerializeIndex StructIndex;
};

FArchive& operator<<(FArchive& Ar, FScriptSparseArrayLayout& ScriptSparseArrayLayout);;

FArchive& operator<<(FArchive& Ar, FScriptSetLayout& ScriptSetLayout);;

FArchive& operator<<(FArchive& Ar, FScriptMapLayout& ScriptMapLayout);

struct FMapPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FMapPropertyArchiveProxy() { }
	FMapPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UMapProperty* MapProperty);

	UMapProperty* CreateMapProperty(const FUHTMakefile& UHTMakefile) const;

	void Resolve(UMapProperty* MapProperty, const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UMapProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	friend FArchive& operator<<(FArchive& Ar, FMapPropertyArchiveProxy& MapPropertyArchiveProxy);

	int32 KeyPropIndex;
	int32 ValuePropIndex;
	FScriptMapLayout MapLayout;
};

struct FSetPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FSetPropertyArchiveProxy() { }
	FSetPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const USetProperty* SetProperty);

	USetProperty* CreateSetProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const USetProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	void Resolve(USetProperty* SetProperty, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FSetPropertyArchiveProxy& SetPropertyArchiveProxy);

	int32 ElementPropIndex;
	FScriptSetLayout SetLayout;
};

struct FArrayPropertyArchiveProxy : public FPropertyArchiveProxy
{
	FArrayPropertyArchiveProxy() { }
	FArrayPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UArrayProperty* ArrayProperty);

	UArrayProperty* CreateArrayProperty(const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UArrayProperty* Property, FUHTMakefile& UHTMakefile)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	}

	void Resolve(UArrayProperty* ArrayProperty, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FArrayPropertyArchiveProxy& ArrayPropertyArchiveProxy);

	int32 InnerIndex;
};