// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNativeCodeGenPCH.h"

#include "AssetRegistryModule.h"
#include "BlueprintNativeCodeGenManifest.h"
#include "BlueprintNativeCodeGenModule.h"
#include "BlueprintNativeCodeGenUtils.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintEditorUtils.h"
#include "Engine/Blueprint.h" // for EBlueprintType
#include "Engine/InheritableComponentHandler.h"

/*******************************************************************************
* NativizationCookControllerImpl
******************************************************************************/

namespace NativizationCookControllerImpl
{
	// If you change this plugin name you must update logic in CookCommand.Automation.cs
	static const TCHAR* DefaultPluginName = TEXT("NativizedAssets");
}

/*******************************************************************************
 * FBlueprintNativeCodeGenModule
 ******************************************************************************/
 
class FBlueprintNativeCodeGenModule : public IBlueprintNativeCodeGenModule
									, public IBlueprintNativeCodeGenCore
{
public:
	FBlueprintNativeCodeGenModule()
	{
	}

	//~ Begin IBlueprintNativeCodeGenModule interface
	virtual void Convert(UPackage* Package, ESavePackageResult ReplacementType, const TCHAR* PlatformName) override;
	virtual void SaveManifest(int32 Id = -1) override;
	virtual void MergeManifest(int32 ManifestIdentifier) override;
	virtual void FinalizeManifest() override;
	virtual void GenerateStubs() override;
	virtual void GenerateFullyConvertedClasses() override;
	virtual void MarkUnconvertedBlueprintAsNecessary(TAssetPtr<UBlueprint> BPPtr) override;
	virtual const TMultiMap<FName, TAssetSubclassOf<UObject>>& GetFunctionsBoundToADelegate() override;

	FFileHelper::EEncodingOptions::Type ForcedEncoding() const
	{
		return FFileHelper::EEncodingOptions::ForceUTF8;
	}
protected:
	virtual void Initialize(const FNativeCodeGenInitData& InitData) override;
	virtual void InitializeForRerunDebugOnly(const TArray< TPair< FString, FString > >& CodegenTargets) override;
	//~ End IBlueprintNativeCodeGenModule interface

	//~ Begin FScriptCookReplacmentCoordinator interface
	virtual EReplacementResult IsTargetedForReplacement(const UPackage* Package) const override;
	virtual EReplacementResult IsTargetedForReplacement(const UObject* Object) const override;
	virtual UClass* FindReplacedClassForObject(const UObject* Object) const override;
	virtual UObject* FindReplacedNameAndOuter(UObject* Object, FName& OutName) const override;
	//~ End FScriptCookReplacmentCoordinator interface
private:
	void ReadConfig();
	void FillTargetedForReplacementQuery();
	void FillIsFunctionUsedInADelegate();
	FBlueprintNativeCodeGenManifest& GetManifest(const TCHAR* PlatformName);
	void GenerateSingleStub(UBlueprint* BP, const TCHAR* PlatformName);
	void CollectBoundFunctions(UBlueprint* BP);
	void GenerateSingleAsset(UField* ForConversion, const TCHAR* PlatformName, TSharedPtr<FNativizationSummary> NativizationSummary = TSharedPtr<FNativizationSummary>());

	TMap< FString, TUniquePtr<FBlueprintNativeCodeGenManifest> > Manifests;

	TArray<FString> ExcludedAssetTypes;
	TArray<TAssetSubclassOf<UBlueprint>> ExcludedBlueprintTypes;
	TSet<FStringAssetReference> ExcludedAssets;
	TArray<FString> TargetPlatformNames;

	// A stub-wrapper must be generated only if the BP is really accessed/required by some other generated code.
	TSet<TAssetPtr<UBlueprint>> StubsRequiredByGeneratedCode;
	TSet<TAssetPtr<UBlueprint>> AllPotentialStubs;

	TSet<TAssetPtr<UBlueprint>> ToGenerate;
	TMultiMap<FName, TAssetSubclassOf<UObject>> FunctionsBoundToADelegate; // is a function could be bound to a delegate, then it must have UFUNCTION macro. So we cannot optimize it.
};

void FBlueprintNativeCodeGenModule::ReadConfig()
{
	GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ExcludedAssetTypes"), ExcludedAssetTypes, GEditorIni);

	{
		TArray<FString> ExcludedBlueprintTypesPath;
		GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ExcludedBlueprintTypes"), ExcludedBlueprintTypesPath, GEditorIni);
		for (FString& Path : ExcludedBlueprintTypesPath)
		{
			TAssetSubclassOf<UBlueprint> ClassPtr;
			ClassPtr = FStringAssetReference(Path);
			ClassPtr.LoadSynchronous();
			ExcludedBlueprintTypes.Add(ClassPtr);
		}
	}

	TArray<FString> ExcludedAssetPaths;
	GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ExcludedAssets"), ExcludedAssetPaths, GEditorIni);
	for (FString& Path : ExcludedAssetPaths)
	{
		ExcludedAssets.Add(FStringAssetReference(Path));
	}
}

void FBlueprintNativeCodeGenModule::MarkUnconvertedBlueprintAsNecessary(TAssetPtr<UBlueprint> BPPtr)
{
	StubsRequiredByGeneratedCode.Add(BPPtr);
}

void FBlueprintNativeCodeGenModule::FillTargetedForReplacementQuery()
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	auto& ConversionQueryDelegate = BackEndModule.OnIsTargetedForConversionQuery();
	auto ShouldConvert = [](const UObject* AssetObj)
	{
		if (ensure(IBlueprintNativeCodeGenCore::Get()))
		{
			EReplacementResult ReplacmentResult = IBlueprintNativeCodeGenCore::Get()->IsTargetedForReplacement(AssetObj);
			return ReplacmentResult == EReplacementResult::ReplaceCompletely;
		}
		return false;
	};
	ConversionQueryDelegate.BindStatic(ShouldConvert);

	auto LocalMarkUnconvertedBlueprintAsNecessary = [](TAssetPtr<UBlueprint> BPPtr)
	{
		IBlueprintNativeCodeGenModule::Get().MarkUnconvertedBlueprintAsNecessary(BPPtr);
	};
	BackEndModule.OnIncludingUnconvertedBP().BindStatic(LocalMarkUnconvertedBlueprintAsNecessary);
}

namespace 
{
	void GetFieldFormPackage(const UPackage* Package, UStruct*& OutStruct, UEnum*& OutEnum, EObjectFlags ExcludedFlags = RF_Transient)
	{
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects, false);
		for (UObject* Entry : Objects)
		{
			if (Entry->HasAnyFlags(ExcludedFlags))
			{
				continue;
			}

			if (FBlueprintSupport::IsDeferredDependencyPlaceholder(Entry))
			{
				continue;
			}

			// Not a skeleton class
			if (UClass* AsClass = Cast<UClass>(Entry))
			{
				if (UBlueprint* GeneratingBP = Cast<UBlueprint>(AsClass->ClassGeneratedBy))
				{
					if (AsClass != GeneratingBP->GeneratedClass)
					{
						continue;
					}
				}
			}

			OutStruct = Cast<UStruct>(Entry);
			if (OutStruct)
			{
				break;
			}

			OutEnum = Cast<UEnum>(Entry);
			if (OutEnum)
			{
				break;
			}
		}
	}
}

void FBlueprintNativeCodeGenModule::CollectBoundFunctions(UBlueprint* BP)
{
	TArray<UFunction*> Functions = IBlueprintCompilerCppBackendModule::CollectBoundFunctions(BP);
	for (UFunction* Func : Functions)
	{
		if (Func)
		{
			FunctionsBoundToADelegate.AddUnique(Func->GetFName(), Func->GetOwnerClass());
		}
	}
}

const TMultiMap<FName, TAssetSubclassOf<UObject>>& FBlueprintNativeCodeGenModule::GetFunctionsBoundToADelegate()
{
	return FunctionsBoundToADelegate;
}

void FBlueprintNativeCodeGenModule::FillIsFunctionUsedInADelegate()
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();

	auto IsFunctionUsed = [](const UFunction* InFunction) -> bool
	{
		auto& TargetFunctionsBoundToADelegate = IBlueprintNativeCodeGenModule::Get().GetFunctionsBoundToADelegate();
		return InFunction && (nullptr != TargetFunctionsBoundToADelegate.FindPair(InFunction->GetFName(), InFunction->GetOwnerClass()));
	};

	BackEndModule.GetIsFunctionUsedInADelegateCallback().BindStatic(IsFunctionUsed);
}

void FBlueprintNativeCodeGenModule::Initialize(const FNativeCodeGenInitData& InitData)
{
	ReadConfig();

	IBlueprintNativeCodeGenCore::Register(this);

	// Each platform will need a manifest, because each platform could cook different assets:
	for (auto& Platform : InitData.CodegenTargets)
	{
		const TCHAR* TargetDirectory = *Platform.Value;
		FString OutputPath = FPaths::Combine(TargetDirectory, NativizationCookControllerImpl::DefaultPluginName);

		Manifests.Add(FString(*Platform.Key), TUniquePtr<FBlueprintNativeCodeGenManifest>(new FBlueprintNativeCodeGenManifest(NativizationCookControllerImpl::DefaultPluginName, OutputPath)));

		TargetPlatformNames.Add(Platform.Key);

		// Clear source code folder
		const FString SourceCodeDir = GetManifest(*Platform.Key).GetTargetPaths().PluginSourceDir();
		UE_LOG(LogBlueprintCodeGen, Log, TEXT("Clear nativized source code directory: %s"), *SourceCodeDir);
		IFileManager::Get().DeleteDirectory(*SourceCodeDir, false, true);
	}

	FillTargetedForReplacementQuery();

	FillIsFunctionUsedInADelegate();
}

void FBlueprintNativeCodeGenModule::InitializeForRerunDebugOnly(const TArray< TPair< FString, FString > >& CodegenTargets)
{
	ReadConfig();
	IBlueprintNativeCodeGenCore::Register(this);
	FillTargetedForReplacementQuery();
	FillIsFunctionUsedInADelegate();

	for (const auto& Platform : CodegenTargets)
	{
		// load the old manifest:
		const TCHAR* TargetDirectory = *Platform.Value;
		FString OutputPath = FPaths::Combine(TargetDirectory, NativizationCookControllerImpl::DefaultPluginName, *FBlueprintNativeCodeGenPaths::GetDefaultManifestPath());
		Manifests.Add(FString(*Platform.Key), TUniquePtr<FBlueprintNativeCodeGenManifest>(new FBlueprintNativeCodeGenManifest(FPaths::ConvertRelativePathToFull(OutputPath))));
		//FBlueprintNativeCodeGenManifest OldManifest(FPaths::ConvertRelativePathToFull(OutputPath));
		// reconvert every assets listed in the manifest:
		for (const auto& ConversionTarget : GetManifest(*Platform.Key).GetConversionRecord())
		{
			// load the package:
			UPackage* Package = LoadPackage(nullptr, *ConversionTarget.Value.TargetObjPath, LOAD_None);

			if (!Package)
			{
				UE_LOG(LogBlueprintCodeGen, Error, TEXT("Unable to load the package: %s"), *ConversionTarget.Value.TargetObjPath);
				continue;
			}

			// reconvert it
			Convert(Package, ESavePackageResult::ReplaceCompletely, *Platform.Key);
		}

		// reconvert every unconverted dependency listed in the manifest:
		for (const auto& ConversionTarget : GetManifest(*Platform.Key).GetUnconvertedDependencies())
		{
			// load the package:
			UPackage* Package = LoadPackage(nullptr, *ConversionTarget.Key.GetPlainNameString(), LOAD_None);

			UStruct* Struct = nullptr;
			UEnum* Enum = nullptr;
			GetFieldFormPackage(Package, Struct, Enum);
			UBlueprint* BP = Cast<UBlueprint>(CastChecked<UClass>(Struct)->ClassGeneratedBy);
			if (ensure(BP))
			{
				CollectBoundFunctions(BP);
				GenerateSingleStub(BP, *Platform.Key);
			}
		}

		for (TAssetPtr<UBlueprint>& BPPtr : ToGenerate)
		{
			UBlueprint* BP = BPPtr.LoadSynchronous();
			if (ensure(BP))
			{
				GenerateSingleAsset(BP->GeneratedClass, *Platform.Key);
			}
		}
	}
}

void FBlueprintNativeCodeGenModule::GenerateFullyConvertedClasses()
{
	TSharedPtr<FNativizationSummary> NativizationSummary(new FNativizationSummary());
	for (TAssetPtr<UBlueprint>& BPPtr : ToGenerate)
	{
		UBlueprint* BP = BPPtr.LoadSynchronous();
		if (ensure(BP))
		{
			for (const FString& PlatformName : TargetPlatformNames)
			{
				GenerateSingleAsset(BP->GeneratedClass, *PlatformName, NativizationSummary);
			}
		}
	}
	
	if (NativizationSummary->InaccessiblePropertyStat.Num())
	{
		UE_LOG(LogBlueprintCodeGen, Warning, TEXT("Nativization Summary - Inaccessible Properties:"));
		NativizationSummary->InaccessiblePropertyStat.ValueSort(TGreater<int32>());
		for (auto& Iter : NativizationSummary->InaccessiblePropertyStat)
		{
			UE_LOG(LogBlueprintCodeGen, Warning, TEXT("\t %s \t - %d"), *Iter.Key.ToString(), Iter.Value);
		}
	}
}

FBlueprintNativeCodeGenManifest& FBlueprintNativeCodeGenModule::GetManifest(const TCHAR* PlatformName)
{
	FString PlatformNameStr(PlatformName);
	TUniquePtr<FBlueprintNativeCodeGenManifest>* Result = Manifests.Find(PlatformNameStr);
	check(Result->IsValid());
	return **Result;
}

void FBlueprintNativeCodeGenModule::GenerateSingleStub(UBlueprint* BP, const TCHAR* PlatformName)
{
	UClass* Class = BP ? BP->GeneratedClass : nullptr;
	if (!ensure(Class))
	{
		return;
	}

	// no PCHFilename should be necessary
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetInfo = Registry.GetAssetByObjectPath(*Class->GetPathName());
	FString FileContents;
	TUniquePtr<IBlueprintCompilerCppBackend> Backend_CPP(IBlueprintCompilerCppBackendModuleInterface::Get().Create());
	// Apparently we can only generate wrappers for classes, so any logic that results in non classes requesting
	// wrappers will fail here:

	FileContents = Backend_CPP->GenerateWrapperForClass(Class);

	if (!FileContents.IsEmpty())
	{
		FFileHelper::SaveStringToFile(FileContents
			, *(GetManifest(PlatformName).CreateUnconvertedDependencyRecord(AssetInfo.PackageName, AssetInfo).GeneratedWrapperPath)
			, ForcedEncoding());
	}
	// The stub we generate still may have dependencies on other modules, so make sure the module dependencies are 
	// still recorded so that the .build.cs is generated correctly. Without this you'll get include related errors 
	// (or possibly linker errors) in stub headers:
	GetManifest(PlatformName).GatherModuleDependencies(BP->GetOutermost());
}

void FBlueprintNativeCodeGenModule::GenerateSingleAsset(UField* ForConversion, const TCHAR* PlatformName, TSharedPtr<FNativizationSummary> NativizationSummary)
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	auto& BackendPCHQuery = BackEndModule.OnPCHFilenameQuery();
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetInfo = Registry.GetAssetByObjectPath(*ForConversion->GetPathName());

	FBlueprintNativeCodeGenPaths TargetPaths = GetManifest(PlatformName).GetTargetPaths();
	BackendPCHQuery.BindLambda([TargetPaths]()->FString
	{
		return TargetPaths.RuntimePCHFilename();
	});

	FConvertedAssetRecord& ConversionRecord = GetManifest(PlatformName).CreateConversionRecord(*ForConversion->GetPathName(), AssetInfo);
	TSharedPtr<FString> HeaderSource(new FString());
	TSharedPtr<FString> CppSource(new FString());

	FBlueprintNativeCodeGenUtils::GenerateCppCode(ForConversion, HeaderSource, CppSource, NativizationSummary);
	bool bSuccess = !HeaderSource->IsEmpty() || !CppSource->IsEmpty();
	// Run the cpp first, because we cue off of the presence of a header for a valid conversion record (see
	// FConvertedAssetRecord::IsValid)
	if (!CppSource->IsEmpty())
	{
		if (!FFileHelper::SaveStringToFile(*CppSource, *ConversionRecord.GeneratedCppPath, ForcedEncoding()))
		{
			bSuccess &= false;
			ConversionRecord.GeneratedCppPath.Empty();
		}
		CppSource->Empty(CppSource->Len());
	}
	else
	{
		ConversionRecord.GeneratedCppPath.Empty();
	}

	if (bSuccess && !HeaderSource->IsEmpty())
	{
		if (!FFileHelper::SaveStringToFile(*HeaderSource, *ConversionRecord.GeneratedHeaderPath, ForcedEncoding()))
		{
			bSuccess &= false;
			ConversionRecord.GeneratedHeaderPath.Empty();
		}
		HeaderSource->Empty(HeaderSource->Len());
	}
	else
	{
		ConversionRecord.GeneratedHeaderPath.Empty();
	}

	if (ensure(bSuccess))
	{
		GetManifest(PlatformName).GatherModuleDependencies(ForConversion->GetOutermost());
	}
	else
	{
		UE_LOG(LogBlueprintCodeGen, Error, TEXT("FBlueprintNativeCodeGenModule::GenerateSingleAsset error: %s"), *GetPathNameSafe(ForConversion));
	}

	BackendPCHQuery.Unbind();
}

void FBlueprintNativeCodeGenModule::GenerateStubs()
{
	TSet<TAssetPtr<UBlueprint>> AlreadyGenerated;
	while (AlreadyGenerated.Num() < StubsRequiredByGeneratedCode.Num())
	{
		const int32 OldGeneratedNum = AlreadyGenerated.Num();
		for (TAssetPtr<UBlueprint>& BPPtr : StubsRequiredByGeneratedCode)
		{
			bool bAlreadyGenerated = false;
			AlreadyGenerated.Add(BPPtr, &bAlreadyGenerated);
			if (bAlreadyGenerated)
			{
				continue;
			}

			ensureMsgf(AllPotentialStubs.Contains(BPPtr), TEXT("A required blueprint doesn't generate stub: %s"), *BPPtr.ToString());
			for (auto& PlatformName : TargetPlatformNames)
			{
				GenerateSingleStub(BPPtr.LoadSynchronous(), *PlatformName);
			}
		}

		if (!ensure(OldGeneratedNum != AlreadyGenerated.Num()))
		{
			break;
		}
	}

	UE_LOG(LogBlueprintCodeGen, Log, TEXT("GenerateStubs - all unconverted bp: %d, generated wrapers: %d"), AllPotentialStubs.Num(), StubsRequiredByGeneratedCode.Num());
}

void FBlueprintNativeCodeGenModule::Convert(UPackage* Package, ESavePackageResult CookResult, const TCHAR* PlatformName)
{
	// Find the struct/enum to convert:
	UStruct* Struct = nullptr;
	UEnum* Enum = nullptr;
	GetFieldFormPackage(Package, Struct, Enum);

	// First we gather information about bound functions.
	UClass* AsClass = Cast<UClass>(Struct);
	UBlueprint* BP = AsClass ? Cast<UBlueprint>(AsClass->ClassGeneratedBy) : nullptr;
	if (BP)
	{
		CollectBoundFunctions(BP);
	}

	if (CookResult != ESavePackageResult::ReplaceCompletely && CookResult != ESavePackageResult::GenerateStub)
	{
		// nothing to convert
		return;
	}

	if (Struct == nullptr && Enum == nullptr)
	{
		ensure(false);
		return;
	}

	if (CookResult == ESavePackageResult::GenerateStub)
	{
		if (ensure(BP))
		{
			ensure(!ToGenerate.Contains(BP));
			AllPotentialStubs.Add(BP);
		}
	}
	else
	{
		check(CookResult == ESavePackageResult::ReplaceCompletely);
		if (AsClass)
		{
			if (ensure(BP))
			{
				ensure(!AllPotentialStubs.Contains(BP));
				ToGenerate.Add(BP);
			}
		}
		else
		{
			UField* ForConversion = Struct ? (UField*)Struct : (UField*)Enum;
			GenerateSingleAsset(ForConversion, PlatformName);
		}
	}
}

void FBlueprintNativeCodeGenModule::SaveManifest(int32 Id )
{
	for (auto& PlatformName : TargetPlatformNames)
	{
		GetManifest(*PlatformName).Save(Id);
	}
}

void FBlueprintNativeCodeGenModule::MergeManifest(int32 ManifestIdentifier)
{
	for (auto& PlatformName : TargetPlatformNames)
	{
		FBlueprintNativeCodeGenManifest& CurrentManifest = GetManifest(*PlatformName);
		FBlueprintNativeCodeGenManifest OtherManifest = FBlueprintNativeCodeGenManifest(CurrentManifest.GetTargetPaths().ManifestFilePath() + FString::FromInt(ManifestIdentifier));
		CurrentManifest.Merge(OtherManifest);
	}
} 

void FBlueprintNativeCodeGenModule::FinalizeManifest()
{
	for(auto& PlatformName : TargetPlatformNames)
	{
		GetManifest(*PlatformName).Save(-1);
		check(FBlueprintNativeCodeGenUtils::FinalizePlugin(GetManifest(*PlatformName)));
	}
}

UClass* FBlueprintNativeCodeGenModule::FindReplacedClassForObject(const UObject* Object) const
{
	// we're only looking to replace class types:
	if (Object && Object->IsA<UField>() 
		&& (EReplacementResult::ReplaceCompletely == IsTargetedForReplacement(Object)))
	{
		for (const UClass* Class = Object->GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (Class == UUserDefinedEnum::StaticClass())
			{
				return UEnum::StaticClass();
			}
			if (Class == UUserDefinedStruct::StaticClass())
			{
				return UScriptStruct::StaticClass();
			}
			if (Class == UBlueprintGeneratedClass::StaticClass())
			{
				return UDynamicClass::StaticClass();
			}
		}
	}
	ensure(!Object || !(Object->IsA<UUserDefinedStruct>() || Object->IsA<UUserDefinedEnum>()));
	return nullptr;
}

UObject* FBlueprintNativeCodeGenModule::FindReplacedNameAndOuter(UObject* Object, FName& OutName) const
{
	OutName = NAME_None;
	UObject* Outer = nullptr;

	UActorComponent* ActorComponent = Cast<UActorComponent>(Object);
	if (ActorComponent)
	{
		//if is child of a BPGC and not child of a CDO
		UBlueprintGeneratedClass* BPGC = nullptr;
		for (UObject* OuterObject = ActorComponent->GetOuter(); OuterObject && !BPGC; OuterObject = OuterObject->GetOuter())
		{
			if (OuterObject->HasAnyFlags(RF_ClassDefaultObject))
			{
				return Outer;
			}
			BPGC = Cast<UBlueprintGeneratedClass>(OuterObject);
		}

		for (UBlueprintGeneratedClass* SuperBPGC = BPGC; SuperBPGC && (OutName == NAME_None); SuperBPGC = Cast<UBlueprintGeneratedClass>(SuperBPGC->GetSuperClass()))
		{
			if (SuperBPGC->InheritableComponentHandler)
			{
				FComponentKey FoundKey = SuperBPGC->InheritableComponentHandler->FindKey(ActorComponent);
				if (FoundKey.IsValid())
				{
					OutName = FoundKey.IsSCSKey() ? FoundKey.GetSCSVariableName() : ActorComponent->GetFName();
					Outer = BPGC->GetDefaultObject(false);
					break;
				}
			}
			if (SuperBPGC->SimpleConstructionScript)
			{
				for (auto Node : SuperBPGC->SimpleConstructionScript->GetAllNodes())
				{
					if (Node->ComponentTemplate == ActorComponent)
					{
						OutName = Node->VariableName;
						if (OutName != NAME_None)
						{
							Outer = BPGC->GetDefaultObject(false);
							break;
						}
					}
				}
			}
		}
	}

	if (Outer && (EReplacementResult::ReplaceCompletely == IsTargetedForReplacement(Object->GetClass())))
	{
		UE_LOG(LogBlueprintCodeGen, Log, TEXT("Object '%s' has replaced name '%s' and outer: '%s'"), *GetPathNameSafe(Object), *OutName.ToString(), *GetPathNameSafe(Outer));
		return Outer;
	}

	return nullptr;
}

EReplacementResult FBlueprintNativeCodeGenModule::IsTargetedForReplacement(const UPackage* Package) const
{
	// non-native packages with enums and structs should be converted, unless they are blacklisted:
	UStruct* Struct = nullptr;
	UEnum* Enum = nullptr;
	GetFieldFormPackage(Package, Struct, Enum, RF_NoFlags);

	UObject* Target = Struct;
	if (Target == nullptr)
	{
		Target = Enum;
	}
	return IsTargetedForReplacement(Target);
}

EReplacementResult FBlueprintNativeCodeGenModule::IsTargetedForReplacement(const UObject* Object) const
{
	const UStruct* Struct = Cast<UStruct>(Object);
	const UEnum* Enum = Cast<UEnum>(Object);

	if (Struct == nullptr && Enum == nullptr)
	{
		return EReplacementResult::DontReplace;
	}

	EReplacementResult Result = EReplacementResult::ReplaceCompletely;
	if (const UClass* BlueprintClass = Cast<UClass>(Struct))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
		{
			const EBlueprintType UnconvertableBlueprintTypes[] = {
				//BPTYPE_Const,		// WTF is a "const" Blueprint?
				BPTYPE_MacroLibrary,
				BPTYPE_LevelScript,
			};

			EBlueprintType BlueprintType = Blueprint->BlueprintType;
			for (int32 TypeIndex = 0; TypeIndex < ARRAY_COUNT(UnconvertableBlueprintTypes); ++TypeIndex)
			{
				if (BlueprintType == UnconvertableBlueprintTypes[TypeIndex])
				{
					Result = EReplacementResult::GenerateStub;
				}
			}

			static const FBoolConfigValueHelper DontNativizeDataOnlyBP(TEXT("BlueprintNativizationSettings"), TEXT("bDontNativizeDataOnlyBP"));
			if (DontNativizeDataOnlyBP)
			{
				if (FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint))
				{
					return EReplacementResult::DontReplace;
				}
			}

			for (UBlueprintGeneratedClass* ParentClassIt = Cast<UBlueprintGeneratedClass>(BlueprintClass->GetSuperClass())
				; ParentClassIt; ParentClassIt = Cast<UBlueprintGeneratedClass>(ParentClassIt->GetSuperClass()))
			{
				EReplacementResult ParentResult = IsTargetedForReplacement(ParentClassIt);
				if (ParentResult != EReplacementResult::ReplaceCompletely)
				{
					Result = EReplacementResult::GenerateStub;
				}
			}

			for (TAssetSubclassOf<UBlueprint> ExcludedBlueprintTypeAsset : ExcludedBlueprintTypes)
			{
				UClass* ExcludedBPClass = ExcludedBlueprintTypeAsset.Get();
				if (!ExcludedBPClass)
				{
					ExcludedBPClass = ExcludedBlueprintTypeAsset.LoadSynchronous();
				}
				if (ExcludedBPClass && Blueprint->IsA(ExcludedBPClass))
				{
					Result = EReplacementResult::GenerateStub;
				}
			}
		}
	}

	auto IsObjectFromDeveloperPackage = [](const UObject* Obj) -> bool
	{
		return Obj && Obj->GetOutermost()->HasAllPackagesFlags(PKG_Developer);
	};

	auto IsDeveloperObject = [&](const UObject* Obj) -> bool
	{
		if (Obj)
		{
			if (IsObjectFromDeveloperPackage(Obj))
			{
				return true;
			}
			const UStruct* StructToTest = Obj->IsA<UStruct>() ? CastChecked<const UStruct>(Obj) : Obj->GetClass();
			for (; StructToTest; StructToTest = StructToTest->GetSuperStruct())
			{
				if (IsObjectFromDeveloperPackage(StructToTest))
				{
					return true;
				}
			}
		}
		return false;
	};

	if (Object && (IsEditorOnlyObject(Object) || IsDeveloperObject(Object)))
	{
		UE_LOG(LogBlueprintCodeGen, Warning, TEXT("Object %s depends on Editor or Development stuff. Is shouldn't be cooked."), *GetPathNameSafe(Object));
		return EReplacementResult::DontReplace;
	}

	// check blacklists:
	// we can't use FindObject, because we may be converting a type while saving
	if (Enum && ExcludedAssetTypes.Find(Enum->GetPathName()) != INDEX_NONE)
	{
		Result = EReplacementResult::GenerateStub;
	}

	while (Struct)
	{
		if (ExcludedAssetTypes.Find(Struct->GetPathName()) != INDEX_NONE)
		{
			Result = EReplacementResult::GenerateStub;
		}
		Struct = Struct->GetSuperStruct();
	}

	if (ExcludedAssets.Contains(Object->GetOutermost()))
	{
		Result = EReplacementResult::GenerateStub;
	}

	return Result;
}

IMPLEMENT_MODULE(FBlueprintNativeCodeGenModule, BlueprintNativeCodeGen);