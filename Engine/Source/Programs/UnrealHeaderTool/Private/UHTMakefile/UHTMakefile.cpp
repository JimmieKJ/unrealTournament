// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealHeaderTool.h"
#include "UHTMakefile.h"
#include "ClassMaps.h"
#include "Scope.h"
#include "UnrealSourceFile.h"
#include "HeaderProvider.h"
#include "ClassArchiveProxy.h"
#include "PropertyArchiveProxy.h"
#include "StructArchiveProxy.h"
#include "PackageArchiveProxy.h"
#include "EnumArchiveProxy.h"
#include "ObjectBaseArchiveProxy.h"
#include "ScriptStructArchiveProxy.h"
#include "ModuleDescriptor.h"
#include "Manifest.h"
#include "ScopedTimers.h"
#include "ScopeExit.h"


void FUHTMakefile::SetupScopeArrays()
{
	TSet<FScope*> AllScopesSet;
	for (auto& Kvp : FScope::ScopeMap)
	{
		AllScopesSet.Add(&Kvp.Value.Get());
	}

	for (FScope* Scope : AllScopesSet)
	{
		if (FFileScope* FileScope = Scope->AsFileScope())
		{
			FileScopes.Add(FileScope);
		}
		else if (FStructScope* StructScope = Scope->AsStructScope())
		{
			StructScopes.Add(StructScope);
		}
		else
		{
			Scopes.Add(Scope);
		}
	}
}

void FUHTMakefile::SetupProxies()
{
	PackageProxies.Empty(Packages.Num());
	for (UPackage* Package : Packages)
	{
		PackageProxies.Add(FPackageArchiveProxy(*this, Package));
	}

	ClassProxies.Empty(Classes.Num());
	for (UClass* Class : Classes)
	{
		ClassProxies.Add(FClassArchiveProxy(*this, Class));
	}

	EnumProxies.Empty(Enums.Num());
	for (UEnum* Enum : Enums)
	{
		EnumProxies.Add(FEnumArchiveProxy(*this, Enum));
	}

	StructProxies.Empty(Structs.Num());
	for (UStruct* Struct : Structs)
	{
		StructProxies.Add(FStructArchiveProxy(*this, Struct));
	}

	ScriptStructProxies.Empty(ScriptStructs.Num());
	for (UScriptStruct* ScriptStruct : ScriptStructs)
	{
		ScriptStructProxies.Add(FScriptStructArchiveProxy(*this, ScriptStruct));
	}

	PropertyProxies.Empty(Properties.Num());
	for (UProperty* Property : Properties)
	{
		PropertyProxies.Add(FPropertyArchiveProxy(*this, Property));
	}

	BytePropertyProxies.Empty(ByteProperties.Num());
	for (UByteProperty* ByteProperty : ByteProperties)
	{
		BytePropertyProxies.Add(FBytePropertyArchiveProxy(*this, ByteProperty));
	}

	Int8PropertyProxies.Empty(Int8Properties.Num());
	for (UInt8Property* Int8Property : Int8Properties)
	{
		Int8PropertyProxies.Add(FInt8PropertyArchiveProxy(*this, Int8Property));
	}

	Int16PropertyProxies.Empty(Int16Properties.Num());
	for (UInt16Property* Int16Property : Int16Properties)
	{
		Int16PropertyProxies.Add(FInt16PropertyArchiveProxy(*this, Int16Property));
	}

	IntPropertyProxies.Empty(IntProperties.Num());
	for (UIntProperty* IntProperty : IntProperties)
	{
		IntPropertyProxies.Add(FIntPropertyArchiveProxy(*this, IntProperty));
	}

	Int64PropertyProxies.Empty(Int64Properties.Num());
	for (UInt64Property* Int64Property : Int64Properties)
	{
		Int64PropertyProxies.Add(FInt64PropertyArchiveProxy(*this, Int64Property));
	}

	UInt16PropertyProxies.Empty(UInt16Properties.Num());
	for (UUInt16Property* UInt16Property : UInt16Properties)
	{
		UInt16PropertyProxies.Add(FUInt16PropertyArchiveProxy(*this, UInt16Property));
	}

	UInt32PropertyProxies.Empty(UInt32Properties.Num());
	for (UUInt32Property* UInt32Property : UInt32Properties)
	{
		UInt32PropertyProxies.Add(FUInt32PropertyArchiveProxy(*this, UInt32Property));
	}

	UInt64PropertyProxies.Empty(UInt64Properties.Num());
	for (UUInt64Property* UInt64Property : UInt64Properties)
	{
		UInt64PropertyProxies.Add(FUInt64PropertyArchiveProxy(*this, UInt64Property));
	}

	FloatPropertyProxies.Empty(FloatProperties.Num());
	for (UFloatProperty* FloatProperty : FloatProperties)
	{
		FloatPropertyProxies.Add(FFloatPropertyArchiveProxy(*this, FloatProperty));
	}

	DoublePropertyProxies.Empty(DoubleProperties.Num());
	for (UDoubleProperty* DoubleProperty : DoubleProperties)
	{
		DoublePropertyProxies.Add(FDoublePropertyArchiveProxy(*this, DoubleProperty));
	}

	BoolPropertyProxies.Empty(BoolProperties.Num());
	for (UBoolProperty* BoolProperty : BoolProperties)
	{
		BoolPropertyProxies.Add(FBoolPropertyArchiveProxy(*this, BoolProperty));
	}

	NamePropertyProxies.Empty(NameProperties.Num());
	for (UNameProperty* NameProperty : NameProperties)
	{
		NamePropertyProxies.Add(FNamePropertyArchiveProxy(*this, NameProperty));
	}

	StrPropertyProxies.Empty(StrProperties.Num());
	for (UStrProperty* StrProperty : StrProperties)
	{
		StrPropertyProxies.Add(FStrPropertyArchiveProxy(*this, StrProperty));
	}

	TextPropertyProxies.Empty(TextProperties.Num());
	for (UTextProperty* TextProperty : TextProperties)
	{
		TextPropertyProxies.Add(FTextPropertyArchiveProxy(*this, TextProperty));
	}

	DelegatePropertyProxies.Empty(DelegateProperties.Num());
	for (UDelegateProperty* DelegateProperty : DelegateProperties)
	{
		DelegatePropertyProxies.Add(FDelegatePropertyArchiveProxy(*this, DelegateProperty));
	}

	MulticastDelegatePropertyProxies.Empty(MulticastDelegateProperties.Num());
	for (UMulticastDelegateProperty* MulticastDelegateProperty : MulticastDelegateProperties)
	{
		MulticastDelegatePropertyProxies.Add(FMulticastDelegatePropertyArchiveProxy(*this, MulticastDelegateProperty));
	}

	ObjectPropertyBaseProxies.Empty(ObjectPropertyBases.Num());
	for (UObjectPropertyBase* ObjectPropertyBase : ObjectPropertyBases)
	{
		ObjectPropertyBaseProxies.Add(FObjectPropertyBaseArchiveProxy(*this, ObjectPropertyBase));
	}

	ClassPropertyProxies.Empty(ClassProperties.Num());
	for (UClassProperty* ClassProperty : ClassProperties)
	{
		ClassPropertyProxies.Add(FClassPropertyArchiveProxy(*this, ClassProperty));
	}

	ObjectPropertyProxies.Empty(ObjectProperties.Num());
	for (UObjectProperty* ObjectProperty : ObjectProperties)
	{
		ObjectPropertyProxies.Add(FObjectPropertyArchiveProxy(*this, ObjectProperty));
	}

	WeakObjectPropertyProxies.Empty(WeakObjectProperties.Num());
	for (UWeakObjectProperty* WeakObjectProperty : WeakObjectProperties)
	{
		WeakObjectPropertyProxies.Add(FWeakObjectPropertyArchiveProxy(*this, WeakObjectProperty));
	}

	LazyObjectPropertyProxies.Empty(LazyObjectProperties.Num());
	for (ULazyObjectProperty* LazyObjectProperty : LazyObjectProperties)
	{
		LazyObjectPropertyProxies.Add(FLazyObjectPropertyArchiveProxy(*this, LazyObjectProperty));
	}

	AssetObjectPropertyProxies.Empty(AssetObjectProperties.Num());
	for (UAssetObjectProperty* AssetObjectProperty : AssetObjectProperties)
	{
		AssetObjectPropertyProxies.Add(FAssetObjectPropertyArchiveProxy(*this, AssetObjectProperty));
	}

	AssetClassPropertyProxies.Empty(AssetClassProperties.Num());
	for (UAssetClassProperty* AssetClassProperty : AssetClassProperties)
	{
		AssetClassPropertyProxies.Add(FAssetClassPropertyArchiveProxy(*this, AssetClassProperty));
	}

	InterfacePropertyProxies.Empty(InterfaceProperties.Num());
	for (UInterfaceProperty* InterfaceProperty : InterfaceProperties)
	{
		InterfacePropertyProxies.Add(FInterfacePropertyArchiveProxy(*this, InterfaceProperty));
	}

	StructPropertyProxies.Empty(StructProperties.Num());
	for (UStructProperty* StructProperty : StructProperties)
	{
		StructPropertyProxies.Add(FStructPropertyArchiveProxy(*this, StructProperty));
	}

	MapPropertyProxies.Empty(MapProperties.Num());
	for (UMapProperty* MapProperty : MapProperties)
	{
		MapPropertyProxies.Add(FMapPropertyArchiveProxy(*this, MapProperty));
	}

	SetPropertyProxies.Empty(SetProperties.Num());
	for (USetProperty* SetProperty : SetProperties)
	{
		SetPropertyProxies.Add(FSetPropertyArchiveProxy(*this, SetProperty));
	}

	ArrayPropertyProxies.Empty(ArrayProperties.Num());
	for (UArrayProperty* ArrayProperty : ArrayProperties)
	{
		ArrayPropertyProxies.Add(FArrayPropertyArchiveProxy(*this, ArrayProperty));
	}

	FunctionProxies.Empty(Functions.Num());
	for (UFunction* Function : Functions)
	{
		FunctionProxies.Add(FFunctionArchiveProxy(*this, Function));
	}

	DelegateFunctionProxies.Empty(DelegateFunctions.Num());
	for (UDelegateFunction* DelegateFunction : DelegateFunctions)
	{
		DelegateFunctionProxies.Add(FDelegateFunctionArchiveProxy(*this, DelegateFunction));
	}

	ModuleDescriptorsArchiveProxy.Empty(ModuleDescriptors.Num());
	for (auto& Kvp : ModuleDescriptors)
	{
		ModuleDescriptorsArchiveProxy.Add(TPairInitializer<FNameArchiveProxy, FUHTMakefileModuleDescriptor>(FNameArchiveProxy(*this, Kvp.Key), Kvp.Value));
	}

	GeneratedCodeCRCProxies.Empty(GeneratedCodeCRCs.Num());
	for (auto& Kvp : GeneratedCodeCRCs)
	{
		GeneratedCodeCRCProxies.Add(TPairInitializer<FSerializeIndex, uint32>(GetFieldIndex(Kvp.Key), Kvp.Value));
	}

	TypeDefinitionInfoMapArchiveProxy = FTypeDefinitionInfoMapArchiveProxy(*this, TypeDefinitionInfoMap);
	
	ModuleNamesProxy.Empty(ModuleNames.Num());
	for (FName Name : ModuleNames)
	{
		ModuleNamesProxy.Add(FNameArchiveProxy(*this, Name));
	}

	UnrealSourceFileProxies.Empty(UnrealSourceFiles.Num());
	for (FUnrealSourceFile* UnrealSourceFile : UnrealSourceFiles)
	{
		UnrealSourceFileProxies.Add(FUnrealSourceFileArchiveProxy(*this, UnrealSourceFile));
	}

	UnrealTypeDefinitionInfoProxies.Empty(UnrealTypeDefinitionInfos.Num());
	for (FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo : UnrealTypeDefinitionInfos)
	{
		UnrealTypeDefinitionInfoProxies.Add(FUnrealTypeDefinitionInfoArchiveProxy(*this, UnrealTypeDefinitionInfo));
	}

	FileScopeProxies.Empty(FileScopes.Num());
	for (FFileScope* FileScope : FileScopes)
	{
		FileScopeProxies.Add(FFileScopeArchiveProxy(*this, FileScope));
	}

	StructScopeProxies.Empty(StructScopes.Num());
	for (FStructScope* StructScope : StructScopes)
	{
		StructScopeProxies.Add(FStructScopeArchiveProxy(*this, StructScope));
	}

	ScopeProxies.Empty(Scopes.Num());
	for (FScope* Scope : Scopes)
	{
		ScopeProxies.Add(FScopeArchiveProxy(*this, Scope));
	}

	PublicClassSetEntryProxies.Empty(PublicClassSetEntries.Num());
	for (UClass* Class : PublicClassSetEntries)
	{
		PublicClassSetEntryProxies.Add(GetClassIndex(Class));
	}

	CompilerMetadataManagerArchiveProxy = FCompilerMetadataManagerArchiveProxy(*this, GScriptHelperEntries);

	MultipleInheritanceBaseClassProxies.Empty(MultipleInheritanceBaseClasses.Num());
	for (FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass : MultipleInheritanceBaseClasses)
	{
		MultipleInheritanceBaseClassProxies.Add(FMultipleInheritanceBaseClassArchiveProxy(*this, MultipleInheritanceBaseClass));
	}

	TokenDataProxies.Empty(TokenDatas.Num());
	for (TSharedPtr<FTokenData>& TokenData : TokenDatas)
	{
		TokenDataProxies.Add(FTokenDataArchiveProxy(*this, TokenData.Get()));
	}

	PropertyDataEntryProxies.Empty(PropertyDataEntries.Num());
	for (auto& PropertyDataEntry : PropertyDataEntries)
	{
		PropertyDataEntryProxies.Add(TPairInitializer<int32, int32>(GetPropertyIndex(PropertyDataEntry.Key), GetTokenIndex(&PropertyDataEntry.Value->Token)));
	}

	UnrealSourceFilesMapEntryProxies.Empty(UnrealSourceFilesMapEntries.Num());
	for (auto& Kvp : UnrealSourceFilesMapEntries)
	{
		UnrealSourceFilesMapEntryProxies.Add(TPairInitializer<FString, int32>(Kvp.Key, GetUnrealSourceFileIndex(Kvp.Value)));
	}

	ClassMetaDataArchiveProxies.Empty(ClassMetaDatas.Num());
	for (const TScopedPointer<FClassMetaData>& MetaData : ClassMetaDatas)
	{
		ClassMetaDataArchiveProxies.Add(FClassMetaDataArchiveProxy(*this, MetaData.GetOwnedPointer()));
	}

	TokenProxies.Empty(Tokens.Num());
	for (FToken* Token : Tokens)
	{
		TokenProxies.Add(FTokenArchiveProxy(*this, Token));
	}

	PropertyBaseProxies.Empty(PropertyBases.Num());
	for (FPropertyBase* PropertyBase : PropertyBases)
	{
		PropertyBaseProxies.Add(FPropertyBaseArchiveProxy(*this, PropertyBase));
	}

	EnumUnderlyingTypeProxies.Empty(EnumUnderlyingTypes.Num());
	for (auto& Kvp : EnumUnderlyingTypes)
	{
		EnumUnderlyingTypeProxies.Add(TPairInitializer<int32, EPropertyType>(GetEnumIndex(Kvp.Key), Kvp.Value));
	}

	StructNameMapEntryProxies.Empty(StructNameMapEntries.Num());
	for (auto& Kvp : StructNameMapEntries)
	{
		StructNameMapEntryProxies.Add(TPairInitializer<FSerializeIndex, FString>(GetStructIndex(Kvp.Key), Kvp.Value));
	}

	InterfaceAllocationProxies.Empty(InterfaceAllocations.Num());
	for (TCHAR* Entry : InterfaceAllocations)
	{
		InterfaceAllocationProxies.Add(Entry);
	}
}

int32 FUHTMakefile::GetPropertyCount() const
{
	return Properties.Num()
		+ ByteProperties.Num()
		+ Int8Properties.Num()
		+ Int16Properties.Num()
		+ IntProperties.Num()
		+ Int64Properties.Num()
		+ UInt16Properties.Num()
		+ UInt32Properties.Num()
		+ UInt64Properties.Num()
		+ FloatProperties.Num()
		+ DoubleProperties.Num()
		+ BoolProperties.Num()
		+ NameProperties.Num()
		+ StrProperties.Num()
		+ TextProperties.Num()
		+ DelegateProperties.Num()
		+ MulticastDelegateProperties.Num()
		+ ObjectPropertyBases.Num()
		+ ClassProperties.Num()
		+ ObjectProperties.Num()
		+ WeakObjectProperties.Num()
		+ LazyObjectProperties.Num()
		+ AssetObjectProperties.Num()
		+ AssetClassProperties.Num()
		+ InterfaceProperties.Num()
		+ StructProperties.Num()
		+ MapProperties.Num()
		+ SetProperties.Num()
		+ ArrayProperties.Num();
}

int32 FUHTMakefile::GetPropertyProxiesCount() const
{
	return Properties.Num()
		+ BytePropertyProxies.Num()
		+ Int8PropertyProxies.Num()
		+ Int16PropertyProxies.Num()
		+ IntPropertyProxies.Num()
		+ Int64PropertyProxies.Num()
		+ UInt16PropertyProxies.Num()
		+ UInt32PropertyProxies.Num()
		+ UInt64PropertyProxies.Num()
		+ FloatPropertyProxies.Num()
		+ DoublePropertyProxies.Num()
		+ BoolPropertyProxies.Num()
		+ NamePropertyProxies.Num()
		+ StrPropertyProxies.Num()
		+ TextPropertyProxies.Num()
		+ DelegatePropertyProxies.Num()
		+ MulticastDelegatePropertyProxies.Num()
		+ ObjectPropertyBaseProxies.Num()
		+ ClassPropertyProxies.Num()
		+ ObjectPropertyProxies.Num()
		+ WeakObjectPropertyProxies.Num()
		+ LazyObjectPropertyProxies.Num()
		+ AssetObjectPropertyProxies.Num()
		+ AssetClassPropertyProxies.Num()
		+ InterfacePropertyProxies.Num()
		+ StructPropertyProxies.Num()
		+ MapPropertyProxies.Num()
		+ SetPropertyProxies.Num()
		+ ArrayPropertyProxies.Num();
}

int32 FUHTMakefile::GetFunctionCount() const
{
	return Functions.Num() + DelegateFunctions.Num();
}

int32 FUHTMakefile::GetFunctionProxiesCount() const
{
	return FunctionProxies.Num() + DelegateFunctionProxies.Num();
}

int32 FUHTMakefile::GetStructsCount() const
{
	return Structs.Num() + ScriptStructs.Num() + Classes.Num() + GetFunctionCount();
}

int32 FUHTMakefile::GetStructProxiesCount() const
{
	return StructProxies.Num() + ScriptStructProxies.Num() + ClassProxies.Num() + GetFunctionProxiesCount();
}

void FUHTMakefile::UpdateModulesCompatibility(FManifest* InManifest)
{
	TArray<FManifestModule>& ManifestModules = Manifest->Modules;
	TArray<FManifestModule>& InManifestModules = InManifest->Modules;
	int32 ManifestModulesNum = ManifestModules.Num();
	int32 InManifestModulesNum = InManifestModules.Num();
	int32 MinModuleIndex = FMath::Min(ManifestModulesNum, InManifestModulesNum);
	bool bForceIncompatibility = false;
	for (int32 i = 0; i < MinModuleIndex; ++i)
	{
		FManifestModule& InModule = InManifestModules[i];
		FManifestModule& Module = ManifestModules[i];
		if (bForceIncompatibility)
		{
			InModule.ForceRegeneration();
		}

		if (!Module.IsCompatibleWith(InModule))
		{
			InModule.ForceRegeneration();
			bForceIncompatibility = true;
		}
	}

	if (bForceIncompatibility)
	{
		for (int32 i = MinModuleIndex; i < ManifestModulesNum; ++i)
		{
			ManifestModules[i].ForceRegeneration();
		}

		for (int32 i = MinModuleIndex; i < InManifestModulesNum; ++i)
		{
			InManifestModules[i].ForceRegeneration();
		}
	}
}

void FUHTMakefile::SaveToFile(const TCHAR* MakefilePath)
{
	FArchive* UHTMakefileArchive = IFileManager::Get().CreateFileWriter(MakefilePath);
	if (!UHTMakefileArchive)
	{
		return;
	}

	ON_SCOPE_EXIT
	{
		UHTMakefileArchive->Close();
	};

	double UHTMakefileLoadTime = 0.0;
	FDurationTimer UHTMakefileLoadTimer(UHTMakefileLoadTime);
	UHTMakefileLoadTimer.Start();
	*UHTMakefileArchive << *this;
	UHTMakefileLoadTimer.Stop();
	UE_LOG(LogCompile, Log, TEXT("Saving UHT makefile took %f seconds"), UHTMakefileLoadTime);
}

void FUHTMakefile::LoadFromFile(const TCHAR* MakefilePath, FManifest* InManifest)
{
	FArchive* UHTMakefileArchive = IFileManager::Get().CreateFileReader(MakefilePath);
	if (!UHTMakefileArchive)
	{
		return;
	}

	ON_SCOPE_EXIT
	{
		UHTMakefileArchive->Close();
	};

	double UHTMakefileLoadTime = 0.0;
	FDurationTimer UHTMakefileLoadTimer(UHTMakefileLoadTime);
	UHTMakefileLoadTimer.Start();
	*UHTMakefileArchive << *this;
	UHTMakefileLoadTimer.Stop();
	UE_LOG(LogCompile, Log, TEXT("Loading UHT makefile took %f seconds"), UHTMakefileLoadTime);

	UpdateModulesCompatibility(InManifest);
}

bool FUHTMakefile::CanLoadModule(const FManifestModule& ManifestModule)
{
	FName ModuleName = FName(*ManifestModule.Name);
	bool bNeedsRegeneration = ManifestModule.NeedsRegeneration();
	bool bUHTMakefileContainsModuleData = HasModule(ModuleName);
	bool bForceRegeneration = ShouldForceRegeneration();
	return !bNeedsRegeneration
		&& !bForceRegeneration
		&& bUHTMakefileContainsModuleData;
}

void FUHTMakefile::ResolveTypeFullyCreatedDuringPreload(int32 Index)
{
	auto& Kvp = InitializationOrder[+EUHTMakefileLoadingPhase::Preload][Index];
	if (IsTypeFullyCreatedDuringPreload(Kvp.Key))
	{
		ResolveObject(Kvp.Key, Kvp.Value);
	}
}

void FUHTMakefile::MoveNewObjects()
{
	Packages.Append(NewPackages);
	NewPackages.Empty();
	UnrealSourceFiles.Append(NewUnrealSourceFiles);
	NewUnrealSourceFiles.Empty();
	FileScopes.Append(NewFileScopes);
	NewFileScopes.Empty();
	StructScopes.Append(NewStructScopes);
	NewStructScopes.Empty();
	UnrealTypeDefinitionInfos.Append(NewUnrealTypeDefinitionInfos);
	NewUnrealTypeDefinitionInfos.Empty();
	TypeDefinitionInfoMap.Append(NewTypeDefinitionInfoMap);
	NewTypeDefinitionInfoMap.Empty();
	Classes.Append(NewClasses);
	NewClasses.Empty();
	UnrealSourceFilesMapEntries.Append(NewUnrealSourceFilesMapEntries);
	NewUnrealSourceFilesMapEntries.Empty();
	PublicClassSetEntries.Append(NewPublicClassSetEntries);
	NewPublicClassSetEntries.Empty();
	InterfaceAllocations.Append(NewInterfaceAllocations);
	NewInterfaceAllocations.Empty();
	StructNameMapEntries.Append(NewStructNameMapEntries);
	NewStructNameMapEntries.Empty();

	bShouldMoveNewObjects = false;
}

int32 FUHTMakefile::GetPackageIndex(UPackage* Package) const
{
	if (!Package)
	{
		return INDEX_NONE;
	}

	return PackageIndexes.FindChecked(Package);
}

UPackage* FUHTMakefile::GetPackageByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	if (!(Packages.IsValidIndex(Index) && PackageProxies.IsValidIndex(Index)))
	{
		UE_LOG(LogCompile, Error, TEXT("Index: %d Packages.Num(): %d PackageProxies.Num(): %d"), Index, Packages.Num(), PackageProxies.Num());
	}
	check(Packages.IsValidIndex(Index) && PackageProxies.IsValidIndex(Index));
	return Packages[Index];
}

int32 FUHTMakefile::GetTokenIndex(const FToken* Token) const
{
	if (!Token)
	{
		return INDEX_NONE;
	}

	return TokenIndexes.FindChecked(Token);
}

const FToken* FUHTMakefile::GetTokenByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	check(Tokens.IsValidIndex(Index));
	return Tokens[Index];
}

void FUHTMakefile::CreateObjectAtInitOrderIndex(int32 Index, EUHTMakefileLoadingPhase UHTMakefileLoadingPhase)
{
	auto& Kvp = InitializationOrder[+UHTMakefileLoadingPhase][Index];
	CreateObject(Kvp.Key, Kvp.Value);
}

void FUHTMakefile::ResolveObjectAtInitOrderIndex(int32 Index, EUHTMakefileLoadingPhase UHTMakefileLoadingPhase)
{
	auto& Kvp = InitializationOrder[+UHTMakefileLoadingPhase][Index];

	// Make sure types are not resolved twice.
	if (IsTypeFullyCreatedDuringPreload(Kvp.Key)
		&& (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Preload))
	{
		return;
	}

	ResolveObject(Kvp.Key, Kvp.Value);
}

void FUHTMakefile::CreateObject(ESerializedObjectType ObjectType, int32 Index)
{
	switch (ObjectType)
	{
	case ESerializedObjectType::EPropertyBase:
		CreatePropertyBase(Index);
		break;
	case ESerializedObjectType::EPackage:
		CreatePackage(Index);
		break;
	case ESerializedObjectType::EGeneratedCodeCRC:
		CreateGeneratedCodeCRC(Index);
		break;
	case ESerializedObjectType::EClass:
		CreateClass(Index);
		break;
	case ESerializedObjectType::EEnum:
		CreateEnum(Index);
		break;
	case ESerializedObjectType::EStruct:
		CreateStruct(Index);
		break;
	case ESerializedObjectType::EScriptStruct:
		CreateScriptStruct(Index);
		break;
	case ESerializedObjectType::EProperty:
		CreateProperty(Index);
		break;
	case ESerializedObjectType::EByteProperty:
		CreateByteProperty(Index);
		break;
	case ESerializedObjectType::EInt8Property:
		CreateInt8Property(Index);
		break;
	case ESerializedObjectType::EInt16Property:
		CreateInt16Property(Index);
		break;
	case ESerializedObjectType::EIntProperty:
		CreateIntProperty(Index);
		break;
	case ESerializedObjectType::EInt64Property:
		CreateInt64Property(Index);
		break;
	case ESerializedObjectType::EUInt16Property:
		CreateUInt16Property(Index);
		break;
	case ESerializedObjectType::EUInt32Property:
		CreateUInt32Property(Index);
		break;
	case ESerializedObjectType::EUInt64Property:
		CreateUInt64Property(Index);
		break;
	case ESerializedObjectType::EFloatProperty:
		CreateFloatProperty(Index);
		break;
	case ESerializedObjectType::EDoubleProperty:
		CreateDoubleProperty(Index);
		break;
	case ESerializedObjectType::EBoolProperty:
		CreateBoolProperty(Index);
		break;
	case ESerializedObjectType::ENameProperty:
		CreateNameProperty(Index);
		break;
	case ESerializedObjectType::EStrProperty:
		CreateStrProperty(Index);
		break;
	case ESerializedObjectType::ETextProperty:
		CreateTextProperty(Index);
		break;
	case ESerializedObjectType::EDelegateProperty:
		CreateDelegateProperty(Index);
		break;
	case ESerializedObjectType::EMulticastDelegateProperty:
		CreateMulticastDelegateProperty(Index);
		break;
	case ESerializedObjectType::EObjectPropertyBase:
		CreateObjectPropertyBase(Index);
		break;
	case ESerializedObjectType::EClassProperty:
		CreateClassProperty(Index);
		break;
	case ESerializedObjectType::EObjectProperty:
		CreateObjectProperty(Index);
		break;
	case ESerializedObjectType::EWeakObjectProperty:
		CreateWeakObjectProperty(Index);
		break;
	case ESerializedObjectType::ELazyObjectProperty:
		CreateLazyObjectProperty(Index);
		break;
	case ESerializedObjectType::EAssetObjectProperty:
		CreateAssetObjectProperty(Index);
		break;
	case ESerializedObjectType::EAssetClassProperty:
		CreateAssetClassProperty(Index);
		break;
	case ESerializedObjectType::EInterfaceProperty:
		CreateInterfaceProperty(Index);
		break;
	case ESerializedObjectType::EStructProperty:
		CreateStructProperty(Index);
		break;
	case ESerializedObjectType::EMapProperty:
		CreateMapProperty(Index);
		break;
	case ESerializedObjectType::ESetProperty:
		CreateSetProperty(Index);
		break;
	case ESerializedObjectType::EArrayProperty:
		CreateArrayProperty(Index);
		break;
	case ESerializedObjectType::EFunction:
		CreateFunction(Index);
		break;
	case ESerializedObjectType::EDelegateFunction:
		CreateDelegateFunction(Index);
		break;
	case ESerializedObjectType::EFileScope:
		// Intentionally empty, file scopes are created with UnrealSourceFiles
	//	CreateFileScope(Index);
		break;
	case ESerializedObjectType::EStructScope:
		CreateStructScope(Index);
		break;
	case ESerializedObjectType::EUnrealTypeDefinitionInfo:
		CreateUnrealTypeDefinitionInfo(Index);
		break;
	case ESerializedObjectType::ETypeDefinitionInfoMapEntry:
		CreateTypeDefinitionInfoMapEntry(Index);
		break;
	case ESerializedObjectType::EUnrealSourceFile:
		CreateUnrealSourceFile(Index);
		break;
	case ESerializedObjectType::EMultipleInheritanceBaseClass:
		CreateMultipleInheritanceBaseClass(Index);
		break;
	case ESerializedObjectType::EUnrealSourceFilesMapEntry:
		CreateUnrealSourceFilesMapEntry(Index);
		break;
	case ESerializedObjectType::EPublicClassSetEntry:
		// Intentionally empty.
		break;
	case ESerializedObjectType::EClassMetaData:
		CreateClassMetaData(Index);
		break;
	case ESerializedObjectType::EGScriptHelperEntry:
		// Intentionally empty
		break;
	case ESerializedObjectType::EToken:
		CreateToken(Index);
		break;
	case ESerializedObjectType::EPropertyDataEntry:
		CreatePropertyDataEntry(Index);
		break;
	case ESerializedObjectType::EEnumUnderlyingType:
		CreateGEnumUnderlyingType(Index);
		break;
	case ESerializedObjectType::EStructNameMapEntry:
		CreateStructNameMapEntry(Index);
		break;
	case ESerializedObjectType::EInterfaceAllocation:
		CreateInterfaceAllocation(Index);
		break;
	default:
		check(0);
		break;
	}
}

void FUHTMakefile::ResolveObject(ESerializedObjectType ObjectType, int32 Index)
{
	switch (ObjectType)
	{
	case ESerializedObjectType::EPropertyBase:
		ResolvePropertyBase(Index);
		break;
	case ESerializedObjectType::EPackage:
		ResolvePackage(Index);
		break;
	case ESerializedObjectType::EClass:
		ResolveClass(Index);
		break;
	case ESerializedObjectType::EEnum:
		ResolveEnum(Index);
		break;
	case ESerializedObjectType::EStruct:
		ResolveStruct(Index);
		break;
	case ESerializedObjectType::EScriptStruct:
		ResolveScriptStruct(Index);
		break;
	case ESerializedObjectType::EProperty:
		ResolveProperty(Index);
		break;
	case ESerializedObjectType::EByteProperty:
		ResolveByteProperty(Index);
		break;
	case ESerializedObjectType::EInt8Property:
		ResolveInt8Property(Index);
		break;
	case ESerializedObjectType::EInt16Property:
		ResolveInt16Property(Index);
		break;
	case ESerializedObjectType::EIntProperty:
		ResolveIntProperty(Index);
		break;
	case ESerializedObjectType::EInt64Property:
		ResolveInt64Property(Index);
		break;
	case ESerializedObjectType::EUInt16Property:
		ResolveUInt16Property(Index);
		break;
	case ESerializedObjectType::EUInt32Property:
		ResolveUInt32Property(Index);
		break;
	case ESerializedObjectType::EUInt64Property:
		ResolveUInt64Property(Index);
		break;
	case ESerializedObjectType::EFloatProperty:
		ResolveFloatProperty(Index);
		break;
	case ESerializedObjectType::EDoubleProperty:
		ResolveDoubleProperty(Index);
		break;
	case ESerializedObjectType::EBoolProperty:
		ResolveBoolProperty(Index);
		break;
	case ESerializedObjectType::ENameProperty:
		ResolveNameProperty(Index);
		break;
	case ESerializedObjectType::EStrProperty:
		ResolveStrProperty(Index);
		break;
	case ESerializedObjectType::ETextProperty:
		ResolveTextProperty(Index);
		break;
	case ESerializedObjectType::EDelegateProperty:
		ResolveDelegateProperty(Index);
		break;
	case ESerializedObjectType::EMulticastDelegateProperty:
		ResolveMulticastDelegateProperty(Index);
		break;
	case ESerializedObjectType::EObjectPropertyBase:
		ResolveObjectPropertyBase(Index);
		break;
	case ESerializedObjectType::EClassProperty:
		ResolveClassProperty(Index);
		break;
	case ESerializedObjectType::EObjectProperty:
		ResolveObjectProperty(Index);
		break;
	case ESerializedObjectType::EWeakObjectProperty:
		ResolveWeakObjectProperty(Index);
		break;
	case ESerializedObjectType::ELazyObjectProperty:
		ResolveLazyObjectProperty(Index);
		break;
	case ESerializedObjectType::EAssetObjectProperty:
		ResolveAssetObjectProperty(Index);
		break;
	case ESerializedObjectType::EAssetClassProperty:
		ResolveAssetClassProperty(Index);
		break;
	case ESerializedObjectType::EInterfaceProperty:
		ResolveInterfaceProperty(Index);
		break;
	case ESerializedObjectType::EStructProperty:
		ResolveStructProperty(Index);
		break;
	case ESerializedObjectType::EMapProperty:
		ResolveMapProperty(Index);
		break;
	case ESerializedObjectType::ESetProperty:
		ResolveSetProperty(Index);
		break;
	case ESerializedObjectType::EArrayProperty:
		ResolveArrayProperty(Index);
		break;
	case ESerializedObjectType::EFunction:
		ResolveFunction(Index);
		break;
	case ESerializedObjectType::EDelegateFunction:
		ResolveDelegateFunction(Index);
		break;
	case ESerializedObjectType::EFileScope:
		ResolveFileScope(Index);
		break;
	case ESerializedObjectType::EStructScope:
		ResolveStructScope(Index);
		break;
	case ESerializedObjectType::EScope:
		ResolveScope(Index);
		break;
	case ESerializedObjectType::EUnrealTypeDefinitionInfo:
		ResolveUnrealTypeDefinitionInfo(Index);
		break;
	case ESerializedObjectType::ETypeDefinitionInfoMapEntry:
		ResolveTypeDefinitionInfoMapEntry(Index);
		break;
	case ESerializedObjectType::EUnrealSourceFile:
		ResolveUnrealSourceFile(Index);
		break;
	case ESerializedObjectType::EGeneratedCodeCRC:
		ResolveGeneratedCodeCRC(Index);
		break;
	case ESerializedObjectType::EGScriptHelperEntry:
		ResolveGScriptHelperEntry(Index);
		break;
	case ESerializedObjectType::EMultipleInheritanceBaseClass:
		ResolveMultipleInheritanceBaseClass(Index);
		break;
	case ESerializedObjectType::EPublicClassSetEntry:
		ResolvePublicClassSetEntry(Index);
		break;
	case ESerializedObjectType::EClassMetaData:
		ResolveClassMetaData(Index);
		break;
	case ESerializedObjectType::EToken:
		ResolveToken(Index);
		break;
	case ESerializedObjectType::EPropertyDataEntry:
		ResolvePropertyDataEntry(Index);
		break;
	case ESerializedObjectType::EUnrealSourceFilesMapEntry:
		// Intentionally empty.
		break;
	case ESerializedObjectType::EEnumUnderlyingType:
		ResolveGEnumUnderlyingType(Index);
		break;
	case ESerializedObjectType::EStructNameMapEntry:
		ResolveStructNameMapEntry(Index);
		break;
	case ESerializedObjectType::EInterfaceAllocation:
		// Intentionally empty.
		break;
	default:
		check(0);
		break;
	}
}

int32 FUHTMakefile::GetUnrealSourceFileIndex(const FUnrealSourceFile* UnrealSourceFile) const
{
	if (!UnrealSourceFile)
	{
		return INDEX_NONE;
	}

	const int32* IndexPtr = UnrealSourceFileIndexes.Find(UnrealSourceFile);
	int32 Result = 0;
	if (IndexPtr)
	{
		return *IndexPtr;
	}
	Result += UnrealSourceFiles.Num();

	int32 Index = NewUnrealSourceFiles.IndexOfByKey(UnrealSourceFile);
	if (Index != INDEX_NONE)
	{
		return Result + Index;
	}

	check(0);
	return INDEX_NONE;
}

FUnrealSourceFile* FUHTMakefile::GetUnrealSourceFileByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	check(UnrealSourceFiles.IsValidIndex(Index) && UnrealSourceFileProxies.IsValidIndex(Index));
	return UnrealSourceFiles[Index];
}

int32 FUHTMakefile::GetNameIndex(FName Name) const
{
	return NameIndices.FindChecked(Name);
}

FName FUHTMakefile::GetNameByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return NAME_None;
	}

	check(ReferencedNames.IsValidIndex(Index));
	return ReferencedNames[Index];
}

FSerializeIndex FUHTMakefile::GetObjectIndex(const UObject* Object) const
{
	if (!Object)
	{
		return FSerializeIndex();
	}

	FSerializeIndex Result = FSerializeIndex(0);

	FSerializeIndex Index = FSerializeIndex(Packages.IndexOfByKey(Object));
	if (Index.Index != INDEX_NONE)
	{
		return Result + Index;
	}
	Result += Packages.Num();

	Index = GetFieldIndex(Cast<UField>(Object));
	if (Index.Index != INDEX_NONE)
	{
		if (Index.Index == IndexOfNativeClass)
		{
			return Index;
		}
		return Result + Index;
	}

	check(0);
	return FSerializeIndex();
}

UObject* FUHTMakefile::GetObjectByIndex(FSerializeIndex Index) const
{
	if (Index.IsNone())
	{
		return nullptr;
	}

	if (Index.HasName())
	{
		return FindObjectFast<UClass>(nullptr, Index.NameProxy.CreateName(*this), true, true);
	}

	if (PackageProxies.IsValidIndex(Index.Index))
	{
		return Packages[Index.Index];
	}
	Index -= PackageProxies.Num();

	return GetFieldByIndex(Index);
}

void FUHTMakefile::LoadModuleData(FName ModuleName, const FManifestModule& ManifestModule)
{
	if (IsPreloading())
	{
		if (ModuleDescriptors.Num() == 0)
		{
			for (auto& Kvp : ModuleDescriptorsArchiveProxy)
			{
				ModuleDescriptors.Add(Kvp.Key.CreateName(*this), Kvp.Value);
			}
		}

		FUHTMakefileModuleDescriptor& UHTMakefileModuleDescriptor = ModuleDescriptors[ModuleName];
		UHTMakefileModuleDescriptor.SetMakefile(this);
	}
	ModuleDescriptors[ModuleName].LoadModuleData(ManifestModule, LoadingPhase);
}

void FUHTMakefile::ResolvePublicClassSetEntry(int32 Index)
{
	UClass* Class = GetClassByIndex(PublicClassSetEntryProxies[Index]);
	PublicClassSetEntries.Add(Class);
	GPublicClassSet.Add(Class);
}

void FUHTMakefile::CreateTypeDefinitionInfoMapEntry(int32 Index)
{
	TypeDefinitionInfoMap.Add(TPairInitializer<UField*, FUnrealTypeDefinitionInfo*>(nullptr, nullptr));
}

void FUHTMakefile::ResolveTypeDefinitionInfoMapEntry(int32 Index)
{
	TypeDefinitionInfoMapArchiveProxy.ResolveIndex(*this, Index);
	auto& Kvp = TypeDefinitionInfoMap[Index];
	GTypeDefinitionInfoMap.Add(Kvp.Key, Kvp.Value->AsShared());
}

void FUHTMakefile::ResolveTypeDefinitionInfoMapEntryPreload(int32 Index)
{
	TypeDefinitionInfoMapArchiveProxy.ResolveClassIndex(*this, Index);
	auto& Kvp = TypeDefinitionInfoMap[Index];
	GTypeDefinitionInfoMap.Add(Kvp.Key, Kvp.Value->AsShared());
}


int32 FUHTMakefile::GetPropertyDataEntryIndex(const TPair<UProperty*, TSharedPtr<FTokenData>>* Entry) const
{
	if (!Entry)
	{
		return INDEX_NONE;
	}

	int32 Result = PropertyDataEntries.IndexOfByPredicate([Entry](const TPair<UProperty*, TSharedPtr<FTokenData>>& Kvp) { return Kvp.Key == Entry->Key; });
	check(Result != INDEX_NONE);
	return Result;
}
const TPair<UProperty*, TSharedPtr<FTokenData>>* FUHTMakefile::GetPropertyDataEntryByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	check(PropertyDataEntries.IsValidIndex(Index));
	return &PropertyDataEntries[Index];
}

FSerializeIndex FUHTMakefile::GetFieldIndex(const UField* Field) const
{
	if (!Field)
	{
		return FSerializeIndex();
	}

	FSerializeIndex Result = FSerializeIndex(0);
	FSerializeIndex SerializeIndex = GetStructIndex(Cast<UStruct>(Field));
	if (SerializeIndex.Index != INDEX_NONE)
	{
		if (SerializeIndex.Index == IndexOfNativeClass)
		{
			return SerializeIndex;
		}
		return Result + SerializeIndex;
	}
	Result += GetStructsCount();

	int32 Index = GetPropertyIndex(Cast<const UProperty>(Field));
	if (Index != INDEX_NONE)
	{
		return Result + Index;
	}
	Result += GetPropertyCount();

	Index = EnumIndexes.FindChecked(Cast<const UEnum>(Field));
	if (Index != INDEX_NONE)
	{
		return Result + Index;
	}

	check(0);
	return FSerializeIndex();
}

UField* FUHTMakefile::GetFieldByIndex(FSerializeIndex Index) const
{
	if (Index.IsNone())
	{
		return nullptr;
	}

	if (Index.HasName())
	{
		return FindObjectFast<UClass>(nullptr, Index.NameProxy.CreateName(*this), true, true);
	}

	UField* Result = GetStructByIndex(Index);
	if (Result)
	{
		return Result;
	}
	Index -= GetStructProxiesCount();

	Result = GetPropertyByIndex(Index.Index);
	if (Result)
	{
		return Result;
	}
	Index -= GetPropertyProxiesCount();

	if (Enums.IsValidIndex(Index.Index))
	{
		return Enums[Index.Index];
	}

	return nullptr;
}

int32 FUHTMakefile::GetEnumIndex(const UEnum* Enum) const
{
	if (!Enum)
	{
		return INDEX_NONE;
	}

	return EnumIndexes.FindChecked(Enum);
}

UEnum* FUHTMakefile::GetEnumByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	if (Enums.IsValidIndex(Index))
	{
		return Enums[Index];
	}

	return nullptr;
}

FSerializeIndex FUHTMakefile::GetStructIndex(const UStruct* Struct) const
{
	if (!Struct)
	{
		return FSerializeIndex();
	}

	FSerializeIndex Result = FSerializeIndex(0);

	const int32* IndexPtr = StructIndexes.Find(Struct);
	if (IndexPtr)
	{
		return Result + *IndexPtr;
	}
	Result += Structs.Num();

	FSerializeIndex Index = GetScriptStructIndex(Cast<UScriptStruct>(Struct));
	if (Index != INDEX_NONE)
	{
		if (Index == IndexOfNativeClass)
		{
			return Index;
		}
		return Result + Index;
	}
	Result += ScriptStructs.Num();

	Index = GetClassIndex(Cast<UClass>(Struct));
	if (Index != INDEX_NONE)
	{
		if (Index == IndexOfNativeClass)
		{
			return Index;
		}
		return Result + Index;
	}
	Result += Classes.Num();

	Index = FSerializeIndex(GetFunctionIndex(Cast<const UFunction>(Struct)));
	if (Index != INDEX_NONE)
	{
		return Result + Index;
	}

	check(0);
	return FSerializeIndex();
}

UStruct* FUHTMakefile::GetStructByIndex(FSerializeIndex Index) const
{
	if (Index.IsNone())
	{
		return nullptr;
	}

	if (Index.HasName())
	{
		return FindObjectFast<UClass>(nullptr, Index.NameProxy.CreateName(*this), true, true);
	}

	if (Structs.IsValidIndex(Index.Index) && StructProxies.IsValidIndex(Index.Index))
	{
		return Structs[Index.Index];
	}
	Index -= StructProxies.Num();

	if (ScriptStructs.IsValidIndex(Index.Index) && ScriptStructProxies.IsValidIndex(Index.Index))
	{
		return ScriptStructs[Index.Index];
	}
	Index -= ScriptStructProxies.Num();

	if (Classes.IsValidIndex(Index.Index) && ClassProxies.IsValidIndex(Index.Index))
	{
		return Classes[Index.Index];
	}
	Index -= ClassProxies.Num();

	return GetFunctionByIndex(Index.Index);
}

int32 FUHTMakefile::GetPropertyIndex(const UProperty* Property) const
{
	if (!Property)
	{
		return INDEX_NONE;
	}
	int32 Result = 0;
	const int32* CurrentIndex = PropertyIndexes.Find(Property);
	if (CurrentIndex)
	{
		return *CurrentIndex;
	}
	Result += Properties.Num();

	CurrentIndex = BytePropertyIndexes.Find(Cast<UByteProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += ByteProperties.Num();

	CurrentIndex = Int8PropertyIndexes.Find(Cast<UInt8Property>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += Int8Properties.Num();

	CurrentIndex = Int16PropertyIndexes.Find(Cast<UInt16Property>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += Int16Properties.Num();

	CurrentIndex = IntPropertyIndexes.Find(Cast<UIntProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += IntProperties.Num();

	CurrentIndex = Int64PropertyIndexes.Find(Cast<UInt64Property>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += Int64Properties.Num();

	CurrentIndex = UInt16PropertyIndexes.Find(Cast<UUInt16Property>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += UInt16Properties.Num();

	CurrentIndex = UInt32PropertyIndexes.Find(Cast<UUInt32Property>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += UInt32Properties.Num();

	CurrentIndex = UInt64PropertyIndexes.Find(Cast<UUInt64Property>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += UInt64Properties.Num();

	CurrentIndex = FloatPropertyIndexes.Find(Cast<UFloatProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += FloatProperties.Num();

	CurrentIndex = DoublePropertyIndexes.Find(Cast<UDoubleProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += DoubleProperties.Num();

	CurrentIndex = BoolPropertyIndexes.Find(Cast<UBoolProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += BoolProperties.Num();

	CurrentIndex = NamePropertyIndexes.Find(Cast<UNameProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += NameProperties.Num();

	CurrentIndex = StrPropertyIndexes.Find(Cast<UStrProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += StrProperties.Num();

	CurrentIndex = TextPropertyIndexes.Find(Cast<UTextProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += TextProperties.Num();

	CurrentIndex = DelegatePropertyIndexes.Find(Cast<UDelegateProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += DelegateProperties.Num();

	CurrentIndex = MulticastDelegatePropertyIndexes.Find(Cast<UMulticastDelegateProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += MulticastDelegateProperties.Num();

	CurrentIndex = ObjectPropertyBaseIndexes.Find(Cast<UObjectPropertyBase>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += ObjectPropertyBases.Num();

	CurrentIndex = ClassPropertyIndexes.Find(Cast<UClassProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += ClassProperties.Num();

	CurrentIndex = ObjectPropertyIndexes.Find(Cast<UObjectProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += ObjectProperties.Num();

	CurrentIndex = WeakObjectPropertyIndexes.Find(Cast<UWeakObjectProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += WeakObjectProperties.Num();

	CurrentIndex = LazyObjectPropertyIndexes.Find(Cast<ULazyObjectProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += LazyObjectProperties.Num();

	CurrentIndex = AssetObjectPropertyIndexes.Find(Cast<UAssetObjectProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += AssetObjectProperties.Num();

	CurrentIndex = AssetClassPropertyIndexes.Find(Cast<UAssetClassProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += AssetClassProperties.Num();

	CurrentIndex = InterfacePropertyIndexes.Find(Cast<UInterfaceProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += InterfaceProperties.Num();

	CurrentIndex = StructPropertyIndexes.Find(Cast<UStructProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += StructProperties.Num();

	CurrentIndex = MapPropertyIndexes.Find(Cast<UMapProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += MapProperties.Num();

	CurrentIndex = SetPropertyIndexes.Find(Cast<USetProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}
	Result += SetProperties.Num();

	CurrentIndex = ArrayPropertyIndexes.Find(Cast<UArrayProperty>(Property));
	if (CurrentIndex)
	{
		return Result + *CurrentIndex;
	}

	check(0);
	return INDEX_NONE;
}

UProperty* FUHTMakefile::GetPropertyByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	if (Properties.IsValidIndex(Index) && PropertyProxies.IsValidIndex(Index))
	{
		return Properties[Index];
	}
	Index -= PropertyProxies.Num();

	if (ByteProperties.IsValidIndex(Index) && BytePropertyProxies.IsValidIndex(Index))
	{
		return ByteProperties[Index];
	}
	Index -= BytePropertyProxies.Num();

	if (Int8Properties.IsValidIndex(Index) && Int8PropertyProxies.IsValidIndex(Index))
	{
		return Int8Properties[Index];
	}
	Index -= Int8PropertyProxies.Num();

	if (Int16Properties.IsValidIndex(Index) && Int16PropertyProxies.IsValidIndex(Index))
	{
		return Int16Properties[Index];
	}
	Index -= Int16PropertyProxies.Num();

	if (IntProperties.IsValidIndex(Index) && IntPropertyProxies.IsValidIndex(Index))
	{
		return IntProperties[Index];
	}
	Index -= IntPropertyProxies.Num();

	if (Int64Properties.IsValidIndex(Index) && Int64PropertyProxies.IsValidIndex(Index))
	{
		return Int64Properties[Index];
	}
	Index -= Int64PropertyProxies.Num();

	if (UInt16Properties.IsValidIndex(Index) && UInt16PropertyProxies.IsValidIndex(Index))
	{
		return UInt16Properties[Index];
	}
	Index -= UInt16PropertyProxies.Num();

	if (UInt32Properties.IsValidIndex(Index) && UInt32PropertyProxies.IsValidIndex(Index))
	{
		return UInt32Properties[Index];
	}
	Index -= UInt32PropertyProxies.Num();

	if (UInt64Properties.IsValidIndex(Index) && UInt64PropertyProxies.IsValidIndex(Index))
	{
		return UInt64Properties[Index];
	}
	Index -= UInt64PropertyProxies.Num();

	if (FloatProperties.IsValidIndex(Index) && FloatPropertyProxies.IsValidIndex(Index))
	{
		return FloatProperties[Index];
	}
	Index -= FloatPropertyProxies.Num();

	if (DoubleProperties.IsValidIndex(Index) && DoublePropertyProxies.IsValidIndex(Index))
	{
		return DoubleProperties[Index];
	}
	Index -= DoublePropertyProxies.Num();

	if (BoolProperties.IsValidIndex(Index) && BoolPropertyProxies.IsValidIndex(Index))
	{
		return BoolProperties[Index];
	}
	Index -= BoolPropertyProxies.Num();

	if (NameProperties.IsValidIndex(Index) && NamePropertyProxies.IsValidIndex(Index))
	{
		return NameProperties[Index];
	}
	Index -= NamePropertyProxies.Num();

	if (StrProperties.IsValidIndex(Index) && StrPropertyProxies.IsValidIndex(Index))
	{
		return StrProperties[Index];
	}
	Index -= StrPropertyProxies.Num();

	if (TextProperties.IsValidIndex(Index) && TextPropertyProxies.IsValidIndex(Index))
	{
		return TextProperties[Index];
	}
	Index -= TextPropertyProxies.Num();

	if (DelegateProperties.IsValidIndex(Index) && DelegatePropertyProxies.IsValidIndex(Index))
	{
		return DelegateProperties[Index];
	}
	Index -= DelegatePropertyProxies.Num();

	if (MulticastDelegateProperties.IsValidIndex(Index) && MulticastDelegatePropertyProxies.IsValidIndex(Index))
	{
		return MulticastDelegateProperties[Index];
	}
	Index -= MulticastDelegatePropertyProxies.Num();

	if (ObjectPropertyBases.IsValidIndex(Index) && ObjectPropertyBaseProxies.IsValidIndex(Index))
	{
		return ObjectPropertyBases[Index];
	}
	Index -= ObjectPropertyBaseProxies.Num();

	if (ClassProperties.IsValidIndex(Index) && ClassPropertyProxies.IsValidIndex(Index))
	{
		return ClassProperties[Index];
	}
	Index -= ClassPropertyProxies.Num();

	if (ObjectProperties.IsValidIndex(Index) && ObjectPropertyProxies.IsValidIndex(Index))
	{
		return ObjectProperties[Index];
	}
	Index -= ObjectPropertyProxies.Num();

	if (WeakObjectProperties.IsValidIndex(Index) && WeakObjectPropertyProxies.IsValidIndex(Index))
	{
		return WeakObjectProperties[Index];
	}
	Index -= WeakObjectPropertyProxies.Num();

	if (LazyObjectProperties.IsValidIndex(Index) && LazyObjectPropertyProxies.IsValidIndex(Index))
	{
		return LazyObjectProperties[Index];
	}
	Index -= LazyObjectPropertyProxies.Num();

	if (AssetObjectProperties.IsValidIndex(Index) && AssetObjectPropertyProxies.IsValidIndex(Index))
	{
		return AssetObjectProperties[Index];
	}
	Index -= AssetObjectPropertyProxies.Num();

	if (AssetClassProperties.IsValidIndex(Index) && AssetClassPropertyProxies.IsValidIndex(Index))
	{
		return AssetClassProperties[Index];
	}
	Index -= AssetClassPropertyProxies.Num();

	if (InterfaceProperties.IsValidIndex(Index) && InterfacePropertyProxies.IsValidIndex(Index))
	{
		return InterfaceProperties[Index];
	}
	Index -= InterfacePropertyProxies.Num();

	if (StructProperties.IsValidIndex(Index) && StructPropertyProxies.IsValidIndex(Index))
	{
		return StructProperties[Index];
	}
	Index -= StructPropertyProxies.Num();

	if (MapProperties.IsValidIndex(Index) && MapPropertyProxies.IsValidIndex(Index))
	{
		return MapProperties[Index];
	}
	Index -= MapPropertyProxies.Num();

	if (SetProperties.IsValidIndex(Index) && SetPropertyProxies.IsValidIndex(Index))
	{
		return SetProperties[Index];
	}
	Index -= SetPropertyProxies.Num();

	if (ArrayProperties.IsValidIndex(Index) && ArrayPropertyProxies.IsValidIndex(Index))
	{
		return ArrayProperties[Index];
	}

	return nullptr;
}

FSerializeIndex FUHTMakefile::GetScriptStructIndex(const UScriptStruct* ScriptStruct) const
{
	if (!ScriptStruct)
	{
		return FSerializeIndex();
	}

	//if (ScriptStruct->HasAnyClassFlags(CLASS_Native))
	//{
	//	return FSerializeIndex(*this, ScriptStruct->GetFName());
	//}

	const int32* Index = ScriptStructIndexes.Find(ScriptStruct);
	return FSerializeIndex(Index ? *Index : INDEX_NONE);
}

UScriptStruct* FUHTMakefile::GetScriptStructByIndex(FSerializeIndex Index) const
{
	if (Index.IsNone())
	{
		return nullptr;
	}

	if (Index.HasName())
	{
		return FindObjectFast<UScriptStruct>(nullptr, Index.NameProxy.CreateName(*this), true, true);
	}

	if (ScriptStructs.IsValidIndex(Index.Index) && ScriptStructProxies.IsValidIndex(Index.Index))
	{
		return ScriptStructs[Index.Index];
	}

	return nullptr;
}

int32 FUHTMakefile::GetPropertyBaseIndex(FPropertyBase* PropertyBase) const
{
	if (!PropertyBase)
	{
		return INDEX_NONE;
	}

	int32 Result = 0;
	const int32* IndexPtr = PropertyBaseIndexes.Find(PropertyBase);
	if (IndexPtr)
	{
		return *IndexPtr;
	}
	Result += PropertyBases.Num();

	return Result + GetTokenIndex(static_cast<FToken*>(PropertyBase));
}

FPropertyBase* FUHTMakefile::GetPropertyBaseByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	if (PropertyBases.IsValidIndex(Index))
	{
		return PropertyBases[Index];
	}
	Index -= PropertyBaseProxies.Num();

	if (Tokens.IsValidIndex(Index))
	{
		return Tokens[Index];
	}

	check(0);
	return nullptr;	
}

FSerializeIndex FUHTMakefile::GetClassIndex(const UClass* Class) const
{
	if (!Class)
	{
		return FSerializeIndex();
	}

	if (Class->HasAnyClassFlags(CLASS_Native))
	{
		return FSerializeIndex(*this, Class->GetFName());
	}

	return FSerializeIndex(ClassIndexes.FindChecked(Class));
}

UClass* FUHTMakefile::GetClassByIndex(FSerializeIndex Index) const
{
	if (Index.IsNone())
	{
		return nullptr;
	}

	if (Index.HasName())
	{
		return FindObjectFast<UClass>(nullptr, Index.NameProxy.CreateName(*this), true, true);
	}

	if (Classes.IsValidIndex(Index.Index))
	{
		return Classes[Index.Index];
	}

	return nullptr;
}

int32 FUHTMakefile::GetFunctionIndex(const UFunction* Function) const
{
	if (!Function)
	{
		return INDEX_NONE;
	}

	int32 Result = 0;
	const int32* Index = FunctionIndexes.Find(Function);
	if (Index)
	{
		return Result + *Index;
	}
	Result += Functions.Num();

	Index = DelegateFunctionIndexes.Find(Cast<UDelegateFunction>(Function));
	if (Index)
	{
		return Result + *Index;
	}

	check(0);
	return INDEX_NONE;
}

UFunction* FUHTMakefile::GetFunctionByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	if (Functions.IsValidIndex(Index))
	{
		return Functions[Index];
	}
	Index -= FunctionProxies.Num();

	if (DelegateFunctions.IsValidIndex(Index))
	{
		return DelegateFunctions[Index];
	}
	Index -= DelegateFunctionProxies.Num();

	return nullptr;
}

int32 FUHTMakefile::GetFileScopeIndex(const FFileScope* FileScope) const
{
	if (!FileScope)
	{
		return INDEX_NONE;
	}

	return FileScopeIndexes.FindChecked(FileScope);
}

int32 FUHTMakefile::GetUnrealTypeDefinitionInfoIndex(FUnrealTypeDefinitionInfo* Info) const
{
	if (!Info)
	{
		return INDEX_NONE;
	}
	return UnrealTypeDefinitionInfoIndexes.FindChecked(Info);
}

FFileScope* FUHTMakefile::GetFileScopeByIndex(int32 Index) const
{
	check(FileScopes.IsValidIndex(Index));
	return FileScopes[Index];
}

FUnrealTypeDefinitionInfo* FUHTMakefile::GetUnrealTypeDefinitionInfoByIndex(int32 Index) const
{
	check(UnrealTypeDefinitionInfos.IsValidIndex(Index));
	return UnrealTypeDefinitionInfos[Index];
}

int32 FUHTMakefile::GetScopeIndex(const FScope* Scope) const
{
	if (!Scope)
	{
		return INDEX_NONE;
	}

	int32 Result = 0;
	const int32* IndexPtr = ScopeIndexes.Find(Scope);
	if (IndexPtr)
	{
		return Result + *IndexPtr;
	}
	Result += Scopes.Num();

	int32 Index = GetStructScopeIndex(static_cast<const FStructScope*>(Scope));
	if (Index != INDEX_NONE)
	{
		return Result + Index;
	}
	Result += StructScopes.Num();

	Index = GetFileScopeIndex(static_cast<const FFileScope*>(Scope));
	if (Index != INDEX_NONE)
	{
		return Result + Index;
	}

	check(0);
	return INDEX_NONE;
}

FScope* FUHTMakefile::GetScopeByIndex(int32 Index) const
{
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	if (Scopes.IsValidIndex(Index) && ScopeProxies.IsValidIndex(Index))
	{
		return Scopes[Index];
	}
	Index -= ScopeProxies.Num();

	if (StructScopes.IsValidIndex(Index) && StructScopeProxies.IsValidIndex(Index))
	{
		return StructScopes[Index];
	}
	Index -= StructScopeProxies.Num();

	if (FileScopes.IsValidIndex(Index) && FileScopeProxies.IsValidIndex(Index))
	{
		return FileScopes[Index];
	}

	check(0);
	return new FStructScope(nullptr, nullptr);
}

int32 FUHTMakefile::GetStructScopeIndex(const FStructScope* StructScope) const
{
	if (!StructScope)
	{
		return INDEX_NONE;
	}

	const int32* Result = StructScopeIndexes.Find(StructScope);
	return Result ? *Result : INDEX_NONE;
}

FStructScope* FUHTMakefile::GetStructScopeByIndex(int32 Index) const
{
	check(StructScopes.IsValidIndex(Index));
	return StructScopes[Index];
}

void FUHTMakefile::SetupAndSaveReferencedNames(FArchive& Ar)
{
	TArray<UObject*> NativeClasses;
	GetObjectsOfClass(UClass::StaticClass(), NativeClasses);

	for (UObject* Object : NativeClasses)
	{
		UClass* Class = static_cast<UClass*>(Object);
		FClassArchiveProxy::AddReferencedNames(Class, *this);
	}

	for (auto& Kvp : ModuleDescriptors)
	{
		AddName(Kvp.Key);
	}

	for (UPackage* Package : Packages)
	{
		FPackageArchiveProxy::AddReferencedNames(Package, *this);
	}

	for (UClass* Class : Classes)
	{
		FClassArchiveProxy::AddReferencedNames(Class, *this);
	}

	for (UEnum* Enum : Enums)
	{
		FEnumArchiveProxy::AddReferencedNames(Enum, *this);
	}

	for (UStruct* Struct : Structs)
	{
		FStructArchiveProxy::AddReferencedNames(Struct, *this);
	}

	for (UScriptStruct* ScriptStruct : ScriptStructs)
	{
		FScriptStructArchiveProxy::AddReferencedNames(ScriptStruct, *this);
	}

	for (UProperty* Property : Properties)
	{
		FPropertyArchiveProxy::AddReferencedNames(Property, *this);
	}

	for (UByteProperty* ByteProperty : ByteProperties)
	{
		FObjectBaseArchiveProxy::AddReferencedNames(ByteProperty, *this);
	}

	for (UInt8Property* Int8Property : Int8Properties)
	{
		FInt8PropertyArchiveProxy::AddReferencedNames(Int8Property, *this);
	}

	for (UInt16Property* Int16Property : Int16Properties)
	{
		FInt16PropertyArchiveProxy::AddReferencedNames(Int16Property, *this);
	}

	for (UIntProperty* IntProperty : IntProperties)
	{
		FIntPropertyArchiveProxy::AddReferencedNames(IntProperty, *this);
	}

	for (UInt64Property* Int64Property : Int64Properties)
	{
		FInt64PropertyArchiveProxy::AddReferencedNames(Int64Property, *this);
	}

	for (UUInt16Property* UInt16Property : UInt16Properties)
	{
		FUInt16PropertyArchiveProxy::AddReferencedNames(UInt16Property, *this);
	}

	for (UUInt32Property* UInt32Property : UInt32Properties)
	{
		FUInt32PropertyArchiveProxy::AddReferencedNames(UInt32Property, *this);
	}

	for (UUInt64Property* UInt64Property : UInt64Properties)
	{
		FUInt64PropertyArchiveProxy::AddReferencedNames(UInt64Property, *this);
	}

	for (UFloatProperty* FloatProperty : FloatProperties)
	{
		FFloatPropertyArchiveProxy::AddReferencedNames(FloatProperty, *this);
	}

	for (UDoubleProperty* DoubleProperty : DoubleProperties)
	{
		FDoublePropertyArchiveProxy::AddReferencedNames(DoubleProperty, *this);
	}

	for (UBoolProperty* BoolProperty : BoolProperties)
	{
		FBoolPropertyArchiveProxy::AddReferencedNames(BoolProperty, *this);
	}

	for (UNameProperty* NameProperty : NameProperties)
	{
		FNamePropertyArchiveProxy::AddReferencedNames(NameProperty, *this);
	}

	for (UStrProperty* StrProperty : StrProperties)
	{
		FStrPropertyArchiveProxy::AddReferencedNames(StrProperty, *this);
	}

	for (UTextProperty* TextProperty : TextProperties)
	{
		FTextPropertyArchiveProxy::AddReferencedNames(TextProperty, *this);
	}

	for (UDelegateProperty* DelegateProperty : DelegateProperties)
	{
		FDelegatePropertyArchiveProxy::AddReferencedNames(DelegateProperty, *this);
	}

	for (UMulticastDelegateProperty* MulticastDelegateProperty : MulticastDelegateProperties)
	{
		FMulticastDelegatePropertyArchiveProxy::AddReferencedNames(MulticastDelegateProperty, *this);
	}

	for (UObjectPropertyBase* ObjectPropertyBase : ObjectPropertyBases)
	{
		FObjectPropertyBaseArchiveProxy::AddReferencedNames(ObjectPropertyBase, *this);
	}

	for (UClassProperty* ClassProperty : ClassProperties)
	{
		FClassPropertyArchiveProxy::AddReferencedNames(ClassProperty, *this);
	}

	for (UObjectProperty* ObjectProperty : ObjectProperties)
	{
		FObjectPropertyArchiveProxy::AddReferencedNames(ObjectProperty, *this);
	}

	for (UWeakObjectProperty* WeakObjectProperty : WeakObjectProperties)
	{
		FWeakObjectPropertyArchiveProxy::AddReferencedNames(WeakObjectProperty, *this);
	}

	for (ULazyObjectProperty* LazyObjectProperty : LazyObjectProperties)
	{
		FLazyObjectPropertyArchiveProxy::AddReferencedNames(LazyObjectProperty, *this);
	}

	for (UAssetObjectProperty* AssetObjectProperty : AssetObjectProperties)
	{
		FAssetObjectPropertyArchiveProxy::AddReferencedNames(AssetObjectProperty, *this);
	}

	for (UAssetClassProperty* AssetClassProperty : AssetClassProperties)
	{
		FAssetClassPropertyArchiveProxy::AddReferencedNames(AssetClassProperty, *this);
	}

	for (UInterfaceProperty* InterfaceProperty : InterfaceProperties)
	{
		FInterfacePropertyArchiveProxy::AddReferencedNames(InterfaceProperty, *this);
	}

	for (UStructProperty* StructProperty : StructProperties)
	{
		FStructPropertyArchiveProxy::AddReferencedNames(StructProperty, *this);
	}

	for (UMapProperty* MapProperty : MapProperties)
	{
		FMapPropertyArchiveProxy::AddReferencedNames(MapProperty, *this);
	}

	for (USetProperty* SetProperty : SetProperties)
	{
		FSetPropertyArchiveProxy::AddReferencedNames(SetProperty, *this);
	}

	for (UArrayProperty* ArrayProperty : ArrayProperties)
	{
		FArrayPropertyArchiveProxy::AddReferencedNames(ArrayProperty, *this);
	}

	for (UFunction* Function : Functions)
	{
		FFunctionArchiveProxy::AddReferencedNames(Function, *this);
	}

	for (UDelegateFunction* DelegateFunction : DelegateFunctions)
	{
		FDelegateFunctionArchiveProxy::AddReferencedNames(DelegateFunction, *this);
	}

	for (FFileScope* FileScope : FileScopes)
	{
		FFileScopeArchiveProxy::AddReferencedNames(FileScope, *this);
	}

	CompilerMetadataManagerArchiveProxy.AddReferencedNames(&GScriptHelper, *this);

	for (FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass : MultipleInheritanceBaseClasses)
	{
		FMultipleInheritanceBaseClassArchiveProxy::AddReferencedNames(MultipleInheritanceBaseClass, *this); 
	}

	for (FClassMetaData* ClassMetaData : ClassMetaDatas)
	{
		FClassMetaDataArchiveProxy::AddReferencedNames(ClassMetaData, *this);
	}

	for (FToken* Token : Tokens)
	{
		FTokenArchiveProxy::AddReferencedNames(Token, *this);
	}

	ReferencedNames.Empty(ReferencedNamesSet.Num());
	NameIndices.Empty(ReferencedNamesSet.Num());
	for (FName Name : ReferencedNamesSet)
	{
		ReferencedNames.Add(Name);
		NameIndices.Add(Name, NameIndices.Num());
	}
	int32 NameCount = NameIndices.Num();
	Ar << NameCount;

	for (FName Name : ReferencedNames)
	{
		Ar << *const_cast<FNameEntry*>(Name.GetDisplayNameEntry());
	}

}

void FUHTMakefile::LoadAndSetupReferencedNames(FArchive& Ar)
{
	int32 NameCount;
	Ar << NameCount;
	for (int32 i = 0; i < NameCount; i++)
	{
		FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);
		Ar << NameEntry;

		FName Name(NameEntry);
		NameIndices.Add(Name, NameIndices.Num());
		ReferencedNames.Add(Name);
	}
}

FArchive& operator<<(FArchive& Ar, FUHTMakefile& UHTMakefile)
{
	if (Ar.IsSaving())
	{
		FMetadataArchiveProxy::AddStaticallyReferencedNames(UHTMakefile);
		UHTMakefile.SetupAndSaveReferencedNames(Ar);
		UHTMakefile.MoveNewObjects();
		UHTMakefile.SetupProxies();
	}
	else if (Ar.IsLoading())
	{
		UHTMakefile.LoadAndSetupReferencedNames(Ar);
	}
	Ar << *UHTMakefile.Manifest;
	Ar << UHTMakefile.InitializationOrder;
	Ar << UHTMakefile.RawFilenames;
	Ar << UHTMakefile.PublicClassSetEntryProxies;
	Ar << UHTMakefile.Version;
	Ar << UHTMakefile.CompilerMetadataManagerArchiveProxy;
	Ar << UHTMakefile.ModuleNamesProxy;
	Ar << UHTMakefile.ModuleDescriptorsArchiveProxy;
	Ar << UHTMakefile.PackageProxies;
	Ar << UHTMakefile.ClassProxies;
	Ar << UHTMakefile.StructProxies;
	Ar << UHTMakefile.GeneratedCodeCRCProxies;
	Ar << UHTMakefile.ScriptStructProxies;
	Ar << UHTMakefile.EnumProxies;
	Ar << UHTMakefile.UnrealSourceFileProxies;
	Ar << UHTMakefile.FileScopeProxies;
	Ar << UHTMakefile.StructScopeProxies;
	Ar << UHTMakefile.ScopeProxies;
	Ar << UHTMakefile.UnrealTypeDefinitionInfoProxies;
	Ar << UHTMakefile.TypeDefinitionInfoMapArchiveProxy;
	Ar << UHTMakefile.PropertyProxies;
	Ar << UHTMakefile.BytePropertyProxies;
	Ar << UHTMakefile.Int8PropertyProxies;
	Ar << UHTMakefile.Int16PropertyProxies;
	Ar << UHTMakefile.IntPropertyProxies;
	Ar << UHTMakefile.Int64PropertyProxies;
	Ar << UHTMakefile.UInt16PropertyProxies;
	Ar << UHTMakefile.UInt32PropertyProxies;
	Ar << UHTMakefile.UInt64PropertyProxies;
	Ar << UHTMakefile.FloatPropertyProxies;
	Ar << UHTMakefile.DoublePropertyProxies;
	Ar << UHTMakefile.BoolPropertyProxies;
	Ar << UHTMakefile.NamePropertyProxies;
	Ar << UHTMakefile.StrPropertyProxies;
	Ar << UHTMakefile.TextPropertyProxies;
	Ar << UHTMakefile.DelegatePropertyProxies;
	Ar << UHTMakefile.MulticastDelegatePropertyProxies;
	Ar << UHTMakefile.ObjectPropertyBaseProxies;
	Ar << UHTMakefile.ClassPropertyProxies;
	Ar << UHTMakefile.ObjectPropertyProxies;
	Ar << UHTMakefile.WeakObjectPropertyProxies;
	Ar << UHTMakefile.LazyObjectPropertyProxies;
	Ar << UHTMakefile.AssetObjectPropertyProxies;
	Ar << UHTMakefile.AssetClassPropertyProxies;
	Ar << UHTMakefile.InterfacePropertyProxies;
	Ar << UHTMakefile.StructPropertyProxies;
	Ar << UHTMakefile.MapPropertyProxies;
	Ar << UHTMakefile.SetPropertyProxies;
	Ar << UHTMakefile.ArrayPropertyProxies;
	Ar << UHTMakefile.DelegateFunctionProxies;
	Ar << UHTMakefile.FunctionProxies;
	Ar << UHTMakefile.UnrealSourceFilesMapEntryProxies;
	Ar << UHTMakefile.ClassMetaDataArchiveProxies;
	Ar << UHTMakefile.TokenProxies;
	Ar << UHTMakefile.PropertyBaseProxies;
	Ar << UHTMakefile.PropertyDataEntryProxies;
	Ar << UHTMakefile.MultipleInheritanceBaseClassProxies;
	Ar << UHTMakefile.EnumUnderlyingTypeProxies;
	Ar << UHTMakefile.StructNameMapEntryProxies;
	Ar << UHTMakefile.InterfaceAllocationProxies;

	if (Ar.IsLoading())
	{
		UHTMakefile.ResolveModuleNames();
	}

	return Ar;
}

FUnrealTypeDefinitionInfoArchiveProxy::FUnrealTypeDefinitionInfoArchiveProxy(const FUHTMakefile& UHTMakefile, const FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo)
{
	LineNumber = UnrealTypeDefinitionInfo->GetLineNumber();
	UnrealSourceFileIndex = UHTMakefile.GetUnrealSourceFileIndex(&UnrealTypeDefinitionInfo->GetUnrealSourceFile());
}

void FUnrealTypeDefinitionInfoArchiveProxy::Resolve(FUnrealTypeDefinitionInfo*& UnrealTypeDefinitionInfo, const FUHTMakefile& UHTMakefile) const
{
	UnrealTypeDefinitionInfo = new FUnrealTypeDefinitionInfo(*UHTMakefile.GetUnrealSourceFileByIndex(UnrealSourceFileIndex), LineNumber);
}

FUnrealTypeDefinitionInfo* FUnrealTypeDefinitionInfoArchiveProxy::CreateUnrealTypeDefinitionInfo(const FUHTMakefile& UHTMakefile) const
{
	return nullptr;
}
