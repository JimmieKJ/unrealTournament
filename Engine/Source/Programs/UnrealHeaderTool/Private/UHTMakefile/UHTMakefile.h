// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UHTMakefile/ScriptStructArchiveProxy.h"
#include "UHTMakefile/PropertyArchiveProxy.h"
#include "UHTMakefile/FunctionArchiveProxy.h"
#include "UHTMakefile/EnumArchiveProxy.h"
#include "UHTMakefile/PackageArchiveProxy.h"
#include "UHTMakefile/UnrealSourceFileArchiveProxy.h"
#include "UHTMakefile/ScopeArchiveProxy.h"
#include "UHTMakefile/FileScopeArchiveProxy.h"
#include "UHTMakefile/StructScopeArchiveProxy.h"
#include "UHTMakefile/ScopeArchiveProxy.h"
#include "UHTMakefile/ClassArchiveProxy.h"
#include "UHTMakefile/UHTMakefileModuleDescriptor.h"
#include "UHTMakefile/TypeDefinitionInfoMapArchiveProxy.h"
#include "UHTMakefile/PropertyBaseArchiveProxy.h"
#include "UHTMakefile/CompilerMetadataManagerArchiveProxy.h"

/**
 * UHT makefiles overview.
 *
 *	Generating makefile
 *		UHT makefiles serve as cache for internal UHT data generated while parsing modules.
 *		Whenever a piece of data relevant for code generation is created, it's added to 
 *		FUHTMakefile instance along with module and source file within which said piece
 *		is created and initialization order. Each type has separate array in which it's stored,
 *		base and derived classes are stored separately.
 *		Apart from data pointers, UHT makefiles store ModuleDescriptors, which are containers 
 *		for HeaderDescriptors. Each header descriptor contains indexes of objects in order of
 *		initialization for each UHT phase - preparsing, full parsing, exporting.
 *
 *	Saving makefile
 *		All objects stored within makefile have their corresponding proxy structs, which are
 *		serialized PODs. All data stored as value types in objects are copied to proxies and
 *		pointers are converted to indexes within FUHTMakefile arrays. Saving FUHTMakefile
 *		serializes all proxies, module descriptors and FManifest used to create FUHTMakefile.
 *
 *	Loading makefile
 *		UHT makefile deserializes all proxy objects and FManifest. There are two steps determining
 *		what can be loaded from makefile:
 *			1. Deserialized manifest is compared on per-module basis with one provided by UBT.
 *				All modules that are fully matching can be loaded from makefile until first
 *				non-matching one is found. All subsequent modules after first non-matching one
 *				must be parsed regularly.
 *			2. All headers from manifest are checked for modifications against timestamps from previous
 *				UHT runs. If at least one header is changed, module can't be loaded from makefile
 *				and all subsequent modules are marked as needing regeneration.
 *		All modules that can be loaded are iterated on, creating objects according to
 *		initialization order. Each object is first created (which fills in value fields), and once all
 *		objects for module are created, they are resolved (which takes indexes from proxy structs
 *		and finds appropriate pointers). Loading is split to three phases to reflect the way data is 
 *		generated.
 */
class FUHTMakefile;
class FUnrealSourceFile;
class FHeaderProvider;
class FFileScope;
class FUnrealTypeDefinitionInfo;
class FStructScope;
class FScope;
class FPropertyBase;
struct FManifestModule;
struct FManifest;

inline FArchive& operator<<(FArchive& Ar, ESerializedObjectType& ObjectType)
{
	if (Ar.IsLoading())
	{
		uint32 ObjectTypeUInt32;
		Ar << ObjectTypeUInt32;
		ObjectType = (ESerializedObjectType)ObjectTypeUInt32;
	}
	else if (Ar.IsSaving())
	{
		uint32 ObjectTypeUInt32 = (uint32)ObjectType;
		Ar << ObjectTypeUInt32;
	}

	return Ar;
}

struct FUnrealTypeDefinitionInfoArchiveProxy
{
	FUnrealTypeDefinitionInfoArchiveProxy(const FUHTMakefile& UHTMakefile, const FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo);
	FUnrealTypeDefinitionInfoArchiveProxy() { }

	int32 LineNumber;
	int32 UnrealSourceFileIndex;

	friend FArchive& operator<<(FArchive& Ar, FUnrealTypeDefinitionInfoArchiveProxy& UnrealTypeDefinitionInfoArchiveProxy)
	{
		Ar << UnrealTypeDefinitionInfoArchiveProxy.LineNumber;
		Ar << UnrealTypeDefinitionInfoArchiveProxy.UnrealSourceFileIndex;

		return Ar;
	}

	void Resolve(FUnrealTypeDefinitionInfo*& UnrealTypeDefinitionInfo, const FUHTMakefile& UHTMakefile) const;
	FUnrealTypeDefinitionInfo* CreateUnrealTypeDefinitionInfo(const FUHTMakefile& UHTMakefile) const;
};

class FUHTMakefile
{
public:
	FUHTMakefile()
		: bShouldForceRegeneration(false)
		, bShouldMoveNewObjects(false)
		, Version(CurrentVersion)
		, LoadingPhase(EUHTMakefileLoadingPhase::Max)
	{
		InitializationOrder.AddDefaulted(+EUHTMakefileLoadingPhase::Max);
	}

	void StartPreloading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Preload;
	}
	void StopPreloading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;

		for (auto& Kvp : ModuleDescriptors)
		{
			Kvp.Value.StopPreloading();
		}
	}
	bool IsPreloading()
	{
		return LoadingPhase == EUHTMakefileLoadingPhase::Preload;
	}
	void StartLoading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Load;

		for (auto& Kvp : ModuleDescriptors)
		{
			Kvp.Value.StartLoading();
		}
	}
	void StopLoading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;

		for (auto& Kvp : ModuleDescriptors)
		{
			Kvp.Value.StopLoading();
		}
	}
	bool IsLoading()
	{
		return LoadingPhase == EUHTMakefileLoadingPhase::Load;
	}
	void StartExporting()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Export;

		for (auto& Kvp : ModuleDescriptors)
		{
			Kvp.Value.StartExporting();
		}
	}
	void StopExporting()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;

		for (auto& Kvp : ModuleDescriptors)
		{
			Kvp.Value.StopExporting();
		}
	}
	bool IsExporting()
	{
		return LoadingPhase == EUHTMakefileLoadingPhase::Export;
	}

	void SetShouldMoveNewObjects()
	{
		bShouldMoveNewObjects = true;
	}
	bool ShouldMoveNewObjects() const
	{
		return bShouldMoveNewObjects;
	}
	void MoveNewObjects();

	void AddModule(FName ModuleName)
	{
		FUHTMakefileModuleDescriptor& ModuleDescriptor = ModuleDescriptors.FindOrAdd(ModuleName);
		ModuleDescriptor.Reset();
		ModuleDescriptor.SetMakefile(this);
		if (!ModuleNames.Contains(ModuleName))
		{
			ModuleNames.Add(ModuleName);
		}
		bShouldForceRegeneration = true;
	}
	bool HasModule(FName ModuleName) const
	{
		return ModuleNames.Contains(ModuleName);
	}
	void LoadModuleData(FName ModuleName, const FManifestModule& ManifestModule);

	void AddPackage(UPackage* Package)
	{
		FUHTMakefileModuleDescriptor& CurrentModule = GetCurrentModule();
		int32 PackageIndex = Packages.Num() + NewPackages.Add(Package);
		CurrentModule.SetPackageIndex(PackageIndex);
		CurrentModule.SetPackage(Package);
		PackageIndexes.Add(Package, PackageIndex);
	}
	void CreatePackage(int32 Index)
	{
		checkSlow(Index == Packages.Num());
		UPackage* Package = PackageProxies[Index].CreatePackage(*this);
		Index = Packages.Add(Package);
		PackageIndexes.Add(Package, Index);
	}
	void ResolvePackage(int32 Index)
	{
		PackageProxies[Index].Resolve(Packages[Index], *this);
	}
	int32 GetPackageIndex(UPackage* Package) const;
	UPackage* GetPackageByIndex(int32 Index) const;

	void AddClass(FUnrealSourceFile* UnrealSourceFile, UClass* Class)
	{
		int32 Index = Classes.Num() + NewClasses.Add(Class);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EClass, Index));
		ClassIndexes.Add(Class, Index);
	}
	void CreateClass(int32 Index)
	{
		checkSlow(Index == Classes.Num());
		UClass* Class = ClassProxies[Index].CreateClass(*this);
		Index = Classes.Add(Class);
		ClassIndexes.Add(Class, Index);
	}
	void ResolveClass(int32 Index)
	{
		ClassProxies[Index].Resolve(Classes[Index], *this);
	}
	FSerializeIndex GetClassIndex(const UClass* Class) const;
	UClass* GetClassByIndex(FSerializeIndex ClassIndex) const;

	void AddEnum(FUnrealSourceFile* UnrealSourceFile, UEnum* Enum)
	{
		int32 Index = Enums.Add(Enum);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EEnum, Index));
		EnumIndexes.Add(Enum, Index);
	}
	void CreateEnum(int32 Index)
	{
		checkSlow(Index == Enums.Num());
		UEnum* Enum = EnumProxies[Index].CreateEnum(*this);
		Index = Enums.Add(Enum);
		EnumIndexes.Add(Enum, Index);
	}
	void ResolveEnum(int32 Index)
	{
		EnumProxies[Index].Resolve(Enums[Index], *this);
	}
	int32 GetEnumIndex(const UEnum* Enum) const;
	UEnum* GetEnumByIndex(int32 EnumIndex) const;

	void AddStruct(FUnrealSourceFile* UnrealSourceFile, UStruct* Struct)
	{
		int32 Index = Structs.Add(Struct);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EStruct, Index));
		StructIndexes.Add(Struct, Index);
	}
	void CreateStruct(int32 Index)
	{
		checkSlow(Index == Structs.Num());
		UStruct* Struct = StructProxies[Index].CreateStruct(*this);
		Index = Structs.Add(Struct);
		StructIndexes.Add(Struct, Index);
	}
	void ResolveStruct(int32 Index)
	{
		StructProxies[Index].Resolve(Structs[Index], *this);
	}
	FSerializeIndex GetStructIndex(const UStruct* Struct) const;
	UStruct* GetStructByIndex(FSerializeIndex StructIndex) const;

	void AddScriptStruct(FUnrealSourceFile* UnrealSourceFile, UScriptStruct* ScriptStruct)
	{
		int32 Index = ScriptStructs.Add(ScriptStruct);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EScriptStruct, Index));
		ScriptStructIndexes.Add(ScriptStruct, Index);
	}
	void CreateScriptStruct(int32 Index)
	{
		checkSlow(Index == ScriptStructs.Num());
		UScriptStruct* ScriptStruct = ScriptStructProxies[Index].CreateScriptStruct(*this);
		Index = ScriptStructs.Add(ScriptStruct);
		ScriptStructIndexes.Add(ScriptStruct, Index);
	}
	void ResolveScriptStruct(int32 Index)
	{
		ScriptStructProxies[Index].Resolve(ScriptStructs[Index], *this);
	}
	FSerializeIndex GetScriptStructIndex(const UScriptStruct* Struct) const;
	UScriptStruct* GetScriptStructByIndex(FSerializeIndex Index) const;

	void AddPropertyBase(FUnrealSourceFile* UnrealSourceFile, FPropertyBase* PropertyBase)
	{
		int32 Index = PropertyBases.Add(PropertyBase);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EPropertyBase, Index));
		PropertyBaseIndexes.Add(PropertyBase, Index);
	}
	void CreatePropertyBase(int32 Index)
	{
		checkSlow(Index == PropertyBases.Num());
		FPropertyBase* PropertyBase = PropertyBaseProxies[Index].CreatePropertyBase(*this);
		Index = PropertyBases.Add(PropertyBase);
		PropertyBaseIndexes.Add(PropertyBase, Index);
	}
	void ResolvePropertyBase(int32 Index)
	{
		PropertyBaseProxies[Index].Resolve(PropertyBases[Index], *this);
	}
	int32 GetPropertyBaseIndex(FPropertyBase* PropertyBase) const;
	FPropertyBase* GetPropertyBaseByIndex(int32 Index) const;

	void AddProperty(FUnrealSourceFile* UnrealSourceFile, UProperty* Property)
	{
		int32 Index = Properties.Add(Property);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EProperty, Index));
		PropertyIndexes.Add(Property, Index);
	}
	void CreateProperty(int32 Index)
	{
		checkSlow(Index == Properties.Num());
		UProperty* Property = PropertyProxies[Index].CreateProperty(*this);
		Index = Properties.Add(Property);
		PropertyIndexes.Add(Property, Index);
	}
	void ResolveProperty(int32 Index)
	{
		PropertyProxies[Index].Resolve(Properties[Index], *this);
	}
	int32 GetPropertyIndex(const UProperty* Property) const;
	UProperty* GetPropertyByIndex(int32 Index) const;

	void AddByteProperty(FUnrealSourceFile* UnrealSourceFile, UByteProperty* ByteProperty)
	{
		int32 Index = ByteProperties.Add(ByteProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EByteProperty, Index));
		BytePropertyIndexes.Add(ByteProperty, Index);
	}
	void CreateByteProperty(int32 Index)
	{
		checkSlow(Index == ByteProperties.Num());
		UByteProperty* ByteProperty = BytePropertyProxies[Index].CreateByteProperty(*this);
		Index = ByteProperties.Add(ByteProperty);
		BytePropertyIndexes.Add(ByteProperty, Index);
	}
	void ResolveByteProperty(int32 Index)
	{
		BytePropertyProxies[Index].Resolve(ByteProperties[Index], *this);
	}

	void AddInt8Property(FUnrealSourceFile* UnrealSourceFile, UInt8Property* Int8Property)
	{
		int32 Index = Int8Properties.Add(Int8Property);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EInt8Property, Index));
		Int8PropertyIndexes.Add(Int8Property, Index);
	}
	void CreateInt8Property(int32 Index)
	{
		checkSlow(Index == Int8Properties.Num());
		UInt8Property* Int8Property = Int8PropertyProxies[Index].CreateInt8Property(*this);
		Index = Int8Properties.Add(Int8Property);
		Int8PropertyIndexes.Add(Int8Property, Index);
	}
	void ResolveInt8Property(int32 Index)
	{
		Int8PropertyProxies[Index].Resolve(Int8Properties[Index], *this);
	}

	void AddInt16Property(FUnrealSourceFile* UnrealSourceFile, UInt16Property* Int16Property)
	{
		int32 Index = Int16Properties.Add(Int16Property);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EInt16Property, Index));
		Int16PropertyIndexes.Add(Int16Property, Index);
	}
	void CreateInt16Property(int32 Index)
	{
		checkSlow(Index == Int16Properties.Num());
		UInt16Property* Int16Property = Int16PropertyProxies[Index].CreateInt16Property(*this);
		Index = Int16Properties.Add(Int16Property);
		Int16PropertyIndexes.Add(Int16Property, Index);
	}
	void ResolveInt16Property(int32 Index)
	{
		Int16PropertyProxies[Index].Resolve(Int16Properties[Index], *this);
	}

	void AddIntProperty(FUnrealSourceFile* UnrealSourceFile, UIntProperty* IntProperty)
	{
		int32 Index = IntProperties.Add(IntProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EIntProperty, Index));
		IntPropertyIndexes.Add(IntProperty, Index);
	}
	void CreateIntProperty(int32 Index)
	{
		checkSlow(Index == IntProperties.Num());
		UIntProperty* IntProperty = IntPropertyProxies[Index].CreateIntProperty(*this);
		Index = IntProperties.Add(IntProperty);
		IntPropertyIndexes.Add(IntProperty, Index);
	}
	void ResolveIntProperty(int32 Index)
	{
		IntPropertyProxies[Index].Resolve(IntProperties[Index], *this);
	}

	void AddInt64Property(FUnrealSourceFile* UnrealSourceFile, UInt64Property* Int64Property)
	{
		int32 Index = Int64Properties.Add(Int64Property);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EInt64Property, Index));
		Int64PropertyIndexes.Add(Int64Property, Index);
	}
	void CreateInt64Property(int32 Index)
	{
		checkSlow(Index == Int64Properties.Num());
		UInt64Property* Int64Property = Int64PropertyProxies[Index].CreateInt64Property(*this);
		Index = Int64Properties.Add(Int64Property);
		Int64PropertyIndexes.Add(Int64Property, Index);
	}
	void ResolveInt64Property(int32 Index)
	{
		Int64PropertyProxies[Index].Resolve(Int64Properties[Index], *this);
	}

	void AddUInt16Property(FUnrealSourceFile* UnrealSourceFile, UUInt16Property* UInt16Property)
	{
		int32 Index = UInt16Properties.Add(UInt16Property);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EUInt16Property, Index));
		UInt16PropertyIndexes.Add(UInt16Property, Index);
	}
	void CreateUInt16Property(int32 Index)
	{
		checkSlow(Index == UInt16Properties.Num());
		UUInt16Property* UInt16Property = UInt16PropertyProxies[Index].CreateUInt16Property(*this);
		Index = UInt16Properties.Add(UInt16Property);
		UInt16PropertyIndexes.Add(UInt16Property, Index);
	}
	void ResolveUInt16Property(int32 Index)
	{
		UInt16PropertyProxies[Index].Resolve(UInt16Properties[Index], *this);
	}

	void AddUInt32Property(FUnrealSourceFile* UnrealSourceFile, UUInt32Property* UInt32Property)
	{
		int32 Index = UInt32Properties.Add(UInt32Property);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EUInt32Property, Index));
		UInt32PropertyIndexes.Add(UInt32Property, Index);
	}
	void CreateUInt32Property(int32 Index)
	{
		checkSlow(Index == UInt32Properties.Num());
		UUInt32Property* UInt32Property = UInt32PropertyProxies[Index].CreateUInt32Property(*this);
		Index = UInt32Properties.Add(UInt32Property);
		UInt32PropertyIndexes.Add(UInt32Property, Index);
	}
	void ResolveUInt32Property(int32 Index)
	{
		UInt32PropertyProxies[Index].Resolve(UInt32Properties[Index], *this);
	}

	void AddUInt64Property(FUnrealSourceFile* UnrealSourceFile, UUInt64Property* UInt64Property)
	{
		int32 Index = UInt64Properties.Add(UInt64Property);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EUInt64Property, Index));
		UInt64PropertyIndexes.Add(UInt64Property, Index);
	}
	void CreateUInt64Property(int32 Index)
	{
		checkSlow(Index == UInt64Properties.Num());
		UUInt64Property* UInt64Property = UInt64PropertyProxies[Index].CreateUInt64Property(*this);
		Index = UInt64Properties.Add(UInt64Property);
		UInt64PropertyIndexes.Add(UInt64Property, Index);
	}
	void ResolveUInt64Property(int32 Index)
	{
		UInt64PropertyProxies[Index].Resolve(UInt64Properties[Index], *this);
	}

	void AddFloatProperty(FUnrealSourceFile* UnrealSourceFile, UFloatProperty* FloatProperty)
	{
		int32 Index = FloatProperties.Add(FloatProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EFloatProperty, Index));
		FloatPropertyIndexes.Add(FloatProperty, Index);
	}
	void CreateFloatProperty(int32 Index)
	{
		checkSlow(Index == FloatProperties.Num());
		UFloatProperty* FloatProperty = FloatPropertyProxies[Index].CreateFloatProperty(*this);
		Index = FloatProperties.Add(FloatProperty);
		FloatPropertyIndexes.Add(FloatProperty, Index);
	}
	void ResolveFloatProperty(int32 Index)
	{
		FloatPropertyProxies[Index].Resolve(FloatProperties[Index], *this);
	}

	void AddDoubleProperty(FUnrealSourceFile* UnrealSourceFile, UDoubleProperty* DoubleProperty)
	{
		int32 Index = DoubleProperties.Add(DoubleProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EDoubleProperty, Index));
		DoublePropertyIndexes.Add(DoubleProperty, Index);
	}
	void CreateDoubleProperty(int32 Index)
	{
		checkSlow(Index == DoubleProperties.Num());
		UDoubleProperty* DoubleProperty = DoublePropertyProxies[Index].CreateDoubleProperty(*this);
		Index = DoubleProperties.Add(DoubleProperty);
		DoublePropertyIndexes.Add(DoubleProperty, Index);
	}
	void ResolveDoubleProperty(int32 Index)
	{
		DoublePropertyProxies[Index].Resolve(DoubleProperties[Index], *this);
	}

	void AddBoolProperty(FUnrealSourceFile* UnrealSourceFile, UBoolProperty* BoolProperty)
	{
		int32 Index = BoolProperties.Add(BoolProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EBoolProperty, Index));
		BoolPropertyIndexes.Add(BoolProperty, Index);
	}
	void CreateBoolProperty(int32 Index)
	{
		checkSlow(Index == BoolProperties.Num());
		UBoolProperty* BoolProperty = BoolPropertyProxies[Index].CreateBoolProperty(*this);
		Index = BoolProperties.Add(BoolProperty);
		BoolPropertyIndexes.Add(BoolProperty, Index);
	}
	void ResolveBoolProperty(int32 Index)
	{
		BoolPropertyProxies[Index].Resolve(BoolProperties[Index], *this);
	}

	void AddNameProperty(FUnrealSourceFile* UnrealSourceFile, UNameProperty* NameProperty)
	{
		int32 Index = NameProperties.Add(NameProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::ENameProperty, Index));
		NamePropertyIndexes.Add(NameProperty, Index);
	}
	void CreateNameProperty(int32 Index)
	{
		checkSlow(Index == NameProperties.Num());
		UNameProperty* NameProperty = NamePropertyProxies[Index].CreateNameProperty(*this);
		Index = NameProperties.Add(NameProperty);
		NamePropertyIndexes.Add(NameProperty, Index);
	}
	void ResolveNameProperty(int32 Index)
	{
		NamePropertyProxies[Index].Resolve(NameProperties[Index], *this);
	}

	void AddStrProperty(FUnrealSourceFile* UnrealSourceFile, UStrProperty* StrProperty)
	{
		int32 Index = StrProperties.Add(StrProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EStrProperty, Index));
		StrPropertyIndexes.Add(StrProperty, Index);
	}
	void CreateStrProperty(int32 Index)
	{
		checkSlow(Index == StrProperties.Num());
		UStrProperty* StrProperty = StrPropertyProxies[Index].CreateStrProperty(*this);
		StrProperties.Add(StrProperty);
		StrPropertyIndexes.Add(StrProperty, Index);
	}
	void ResolveStrProperty(int32 Index)
	{
		StrPropertyProxies[Index].Resolve(StrProperties[Index], *this);
	}

	void AddTextProperty(FUnrealSourceFile* UnrealSourceFile, UTextProperty* TextProperty)
	{
		int32 Index = TextProperties.Add(TextProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::ETextProperty, Index));
		TextPropertyIndexes.Add(TextProperty, Index);
	}
	void CreateTextProperty(int32 Index)
	{
		checkSlow(Index == TextProperties.Num());
		UTextProperty* TextProperty = TextPropertyProxies[Index].CreateTextProperty(*this);
		Index = TextProperties.Add(TextProperty);
		TextPropertyIndexes.Add(TextProperty, Index);
	}
	void ResolveTextProperty(int32 Index)
	{
		TextPropertyProxies[Index].Resolve(TextProperties[Index], *this);
	}

	void AddDelegateProperty(FUnrealSourceFile* UnrealSourceFile, UDelegateProperty* DelegateProperty)
	{
		int32 Index = DelegateProperties.Add(DelegateProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EDelegateProperty, Index));
		DelegatePropertyIndexes.Add(DelegateProperty, Index);
	}
	void CreateDelegateProperty(int32 Index)
	{
		checkSlow(Index == DelegateProperties.Num());
		UDelegateProperty* DelegateProperty = DelegatePropertyProxies[Index].CreateDelegateProperty(*this);
		Index = DelegateProperties.Add(DelegateProperty);
		DelegatePropertyIndexes.Add(DelegateProperty, Index);
	}
	void ResolveDelegateProperty(int32 Index)
	{
		DelegatePropertyProxies[Index].Resolve(DelegateProperties[Index], *this);
	}

	void AddMulticastDelegateProperty(FUnrealSourceFile* UnrealSourceFile, UMulticastDelegateProperty* MulticastDelegateProperty)
	{
		int32 Index = MulticastDelegateProperties.Add(MulticastDelegateProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EMulticastDelegateProperty, Index));
		MulticastDelegatePropertyIndexes.Add(MulticastDelegateProperty, Index);
	}
	void CreateMulticastDelegateProperty(int32 Index)
	{
		checkSlow(Index == MulticastDelegateProperties.Num());
		UMulticastDelegateProperty* MulticastDelegateProperty = MulticastDelegatePropertyProxies[Index].CreateMulticastDelegateProperty(*this);
		Index = MulticastDelegateProperties.Add(MulticastDelegateProperty);
		MulticastDelegatePropertyIndexes.Add(MulticastDelegateProperty, Index);
	}
	void ResolveMulticastDelegateProperty(int32 Index)
	{
		MulticastDelegatePropertyProxies[Index].Resolve(MulticastDelegateProperties[Index], *this);
	}

	void AddObjectPropertyBase(FUnrealSourceFile* UnrealSourceFile, UObjectPropertyBase* ObjectPropertyBase)
	{
		int32 Index = ObjectPropertyBases.Add(ObjectPropertyBase);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EObjectPropertyBase, Index));
		ObjectPropertyBaseIndexes.Add(ObjectPropertyBase, Index);
	}
	void CreateObjectPropertyBase(int32 Index)
	{
		checkSlow(Index == ObjectPropertyBases.Num());
		UObjectPropertyBase* ObjectPropertyBase = ObjectPropertyBaseProxies[Index].CreateObjectPropertyBase(*this);
		Index = ObjectPropertyBases.Add(ObjectPropertyBase);
		ObjectPropertyBaseIndexes.Add(ObjectPropertyBase, Index);
	}
	void ResolveObjectPropertyBase(int32 Index)
	{
		ObjectPropertyBaseProxies[Index].Resolve(ObjectPropertyBases[Index], *this);
	}

	void AddClassProperty(FUnrealSourceFile* UnrealSourceFile, UClassProperty* ClassProperty)
	{
		int32 Index = ClassProperties.Add(ClassProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EClassProperty, Index));
		ClassPropertyIndexes.Add(ClassProperty, Index);
	}
	void CreateClassProperty(int32 Index)
	{
		checkSlow(Index == ClassProperties.Num());
		UClassProperty* ClassProperty = ClassPropertyProxies[Index].CreateClassProperty(*this);
		Index = ClassProperties.Add(ClassProperty);
		ClassPropertyIndexes.Add(ClassProperty, Index);
	}
	void ResolveClassProperty(int32 Index)
	{
		ClassPropertyProxies[Index].Resolve(ClassProperties[Index], *this);
	}

	void AddObjectProperty(FUnrealSourceFile* UnrealSourceFile, UObjectProperty* ObjectProperty)
	{
		int32 Index = ObjectProperties.Add(ObjectProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EObjectProperty, Index));
		ObjectPropertyIndexes.Add(ObjectProperty, Index);
	}
	void CreateObjectProperty(int32 Index)
	{
		checkSlow(Index == ObjectProperties.Num());
		UObjectProperty* ObjectProperty = ObjectPropertyProxies[Index].CreateObjectProperty(*this);
		Index = ObjectProperties.Add(ObjectProperty);
		ObjectPropertyIndexes.Add(ObjectProperty, Index);
	}
	void ResolveObjectProperty(int32 Index)
	{
		ObjectPropertyProxies[Index].Resolve(ObjectProperties[Index], *this);
	}

	void AddWeakObjectProperty(FUnrealSourceFile* UnrealSourceFile, UWeakObjectProperty* WeakObjectProperty)
	{
		int32 Index = WeakObjectProperties.Add(WeakObjectProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EWeakObjectProperty, Index));
		WeakObjectPropertyIndexes.Add(WeakObjectProperty, Index);
	}
	void CreateWeakObjectProperty(int32 Index)
	{
		checkSlow(Index == WeakObjectProperties.Num());
		UWeakObjectProperty* WeakObjectProperty = WeakObjectPropertyProxies[Index].CreateWeakObjectProperty(*this);
		Index = WeakObjectProperties.Add(WeakObjectProperty);
		WeakObjectPropertyIndexes.Add(WeakObjectProperty, Index);
	}
	void ResolveWeakObjectProperty(int32 Index)
	{
		WeakObjectPropertyProxies[Index].Resolve(WeakObjectProperties[Index], *this);
	}

	void AddLazyObjectProperty(FUnrealSourceFile* UnrealSourceFile, ULazyObjectProperty* LazyObjectProperty)
	{
		int32 Index = LazyObjectProperties.Add(LazyObjectProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::ELazyObjectProperty, Index));
		LazyObjectPropertyIndexes.Add(LazyObjectProperty, Index);
	}
	void CreateLazyObjectProperty(int32 Index)
	{
		checkSlow(Index == LazyObjectProperties.Num());
		ULazyObjectProperty* LazyObjectProperty = LazyObjectPropertyProxies[Index].CreateLazyObjectProperty(*this);
		Index = LazyObjectProperties.Add(LazyObjectProperty);
		LazyObjectPropertyIndexes.Add(LazyObjectProperty, Index);
	}
	void ResolveLazyObjectProperty(int32 Index)
	{
		LazyObjectPropertyProxies[Index].Resolve(LazyObjectProperties[Index], *this);
	}

	void AddAssetObjectProperty(FUnrealSourceFile* UnrealSourceFile, UAssetObjectProperty* AssetObjectProperty)
	{
		int32 Index = AssetObjectProperties.Add(AssetObjectProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EAssetObjectProperty, Index));
		AssetObjectPropertyIndexes.Add(AssetObjectProperty, Index);
	}
	void CreateAssetObjectProperty(int32 Index)
	{
		checkSlow(Index == AssetObjectProperties.Num());
		UAssetObjectProperty* AssetObjectProperty = AssetObjectPropertyProxies[Index].CreateAssetObjectProperty(*this);
		Index = AssetObjectProperties.Add(AssetObjectProperty);
		AssetObjectPropertyIndexes.Add(AssetObjectProperty, Index);
	}
	void ResolveAssetObjectProperty(int32 Index)
	{
		AssetObjectPropertyProxies[Index].Resolve(AssetObjectProperties[Index], *this);
	}

	void AddAssetClassProperty(FUnrealSourceFile* UnrealSourceFile, UAssetClassProperty* AssetClassProperty)
	{
		int32 Index = AssetClassProperties.Add(AssetClassProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EAssetClassProperty, Index));
		AssetClassPropertyIndexes.Add(AssetClassProperty, Index);
	}
	void CreateAssetClassProperty(int32 Index)
	{
		checkSlow(Index == AssetClassProperties.Num());
		UAssetClassProperty* AssetClassProperty = AssetClassPropertyProxies[Index].CreateAssetClassProperty(*this);
		Index = AssetClassProperties.Add(AssetClassProperty);
		AssetClassPropertyIndexes.Add(AssetClassProperty, Index);
	}
	void ResolveAssetClassProperty(int32 Index)
	{
		AssetClassPropertyProxies[Index].Resolve(AssetClassProperties[Index], *this);
	}

	void AddInterfaceProperty(FUnrealSourceFile* UnrealSourceFile, UInterfaceProperty* InterfaceProperty)
	{
		int32 Index = InterfaceProperties.Add(InterfaceProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EInterfaceProperty, Index));
		InterfacePropertyIndexes.Add(InterfaceProperty, Index);
	}
	void CreateInterfaceProperty(int32 Index)
	{
		checkSlow(Index == InterfaceProperties.Num());
		UInterfaceProperty* InterfaceProperty = InterfacePropertyProxies[Index].CreateInterfaceProperty(*this);
		Index = InterfaceProperties.Add(InterfaceProperty);
		InterfacePropertyIndexes.Add(InterfaceProperty, Index);
	}
	void ResolveInterfaceProperty(int32 Index)
	{
		InterfacePropertyProxies[Index].Resolve(InterfaceProperties[Index], *this);
	}

	void AddStructProperty(FUnrealSourceFile* UnrealSourceFile, UStructProperty* StructProperty)
	{
		int32 Index = StructProperties.Add(StructProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EStructProperty, Index));
		StructPropertyIndexes.Add(StructProperty, Index);
	}
	void CreateStructProperty(int32 Index)
	{
		checkSlow(Index == StructProperties.Num());
		UStructProperty* StructProperty = StructPropertyProxies[Index].CreateStructProperty(*this);
		Index = StructProperties.Add(StructProperty);
		StructPropertyIndexes.Add(StructProperty, Index);
	}
	void ResolveStructProperty(int32 Index)
	{
		StructPropertyProxies[Index].Resolve(StructProperties[Index], *this);
	}

	void AddMapProperty(FUnrealSourceFile* UnrealSourceFile, UMapProperty* MapProperty)
	{
		int32 Index = MapProperties.Add(MapProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EMapProperty, Index));
		MapPropertyIndexes.Add(MapProperty, Index);
	}
	void CreateMapProperty(int32 Index)
	{
		checkSlow(Index == MapProperties.Num());
		UMapProperty* MapProperty = MapPropertyProxies[Index].CreateMapProperty(*this);
		Index = MapProperties.Add(MapProperty);
		MapPropertyIndexes.Add(MapProperty, Index);
	}
	void ResolveMapProperty(int32 Index)
	{
		MapPropertyProxies[Index].Resolve(MapProperties[Index], *this);
	}

	void AddSetProperty(FUnrealSourceFile* UnrealSourceFile, USetProperty* SetProperty)
	{
		int32 Index = SetProperties.Add(SetProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::ESetProperty, Index));
		SetPropertyIndexes.Add(SetProperty, Index);
	}
	void CreateSetProperty(int32 Index)
	{
		checkSlow(Index == SetProperties.Num());
		USetProperty* SetProperty = SetPropertyProxies[Index].CreateSetProperty(*this);
		Index = SetProperties.Add(SetProperty);
		SetPropertyIndexes.Add(SetProperty, Index);
	}
	void ResolveSetProperty(int32 Index)
	{
		SetPropertyProxies[Index].Resolve(SetProperties[Index], *this);
	}

	void AddArrayProperty(FUnrealSourceFile* UnrealSourceFile, UArrayProperty* ArrayProperty)
	{
		int32 Index = ArrayProperties.Add(ArrayProperty);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EArrayProperty, Index));
		ArrayPropertyIndexes.Add(ArrayProperty, Index);
	}
	void CreateArrayProperty(int32 Index)
	{
		checkSlow(Index == ArrayProperties.Num());
		UArrayProperty* ArrayProperty = ArrayPropertyProxies[Index].CreateArrayProperty(*this);
		Index = ArrayProperties.Add(ArrayProperty);
		ArrayPropertyIndexes.Add(ArrayProperty, Index);
	}
	void ResolveArrayProperty(int32 Index)
	{
		ArrayPropertyProxies[Index].Resolve(ArrayProperties[Index], *this);
	}

	void AddFunction(FUnrealSourceFile* UnrealSourceFile, UFunction* Function)
	{
		int32 Index = Functions.Add(Function);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EFunction, Index));
		FunctionIndexes.Add(Function, Index);
	}
	void CreateFunction(int32 Index)
	{
		checkSlow(Index == Functions.Num());
		UFunction* Function = FunctionProxies[Index].CreateFunction(*this);
		Index = Functions.Add(Function);
		FunctionIndexes.Add(Function, Index);
	}
	void ResolveFunction(int32 Index)
	{
		FunctionProxies[Index].Resolve(Functions[Index], *this);
	}
	int32 GetFunctionIndex(const UFunction* Function) const;
	UFunction* GetFunctionByIndex(int32 Index) const;

	void AddDelegateFunction(FUnrealSourceFile* UnrealSourceFile, UDelegateFunction* DelegateFunction)
	{
		int32 Index = DelegateFunctions.Add(DelegateFunction);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EDelegateFunction, Index));
		DelegateFunctionIndexes.Add(DelegateFunction, Index);
	}
	void CreateDelegateFunction(int32 Index)
	{
		checkSlow(Index == DelegateFunctions.Num());
		UDelegateFunction* DelegateFunction = DelegateFunctionProxies[Index].CreateDelegateFunction(*this);
		Index = DelegateFunctions.Add(DelegateFunction);
		DelegateFunctionIndexes.Add(DelegateFunction, Index);
	}
	void ResolveDelegateFunction(int32 Index)
	{
		DelegateFunctionProxies[Index].Resolve(DelegateFunctions[Index], *this);
	}

	void AddFileScope(FFileScope* FileScope)
	{
		int32 Index = FileScopes.Add(FileScope);
		FileScopeIndexes.Add(FileScope, Index);
	}
	void ResolveFileScope(int32 Index) const
	{
		FileScopeProxies[Index].Resolve(FileScopes[Index], *this);
	}
	int32 GetFileScopeIndex(const FFileScope* FileScope) const;
	FFileScope* GetFileScopeByIndex(int32 Index) const;

	void AddStructScope(FUnrealSourceFile* UnrealSourceFile, FStructScope* StructScope)
	{
		int32 Index = NewStructScopes.Add(StructScope) + StructScopes.Num();
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EStructScope, Index));
		StructScopeIndexes.Add(StructScope, Index);
	}
	void CreateStructScope(int32 Index)
	{
		checkSlow(Index == StructScopes.Num());
		FStructScope* StructScope = StructScopeProxies[Index].CreateStructScope(*this);
		Index = StructScopes.Add(StructScope);
		StructScopeIndexes.Add(StructScope, Index);
	}
	void ResolveStructScope(int32 Index)
	{
		StructScopeProxies[Index].Resolve(StructScopes[Index], *this);
	}
	int32 GetStructScopeIndex(const FStructScope* StructScope) const;
	FStructScope* GetStructScopeByIndex(int32 Index) const;

	void AddScope(FUnrealSourceFile* UnrealSourceFile, FScope* Scope)
	{
		int32 Index = Scopes.Add(Scope);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EScope, Index));
		ScopeIndexes.Add(Scope, Index);
	}
	void ResolveScope(int32 Index)
	{
		ScopeProxies[Index].Resolve(Scopes[Index], *this);
	}
	int32 GetScopeIndex(const FScope* Scope) const;
	FScope* GetScopeByIndex(int32 Index) const;

	void AddUnrealTypeDefinitionInfo(FUnrealSourceFile* UnrealSourceFile, FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo)
	{
		int32 Index = UnrealTypeDefinitionInfos.Num() + NewUnrealTypeDefinitionInfos.Add(UnrealTypeDefinitionInfo);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EUnrealTypeDefinitionInfo, Index));
		UnrealTypeDefinitionInfoIndexes.Add(UnrealTypeDefinitionInfo, Index);
	}
	void CreateUnrealTypeDefinitionInfo(int32 Index)
	{
		checkSlow(Index == UnrealTypeDefinitionInfos.Num());
		Index = UnrealTypeDefinitionInfos.Add(UnrealTypeDefinitionInfoProxies[Index].CreateUnrealTypeDefinitionInfo(*this));
	}
	void ResolveUnrealTypeDefinitionInfo(int32 Index)
	{
		FUnrealTypeDefinitionInfo*& UnrealTypeDefinitionInfo = UnrealTypeDefinitionInfos[Index];
		UnrealTypeDefinitionInfoProxies[Index].Resolve(UnrealTypeDefinitionInfo, *this);
		UnrealTypeDefinitionInfoIndexes.Add(UnrealTypeDefinitionInfo, Index);
	}
	int32 GetUnrealTypeDefinitionInfoIndex(FUnrealTypeDefinitionInfo* Info) const;
	FUnrealTypeDefinitionInfo* GetUnrealTypeDefinitionInfoByIndex(int32 Index) const;

	void AddTypeDefinitionInfoMapEntry(FUnrealSourceFile* SourceFile, UField* Field, FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo)
	{
		int32 Index = TypeDefinitionInfoMap.Num() + NewTypeDefinitionInfoMap.Add(TPairInitializer<UField*, FUnrealTypeDefinitionInfo*>(Field, UnrealTypeDefinitionInfo));
		GetHeaderDescriptor(SourceFile).AddEntry(AddObject(ESerializedObjectType::ETypeDefinitionInfoMapEntry, Index));
	}
	void CreateTypeDefinitionInfoMapEntry(int32 Index);
	void ResolveTypeDefinitionInfoMapEntry(int32 Index);
	void ResolveTypeDefinitionInfoMapEntryPreload(int32 Index);
	TPair<UField*, FUnrealTypeDefinitionInfo*>* GetTypeDefinitionInfoMapEntryByIndex(int32 Index)
	{
		return &TypeDefinitionInfoMap[Index];
	}

	void AddUnrealSourceFilesMapEntry(FUnrealSourceFile* UnrealSourceFile, const FString& RawFilename)
	{
		int32 Index = UnrealSourceFilesMapEntries.Num() + NewUnrealSourceFilesMapEntries.Add(TPairInitializer<FString, FUnrealSourceFile*>(RawFilename, UnrealSourceFile));
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EUnrealSourceFilesMapEntry, Index));
	}
	void CreateUnrealSourceFilesMapEntry(int32 Index)
	{
		auto& Kvp = UnrealSourceFilesMapEntryProxies[Index];
		UnrealSourceFilesMapEntries.Add(TPairInitializer<FString, FUnrealSourceFile*>(Kvp.Key, GetUnrealSourceFileByIndex(Kvp.Value)));
	}
	void AddPublicClassSetEntry(FUnrealSourceFile* UnrealSourceFile, UClass* Class)
	{
		int32 Index = PublicClassSetEntries.Num() + NewPublicClassSetEntries.Add(Class);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EPublicClassSetEntry, Index));
	}

	void AddGEnumUnderlyingType(FUnrealSourceFile* UnrealSourceFile, UEnum* Enum, EPropertyType Type)
	{
		int32 Index = EnumUnderlyingTypes.Add(TPairInitializer<UEnum*, EPropertyType>(Enum, Type));
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EEnumUnderlyingType, Index));
	}
	void CreateGEnumUnderlyingType(int32 Index)
	{
		EnumUnderlyingTypes.Add(TPairInitializer<UEnum*, EPropertyType>(nullptr, EnumUnderlyingTypeProxies[Index].Value));
	}
	void ResolveGEnumUnderlyingType(int32 Index)
	{
		EnumUnderlyingTypes[Index].Key = GetEnumByIndex(EnumUnderlyingTypeProxies[Index].Key);
		GEnumUnderlyingTypes.Add(EnumUnderlyingTypes[Index].Key, EnumUnderlyingTypes[Index].Value);
	}

	void AddPublicSourceFileSetEntry(FUnrealSourceFile* UnrealSourceFile)
	{
		int32 Index = PublicSourceFileSetEntries.Add(UnrealSourceFile);
		AddObject(ESerializedObjectType::EPublicSourceFileSetEntry, Index);
	}
	void ResolvePublicClassSetEntry(int32 Index);

	void AddUnrealSourceFile(FUnrealSourceFile* UnrealSourceFile)
	{
		int32 UnrealSourceFileIndex = UnrealSourceFiles.Num() + NewUnrealSourceFiles.Add(UnrealSourceFile);
		FFileScope* FileScope = &UnrealSourceFile->GetScope().Get();
		int32 FileScopeIndex = FileScopes.Num() + NewFileScopes.Add(FileScope);
		FUHTMakefileHeaderDescriptor& HeaderDescriptor = GetHeaderDescriptor(UnrealSourceFile);
		check(HeaderDescriptor.GetSourceFile() == nullptr);
		HeaderDescriptor.SetSourceFile(UnrealSourceFile);
		HeaderDescriptor.AddEntry(AddObject(ESerializedObjectType::EUnrealSourceFile, UnrealSourceFileIndex));
		HeaderDescriptor.AddEntry(AddObject(ESerializedObjectType::EFileScope, FileScopeIndex));
		UnrealSourceFileIndexes.Add(UnrealSourceFile, UnrealSourceFileIndex);
		FileScopeIndexes.Add(FileScope, FileScopeIndex);
	}
	void CreateUnrealSourceFile(int32 Index)
	{
		checkSlow(UnrealSourceFiles.Num() == Index);
		FUnrealSourceFile* UnrealSourceFile = UnrealSourceFileProxies[Index].CreateUnrealSourceFile(*this);
		Index = UnrealSourceFiles.Add(UnrealSourceFile);
		UnrealSourceFileIndexes.Add(UnrealSourceFile, Index);
	}
	void ResolveUnrealSourceFile(int32 Index)
	{
		UnrealSourceFileProxies[Index].Resolve(UnrealSourceFiles[Index], *this);
	}
	int32 GetUnrealSourceFileIndex(const FUnrealSourceFile* UnrealSourceFile) const;
	FUnrealSourceFile* GetUnrealSourceFileByIndex(int32 Index) const;

	void AddGScriptHelperEntry(FUnrealSourceFile* UnrealSourceFile, UStruct* Struct, FClassMetaData* ClassMetaData)
	{
		int32 Index = GScriptHelperEntries.Add(TPairInitializer<UStruct*, FClassMetaData*>(Struct, ClassMetaData));
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EGScriptHelperEntry, Index));
	}
	void CreateGScriptHelperEntry(UStruct* Struct, FClassMetaData* ClassMetaData)
	{
		GScriptHelperEntries.Add(TPairInitializer<UStruct*, FClassMetaData*>(Struct, ClassMetaData));
	}
	void ResolveGScriptHelperEntry(int32 Index)
	{
		CompilerMetadataManagerArchiveProxy.Resolve(Index, GScriptHelper, *this);
	}

	void AddToken(FUnrealSourceFile* UnrealSourceFile, FToken* Token)
	{
		int32 Index = Tokens.Add(Token);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EToken, Index));
		TokenIndexes.Add(Token, Index);
	}
	void CreateToken(int32 Index)
	{
		FToken* Token = TokenProxies[Index].CreateToken();
		Index = Tokens.Add(Token);
		TokenIndexes.Add(Token, Index);
	}
	void ResolveToken(int32 Index)
	{
		TokenProxies[Index].Resolve(Tokens[Index], *this);
	}
	int32 GetTokenIndex(const FToken* Token) const;
	const FToken* GetTokenByIndex(int32 Index) const;

	void AddGeneratedCodeCRC(FUnrealSourceFile* SourceFile, UField* Field, uint32 CRC)
	{
		int32 Index = GeneratedCodeCRCs.Add(TPairInitializer<UField*, uint32>(Field, CRC));
		GetHeaderDescriptor(SourceFile).AddEntry(AddObject(ESerializedObjectType::EGeneratedCodeCRC, Index));
	}
	void CreateGeneratedCodeCRC(int32 Index)
	{
		checkSlow(Index == GeneratedCodeCRCs.Num());
		GeneratedCodeCRCs.Add(TPairInitializer<UField*, uint32>(nullptr, GeneratedCodeCRCProxies[Index].Value));
	}
	void ResolveGeneratedCodeCRC(int32 Index)
	{
		auto& Kvp = GeneratedCodeCRCs[Index];
		Kvp.Key = GetFieldByIndex(GeneratedCodeCRCProxies[Index].Key);
		GGeneratedCodeCRCs.Add(Kvp.Key, Kvp.Value);
	}
	TPair<FSerializeIndex, uint32>& GetGeneratedCodeCRCProxy(int32 Index)
	{
		check(GeneratedCodeCRCProxies.IsValidIndex(Index));
		return GeneratedCodeCRCProxies[Index];
	}
	
	void AddClassMetaData(FUnrealSourceFile* SourceFile, TScopedPointer<FClassMetaData>& ClassMetaData)
	{
		int32 Index = ClassMetaDatas.Add(ClassMetaData);
		GetHeaderDescriptor(SourceFile).AddEntry(AddObject(ESerializedObjectType::EClassMetaData, Index));
	}
	void CreateClassMetaData(int32 Index)
	{
		checkSlow(Index == ClassMetaDatas.Num());
		ClassMetaDatas.Add(ClassMetaDataArchiveProxies[Index].CreateClassMetaData());
	}
	void ResolveClassMetaData(int32 Index)
	{
		ClassMetaDataArchiveProxies[Index].Resolve(ClassMetaDatas[Index], *this);
	}

	void AddPropertyDataEntry(FUnrealSourceFile* UnrealSourceFile, TSharedPtr<FTokenData>& TokenData, UProperty* Property)
	{
		FToken* Token = &TokenData->Token;
		int32 TokenIndex = Tokens.Add(Token);
		TokenIndexes.Add(Token, TokenIndex);

		FUHTMakefileHeaderDescriptor& HeaderDescriptor = GetHeaderDescriptor(UnrealSourceFile);
		HeaderDescriptor.AddEntry(AddObject(ESerializedObjectType::EToken, TokenIndex));

		int32 PropertyDataEntryIndex = PropertyDataEntries.Add(TPairInitializer<UProperty*, TSharedPtr<FTokenData>>(Property, TokenData));
		HeaderDescriptor.AddEntry(AddObject(ESerializedObjectType::EPropertyDataEntry, PropertyDataEntryIndex));
	}
	void CreatePropertyDataEntry(int32 Index)
	{
		PropertyDataEntries.Add(TPairInitializer<UProperty*, TSharedPtr<FTokenData>>(nullptr, TSharedPtr<FTokenData>(nullptr)));
	}
	void ResolvePropertyDataEntry(int32 Index)
	{
		auto& Entry = PropertyDataEntries[Index];
		auto& EntryProxy = PropertyDataEntryProxies[Index];
		Entry.Key = GetPropertyByIndex(EntryProxy.Key);
		const FToken* Token = GetTokenByIndex(EntryProxy.Value);
		TokenIndexes.FindAndRemoveChecked(Token);
		Entry.Value = TSharedPtr<FTokenData>(new FTokenData(*Token));
		Tokens[EntryProxy.Value] = &Entry.Value->Token;
		TokenIndexes.Add(&Entry.Value->Token, EntryProxy.Value);
	}
	int32 GetPropertyDataEntryIndex(const TPair<UProperty*, TSharedPtr<FTokenData>>* Entry) const;
	const TPair<UProperty*, TSharedPtr<FTokenData>>* GetPropertyDataEntryByIndex(int32 Index) const;

	void AddMultipleInheritanceBaseClass(FUnrealSourceFile* UnrealSourceFile, FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass)
	{
		int32 Index = MultipleInheritanceBaseClasses.Add(MultipleInheritanceBaseClass);
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EMultipleInheritanceBaseClass, Index));
	}
	void CreateMultipleInheritanceBaseClass(int32 Index)
	{
		checkSlow(Index == MultipleInheritanceBaseClasses.Num());
		MultipleInheritanceBaseClasses.Add(MultipleInheritanceBaseClassProxies[Index].CreateMultipleInheritanceBaseClass());
	}
	void ResolveMultipleInheritanceBaseClass(int32 Index)
	{
		MultipleInheritanceBaseClassProxies[Index].Resolve(MultipleInheritanceBaseClasses[Index], *this);
	}

	void AddInterfaceAllocation(FUnrealSourceFile* UnrealSourceFile, TCHAR* InterfaceAllocation)
	{
		int32 Index = NewInterfaceAllocations.Add(InterfaceAllocation) + InterfaceAllocations.Num();
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EInterfaceAllocation, Index));
	}
	void CreateInterfaceAllocation(int32 Index)
	{
		FString& InterfaceProxy = InterfaceAllocationProxies[Index];
		int32 Strlen = InterfaceProxy.Len();
		TCHAR* InterfaceAllocation;
		if (Strlen)
		{
			InterfaceAllocation = new TCHAR[Strlen + 1];
			FMemory::Memcpy(InterfaceAllocation, *InterfaceProxy, Strlen + 1);
		}
		else
		{
			InterfaceAllocation = nullptr;
		}
		NameLookupCPP->InterfaceAllocations.Add(InterfaceAllocation);
		InterfaceAllocations.Add(InterfaceAllocation);
	}

	void AddStructNameMapEntry(FUnrealSourceFile* UnrealSourceFile, UStruct* Struct, TCHAR* NameCPP)
	{
		int32 Index = NewStructNameMapEntries.Add(TPairInitializer<UStruct*, TCHAR*>(Struct, NameCPP)) + StructNameMapEntries.Num();
		GetHeaderDescriptor(UnrealSourceFile).AddEntry(AddObject(ESerializedObjectType::EStructNameMapEntry, Index));
	}
	void CreateStructNameMapEntry(int32 Index)
	{
		StructNameMapEntries.AddDefaulted();
		auto& ProxyKvp = StructNameMapEntryProxies[Index];
		auto& Kvp = StructNameMapEntries[Index];
		Kvp.Key = nullptr;
		int32 Strlen = ProxyKvp.Value.Len();
		if (Strlen)
		{
			Kvp.Value = new TCHAR[Strlen + 1];
			FMemory::Memcpy(Kvp.Value, *ProxyKvp.Value, sizeof(TCHAR) * (Strlen + 1));
		}
		else
		{
			Kvp.Value = nullptr;
		}
	}
	void ResolveStructNameMapEntry(int32 Index)
	{
		auto& Kvp = StructNameMapEntries[Index];
		Kvp.Key = GetStructByIndex(StructNameMapEntryProxies[Index].Key);
		NameLookupCPP->StructNameMap.Add(Kvp.Key, Kvp.Value);
	}


	FSerializeIndex GetFieldIndex(const UField* Field) const;
	UField* GetFieldByIndex(FSerializeIndex Index) const;

	FSerializeIndex GetObjectIndex(const UObject* Object) const;
	UObject* GetObjectByIndex(FSerializeIndex Index) const;

	int32 GetNameIndex(FName Name) const;
	FName GetNameByIndex(int32 Index) const;

	friend FArchive& operator<<(FArchive& Ar, FUHTMakefile& UHTMakefile);

	void SetCurrentModuleName(FName InName)
	{
		CurrentModuleName = InName;
	}

	FName GetCurrentModuleName()
	{
		check(CurrentModuleName != NAME_None);
		return CurrentModuleName;
	}


	FUHTMakefileModuleDescriptor& GetCurrentModule()
	{
		return GetModuleDescriptor(GetCurrentModuleName());
	}

	FUHTMakefileHeaderDescriptor& GetHeaderDescriptorBySourceFileIndex(int32 UnrealSourceFileIndex)
	{
		FUHTMakefileModuleDescriptor& CurrentModule = GetCurrentModule();
		if (CurrentModule.HasHeaderDescriptor(UnrealSourceFileIndex))
		{
			return CurrentModule.GetHeaderDescriptor(UnrealSourceFileIndex);
		}
		else
		{
			for (auto& Kvp : ModuleDescriptors)
			{
				if (Kvp.Key == CurrentModuleName)
				{
					continue;
				}

				FUHTMakefileModuleDescriptor& ModuleDescriptor = Kvp.Value;
				if (ModuleDescriptor.HasHeaderDescriptor(UnrealSourceFileIndex))
				{
					FUHTMakefileHeaderDescriptor& HeaderDescriptor = ModuleDescriptor.GetHeaderDescriptor(UnrealSourceFileIndex);
					return HeaderDescriptor;
				}
			}
		}

		// Header descriptor not found, add a new one.
		return CurrentModule.GetHeaderDescriptor(UnrealSourceFileIndex);
	}

	FUHTMakefileHeaderDescriptor& GetHeaderDescriptor(FUnrealSourceFile* UnrealSourceFile)
	{
		int32 UnrealSourceFileIndex = GetUnrealSourceFileIndex(UnrealSourceFile);
		return GetHeaderDescriptorBySourceFileIndex(UnrealSourceFileIndex);
	}

	FUHTMakefileModuleDescriptor& GetModuleDescriptor(FName ModuleName)
	{
		return ModuleDescriptors.FindOrAdd(ModuleName);
	}
	int32 AddObject(ESerializedObjectType SerializedObjectType, int32 Index)
	{
		checkSlow(LoadingPhase != EUHTMakefileLoadingPhase::Max);
		return InitializationOrder[+LoadingPhase].Add(TPairInitializer<ESerializedObjectType, int32>(SerializedObjectType, Index));
	}

	TPair<ESerializedObjectType, int32> GetFromInitializationOrder(int32 Index, EUHTMakefileLoadingPhase UHTMakefileLoadingPhase)
	{
		return InitializationOrder[+UHTMakefileLoadingPhase][Index];
	}

	void CreateObjectAtInitOrderIndex(int32 Index, EUHTMakefileLoadingPhase UHTMakefileLoadingPhase);
	void ResolveObjectAtInitOrderIndex(int32 Index, EUHTMakefileLoadingPhase UHTMakefileLoadingPhase);

	void ResolveTypeFullyCreatedDuringPreload(int32 ObjectIndex);


	void AddToHeaderOrder(FUnrealSourceFile* SourceFile)
	{
		for (auto& Kvp : ModuleDescriptors)
		{
			if (Kvp.Value.GetPackage() == SourceFile->GetPackage())
			{
				Kvp.Value.AddToHeaderOrder(SourceFile);
				break;
			}
		}
		//GetCurrentModule().AddToHeaderOrder(SourceFile);
	}
	bool ShouldForceRegeneration() const
	{
		return bShouldForceRegeneration;
	}

	void AddName(FName Name)
	{
		ReferencedNamesSet.Add(Name);
	}
	void SetNameLookupCPP(FNameLookupCPP* InNameLookupCPP)
	{
		NameLookupCPP = InNameLookupCPP;
		NameLookupCPP->SetUHTMakefile(this);
	}
	void SetManifest(FManifest* InManifest)
	{
		Manifest = InManifest;
	}
	bool CanLoadModule(const FManifestModule& ManifestModule);
	void LoadFromFile(const TCHAR* MakefilePath, FManifest* Manifest);
	void SaveToFile(const TCHAR* MakefilePath);
private:
	bool bShouldForceRegeneration;
	bool bShouldMoveNewObjects;
	void CreateObject(ESerializedObjectType ObjectType, int32 Index);
	void ResolveObject(ESerializedObjectType ObjectType, int32 Index);
	TArray<FName> ReferencedNames;
	TArray<TArray<TPair<ESerializedObjectType, int32>>> InitializationOrder;
	struct FUHTMakefileNameMapKeyFuncs : DefaultKeyFuncs<FName, false>
	{
		static FORCEINLINE bool Matches(FName A, FName B)
		{
			// The linker requires that FNames preserve case, but the numeric suffix can be ignored since
			// that is stored separately for each FName instance saved
			return A.IsEqual(B, ENameCase::CaseSensitive, false/*bCompareNumber*/);
		}

		static FORCEINLINE uint32 GetKeyHash(FName Key)
		{
			return Key.GetComparisonIndex();
		}
	};

	TSet<FName, FUHTMakefileNameMapKeyFuncs> ReferencedNamesSet;
	TMap<FName, int32, FDefaultSetAllocator, TLinkerNameMapKeyFuncs<int32>> NameIndices;

	void ResolveModuleNames()
	{
		for (const FNameArchiveProxy& NameArchiveProxy : ModuleNamesProxy)
		{
			ModuleNames.Add(NameArchiveProxy.CreateName(*this));
		}
	}

	void SetupScopeArrays();
	void SetupProxies();
	void SetupAndSaveReferencedNames(FArchive& Ar);
	void LoadAndSetupReferencedNames(FArchive& Ar);
	int32 GetPropertyCount() const;
	int32 GetPropertyProxiesCount() const;
	int32 GetFunctionCount() const;
	int32 GetFunctionProxiesCount() const;
	int32 GetStructsCount() const;
	int32 GetStructProxiesCount() const;

	TArray<FMultipleInheritanceBaseClass*> MultipleInheritanceBaseClasses;
	TArray<FMultipleInheritanceBaseClassArchiveProxy> MultipleInheritanceBaseClassProxies;

	TArray<TSharedPtr<FTokenData>> TokenDatas;
	TArray<FTokenDataArchiveProxy> TokenDataProxies;

	TArray<TPair<UProperty*, TSharedPtr<FTokenData>>> PropertyDataEntries;
	TArray<TPair<int32, int32>> PropertyDataEntryProxies;

	FCompilerMetadataManagerArchiveProxy CompilerMetadataManagerArchiveProxy;
	TArray<TPair<UStruct*, FClassMetaData*>> GScriptHelperEntries;

	TArray<TPair<FNameArchiveProxy, FUHTMakefileModuleDescriptor>> ModuleDescriptorsArchiveProxy;
	TMap<FName, FUHTMakefileModuleDescriptor> ModuleDescriptors;

	TArray<FTokenArchiveProxy> TokenProxies;
	TArray<FToken*> Tokens;
	TMap<FToken*, int32> TokenIndexes;

	TArray<TPair<FSerializeIndex, uint32>> GeneratedCodeCRCProxies;
	TArray<TPair<UField*, uint32>> GeneratedCodeCRCs;

	/** Current version of makefile. Bump if saved file format is changed. */
	uint64 Version;

	/** Current version. Bump if serialization code changes. */
	static const uint64 CurrentVersion = 1;

	bool IsTypeFullyCreatedDuringPreload(ESerializedObjectType SerializedObjectType)
	{
		return SerializedObjectType == ESerializedObjectType::ETypeDefinitionInfoMapEntry
			|| SerializedObjectType == ESerializedObjectType::EUnrealTypeDefinitionInfo;
	}
	void UpdateModulesCompatibility(FManifest* Manifest);
	FManifest* Manifest;
	FName CurrentModuleName;

	EUHTMakefileLoadingPhase LoadingPhase;

	TArray<TPair<UEnum*, EPropertyType>> EnumUnderlyingTypes;
	TArray<TPair<int32, EPropertyType>> EnumUnderlyingTypeProxies;

	TArray<FPropertyData*> PropertyDatas;
	TArray<FPropertyDataArchiveProxy> PropertyDataProxies;

	TArray<FNameArchiveProxy> ModuleNamesProxy;
	TArray<FName> ModuleNames;

	TArray<TScopedPointer<FClassMetaData>> ClassMetaDatas;
	TArray<FClassMetaDataArchiveProxy> ClassMetaDataArchiveProxies;

	TArray<FPackageArchiveProxy> PackageProxies;
	TArray<UPackage*> Packages;
	TMap<UPackage*, int32> PackageIndexes;
	TArray<UPackage*> NewPackages;

	TArray<FUnrealSourceFileArchiveProxy> UnrealSourceFileProxies;
	TArray<FUnrealSourceFile*> UnrealSourceFiles;
	TMap<FUnrealSourceFile*, int32> UnrealSourceFileIndexes;
	TArray<FUnrealSourceFile*> NewUnrealSourceFiles;

	TArray<FClassArchiveProxy> ClassProxies;
	TArray<UClass*> Classes;
	TMap<UClass*, int32> ClassIndexes;
	TArray<UClass*> NewClasses;

	TArray<FStructArchiveProxy> StructProxies;
	TArray<UStruct*> Structs;
	TMap<UStruct*, int32> StructIndexes;

	TArray<FScriptStructArchiveProxy> ScriptStructProxies;
	TArray<UScriptStruct*> ScriptStructs;
	TMap<UScriptStruct*, int32> ScriptStructIndexes;

	TArray<FPropertyBaseArchiveProxy> PropertyBaseProxies;
	TArray<FPropertyBase*> PropertyBases;
	TMap<FPropertyBase*, int32> PropertyBaseIndexes;

	TArray<FEnumArchiveProxy> EnumProxies;
	TArray<UEnum*> Enums;
	TMap<UEnum*, int32> EnumIndexes;

	TArray<FFileScopeArchiveProxy> FileScopeProxies;
	TArray<FFileScope*> FileScopes;
	TMap<FFileScope*, int32> FileScopeIndexes;
	TArray<FFileScope*> NewFileScopes;

	TArray<FStructScopeArchiveProxy> StructScopeProxies;
	TArray<FStructScope*> StructScopes;
	TMap<FStructScope*, int32> StructScopeIndexes;
	TArray<FStructScope*> NewStructScopes;

	TArray<FScopeArchiveProxy> ScopeProxies;
	TArray<FScope*> Scopes;
	TMap<FScope*, int32> ScopeIndexes;

	TArray<FUnrealTypeDefinitionInfoArchiveProxy> UnrealTypeDefinitionInfoProxies;
	TArray<FUnrealTypeDefinitionInfo*> UnrealTypeDefinitionInfos;
	TMap<FUnrealTypeDefinitionInfo*, int32> UnrealTypeDefinitionInfoIndexes;
	TArray<FUnrealTypeDefinitionInfo*> NewUnrealTypeDefinitionInfos;

	TArray<FString> RawFilenames;

	TArray<FSerializeIndex> PublicClassSetEntryProxies;
	TArray<UClass*> PublicClassSetEntries;
	TArray<UClass*> NewPublicClassSetEntries;

	TArray<int32> PublicSourceFileSetEntryProxies;
	TArray<FUnrealSourceFile*> PublicSourceFileSetEntries;

	TArray<TPair<FString, int32>> UnrealSourceFilesMapEntryProxies;
	TArray<TPair<FString, FUnrealSourceFile*>> UnrealSourceFilesMapEntries;
	TArray<TPair<FString, FUnrealSourceFile*>> NewUnrealSourceFilesMapEntries;

	FTypeDefinitionInfoMapArchiveProxy TypeDefinitionInfoMapArchiveProxy;
	TArray<TPair<UField*, FUnrealTypeDefinitionInfo*>> TypeDefinitionInfoMap;
	TArray<TPair<UField*, FUnrealTypeDefinitionInfo*>> NewTypeDefinitionInfoMap;

	TArray<FPropertyArchiveProxy> PropertyProxies;
	TArray<UProperty*> Properties;
	TMap<UProperty*, int32> PropertyIndexes;

	TArray<FBytePropertyArchiveProxy> BytePropertyProxies;
	TArray<UByteProperty*> ByteProperties;
	TMap<UByteProperty*, int32> BytePropertyIndexes;

	TArray<FInt8PropertyArchiveProxy> Int8PropertyProxies;
	TArray<UInt8Property*> Int8Properties;
	TMap<UInt8Property*, int32> Int8PropertyIndexes;

	TArray<FInt16PropertyArchiveProxy> Int16PropertyProxies;
	TArray<UInt16Property*> Int16Properties;
	TMap<UInt16Property*, int32> Int16PropertyIndexes;

	TArray<FIntPropertyArchiveProxy> IntPropertyProxies;
	TArray<UIntProperty*> IntProperties;
	TMap<UIntProperty*, int32> IntPropertyIndexes;

	TArray<FInt64PropertyArchiveProxy> Int64PropertyProxies;
	TArray<UInt64Property*> Int64Properties;
	TMap<UInt64Property*, int32> Int64PropertyIndexes;

	TArray<FUInt16PropertyArchiveProxy> UInt16PropertyProxies;
	TArray<UUInt16Property*> UInt16Properties;
	TMap<UUInt16Property*, int32> UInt16PropertyIndexes;

	TArray<FUInt32PropertyArchiveProxy> UInt32PropertyProxies;
	TArray<UUInt32Property*> UInt32Properties;
	TMap<UUInt32Property*, int32> UInt32PropertyIndexes;

	TArray<FUInt64PropertyArchiveProxy> UInt64PropertyProxies;
	TArray<UUInt64Property*> UInt64Properties;
	TMap<UUInt64Property*, int32> UInt64PropertyIndexes;

	TArray<FFloatPropertyArchiveProxy> FloatPropertyProxies;
	TArray<UFloatProperty*> FloatProperties;
	TMap<UFloatProperty*, int32> FloatPropertyIndexes;

	TArray<FDoublePropertyArchiveProxy> DoublePropertyProxies;
	TArray<UDoubleProperty*> DoubleProperties;
	TMap<UDoubleProperty*, int32> DoublePropertyIndexes;

	TArray<FBoolPropertyArchiveProxy> BoolPropertyProxies;
	TArray<UBoolProperty*> BoolProperties;
	TMap<UBoolProperty*, int32> BoolPropertyIndexes;

	TArray<FNamePropertyArchiveProxy> NamePropertyProxies;
	TArray<UNameProperty*> NameProperties;
	TMap<UNameProperty*, int32> NamePropertyIndexes;

	TArray<FStrPropertyArchiveProxy> StrPropertyProxies;
	TArray<UStrProperty*> StrProperties;
	TMap<UStrProperty*, int32> StrPropertyIndexes;

	TArray<FTextPropertyArchiveProxy> TextPropertyProxies;
	TArray<UTextProperty*> TextProperties;
	TMap<UTextProperty*, int32> TextPropertyIndexes;

	TArray<FDelegatePropertyArchiveProxy> DelegatePropertyProxies;
	TArray<UDelegateProperty*> DelegateProperties;
	TMap<UDelegateProperty*, int32> DelegatePropertyIndexes;

	TArray<FMulticastDelegatePropertyArchiveProxy> MulticastDelegatePropertyProxies;
	TArray<UMulticastDelegateProperty*> MulticastDelegateProperties;
	TMap<UMulticastDelegateProperty*, int32> MulticastDelegatePropertyIndexes;

	TArray<FObjectPropertyBaseArchiveProxy> ObjectPropertyBaseProxies;
	TArray<UObjectPropertyBase*> ObjectPropertyBases;
	TMap<UObjectPropertyBase*, int32> ObjectPropertyBaseIndexes;

	TArray<FClassPropertyArchiveProxy> ClassPropertyProxies;
	TArray<UClassProperty*> ClassProperties;
	TMap<UClassProperty*, int32> ClassPropertyIndexes;

	TArray<FObjectPropertyArchiveProxy> ObjectPropertyProxies;
	TArray<UObjectProperty*> ObjectProperties;
	TMap<UObjectProperty*, int32> ObjectPropertyIndexes;

	TArray<FWeakObjectPropertyArchiveProxy> WeakObjectPropertyProxies;
	TArray<UWeakObjectProperty*> WeakObjectProperties;
	TMap<UWeakObjectProperty*, int32> WeakObjectPropertyIndexes;

	TArray<FLazyObjectPropertyArchiveProxy> LazyObjectPropertyProxies;
	TArray<ULazyObjectProperty*> LazyObjectProperties;
	TMap<ULazyObjectProperty*, int32> LazyObjectPropertyIndexes;

	TArray<FAssetObjectPropertyArchiveProxy> AssetObjectPropertyProxies;
	TArray<UAssetObjectProperty*> AssetObjectProperties;
	TMap<UAssetObjectProperty*, int32> AssetObjectPropertyIndexes;

	TArray<FAssetClassPropertyArchiveProxy> AssetClassPropertyProxies;
	TArray<UAssetClassProperty*> AssetClassProperties;
	TMap<UAssetClassProperty*, int32> AssetClassPropertyIndexes;

	TArray<FInterfacePropertyArchiveProxy> InterfacePropertyProxies;
	TArray<UInterfaceProperty*> InterfaceProperties;
	TMap<UInterfaceProperty*, int32> InterfacePropertyIndexes;

	TArray<FStructPropertyArchiveProxy> StructPropertyProxies;
	TArray<UStructProperty*> StructProperties;
	TMap<UStructProperty*, int32> StructPropertyIndexes;

	TArray<FMapPropertyArchiveProxy> MapPropertyProxies;
	TArray<UMapProperty*> MapProperties;
	TMap<UMapProperty*, int32> MapPropertyIndexes;

	TArray<FSetPropertyArchiveProxy> SetPropertyProxies;
	TArray<USetProperty*> SetProperties;
	TMap<USetProperty*, int32> SetPropertyIndexes;

	TArray<FArrayPropertyArchiveProxy> ArrayPropertyProxies;
	TArray<UArrayProperty*> ArrayProperties;
	TMap<UArrayProperty*, int32> ArrayPropertyIndexes;

	TArray<FFunctionArchiveProxy> FunctionProxies;
	TArray<UFunction*> Functions;
	TMap<UFunction*, int32> FunctionIndexes;

	TArray<FDelegateFunctionArchiveProxy> DelegateFunctionProxies;
	TArray<UDelegateFunction*> DelegateFunctions;
	TMap<UDelegateFunction*, int32> DelegateFunctionIndexes;

	FNameLookupCPP* NameLookupCPP;

	TArray<TPair<FSerializeIndex, FString>> StructNameMapEntryProxies;
	TArray<TPair<UStruct*, TCHAR*>> StructNameMapEntries;
	TArray<TPair<UStruct*, TCHAR*>> NewStructNameMapEntries;

	TArray<FString> InterfaceAllocationProxies;
	TArray<TCHAR*> InterfaceAllocations;
	TArray<TCHAR*> NewInterfaceAllocations;
};
