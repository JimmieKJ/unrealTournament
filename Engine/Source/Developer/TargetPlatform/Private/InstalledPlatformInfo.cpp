// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TargetPlatformPrivatePCH.h"
#include "InstalledPlatformInfo.h"

DEFINE_LOG_CATEGORY_STATIC(LogInstalledPlatforms, Log, All);

EProjectType EProjectTypeFromString(const FString& ProjectTypeName)
{
	if (FCString::Strcmp(*ProjectTypeName, TEXT("Any")) == 0)
	{
		return EProjectType::Any;
	}
	else if (FCString::Strcmp(*ProjectTypeName, TEXT("Code")) == 0)
	{
		return EProjectType::Code;
	}
	else if (FCString::Strcmp(*ProjectTypeName, TEXT("Content")) == 0)
	{
		return EProjectType::Content;
	}
	else
	{
		return EProjectType::Unknown;
	}
}

FInstalledPlatformInfo::FInstalledPlatformInfo()
{
	TArray<FString> InstalledPlatforms;
	GConfig->GetArray(TEXT("InstalledPlatforms"), TEXT("InstalledPlatformConfigurations"), InstalledPlatforms, GEngineIni);

	for (FString& InstalledPlatform : InstalledPlatforms)
	{
		ParsePlatformConfiguration(InstalledPlatform);
	}
}

void FInstalledPlatformInfo::ParsePlatformConfiguration(FString PlatformConfiguration)
{
	// Trim whitespace at the beginning.
	PlatformConfiguration = PlatformConfiguration.Trim();
	// Remove brackets.
	PlatformConfiguration.RemoveFromStart(TEXT("("));
	PlatformConfiguration.RemoveFromEnd(TEXT(")"));

	bool bCanCreateEntry = true;

	FString ConfigurationName;
	EBuildConfigurations::Type Configuration = EBuildConfigurations::Unknown;
	if (FParse::Value(*PlatformConfiguration, TEXT("Configuration="), ConfigurationName))
	{
		Configuration = EBuildConfigurations::FromString(ConfigurationName);
	}
	if (Configuration == EBuildConfigurations::Unknown)
	{
		UE_LOG(LogInstalledPlatforms, Warning, TEXT("Unable to read configuration from %s"), *PlatformConfiguration);
		bCanCreateEntry = false;
	}

	FString	PlatformName;
	if (!FParse::Value(*PlatformConfiguration, TEXT("PlatformName="), PlatformName))
	{
		UE_LOG(LogInstalledPlatforms, Warning, TEXT("Unable to read platform from %s"), *PlatformConfiguration);
		bCanCreateEntry = false;
	}

	FString RequiredFile;
	if (FParse::Value(*PlatformConfiguration, TEXT("RequiredFile="), RequiredFile))
	{
		RequiredFile = FPaths::Combine(*FPaths::RootDir(), *RequiredFile);
	}

	FString ProjectTypeName;
	EProjectType ProjectType =  EProjectType::Any;
	if (FParse::Value(*PlatformConfiguration, TEXT("ProjectType="), ProjectTypeName))
	{
		ProjectType = EProjectTypeFromString(ProjectTypeName);
	}
	if (ProjectType == EProjectType::Unknown)
	{
		UE_LOG(LogInstalledPlatforms, Warning, TEXT("Unable to read project type from %s"), *PlatformConfiguration);
		bCanCreateEntry = false;
	}

	bool bCanBeDisplayed = false;
	FParse::Bool(*PlatformConfiguration, TEXT("bCanBeDisplayed="), bCanBeDisplayed);
	
	if (bCanCreateEntry)
	{
		FInstalledPlatformConfiguration NewConfig = {Configuration, PlatformName, RequiredFile, ProjectType, bCanBeDisplayed};
		InstalledPlatformConfigurations.Add(NewConfig);
	}
}

bool FInstalledPlatformInfo::IsValidConfiguration(const EBuildConfigurations::Type Configuration, EProjectType ProjectType) const
{
	if (FApp::IsEngineInstalled())
	{
		for (const FInstalledPlatformConfiguration& PlatformConfiguration : InstalledPlatformConfigurations)
		{
			if (PlatformConfiguration.Configuration == Configuration)
			{
				if (ProjectType == EProjectType::Any || PlatformConfiguration.ProjectType == EProjectType::Any
				 || PlatformConfiguration.ProjectType == ProjectType)
				{
					if (PlatformConfiguration.RequiredFile.IsEmpty()
						|| FPaths::FileExists(PlatformConfiguration.RequiredFile))
					{
						return true;
					}
				}
			}
		}

		return false;
	}
	return true;
}

bool FInstalledPlatformInfo::IsValidPlatform(const FString& PlatformName, EProjectType ProjectType) const
{
	if (FApp::IsEngineInstalled())
	{
		for (const FInstalledPlatformConfiguration& PlatformConfiguration : InstalledPlatformConfigurations)
		{
			if (PlatformConfiguration.PlatformName == PlatformName)
			{
				if (ProjectType == EProjectType::Any || PlatformConfiguration.ProjectType == EProjectType::Any
				 || PlatformConfiguration.ProjectType == ProjectType)
				{
					if (PlatformConfiguration.RequiredFile.IsEmpty()
						|| FPaths::FileExists(PlatformConfiguration.RequiredFile))
					{
						return true;
					}
				}
			}
		}

		return false;
	}
	return true;
}

bool FInstalledPlatformInfo::IsValidPlatformAndConfiguration(const EBuildConfigurations::Type Configuration, const FString& PlatformName, EProjectType ProjectType) const
{
	if (FApp::IsEngineInstalled())
	{
		for (const FInstalledPlatformConfiguration& PlatformConfiguration : InstalledPlatformConfigurations)
		{
			if (PlatformConfiguration.Configuration == Configuration
			 && PlatformConfiguration.PlatformName == PlatformName)
			{
				if (ProjectType == EProjectType::Any || PlatformConfiguration.ProjectType == EProjectType::Any
				 || PlatformConfiguration.ProjectType == ProjectType)
				{
					if (PlatformConfiguration.RequiredFile.IsEmpty()
					 || FPaths::FileExists(PlatformConfiguration.RequiredFile))
					{
						return true;
					}
				}
			}
		}

		return false;
	}
	return true;
}

bool FInstalledPlatformInfo::CanDisplayPlatform(const FString& PlatformName, EProjectType ProjectType) const
{
	if (FApp::IsEngineInstalled())
	{
		for (const FInstalledPlatformConfiguration& PlatformConfiguration : InstalledPlatformConfigurations)
		{
			if (PlatformConfiguration.PlatformName == PlatformName)
			{
				if ( PlatformConfiguration.bCanBeDisplayed
					|| ProjectType == EProjectType::Any || PlatformConfiguration.ProjectType == EProjectType::Any
					|| PlatformConfiguration.ProjectType == ProjectType )
				{
					return true;
				}
			}
		}

		return false;
	}
	return true;
}

