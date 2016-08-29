// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNativeCodeGenPCH.h"
#include "BlueprintNativeCodeGenManifest.h"
#include "BlueprintNativeCodeGenUtils.h"
#include "Kismet2/KismetReinstanceUtilities.h"	 // for FBlueprintCompileReinstancer
#include "Kismet2/CompilerResultsLog.h" 
#include "KismetCompilerModule.h"
#include "Engine/Blueprint.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "Kismet2/KismetEditorUtilities.h"		 // for CompileBlueprint()
#include "OutputDevice.h"						 // for GWarn
#include "GameProjectUtils.h"					 // for GenerateGameModuleBuildFile
#include "Editor/GameProjectGeneration/Public/GameProjectUtils.h" // for GenerateGameModuleBuildFile()
#include "App.h"								 // for GetGameName()
#include "BlueprintSupport.h"					 // for FReplaceCookedBPGC
#include "IBlueprintCompilerCppBackendModule.h"	 // for OnPCHFilenameQuery()
#include "BlueprintNativeCodeGenModule.h"
#include "ScopeExit.h"
#include "Editor/Kismet/Public/FindInBlueprintManager.h" // for FDisableGatheringDataOnScope
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h" // for FBlueprintEditorUtils::FForceFastBlueprintDuplicationScope

DEFINE_LOG_CATEGORY(LogBlueprintCodeGen)

/*******************************************************************************
 * BlueprintNativeCodeGenUtilsImpl
 ******************************************************************************/

namespace BlueprintNativeCodeGenUtilsImpl
{
	static FString CoreModuleName   = TEXT("Core");
	static FString EngineModuleName = TEXT("Engine");
	static FString EngineHeaderFile = TEXT("Engine.h");

	/**
	 * Deletes the files/directories in the supplied array.
	 * 
	 * @param  TargetPaths    The set of directory and file paths that you want deleted.
	 * @return True if all the files/directories were successfully deleted, other wise false.
	 */
	static bool WipeTargetPaths(const FBlueprintNativeCodeGenPaths& TargetPaths);
	
	/**
	 * Creates and fills out a new .uplugin file for the converted assets.
	 * 
	 * @param  PluginName	The name of the plugin you're generating.
	 * @param  TargetPaths	Defines the file path/name for the plugin file.
	 * @return True if the file was successfully saved, otherwise false.
	 */
	static bool GeneratePluginDescFile(const FString& PluginName, const FBlueprintNativeCodeGenPaths& TargetPaths);

	/**
	 * Creates a module implementation and header file for the converted assets'
	 * module (provides a IMPLEMENT_MODULE() declaration, which is required for 
	 * the module to function).
	 * 
	 * @param  TargetPaths    Defines the file path/name for the target files.
	 * @return True if the files were successfully generated, otherwise false.
	 */
	static bool GenerateModuleSourceFiles(const FBlueprintNativeCodeGenPaths& TargetPaths);

	/**
	 * Creates and fills out a new .Build.cs file for the plugin's runtime module.
	 * 
	 * @param  Manifest    Defines where the module file should be saved, what it should be named, etc..
	 * @return True if the file was successfully saved, otherwise false.
	 */
	static bool GenerateModuleBuildFile(const FBlueprintNativeCodeGenManifest& Manifest);

	/**
	 * Determines what the expected native class will be for an asset that was  
	 * or will be converted.
	 * 
	 * @param  ConversionRecord    Identifies the asset's original type (which is used to infer the replacement type from).
	 * @return Either a class, enum, or struct class (depending on the asset's type).
	 */
	static UClass* ResolveReplacementType(const FConvertedAssetRecord& ConversionRecord);
}

//------------------------------------------------------------------------------
static bool BlueprintNativeCodeGenUtilsImpl::WipeTargetPaths(const FBlueprintNativeCodeGenPaths& TargetPaths)
{
	IFileManager& FileManager = IFileManager::Get();

	bool bSuccess = true;
	bSuccess &= FileManager.Delete(*TargetPaths.PluginFilePath());
	bSuccess &= FileManager.DeleteDirectory(*TargetPaths.PluginSourceDir(), /*RequireExists =*/false, /*Tree =*/true);
	bSuccess &= FileManager.Delete(*TargetPaths.ManifestFilePath());

	return bSuccess;
}

//------------------------------------------------------------------------------
static bool BlueprintNativeCodeGenUtilsImpl::GeneratePluginDescFile(const FString& PluginName, const FBlueprintNativeCodeGenPaths& TargetPaths)
{
	FPluginDescriptor PluginDesc;
	PluginDesc.FriendlyName = PluginName;
	PluginDesc.CreatedBy    = TEXT("Epic Games, Inc.");
	PluginDesc.CreatedByURL = TEXT("http://epicgames.com");
	PluginDesc.Description  = TEXT("A programatically generated plugin which contains source files produced from Blueprint assets. The aim of this is to help performance by eliminating script overhead for the converted assets (using the source files in place of thier coresponding assets).");
	PluginDesc.DocsURL      = TEXT("@TODO");
	PluginDesc.SupportURL   = TEXT("https://answers.unrealengine.com/");
	PluginDesc.Category     = TEXT("Intermediate");
	PluginDesc.bEnabledByDefault  = true;
	PluginDesc.bCanContainContent = false;
	PluginDesc.bIsBetaVersion     = true; // @TODO: change once we're confident in the feature

	FModuleDescriptor RuntimeModuleDesc;
	RuntimeModuleDesc.Name = *TargetPaths.RuntimeModuleName();
	RuntimeModuleDesc.Type = EHostType::Runtime;
	// load at startup (during engine init), after game modules have been loaded 
	RuntimeModuleDesc.LoadingPhase = ELoadingPhase::Default;

	PluginDesc.Modules.Add(RuntimeModuleDesc);

	FText ErrorMessage;
	bool bSuccess = PluginDesc.Save(TargetPaths.PluginFilePath(), ErrorMessage);

	if (!bSuccess)
	{
		UE_LOG(LogBlueprintCodeGen, Error, TEXT("Failed to generate the plugin description file: %s"), *ErrorMessage.ToString());
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
static bool BlueprintNativeCodeGenUtilsImpl::GenerateModuleSourceFiles(const FBlueprintNativeCodeGenPaths& TargetPaths)
{
	FText FailureReason;

	TArray<FString> PchIncludes;
	PchIncludes.Add(EngineHeaderFile);
	PchIncludes.Add(TEXT("GeneratedCodeHelpers.h"));

	TArray<FString> FilesToIncludeInModuleHeader;
	GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("FilesToIncludeInModuleHeader"), FilesToIncludeInModuleHeader, GEditorIni);
	PchIncludes.Append(FilesToIncludeInModuleHeader);

	bool bSuccess = GameProjectUtils::GeneratePluginModuleHeaderFile(TargetPaths.RuntimeModuleFile(FBlueprintNativeCodeGenPaths::HFile), PchIncludes, FailureReason);

	if (bSuccess)
	{
		const FString NoStartupCode = TEXT("");
		bSuccess &= GameProjectUtils::GeneratePluginModuleCPPFile(TargetPaths.RuntimeModuleFile(FBlueprintNativeCodeGenPaths::CppFile),
			TargetPaths.RuntimeModuleName(), NoStartupCode, FailureReason);
	}

	if (!bSuccess)
	{
		UE_LOG(LogBlueprintCodeGen, Error, TEXT("Failed to generate module source files: %s"), *FailureReason.ToString());
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
static bool BlueprintNativeCodeGenUtilsImpl::GenerateModuleBuildFile(const FBlueprintNativeCodeGenManifest& Manifest)
{
	FModuleManager& ModuleManager = FModuleManager::Get();
	
	TArray<FString> PublicDependencies;
	// for IModuleInterface
	PublicDependencies.Add(CoreModuleName);
	// for Engine.h
	PublicDependencies.Add(EngineModuleName);

	if (GameProjectUtils::ProjectHasCodeFiles()) 
	{
		const FString GameModuleName = FApp::GetGameName();
		if (ModuleManager.ModuleExists(*GameModuleName))
		{
			PublicDependencies.Add(GameModuleName);
		}
	}

	TArray<FString> PrivateDependencies;

	const TArray<UPackage*>& ModulePackages = Manifest.GetModuleDependencies();
	PrivateDependencies.Reserve(ModulePackages.Num());

	for (UPackage* ModulePkg : ModulePackages)
	{
		const FString PkgModuleName = FPackageName::GetLongPackageAssetName(ModulePkg->GetName());
		if (ModuleManager.ModuleExists(*PkgModuleName))
		{
			PrivateDependencies.Add(PkgModuleName);
		}
		else
		{
			UE_LOG(LogBlueprintCodeGen, Warning, TEXT("Failed to find module for package: %s"), *PkgModuleName);
		}
	}

	FBlueprintNativeCodeGenPaths TargetPaths = Manifest.GetTargetPaths();

	FText ErrorMessage;
	bool bSuccess = GameProjectUtils::GenerateGameModuleBuildFile(TargetPaths.RuntimeBuildFile(), TargetPaths.RuntimeModuleName(),
		PublicDependencies, PrivateDependencies, ErrorMessage);

	if (!bSuccess)
	{
		UE_LOG(LogBlueprintCodeGen, Error, TEXT("Failed to generate module build file: %s"), *ErrorMessage.ToString());
	}
	return bSuccess;
}

//------------------------------------------------------------------------------
static UClass* BlueprintNativeCodeGenUtilsImpl::ResolveReplacementType(const FConvertedAssetRecord& ConversionRecord)
{
	const UClass* AssetType = ConversionRecord.AssetType;
	check(AssetType != nullptr);

	if (AssetType->IsChildOf<UUserDefinedEnum>())
	{
		return UEnum::StaticClass();
	}
	else if (AssetType->IsChildOf<UUserDefinedStruct>())
	{
		return UScriptStruct::StaticClass();
	}
	else if (AssetType->IsChildOf<UBlueprint>())
	{
		return UDynamicClass::StaticClass();
	}
	else
	{
		UE_LOG(LogBlueprintCodeGen, Error, TEXT("Unsupported asset type (%s); cannot determine replacement type."), *AssetType->GetName());
	}
	return nullptr;
}

/*******************************************************************************
 * FBlueprintNativeCodeGenUtils
 ******************************************************************************/

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenUtils::FinalizePlugin(const FBlueprintNativeCodeGenManifest& Manifest)
{
	bool bSuccess = true;
	FBlueprintNativeCodeGenPaths TargetPaths = Manifest.GetTargetPaths();
	bSuccess = bSuccess && BlueprintNativeCodeGenUtilsImpl::GenerateModuleBuildFile(Manifest);
	bSuccess = bSuccess && BlueprintNativeCodeGenUtilsImpl::GenerateModuleSourceFiles(TargetPaths);
	bSuccess = bSuccess && BlueprintNativeCodeGenUtilsImpl::GeneratePluginDescFile(TargetPaths.RuntimeModuleName(), TargetPaths);
	return bSuccess;
}

//------------------------------------------------------------------------------
void FBlueprintNativeCodeGenUtils::GenerateCppCode(UObject* Obj, TSharedPtr<FString> OutHeaderSource, TSharedPtr<FString> OutCppSource, TSharedPtr<FNativizationSummary> NativizationSummary)
{
	auto UDEnum = Cast<UUserDefinedEnum>(Obj);
	auto UDStruct = Cast<UUserDefinedStruct>(Obj);
	auto BPGC = Cast<UClass>(Obj);
	auto InBlueprintObj = BPGC ? Cast<UBlueprint>(BPGC->ClassGeneratedBy) : Cast<UBlueprint>(Obj);

	OutHeaderSource->Empty();
	OutCppSource->Empty();

	if (InBlueprintObj)
	{
		if (EBlueprintStatus::BS_Error == InBlueprintObj->Status)
		{
			UE_LOG(LogBlueprintCodeGen, Error, TEXT("Cannot convert \"%s\". It has errors."), *InBlueprintObj->GetPathName());
			return;
		}

		check(InBlueprintObj->GetOutermost() != GetTransientPackage());
		if (!ensureMsgf(InBlueprintObj->GeneratedClass, TEXT("Invalid generated class for %s"), *InBlueprintObj->GetName()))
		{
			return;
		}
		check(OutHeaderSource.IsValid());
		check(OutCppSource.IsValid());

		FDisableGatheringDataOnScope DisableFib;

		const FString TempPackageName = FString::Printf(TEXT("/Temp/__TEMP_BP__%s"), *InBlueprintObj->GetOutermost()->GetPathName());
		UPackage* TempPackage = CreatePackage(nullptr, *TempPackageName);
		check(TempPackage);
		ON_SCOPE_EXIT
		{
			TempPackage->RemoveFromRoot();
			TempPackage->MarkPendingKill();
		};

		UBlueprint* DuplicateBP = nullptr;
		{
			FBlueprintDuplicationScopeFlags BPDuplicationFlags(FBlueprintDuplicationScopeFlags::NoExtraCompilation 
				| FBlueprintDuplicationScopeFlags::TheSameTimelineGuid 
				| FBlueprintDuplicationScopeFlags::ValidatePinsUsingSourceClass
				| FBlueprintDuplicationScopeFlags::TheSameNodeGuid);
			DuplicateBP = DuplicateObject<UBlueprint>(InBlueprintObj, TempPackage, *InBlueprintObj->GetName());
		}
		ensure((nullptr != DuplicateBP->GeneratedClass) && (InBlueprintObj->GeneratedClass != DuplicateBP->GeneratedClass));
		ON_SCOPE_EXIT
		{
			DuplicateBP->RemoveFromRoot();
			DuplicateBP->MarkPendingKill();
		};

		IBlueprintCompilerCppBackendModule& CodeGenBackend = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		CodeGenBackend.GetOriginalClassMap().Add(*DuplicateBP->GeneratedClass, *InBlueprintObj->GeneratedClass);
		CodeGenBackend.NativizationSummary() = NativizationSummary;

		{
			TSharedPtr<FBlueprintCompileReinstancer> Reinstancer = FBlueprintCompileReinstancer::Create(DuplicateBP->GeneratedClass);
			IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
			TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
			FCompilerResultsLog Results;

			FKismetCompilerOptions CompileOptions;
			CompileOptions.CompileType = EKismetCompileType::Cpp;
			CompileOptions.OutCppSourceCode = OutCppSource;
			CompileOptions.OutHeaderSourceCode = OutHeaderSource;

			Compiler.CompileBlueprint(DuplicateBP, CompileOptions, Results);

			Compiler.RemoveBlueprintGeneratedClasses(DuplicateBP);
		}

		if (EBlueprintType::BPTYPE_Interface == DuplicateBP->BlueprintType && OutCppSource.IsValid())
		{
			OutCppSource->Empty(); // ugly temp hack
		}
	}
	else if ((UDEnum || UDStruct) && OutHeaderSource.IsValid())
	{
		IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
		if (UDEnum)
		{
			*OutHeaderSource = Compiler.GenerateCppCodeForEnum(UDEnum);
		}
		else if (UDStruct)
		{
			*OutHeaderSource = Compiler.GenerateCppCodeForStruct(UDStruct);
		}
	}
	else
	{
		ensure(false);
	}
}

/*******************************************************************************
 * FScopedFeedbackContext
 ******************************************************************************/

//------------------------------------------------------------------------------
FBlueprintNativeCodeGenUtils::FScopedFeedbackContext::FScopedFeedbackContext()
	: OldContext(GWarn)
	, ErrorCount(0)
	, WarningCount(0)
{
	TreatWarningsAsErrors = GWarn->TreatWarningsAsErrors;
	GWarn = this;
}

//------------------------------------------------------------------------------
FBlueprintNativeCodeGenUtils::FScopedFeedbackContext::~FScopedFeedbackContext()
{
	GWarn = OldContext;
}

//------------------------------------------------------------------------------
bool FBlueprintNativeCodeGenUtils::FScopedFeedbackContext::HasErrors()
{
	return (ErrorCount > 0) || (TreatWarningsAsErrors && (WarningCount > 0));
}

//------------------------------------------------------------------------------
void FBlueprintNativeCodeGenUtils::FScopedFeedbackContext::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	switch (Verbosity)
	{
	case ELogVerbosity::Warning:
		{
			++WarningCount;
		} 
		break;

	case ELogVerbosity::Error:
	case ELogVerbosity::Fatal:
		{
			++ErrorCount;
		} 
		break;

	default:
		break;
	}

	OldContext->Serialize(V, Verbosity, Category);
}

//------------------------------------------------------------------------------
void FBlueprintNativeCodeGenUtils::FScopedFeedbackContext::Flush()
{
	WarningCount = ErrorCount = 0;
	OldContext->Flush();
}
