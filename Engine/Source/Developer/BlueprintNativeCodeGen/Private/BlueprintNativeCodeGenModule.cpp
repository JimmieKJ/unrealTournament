// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNativeCodeGenPCH.h"

#include "AssetRegistryModule.h"
#include "BlueprintNativeCodeGenManifest.h"
#include "BlueprintNativeCodeGenModule.h"
#include "BlueprintNativeCodeGenUtils.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "NativeCodeGenCommandlineParams.h"

/*******************************************************************************
* NativizationCookControllerImpl
******************************************************************************/

namespace NativizationCookControllerImpl
{
#if WITH_EDITORONLY_DATA
	static const FString ConvertAssetsCommand = TEXT("NativizeAssets");
	static const FString ConvertedManifestParam = TEXT("NativizedAssetManifest=");
	static const FString GeneratedPluginParam = TEXT("NativizedAssetPlugin=");

	//--------------------------------------------------------------------------
	static bool IsRunningCookCommandlet()
	{
		// @TODO: figure out how to determine if this is the cook commandlet
		return IsRunningCommandlet();
	}
#endif // WITH_EDITORONLY_DATA
}

/*******************************************************************************
* UBlueprintNativeCodeGenConfig
*******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintNativeCodeGenConfig::UBlueprintNativeCodeGenConfig(FObjectInitializer const& ObjectInitializer)
: Super(ObjectInitializer)
{
}

/*******************************************************************************
* FCookCommandParams
******************************************************************************/

//------------------------------------------------------------------------------
FCookCommandParams::FCookCommandParams(const FString& Commandline)
: bRunConversion(false)
{
	using namespace NativizationCookControllerImpl;

#if WITH_EDITORONLY_DATA
	if (IsRunningCookCommandlet())
	{
		IFileManager& FileManager = IFileManager::Get();
		bRunConversion = FParse::Param(*Commandline, *ConvertAssetsCommand);

		if (FParse::Value(*Commandline, *ConvertedManifestParam, ManifestFilePath))
		{
			bRunConversion |= !FileManager.FileExists(*ManifestFilePath);
		}

		FString PluginPath;
		if (FParse::Value(*Commandline, *GeneratedPluginParam, PluginPath))
		{
			if (PluginPath.EndsWith(TEXT(".uplugin")))
			{
				PluginName = FPaths::GetBaseFilename(PluginPath);
				PluginPath = FPaths::GetPath(PluginPath);
			}
			OutputPath = PluginPath;
			bRunConversion = true;
		}

		if (bRunConversion && ManifestFilePath.IsEmpty())
		{
			ManifestFilePath = FPaths::Combine(*FPaths::Combine(*FPaths::GameIntermediateDir(), TEXT("Cook")), TEXT("BlueprintConversionManifest"));
		}
	}
#endif // WITH_EDITORONLY_DATA
}

//------------------------------------------------------------------------------
TArray<FString> FCookCommandParams::ToConversionParams() const
{
	TArray<FString> ConversionCmdParams;
	if (!PluginName.IsEmpty())
	{
		ConversionCmdParams.Add(FString::Printf(TEXT("pluginName=%s"), *PluginName));
	}
	if (!OutputPath.IsEmpty())
	{
		ConversionCmdParams.Add(FString::Printf(TEXT("output=\"%s\""), *OutputPath));
	}
	if (!ManifestFilePath.IsEmpty())
	{
		ConversionCmdParams.Add(FString::Printf(TEXT("manifest=\"%s\""), *ManifestFilePath));
	}
	return ConversionCmdParams;
}

/*******************************************************************************
 * FBlueprintNativeCodeGenModule
 ******************************************************************************/
 
class FBlueprintNativeCodeGenModule : public IBlueprintNativeCodeGenModule
{
public:
	FBlueprintNativeCodeGenModule()
		: Manifest(nullptr)
	{
		FCookCommandParams CookParams(FCommandLine::Get());
		FNativeCodeGenCommandlineParams CommandLineParams(CookParams.ToConversionParams());
		Manifest = new FBlueprintNativeCodeGenManifest(CommandLineParams);

		IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		auto& BackendPCHQuery = BackEndModule.OnPCHFilenameQuery();

		FBlueprintNativeCodeGenPaths TargetPaths = GetManifest().GetTargetPaths();
		BackendPCHQuery.BindLambda([TargetPaths]()->FString
		{
			return TargetPaths.RuntimePCHFilename();
		});

		auto& ConversionQueryDelegate = BackEndModule.OnIsTargetedForConversionQuery();
		auto ShouldConvert = [](const UObject* AssetObj)
		{
			if (FScriptCookReplacementCoordinator::Get())
			{
				EReplacementResult ReplacmentResult = FScriptCookReplacementCoordinator::Get()->IsTargetedForReplacement(AssetObj);
				return ReplacmentResult == EReplacementResult::ReplaceCompletely;
			}
			return false;
		};
		ConversionQueryDelegate.BindStatic(ShouldConvert);
	}

	//~ Begin IBlueprintNativeCodeGenModule interface
	virtual void Convert(UPackage* Package, EReplacementResult ReplacementType) override;
	virtual void SaveManifest(const TCHAR* Filename) override;
	virtual void MergeManifest(const TCHAR* Filename) override;
	virtual void FinalizeManifest() override;
	//~ End IBlueprintNativeCodeGenModule interface
private:
	FBlueprintNativeCodeGenManifest& GetManifest();

	FBlueprintNativeCodeGenManifest* Manifest;
};

FBlueprintNativeCodeGenManifest& FBlueprintNativeCodeGenModule::GetManifest()
{
	check(Manifest);
	return *Manifest;
}

void FBlueprintNativeCodeGenModule::Convert(UPackage* Package, EReplacementResult ReplacementType)
{
	// Find the struct/enum to convert:
	UStruct* Struct = nullptr;
	UEnum* Enum = nullptr;
	TArray<UObject*> Objects;
	GetObjectsWithOuter(Package, Objects, false);
	for (auto Entry : Objects)
	{
		if (Entry->HasAnyFlags(RF_Transient))
		{
			continue;
		}

		Struct = Cast<UStruct>(Entry);
		if (Struct)
		{
			break;
		}

		Enum = Cast<UEnum>(Entry);
		if (Enum)
		{
			break;
		}
	}

	if (Struct == nullptr && Enum == nullptr)
	{
		check(false);
		return;
	}

	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	auto GenerateWrapperSrcFile = [](const FUnconvertedDependencyRecord& UnconvertedDependency, UClass* Class)
	{
		FString FileContents;
		TUniquePtr<IBlueprintCompilerCppBackend> Backend_CPP(IBlueprintCompilerCppBackendModuleInterface::Get().Create());
		FileContents = Backend_CPP->GenerateWrapperForClass(Class);

		if (!FileContents.IsEmpty())
		{
			FFileHelper::SaveStringToFile(FileContents, *UnconvertedDependency.GeneratedWrapperPath);
		}
	};

	if (ReplacementType == EReplacementResult::GenerateStub)
	{
		check(ReplacementType == EReplacementResult::GenerateStub);
		FAssetData AssetInfo = Registry.GetAssetByObjectPath(*Struct->GetPathName());
		GenerateWrapperSrcFile(GetManifest().CreateUnconvertedDependencyRecord(AssetInfo.PackageName, AssetInfo, FName()), CastChecked<UClass>(Struct));
		// The stub we generate still may have dependencies on other modules, so make sure the module dependencies are 
		// still recorded so that the .build.cs is generated correctly. Without this you'll get include related errors 
		// (or possibly linker errors) in stub headers:
		GetManifest().GatherModuleDependencies(Package);
	}
	else
	{
		check(ReplacementType == EReplacementResult::ReplaceCompletely);
		// convert:
		UField* ForConversion = Enum;
		if (ForConversion == nullptr)
		{
			ForConversion = Struct;
		}

		FAssetData AssetInfo = Registry.GetAssetByObjectPath(*ForConversion->GetPathName());
		FConvertedAssetRecord& ConversionRecord = GetManifest().CreateConversionRecord(ForConversion->GetFName(), AssetInfo);
		TSharedPtr<FString> HeaderSource(new FString());
		TSharedPtr<FString> CppSource(new FString());

		FBlueprintNativeCodeGenUtils::GenerateCppCode(ForConversion, HeaderSource, CppSource);
		bool bSuccess = !HeaderSource->IsEmpty() || !CppSource->IsEmpty();
		// Run the cpp first, because we cue off of the presence of a header for a valid conversion record (see
		// FConvertedAssetRecord::IsValid)
		if (!CppSource->IsEmpty())
		{
			if (!FFileHelper::SaveStringToFile(*CppSource, *ConversionRecord.GeneratedCppPath))
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
			if (!FFileHelper::SaveStringToFile(*HeaderSource, *ConversionRecord.GeneratedHeaderPath))
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

		check(bSuccess);
		if (bSuccess)
		{
			GetManifest().GatherModuleDependencies(Package);
		}
	}
}

void FBlueprintNativeCodeGenModule::SaveManifest(const TCHAR* Filename)
{
	GetManifest().SaveAs(Filename);
}

void FBlueprintNativeCodeGenModule::MergeManifest(const TCHAR* Filename)
{
	GetManifest().Merge(Filename);
} 

void FBlueprintNativeCodeGenModule::FinalizeManifest()
{
	GetManifest().Save();

	FCookCommandParams CookParams(FCommandLine::Get());
	FNativeCodeGenCommandlineParams CommandLineParams(CookParams.ToConversionParams());
	check(FPaths::IsSamePath(CommandLineParams.ManifestFilePath, CommandLineParams.ManifestFilePath));
	check(FBlueprintNativeCodeGenUtils::FinalizePlugin(GetManifest(), CommandLineParams));
}

IMPLEMENT_MODULE(FBlueprintNativeCodeGenModule, BlueprintNativeCodeGen);