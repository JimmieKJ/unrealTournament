// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNativeCodeGenPCH.h"
#include "BlueprintNativeCodeGenCoordinator.h"
#include "NativeCodeGenCommandlineParams.h"
#include "Engine/Blueprint.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "ARFilter.h"
#include "PackageName.h"
#include "FileManager.h"
#include "AssetRegistryModule.h"
#include "BlueprintNativeCodeGenUtils.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "BlueprintSupport.h"	// for FReplaceCookedBPGC::OnQueryIfAssetIsTargetedForConversion()

/*******************************************************************************
 * UBlueprintNativeCodeGenConfig
*******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintNativeCodeGenConfig::UBlueprintNativeCodeGenConfig(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/*******************************************************************************
 * BlueprintNativeCodeGenCoordinatorImpl declaration
*******************************************************************************/

namespace BlueprintNativeCodeGenCoordinatorImpl
{
	static const UClass* TargetAssetTypes[] = {
		UBlueprint::StaticClass(),
		UUserDefinedStruct::StaticClass(),
		UUserDefinedEnum::StaticClass(),
	};

	static const int32 IncompatibleBlueprintTypeMask = (1 << BPTYPE_LevelScript) |
		(1 << BPTYPE_MacroLibrary) | (1 << BPTYPE_Const);

	static const FName BlueprintTypePropertyName("BlueprintType");
	static const FName ParentClassTagName("ParentClassPackage");
	static const FName AssetInterfacesTagName("ImplementedInterfaces");

	/**  */
	static IAssetRegistry& GetAssetRegistry();

	/**  */
	static void GatherAssetsToConvert(const FNativeCodeGenCommandlineParams& CommandlineParams, TArray<FAssetData>& ConversionQueueOut);

	/**  */
	static void GatherParentAssets(const FAssetData& TargetAsset, TArray<FAssetData>& AssetsOut);

	/**  */
	static void GatherInterfaceAssets(const FAssetData& TargetAsset, TArray<FAssetData>& AssetsOut);

	/**  */
	static UStruct* GetAssetStruct(UObject* AssetObj);

	/**  */
	static void GatherInnerDependencies(UObject* AssetObj, TArray<FAssetData>& AssetsOut);
}

/*******************************************************************************
 * FScopedAssetCollector
*******************************************************************************/

/**  */
struct FScopedAssetCollector
{
public:
	FScopedAssetCollector(const FARFilter& SharedFilter, const UClass* AssetType, TArray<FAssetData>& AssetsOut)
		: TargetType(AssetType)
	{
		if (FScopedAssetCollector** SpecializedCollector = SpecializedAssetCollectors.Find(TargetType))
		{
			(*SpecializedCollector)->GatherAssets(SharedFilter, AssetsOut);
		}
		else
		{
			GatherAssets(SharedFilter, AssetsOut);
		}
	}

protected:
	FScopedAssetCollector(const UClass* AssetType) : TargetType(AssetType) {}

	/**
	 * 
	 * 
	 * @param  SharedFilter    
	 * @param  AssetsOut    
	 */
	virtual void GatherAssets(const FARFilter& SharedFilter, TArray<FAssetData>& AssetsOut) const;

	const UClass* TargetType;
	static TMap<const UClass*, FScopedAssetCollector*> SpecializedAssetCollectors;
};
TMap<const UClass*, FScopedAssetCollector*> FScopedAssetCollector::SpecializedAssetCollectors;

//------------------------------------------------------------------------------
void FScopedAssetCollector::GatherAssets(const FARFilter& SharedFilter, TArray<FAssetData>& AssetsOut) const
{
	IAssetRegistry& AssetRegistry = BlueprintNativeCodeGenCoordinatorImpl::GetAssetRegistry();

	FARFilter ClassFilter;
	ClassFilter.Append(SharedFilter);
	ClassFilter.ClassNames.Add(TargetType->GetFName());

	if (ClassFilter.PackagePaths.Num() > 0)
	{
		// we want the union of specified package names and paths, not the 
		// intersection (how GetAssets() is set up to work), so here we query 
		// for both separately
		ClassFilter.PackageNames.Empty();
		AssetRegistry.GetAssets(ClassFilter, AssetsOut);
		
		if (SharedFilter.PackageNames.Num() > 0)
		{
			ClassFilter.PackageNames = SharedFilter.PackageNames;
			ClassFilter.PackagePaths.Empty();
			AssetRegistry.GetAssets(ClassFilter, AssetsOut);
		}
	}
	else
	{
		AssetRegistry.GetAssets(ClassFilter, AssetsOut);
	}
}

/*******************************************************************************
 * FSpecializedAssetCollector<>
*******************************************************************************/

template<class AssetType>
struct FSpecializedAssetCollector : public FScopedAssetCollector
{
public:
	FSpecializedAssetCollector() : FScopedAssetCollector(AssetType::StaticClass())
	{
		SpecializedAssetCollectors.Add(AssetType::StaticClass(), this);
	}

	~FSpecializedAssetCollector()
	{
		SpecializedAssetCollectors.Remove(AssetType::StaticClass());
	}

	// FScopedAssetCollector interface
	virtual void GatherAssets(const FARFilter& SharedFilter, TArray<FAssetData>& AssetsOut) const override;
	// End FScopedAssetCollector interface
};

//------------------------------------------------------------------------------
template<>
void FSpecializedAssetCollector<UBlueprint>::GatherAssets(const FARFilter& SharedFilter, TArray<FAssetData>& AssetsOut) const
{
	FARFilter BlueprintFilter;
	BlueprintFilter.Append(SharedFilter);

	if (UByteProperty* TypeProperty = FindField<UByteProperty>(TargetType, BlueprintNativeCodeGenCoordinatorImpl::BlueprintTypePropertyName))
	{
		ensure(TypeProperty->HasAnyPropertyFlags(CPF_AssetRegistrySearchable));

		UEnum* TypeEnum = TypeProperty->Enum;
		if (ensure(TypeEnum != nullptr))
		{
			for (int32 BlueprintType = 0; BlueprintType < TypeEnum->GetMaxEnumValue(); ++BlueprintType)
			{
				if (!TypeEnum->IsValidEnumValue(BlueprintType))
				{
					continue;
				}

				// explicitly ignore unsupported Blueprint conversion types
				if ((BlueprintNativeCodeGenCoordinatorImpl::IncompatibleBlueprintTypeMask & (1 << BlueprintType)) != 0)
				{
					continue;
				}
				BlueprintFilter.TagsAndValues.Add(TypeProperty->GetFName(), TypeEnum->GetEnumName(BlueprintType));
			}
		}
	}

	const int32 StartingIndex = AssetsOut.Num();
	FScopedAssetCollector::GatherAssets(BlueprintFilter, AssetsOut);
	const int32 EndingIndex   = AssetsOut.Num();

	// have to ensure that we convert the entire inheritance chain
	for (int32 BlueprintIndex = StartingIndex; BlueprintIndex < EndingIndex; ++BlueprintIndex)
	{
		BlueprintNativeCodeGenCoordinatorImpl::GatherParentAssets(AssetsOut[BlueprintIndex], AssetsOut);
		BlueprintNativeCodeGenCoordinatorImpl::GatherInterfaceAssets(AssetsOut[BlueprintIndex], AssetsOut);

		// GatherInnerDependencies() will gather other required depenencies 
		// (like user-defined structs/enums), but we don't want to execute that
		// quite yet; 
		//     1) because we'd have to load the asset for that here, and...
		//     2) we want to do it later, when we're processing the conversion 
		//        queue, once the CppBackend's 
		//        OnCheckIsAssetTargetedForConversion() is setup to prevent 
		//        unwanted Blueprint classes from being converted
		// 
	}
}

/*******************************************************************************
 * BlueprintNativeCodeGenCoordinatorImpl definition
*******************************************************************************/

//------------------------------------------------------------------------------
static IAssetRegistry& BlueprintNativeCodeGenCoordinatorImpl::GetAssetRegistry()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	return AssetRegistryModule.Get();
}

//------------------------------------------------------------------------------
static void BlueprintNativeCodeGenCoordinatorImpl::GatherAssetsToConvert(const FNativeCodeGenCommandlineParams& CommandlineParams, TArray<FAssetData>& ConversionQueueOut)
{
	IAssetRegistry& AssetRegistry = GetAssetRegistry();
	// have to make sure that the asset registry is fully populated
	AssetRegistry.SearchAllAssets(/*bSynchronousSearch =*/true);

	FARFilter AssetFilter;
	AssetFilter.bRecursivePaths = true;	

	auto SetFilterPackages = [&AssetFilter](const TArray<FString>& PackageList)
	{
		IFileManager& FileManager = IFileManager::Get();

		for (const FString& PackagePath : PackageList)
		{
			const FString RelativePath = FPackageName::LongPackageNameToFilename(PackagePath);
			if (FileManager.DirectoryExists(*RelativePath))
			{
				AssetFilter.PackagePaths.AddUnique(*PackagePath);
			}
			else
			{
				AssetFilter.PackageNames.AddUnique(*PackagePath);
			}
		}
	};

	const UBlueprintNativeCodeGenConfig* ConfigSettings = GetDefault<UBlueprintNativeCodeGenConfig>();
	SetFilterPackages(ConfigSettings->PackagesToAlwaysConvert);
	SetFilterPackages(CommandlineParams.WhiteListedAssetPaths);

	// will be utilized if UBlueprint is an entry in TargetAssetTypes (auto registers itself as a handler)
	FSpecializedAssetCollector<UBlueprint> BlueprintAssetCollector;

	const int32 TargetClassCount = sizeof(TargetAssetTypes) / sizeof(TargetAssetTypes[0]);
	for (int32 ClassIndex = 0; ClassIndex < TargetClassCount; ++ClassIndex)
	{
		const UClass* TargetClass = TargetAssetTypes[ClassIndex];
		FScopedAssetCollector AssetCollector(AssetFilter, TargetClass, ConversionQueueOut);
	}
}

//------------------------------------------------------------------------------
static void BlueprintNativeCodeGenCoordinatorImpl::GatherParentAssets(const FAssetData& TargetAsset, TArray<FAssetData>& AssetsOut)
{
	IAssetRegistry& AssetRegistry = GetAssetRegistry();

	if (const FString* ParentClassPkg = TargetAsset.TagsAndValues.Find(ParentClassTagName))
	{
		TArray<FAssetData> ParentPackages;
		AssetRegistry.GetAssetsByPackageName(**ParentClassPkg, ParentPackages);

		// should either be 0 (when the parent is a native class), or 1 (the parent is another asset) 
		ensure(ParentPackages.Num() <= 1);

		for (const FAssetData& ParentData : ParentPackages)
		{
			AssetsOut.AddUnique(ParentData); // keeps us from converting parents that were already flagged for conversion
			GatherParentAssets(ParentData, AssetsOut);
			GatherInterfaceAssets(ParentData, AssetsOut);
		}
	}
}

//------------------------------------------------------------------------------
static void BlueprintNativeCodeGenCoordinatorImpl::GatherInterfaceAssets(const FAssetData& TargetAsset, TArray<FAssetData>& AssetsOut)
{
	IAssetRegistry& AssetRegistry = GetAssetRegistry();

	if (const FString* InterfacesListPtr = TargetAsset.TagsAndValues.Find(AssetInterfacesTagName))
	{
		static const FString InterfaceClassLabel(TEXT("(Interface="));
		static const FString BlueprintClassName = UBlueprintGeneratedClass::StaticClass()->GetName();
		static const FString ClassPathDelimiter = TEXT("'");
		static const FString PackageDelimiter   = TEXT(".");

		FString InterfacesList = *InterfacesListPtr;
		do
		{
			const int32 SearchIndex = InterfacesList.Find(InterfaceClassLabel);
			if (SearchIndex != INDEX_NONE)
			{
				int32 InterfaceClassIndex = SearchIndex + InterfaceClassLabel.Len();
				InterfacesList = InterfacesList.Mid(InterfaceClassIndex);

				if (InterfacesList.StartsWith(BlueprintClassName))
				{
					InterfaceClassIndex = BlueprintClassName.Len();

					const int32 ClassEndIndex = InterfacesList.Find(ClassPathDelimiter, ESearchCase::IgnoreCase, ESearchDir::FromStart, InterfaceClassIndex+1);
					const FString ClassPath = InterfacesList.Mid(InterfaceClassIndex+1, ClassEndIndex - InterfaceClassIndex);

					FString ClassPackagePath, ClassName;
					if (ClassPath.Split(PackageDelimiter, &ClassPackagePath, &ClassName))
					{
						TArray<FAssetData> InterfacePackages;
						AssetRegistry.GetAssetsByPackageName(*ClassPackagePath, InterfacePackages);

						// should find a single asset (since we know this is for a Blueprint generated class)
						ensure(InterfacePackages.Num() == 1);

						for (const FAssetData& Interface : InterfacePackages)
						{
							// AddUnique() keeps us from converting interfaces that were already flagged for conversion
							AssetsOut.AddUnique(Interface); 
						}
					}
				}
			}
			else
			{
				break;
			}
		} while (!InterfacesList.IsEmpty());
	}
}

//------------------------------------------------------------------------------
static UStruct* BlueprintNativeCodeGenCoordinatorImpl::GetAssetStruct(UObject* AssetObj)
{
	if (UStruct* StructAsset = Cast<UStruct>(AssetObj))
	{
		return StructAsset;
	}
	else if (UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetObj))
	{
		return BlueprintAsset->GeneratedClass;
	}
	return nullptr;
}

//------------------------------------------------------------------------------
static void BlueprintNativeCodeGenCoordinatorImpl::GatherInnerDependencies(UObject* AssetObj, TArray<FAssetData>& AssetsOut)
{
	if (UStruct* StructAsset = GetAssetStruct(AssetObj))
	{
		IAssetRegistry& AssetRegistry = GetAssetRegistry();
		auto AddDependencyForConversion = [&AssetsOut, &AssetRegistry](UObject* Object)->bool
		{
			ensure(Object->IsAsset());
			FAssetData AssetInfo = AssetRegistry.GetAssetByObjectPath(*Object->GetPathName());

			const bool bAssetFound = AssetInfo.IsValid();
			if (bAssetFound)
			{
				AssetsOut.AddUnique(AssetInfo);
			}
			return bAssetFound;
		};

		FGatherConvertedClassDependencies AssetDependencies(StructAsset);

		for (UBlueprintGeneratedClass* BlueprintClass : AssetDependencies.ConvertedClasses)
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
			{
				AddDependencyForConversion(Blueprint);
			}
		}
		for (UUserDefinedStruct* StructDependency : AssetDependencies.ConvertedStructs)
		{
			AddDependencyForConversion(StructDependency);
		}
		for (UUserDefinedEnum* EnumAsset : AssetDependencies.ConvertedEnum)
		{
			AddDependencyForConversion(EnumAsset);
		}
	}
}

/*******************************************************************************
 * FBlueprintNativeCodeGenCoordinator
*******************************************************************************/

//------------------------------------------------------------------------------
FBlueprintNativeCodeGenCoordinator::FBlueprintNativeCodeGenCoordinator(const FNativeCodeGenCommandlineParams& CommandlineParams)
	: Manifest(CommandlineParams)
{
	BlueprintNativeCodeGenCoordinatorImpl::GatherAssetsToConvert(CommandlineParams, ConversionQueue);
}

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenCoordinator::IsTargetedForConversion(const FString AssetPath)
{
	bool bHasBeenConverted = false;
	if (FConvertedAssetRecord* ConversionRecord = Manifest.FindConversionRecord(AssetPath))
	{
		// interfaces don't generate cpp files
		bHasBeenConverted = !ConversionRecord->GeneratedHeaderPath.IsEmpty() || !ConversionRecord->GeneratedCppPath.IsEmpty();
		
		IFileManager& FileManager = IFileManager::Get();
		bHasBeenConverted &= ConversionRecord->GeneratedHeaderPath.IsEmpty() || FileManager.FileExists(*ConversionRecord->GeneratedHeaderPath);
		bHasBeenConverted &= ConversionRecord->GeneratedCppPath.IsEmpty()    || FileManager.FileExists(*ConversionRecord->GeneratedCppPath);
	}

	bool bIsQueuedForConversion = false;
	if (!bHasBeenConverted)
	{
		// @TODO: consider using a map to speed this up

		IAssetRegistry& AssetRegistry = BlueprintNativeCodeGenCoordinatorImpl::GetAssetRegistry();
		TArray<FAssetData> AssetPackages;
		AssetRegistry.GetAssetsByPackageName(*AssetPath, AssetPackages);

		// should either be 0 (the asset doesn't exist), or 1 (the package contains a singular asset) 
		ensure(AssetPackages.Num() <= 1);

		for (const FAssetData& FoundAsset : AssetPackages)
		{
			// we shouldn't come up with multiple assets, but if we do... I guess OR the result?
			bIsQueuedForConversion |= (ConversionQueue.Find(FoundAsset) != INDEX_NONE);
		}
	}
	return bHasBeenConverted || bIsQueuedForConversion;
}

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenCoordinator::IsTargetedForConversion(const UBlueprint* Blueprint)
{
	UPackage* AssetPackage = Blueprint->GetOutermost();
	return IsTargetedForConversion(AssetPackage->GetPathName());
}

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenCoordinator::IsTargetedForConversion(const UClass* Class)
{
	if (Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		UPackage* AssetPackage = Class->GetOutermost();
		return IsTargetedForConversion(AssetPackage->GetPathName());
	}
	return false;
}

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenCoordinator::IsTargetedForConversion(const UEnum* Enum)
{
	if (const UUserDefinedEnum* EnumAsset = Cast<UUserDefinedEnum>(Enum))
	{
		UPackage* AssetPackage = EnumAsset->GetOutermost();
		return IsTargetedForConversion(AssetPackage->GetPathName());
	}
	return false;
}

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenCoordinator::IsTargetedForConversion(const UStruct* Struct)
{
	if (const UUserDefinedStruct* StructAsset = Cast<UUserDefinedStruct>(Struct))
	{
		UPackage* AssetPackage = StructAsset->GetOutermost();
		return IsTargetedForConversion(AssetPackage->GetPathName());
	}
	return false;
}

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenCoordinator::ProcessConversionQueue(const FConversionDelegate& ConversionDelegate)
{
	auto& ConversionQueryDelegate = FReplaceCookedBPGC::Get().OnQueryIfAssetIsTargetedForConversion();
	auto IsAssetTargetedForConversion = [this](const UObject* AssetObj)->bool
	{
		return IsTargetedForConversion(AssetObj->GetOutermost()->GetPathName());
	};
	ConversionQueryDelegate.BindLambda(IsAssetTargetedForConversion);

	bool bSuccess = true;
	// pre-process the conversion queue (has to happen before converting, 
	// so we can gather module dependencies in FBlueprintNativeCodeGenManifest::LogDependencies);
	// if this were to happen in the same loop as converting, then the compilation
	// of one asset could result in a later asset having its linker/loader reset
	// (before we processed it... meaning we wouldn't have an easy way to 
	// determine that later Blueprint's module dependencies)
	for (int32 AssetIndex = 0; AssetIndex < ConversionQueue.Num() && bSuccess; ++AssetIndex)
	{
		const FAssetData& AssetInfo = ConversionQueue[AssetIndex];

		// load asset, add dependencies that can only be gathered from a 
		// loaded asset (property types, etc.)
		UObject* AssetObj = AssetInfo.GetAsset();
		// NOTE: this will most likely grow the ConversionQueue as we iterate through it
		BlueprintNativeCodeGenCoordinatorImpl::GatherInnerDependencies(AssetObj, ConversionQueue);

		bSuccess &= Manifest.LogDependencies(AssetInfo);
		if (!bSuccess)
		{
			UE_LOG(LogBlueprintCodeGen, Error, TEXT("Failed to pre-process '%s' for conversion"), *AssetInfo.AssetName.ToString());
		}
	}

	for (int32 AssetIndex = 0; AssetIndex < ConversionQueue.Num() && bSuccess; ++AssetIndex)
	{
		const FAssetData& AssetInfo = ConversionQueue[AssetIndex];

		FConvertedAssetRecord& ConversionRecord = Manifest.CreateConversionRecord(AssetInfo);
		bSuccess &= ConversionDelegate.Execute(ConversionRecord);

		if (!bSuccess)
		{
			UE_LOG(LogBlueprintCodeGen, Error, TEXT("Failed to fully convert '%s'"), *AssetInfo.AssetName.ToString());
		}
	}

	ConversionQueryDelegate.Unbind();
	return bSuccess;
}

