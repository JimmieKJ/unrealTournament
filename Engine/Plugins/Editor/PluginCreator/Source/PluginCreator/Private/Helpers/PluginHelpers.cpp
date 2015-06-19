// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginCreatorPrivatePCH.h"
#include "PluginHelpers.h"
#include "GameProjectUtils.h"

#define LOCTEXT_NAMESPACE "PluginHelpers"

bool FPluginHelpers::ReadTemplateFile(const FString& TemplateFileName, FString& OutFileContents, FText& OutFailReason)
{
	const FString FullFileName = GetPluginCreatorRootPath() / TEXT("Templates") / TemplateFileName;
	if (FFileHelper::LoadFileToString(OutFileContents, *FullFileName))
	{
		return true;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("FullFileName"), FText::FromString(FullFileName));
	OutFailReason = FText::Format(LOCTEXT("FailedToReadTemplateFile", "Failed to read template file \"{FullFileName}\""), Args);
	return false;
}

bool FPluginHelpers::CreatePluginBuildFile(const FString& NewBuildFileName, const FString& PluginName, FText& OutFailReason, FString TemplateType)
{
	FString Template;

	FString TemplateFileName = TEXT("PluginModule.Build.cs.template");

	if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
	{
		return false;
	}

	TArray<FString> PublicIncludePathsNames;
	TArray<FString> PrivateIncludePathsNames;
	TArray<FString> PublicDependencyModuleNames;
	TArray<FString> PrivateDependencyModuleNames;
	TArray<FString> DynamicallyLoadedModuleNames;

	PrivateDependencyModuleNames.Add("Slate");
	PrivateDependencyModuleNames.Add("SlateCore");

	FString FinalOutput = Template.Replace(TEXT("%PUBLIC_INCLUDE_PATHS_NAMES%"), *GameProjectUtils::MakeCommaDelimitedList(PublicIncludePathsNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PRIVATE_INCLUDE_PATHS_NAMES%"), *GameProjectUtils::MakeCommaDelimitedList(PrivateIncludePathsNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_DEPENDENCY_MODULE_NAMES%"), *GameProjectUtils::MakeCommaDelimitedList(PublicDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PRIVATE_DEPENDENCY_MODULE_NAMES%"), *GameProjectUtils::MakeCommaDelimitedList(PrivateDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%DYNAMICALLY_LOADED_MODULES_NAMES%"), *GameProjectUtils::MakeCommaDelimitedList(DynamicallyLoadedModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *PluginName, ESearchCase::CaseSensitive);

	return GameProjectUtils::WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool FPluginHelpers::CreatePluginHeaderFile(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType)
{
	FString Template;

	FString TemplateFileName = TEXT("PluginModule.h.template");

	if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
	{
		return false;
	}

	TArray<FString> PublicHeaderIncludes;

	FString	FinalOutput = Template.Replace(TEXT("%PUBLIC_HEADER_INCLUDES%"), *GameProjectUtils::MakeIncludeList(PublicHeaderIncludes), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PLUGIN_NAME%"), *PluginName, ESearchCase::CaseSensitive);

	return GameProjectUtils::WriteOutputFile(FolderPath / PluginName + TEXT(".h"), FinalOutput, OutFailReason);
}

bool FPluginHelpers::CreatePluginCPPFile(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType)
{
	FString Template;

	FString TemplateFileName = TEXT("PluginModule.cpp.template");

	if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
	{
		return false;
	}

	TArray<FString> PublicHeaderIncludes;

	FString	FinalOutput = Template.Replace(TEXT("%PUBLIC_HEADER_INCLUDES%"), *GameProjectUtils::MakeIncludeList(PublicHeaderIncludes), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PLUGIN_NAME%"), *PluginName, ESearchCase::CaseSensitive);

	return GameProjectUtils::WriteOutputFile(FolderPath / PluginName + TEXT(".cpp"), FinalOutput, OutFailReason);
}

bool FPluginHelpers::CreatePrivatePCHFile(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType)
{
	FString Template;
	FString TemplateFileName = TEXT("PluginModulePCH.h.template");

	if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
	{
		return false;
	}

	TArray<FString> PublicHeaderIncludes;

	FString	FinalOutput = Template.Replace(TEXT("%PLUGIN_NAME%"), *PluginName, ESearchCase::CaseSensitive);

	return GameProjectUtils::WriteOutputFile(FolderPath / PluginName + TEXT("PrivatePCH.h"), FinalOutput, OutFailReason);
}


bool FPluginHelpers::SavePluginDescriptor(const FString& NewProjectFilename, const FPluginDescriptor& PluginDescriptor)
{
	FString Text;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&Text);

	Writer->WriteObjectStart();

	// Write all the simple fields

	Writer->WriteValue(TEXT("FileVersion"), PluginDescriptor.FileVersion);

	Writer->WriteValue(TEXT("FriendlyName"), PluginDescriptor.FriendlyName);
	Writer->WriteValue(TEXT("Version"), PluginDescriptor.Version);
	Writer->WriteValue(TEXT("VersionName"), PluginDescriptor.VersionName);
	Writer->WriteValue(TEXT("CreatedBy"), PluginDescriptor.CreatedBy);
	Writer->WriteValue(TEXT("CreatedByURL"), PluginDescriptor.CreatedByURL);
	Writer->WriteValue(TEXT("Category"), PluginDescriptor.Category);
	Writer->WriteValue(TEXT("Description"), PluginDescriptor.Description);
	Writer->WriteValue(TEXT("EnabledByDefault"), PluginDescriptor.bEnabledByDefault);


	// Write the module list
	FModuleDescriptor::WriteArray(Writer.Get(), TEXT("Modules"), PluginDescriptor.Modules);

	Writer->WriteObjectEnd();

	Writer->Close();

	return FFileHelper::SaveStringToFile(Text, *NewProjectFilename);
}

bool FPluginHelpers::CreatePluginStyleFiles(const FString& PrivateFolderPath, const FString& PublicFolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType)
{
	{
		// Make Style.h file first

		FString Template;
		FString TemplateFileName = TEXT("PluginStyle.h.template");

		if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
		{
			return false;
		}

		TArray<FString> PublicHeaderIncludes;

		FString	FinalOutput = Template.Replace(TEXT("%PLUGIN_NAME%"), *PluginName, ESearchCase::CaseSensitive);

		if (!GameProjectUtils::WriteOutputFile(PublicFolderPath / PluginName + TEXT("Style.h"), FinalOutput, OutFailReason))
		{
			return false;
		}
	}

	{
		// Now .cpp file

		FString Template;

		FString TemplateFileName = TEXT("PluginStyle.cpp.template");

		if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
		{
			return false;
		}

		TArray<FString> PublicHeaderIncludes;

		FString	FinalOutput = Template.Replace(TEXT("%PLUGIN_NAME%"), *PluginName, ESearchCase::CaseSensitive);

		if (!GameProjectUtils::WriteOutputFile(PrivateFolderPath / PluginName + TEXT("Style.cpp"), FinalOutput, OutFailReason))
		{
			return false;
		}
	}

	return true;
	
}

bool FPluginHelpers::CreateCommandsFiles(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType)
{
	{
		// Make Commands.h file first
		FString Template;

		FString TemplateFileName = TEXT("PluginCommands.h.template");

		if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
		{
			return false;
		}

		TArray<FString> PublicHeaderIncludes;
		FString ButtonLabel = TEXT("BUTTON LABEL");

		FString	FinalOutput = Template.Replace(TEXT("%PLUGIN_NAME%"), *PluginName, ESearchCase::CaseSensitive);
		FinalOutput = FinalOutput.Replace(TEXT("%PLUGIN_BUTTON_LABEL%"), *PluginName, ESearchCase::CaseSensitive);

		if (!GameProjectUtils::WriteOutputFile(FolderPath / PluginName + TEXT("Commands.h"), FinalOutput, OutFailReason))
		{
			return false;
		}
	}

	{
		// Now .cpp file
		FString Template;

		FString TemplateFileName = TEXT("PluginCommands.cpp.template");

		if (!ReadTemplateFile(TemplateType.IsEmpty() ? TemplateFileName : TemplateType / TemplateFileName, Template, OutFailReason))
		{
			return false;
		}

		TArray<FString> PublicHeaderIncludes;
		FString ButtonLabel = TEXT("BUTTON LABEL");

		FString	FinalOutput = Template.Replace(TEXT("%PLUGIN_NAME%"), *PluginName, ESearchCase::CaseSensitive);
		FinalOutput = FinalOutput.Replace(TEXT("%PLUGIN_BUTTON_LABEL%"), *PluginName, ESearchCase::CaseSensitive);

		if (!GameProjectUtils::WriteOutputFile(FolderPath / PluginName + TEXT("Commands.cpp"), FinalOutput, OutFailReason))
		{
			return false;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE