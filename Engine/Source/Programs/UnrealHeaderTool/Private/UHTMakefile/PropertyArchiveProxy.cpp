// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/PropertyArchiveProxy.h"

FPropertyArchiveProxy::FPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UProperty* Property)
	: FFieldArchiveProxy(UHTMakefile, Property)
{
	ArrayDim = Property->ArrayDim;
	ElementSize = Property->ElementSize;
	PropertyFlags = Property->PropertyFlags;
	RepIndex = Property->RepIndex;
	RepNotifyFunc = FNameArchiveProxy(UHTMakefile, Property->RepNotifyFunc);
	Offset_Internal = Property->Offset_Internal;
	PropertyLinkNextIndex = UHTMakefile.GetPropertyIndex(Property->PropertyLinkNext);
	NextRefIndex = UHTMakefile.GetPropertyIndex(Property->NextRef);
	DestructorLinkNextIndex = UHTMakefile.GetPropertyIndex(Property->DestructorLinkNext);
	PostConstructLinkNextIndex = UHTMakefile.GetPropertyIndex(Property->PostConstructLinkNext);
}

UProperty* FPropertyArchiveProxy::CreateProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UProperty* Property = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UProperty(FObjectInitializer());
	PostConstruct(Property, UHTMakefile);
	return Property;
}

void FPropertyArchiveProxy::Resolve(UProperty* Property, const FUHTMakefile& UHTMakefile) const
{
	FFieldArchiveProxy::Resolve(Property, UHTMakefile);
	Property->PropertyLinkNext = UHTMakefile.GetPropertyByIndex(PropertyLinkNextIndex);
	Property->NextRef = UHTMakefile.GetPropertyByIndex(NextRefIndex);
	Property->DestructorLinkNext = UHTMakefile.GetPropertyByIndex(DestructorLinkNextIndex);
	Property->PostConstructLinkNext = UHTMakefile.GetPropertyByIndex(PostConstructLinkNextIndex);
}

void FPropertyArchiveProxy::AddReferencedNames(const UProperty* Property, FUHTMakefile& UHTMakefile)
{
	FFieldArchiveProxy::AddReferencedNames(Property, UHTMakefile);
	UHTMakefile.AddName(Property->RepNotifyFunc);
}

void FPropertyArchiveProxy::PostConstruct(UProperty* Property, const FUHTMakefile& UHTMakefile) const
{
	Property->ArrayDim = ArrayDim;
	Property->ElementSize = ElementSize;
	Property->PropertyFlags = PropertyFlags;
	Property->RepIndex = RepIndex;
	Property->RepNotifyFunc = RepNotifyFunc.CreateName(UHTMakefile);
	Property->Offset_Internal = Offset_Internal;
}

FArchive& operator<<(FArchive& Ar, FPropertyArchiveProxy& PropertyArchiveProxy)
{
	Ar << static_cast<FFieldArchiveProxy&>(PropertyArchiveProxy);
	Ar << PropertyArchiveProxy.ArrayDim;
	Ar << PropertyArchiveProxy.ElementSize;
	Ar << PropertyArchiveProxy.PropertyFlags;
	Ar << PropertyArchiveProxy.RepIndex;
	Ar << PropertyArchiveProxy.RepNotifyFunc;
	Ar << PropertyArchiveProxy.Offset_Internal;
	Ar << PropertyArchiveProxy.PropertyLinkNextIndex;
	Ar << PropertyArchiveProxy.NextRefIndex;
	Ar << PropertyArchiveProxy.DestructorLinkNextIndex;
	Ar << PropertyArchiveProxy.PostConstructLinkNextIndex;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FArrayPropertyArchiveProxy& ArrayPropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(ArrayPropertyArchiveProxy);
	Ar << ArrayPropertyArchiveProxy.InnerIndex;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FMapPropertyArchiveProxy& MapPropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(MapPropertyArchiveProxy);
	Ar << MapPropertyArchiveProxy.KeyPropIndex;
	Ar << MapPropertyArchiveProxy.ValuePropIndex;
	Ar << MapPropertyArchiveProxy.MapLayout;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FSetPropertyArchiveProxy& SetPropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(SetPropertyArchiveProxy);
	Ar << SetPropertyArchiveProxy.ElementPropIndex;
	Ar << SetPropertyArchiveProxy.SetLayout;

	return Ar;
}

FBytePropertyArchiveProxy::FBytePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UByteProperty* ByteProperty)
	: FPropertyArchiveProxy(UHTMakefile, ByteProperty)
{
	EnumIndex = UHTMakefile.GetEnumIndex(ByteProperty->Enum);
}

UByteProperty* FBytePropertyArchiveProxy::CreateByteProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UByteProperty* ByteProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UByteProperty(FObjectInitializer());
	PostConstruct(ByteProperty, UHTMakefile);
	return ByteProperty;
}

void FBytePropertyArchiveProxy::Resolve(UByteProperty* ByteProperty, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::Resolve(ByteProperty, UHTMakefile);
	ByteProperty->Enum = UHTMakefile.GetEnumByIndex(EnumIndex);
}

FArchive& operator<<(FArchive& Ar, FBytePropertyArchiveProxy& BytePropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(BytePropertyArchiveProxy);
	Ar << BytePropertyArchiveProxy.EnumIndex;

	return Ar;
}

FObjectPropertyBaseArchiveProxy::FObjectPropertyBaseArchiveProxy(FUHTMakefile& UHTMakefile, const UObjectPropertyBase* ObjectPropertyBase)
	: FPropertyArchiveProxy(UHTMakefile, ObjectPropertyBase)
{
	PropertyClassIndex = UHTMakefile.GetClassIndex(ObjectPropertyBase->PropertyClass);
}

UObjectPropertyBase* FObjectPropertyBaseArchiveProxy::CreateObjectPropertyBase(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UObjectPropertyBase* ObjectPropertyBase = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UObjectPropertyBase(FObjectInitializer());
	PostConstruct(ObjectPropertyBase, UHTMakefile);
	return ObjectPropertyBase;
}

void FObjectPropertyBaseArchiveProxy::Resolve(UObjectPropertyBase* ObjectPropertyBase, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::Resolve(ObjectPropertyBase, UHTMakefile);
	ObjectPropertyBase->PropertyClass = UHTMakefile.GetClassByIndex(PropertyClassIndex);
}

FArchive& operator<<(FArchive& Ar, FObjectPropertyBaseArchiveProxy& ObjectPropertyBaseArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(ObjectPropertyBaseArchiveProxy);
	Ar << ObjectPropertyBaseArchiveProxy.PropertyClassIndex;

	return Ar;
}

FInt8PropertyArchiveProxy::FInt8PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInt8Property* Int8Property)
	: FPropertyArchiveProxy(UHTMakefile, Int8Property)
{ }

UInt8Property* FInt8PropertyArchiveProxy::CreateInt8Property(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UInt8Property* Int8Property = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UInt8Property(FObjectInitializer());
	PostConstruct(Int8Property, UHTMakefile);
	return Int8Property;
}

FInt16PropertyArchiveProxy::FInt16PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInt16Property* Int16Property)
	: FPropertyArchiveProxy(UHTMakefile, Int16Property)
{ }

UInt16Property* FInt16PropertyArchiveProxy::CreateInt16Property(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UInt16Property* Int16Property = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UInt16Property(FObjectInitializer());
	PostConstruct(Int16Property, UHTMakefile);
	return Int16Property;
}

FIntPropertyArchiveProxy::FIntPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UIntProperty* IntProperty)
	: FPropertyArchiveProxy(UHTMakefile, IntProperty)
{ }

UIntProperty* FIntPropertyArchiveProxy::CreateIntProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UIntProperty* IntProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UIntProperty(FObjectInitializer());
	PostConstruct(IntProperty, UHTMakefile);
	return IntProperty;
}

FInt64PropertyArchiveProxy::FInt64PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInt64Property* Int64Property)
	: FPropertyArchiveProxy(UHTMakefile, Int64Property)
{ }

UInt64Property* FInt64PropertyArchiveProxy::CreateInt64Property(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UInt64Property* Int64Property = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UInt64Property(FObjectInitializer());
	PostConstruct(Int64Property, UHTMakefile);
	return Int64Property;
}

FUInt16PropertyArchiveProxy::FUInt16PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UUInt16Property* UInt16Property)
	: FPropertyArchiveProxy(UHTMakefile, UInt16Property)
{ }

UUInt16Property* FUInt16PropertyArchiveProxy::CreateUInt16Property(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UUInt16Property* UInt16Property = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UUInt16Property(FObjectInitializer());
	PostConstruct(UInt16Property, UHTMakefile);
	return UInt16Property;
}

FUInt32PropertyArchiveProxy::FUInt32PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UUInt32Property* UInt32Property)
	: FPropertyArchiveProxy(UHTMakefile, UInt32Property)
{ }

UUInt32Property* FUInt32PropertyArchiveProxy::CreateUInt32Property(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UUInt32Property* UInt32Property = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UUInt32Property(FObjectInitializer());
	PostConstruct(UInt32Property, UHTMakefile);
	return UInt32Property;
}

FUInt64PropertyArchiveProxy::FUInt64PropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UUInt64Property* UInt64Property)
	: FPropertyArchiveProxy(UHTMakefile, UInt64Property)
{ }

UUInt64Property* FUInt64PropertyArchiveProxy::CreateUInt64Property(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UUInt64Property* UInt64Property = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UUInt64Property(FObjectInitializer());
	PostConstruct(UInt64Property, UHTMakefile);
	return UInt64Property;
}

FFloatPropertyArchiveProxy::FFloatPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UFloatProperty* FloatProperty)
	: FPropertyArchiveProxy(UHTMakefile, FloatProperty)
{ }

UFloatProperty* FFloatPropertyArchiveProxy::CreateFloatProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UFloatProperty* FloatProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UFloatProperty(FObjectInitializer());
	PostConstruct(FloatProperty, UHTMakefile);
	return FloatProperty;
}

FDoublePropertyArchiveProxy::FDoublePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UDoubleProperty* DoubleProperty)
	: FPropertyArchiveProxy(UHTMakefile, DoubleProperty)
{ }

UDoubleProperty* FDoublePropertyArchiveProxy::CreateDoubleProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UDoubleProperty* DoubleProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UDoubleProperty(FObjectInitializer());
	PostConstruct(DoubleProperty, UHTMakefile);
	return DoubleProperty;
}

FBoolPropertyArchiveProxy::FBoolPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UBoolProperty* BoolProperty)
	: FPropertyArchiveProxy(UHTMakefile, BoolProperty)
{
	FieldSize = BoolProperty->FieldSize;
	ByteOffset = BoolProperty->ByteOffset;
	ByteMask = BoolProperty->ByteMask;
	FieldMask = BoolProperty->FieldMask;
}

UBoolProperty* FBoolPropertyArchiveProxy::CreateBoolProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UBoolProperty* BoolProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UBoolProperty(FObjectInitializer());
	PostConstruct(BoolProperty, UHTMakefile);
	return BoolProperty;
}

void FBoolPropertyArchiveProxy::PostConstruct(UBoolProperty* BoolProperty, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::PostConstruct(BoolProperty, UHTMakefile);
	BoolProperty->FieldSize = FieldSize;
	BoolProperty->ByteOffset = ByteOffset;
	BoolProperty->ByteMask = ByteMask;
	BoolProperty->FieldMask = FieldMask;
}

FNamePropertyArchiveProxy::FNamePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UNameProperty* NameProperty) : FPropertyArchiveProxy(UHTMakefile, NameProperty)
{ }

UNameProperty* FNamePropertyArchiveProxy::CreateNameProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UNameProperty* NameProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UNameProperty(FObjectInitializer());
	PostConstruct(NameProperty, UHTMakefile);
	return NameProperty;
}

FStrPropertyArchiveProxy::FStrPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UStrProperty* StrProperty)
	: FPropertyArchiveProxy(UHTMakefile, StrProperty)
{ }

UStrProperty* FStrPropertyArchiveProxy::CreateStrProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UStrProperty* StrProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UStrProperty(FObjectInitializer());
	PostConstruct(StrProperty, UHTMakefile);
	return StrProperty;
}

FTextPropertyArchiveProxy::FTextPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UTextProperty* TextProperty)
	: FPropertyArchiveProxy(UHTMakefile, TextProperty)
{ }

UTextProperty* FTextPropertyArchiveProxy::CreateTextProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UTextProperty* TextProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UTextProperty(FObjectInitializer());
	PostConstruct(TextProperty, UHTMakefile);
	return TextProperty;
}

FDelegatePropertyArchiveProxy::FDelegatePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UDelegateProperty* DelegateProperty)
	: FPropertyArchiveProxy(UHTMakefile, DelegateProperty)
{
	SignatureFunctionIndex = UHTMakefile.GetFunctionIndex(DelegateProperty->SignatureFunction);
}

UDelegateProperty* FDelegatePropertyArchiveProxy::CreateDelegateProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UDelegateProperty* DelegateProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UDelegateProperty(FObjectInitializer());
	PostConstruct(DelegateProperty, UHTMakefile);
	return DelegateProperty;
}

void FDelegatePropertyArchiveProxy::Resolve(UDelegateProperty* DelegateProperty, const FUHTMakefile& UHTMakefile)
{
	FPropertyArchiveProxy::Resolve(DelegateProperty, UHTMakefile);
	DelegateProperty->SignatureFunction = UHTMakefile.GetFunctionByIndex(SignatureFunctionIndex);
}

void FDelegatePropertyArchiveProxy::AddReferencedNames(const UDelegateProperty* Property, FUHTMakefile& UHTMakefile)
{
	FPropertyArchiveProxy::AddReferencedNames(Property, UHTMakefile);
}

FMulticastDelegatePropertyArchiveProxy::FMulticastDelegatePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UMulticastDelegateProperty* MulticastDelegateProperty)
	: FPropertyArchiveProxy(UHTMakefile, MulticastDelegateProperty)
{ }

UMulticastDelegateProperty* FMulticastDelegatePropertyArchiveProxy::CreateMulticastDelegateProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UMulticastDelegateProperty* MulticastDelegateProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UMulticastDelegateProperty(FObjectInitializer());
	PostConstruct(MulticastDelegateProperty, UHTMakefile);
	return MulticastDelegateProperty;
}

FClassPropertyArchiveProxy::FClassPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UClassProperty* ClassProperty)
	: FObjectPropertyBaseArchiveProxy(UHTMakefile, ClassProperty)
{
	MetaClassIndex = UHTMakefile.GetClassIndex(ClassProperty->MetaClass);
}

UClassProperty* FClassPropertyArchiveProxy::CreateClassProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UClassProperty* ClassProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UClassProperty(FObjectInitializer());
	PostConstruct(ClassProperty, UHTMakefile);
	return ClassProperty;
}

void FClassPropertyArchiveProxy::Resolve(UClassProperty* ClassProperty, const FUHTMakefile& UHTMakefile) const
{
	FObjectPropertyBaseArchiveProxy::Resolve(ClassProperty, UHTMakefile);
	ClassProperty->MetaClass = UHTMakefile.GetClassByIndex(MetaClassIndex);
}

FArchive& operator<<(FArchive& Ar, FClassPropertyArchiveProxy& ClassPropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(ClassPropertyArchiveProxy);
	Ar << ClassPropertyArchiveProxy.MetaClassIndex;

	return Ar;
}

FObjectPropertyArchiveProxy::FObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UObjectProperty* ObjectProperty)
	: FObjectPropertyBaseArchiveProxy(UHTMakefile, ObjectProperty)
{ }

UObjectProperty* FObjectPropertyArchiveProxy::CreateObjectProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UObjectProperty* ObjectProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UObjectProperty(FObjectInitializer());
	PostConstruct(ObjectProperty, UHTMakefile);
	return ObjectProperty;
}

FWeakObjectPropertyArchiveProxy::FWeakObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UWeakObjectProperty* WeakObjectProperty)
	: FObjectPropertyBaseArchiveProxy(UHTMakefile, WeakObjectProperty)
{ }

UWeakObjectProperty* FWeakObjectPropertyArchiveProxy::CreateWeakObjectProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UWeakObjectProperty* WeakObjectProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UWeakObjectProperty(FObjectInitializer());
	PostConstruct(WeakObjectProperty, UHTMakefile);
	return WeakObjectProperty;
}

FLazyObjectPropertyArchiveProxy::FLazyObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const ULazyObjectProperty* LazyObjectProperty)
	: FObjectPropertyBaseArchiveProxy(UHTMakefile, LazyObjectProperty)
{ }

ULazyObjectProperty* FLazyObjectPropertyArchiveProxy::CreateLazyObjectProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	ULazyObjectProperty* LazyObjectProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) ULazyObjectProperty(FObjectInitializer());
	PostConstruct(LazyObjectProperty, UHTMakefile);
	return LazyObjectProperty;
}

FAssetObjectPropertyArchiveProxy::FAssetObjectPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UAssetObjectProperty* AssetObjectProperty)
	: FObjectPropertyBaseArchiveProxy(UHTMakefile, AssetObjectProperty)
{ }

UAssetObjectProperty* FAssetObjectPropertyArchiveProxy::CreateAssetObjectProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UAssetObjectProperty* AssetObjectProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UAssetObjectProperty(FObjectInitializer());
	PostConstruct(AssetObjectProperty, UHTMakefile);
	return AssetObjectProperty;
}

FAssetClassPropertyArchiveProxy::FAssetClassPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UAssetClassProperty* AssetClassProperty)
	: FObjectPropertyBaseArchiveProxy(UHTMakefile, AssetClassProperty)
{
	MetaClassIndex = UHTMakefile.GetClassIndex(AssetClassProperty->MetaClass);
}

UAssetClassProperty* FAssetClassPropertyArchiveProxy::CreateAssetClassProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UAssetClassProperty* AssetClassProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UAssetClassProperty(FObjectInitializer());
	PostConstruct(AssetClassProperty, UHTMakefile);
	return AssetClassProperty;
}

void FAssetClassPropertyArchiveProxy::Resolve(UAssetClassProperty* AssetClassProperty, const FUHTMakefile& UHTMakefile) const
{
	FObjectPropertyBaseArchiveProxy::Resolve(AssetClassProperty, UHTMakefile);
	AssetClassProperty->MetaClass = UHTMakefile.GetClassByIndex(MetaClassIndex);
}

FArchive& operator<<(FArchive& Ar, FAssetClassPropertyArchiveProxy& AssetClassPropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(AssetClassPropertyArchiveProxy);
	Ar << AssetClassPropertyArchiveProxy.MetaClassIndex;

	return Ar;
}

FInterfacePropertyArchiveProxy::FInterfacePropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UInterfaceProperty* InterfaceProperty)
	: FPropertyArchiveProxy(UHTMakefile, InterfaceProperty)
{
	InterfaceClassIndex = UHTMakefile.GetClassIndex(InterfaceProperty->InterfaceClass);
}

UInterfaceProperty* FInterfacePropertyArchiveProxy::CreateInterfaceProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UInterfaceProperty* InterfaceProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UInterfaceProperty(FObjectInitializer());
	PostConstruct(InterfaceProperty, UHTMakefile);
	return InterfaceProperty;
}

void FInterfacePropertyArchiveProxy::Resolve(UInterfaceProperty* InterfaceProperty, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::Resolve(InterfaceProperty, UHTMakefile);
	InterfaceProperty->InterfaceClass = UHTMakefile.GetClassByIndex(InterfaceClassIndex);
}

FArchive& operator<<(FArchive& Ar, FInterfacePropertyArchiveProxy& InterfacePropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(InterfacePropertyArchiveProxy);
	Ar << InterfacePropertyArchiveProxy.InterfaceClassIndex;

	return Ar;
}

FStructPropertyArchiveProxy::FStructPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UStructProperty* StructProperty)
	: FPropertyArchiveProxy(UHTMakefile, StructProperty)
{
	StructIndex = UHTMakefile.GetScriptStructIndex(StructProperty->Struct);
}

UStructProperty* FStructPropertyArchiveProxy::CreateStructProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UStructProperty* StructProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UStructProperty(FObjectInitializer());
	PostConstruct(StructProperty, UHTMakefile);
	return StructProperty;
}

void FStructPropertyArchiveProxy::Resolve(UStructProperty* StructProperty, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::Resolve(StructProperty, UHTMakefile);
	StructProperty->Struct = UHTMakefile.GetScriptStructByIndex(StructIndex);
}

FArchive& operator<<(FArchive& Ar, FStructPropertyArchiveProxy& StructPropertyArchiveProxy)
{
	Ar << static_cast<FPropertyArchiveProxy&>(StructPropertyArchiveProxy);
	Ar << StructPropertyArchiveProxy.StructIndex;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FScriptSparseArrayLayout& ScriptSparseArrayLayout)
{
	Ar << ScriptSparseArrayLayout.ElementOffset;
	Ar << ScriptSparseArrayLayout.Alignment;
	Ar << ScriptSparseArrayLayout.Size;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FScriptSetLayout& ScriptSetLayout)
{
	Ar << ScriptSetLayout.ElementOffset;
	Ar << ScriptSetLayout.HashNextIdOffset;
	Ar << ScriptSetLayout.HashIndexOffset;
	Ar << ScriptSetLayout.Size;
	Ar << ScriptSetLayout.SparseArrayLayout;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FScriptMapLayout& ScriptMapLayout)
{
	Ar << ScriptMapLayout.KeyOffset;
	Ar << ScriptMapLayout.ValueOffset;
	Ar << ScriptMapLayout.SetLayout;

	return Ar;
}

FMapPropertyArchiveProxy::FMapPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UMapProperty* MapProperty)
	: FPropertyArchiveProxy(UHTMakefile, MapProperty)
{
	KeyPropIndex = UHTMakefile.GetPropertyIndex(MapProperty->KeyProp);
	ValuePropIndex = UHTMakefile.GetPropertyIndex(MapProperty->ValueProp);
	MapLayout = MapProperty->MapLayout;
}

UMapProperty* FMapPropertyArchiveProxy::CreateMapProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UMapProperty* MapProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UMapProperty(FObjectInitializer());
	PostConstruct(MapProperty, UHTMakefile);
	return MapProperty;
}

void FMapPropertyArchiveProxy::Resolve(UMapProperty* MapProperty, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::Resolve(MapProperty, UHTMakefile);
	MapProperty->KeyProp = UHTMakefile.GetPropertyByIndex(KeyPropIndex);
	MapProperty->ValueProp = UHTMakefile.GetPropertyByIndex(ValuePropIndex);
	MapProperty->MapLayout = MapLayout;
}

FSetPropertyArchiveProxy::FSetPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const USetProperty* SetProperty)
	: FPropertyArchiveProxy(UHTMakefile, SetProperty)
{
	ElementPropIndex = UHTMakefile.GetPropertyIndex(SetProperty->ElementProp);
	SetLayout = SetProperty->SetLayout;
}

USetProperty* FSetPropertyArchiveProxy::CreateSetProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	USetProperty* SetProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) USetProperty(FObjectInitializer());
	PostConstruct(SetProperty, UHTMakefile);
	return SetProperty;
}

void FSetPropertyArchiveProxy::Resolve(USetProperty* SetProperty, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::Resolve(SetProperty, UHTMakefile);
	SetProperty->ElementProp = UHTMakefile.GetPropertyByIndex(ElementPropIndex);
	SetProperty->SetLayout = SetLayout;
}

UArrayProperty* FArrayPropertyArchiveProxy::CreateArrayProperty(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UArrayProperty* ArrayProperty = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UArrayProperty(FObjectInitializer());
	PostConstruct(ArrayProperty, UHTMakefile);
	return ArrayProperty;
}

void FArrayPropertyArchiveProxy::Resolve(UArrayProperty* ArrayProperty, const FUHTMakefile& UHTMakefile) const
{
	FPropertyArchiveProxy::Resolve(ArrayProperty, UHTMakefile);
	ArrayProperty->Inner = UHTMakefile.GetPropertyByIndex(InnerIndex);
}

FArrayPropertyArchiveProxy::FArrayPropertyArchiveProxy(FUHTMakefile& UHTMakefile, const UArrayProperty* ArrayProperty)
	: FPropertyArchiveProxy(UHTMakefile, ArrayProperty)
{
	InnerIndex = UHTMakefile.GetPropertyIndex(ArrayProperty->Inner);
}
