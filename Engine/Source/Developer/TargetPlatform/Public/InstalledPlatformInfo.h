// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EProjectType : uint8
{
	Unknown,
	Any,
	Code,
	Content,
};

EProjectType EProjectTypeFromString(const FString& ProjectTypeName);

/**
 * Information about a single installed platform configuration
 */
struct FInstalledPlatformConfiguration
{
	/** Build Configuration of this combination */
	EBuildConfigurations::Type Configuration;

	/** Name of the Platform for this combination */
	FString PlatformName;

	/** Location of a file that must exist for this combination to be valid (optional) */
	FString RequiredFile;

	/** Type of project this configuration can be used for */
	EProjectType ProjectType;

	/** Whether to display this platform as an option even if it is not valid */
	bool bCanBeDisplayed;
};

/**
 * Singleton class for accessing information about installed platform configurations
 */
class TARGETPLATFORM_API FInstalledPlatformInfo
{
public:
	/**
	 * Accessor for singleton
	 */
	static FInstalledPlatformInfo& Get()
	{
		static FInstalledPlatformInfo InfoSingleton;
		return InfoSingleton;
	}

	/**
	 * Queries whether a configuration is valid for any available platform
	 */
	bool IsValidConfiguration(const EBuildConfigurations::Type Configuration, EProjectType ProjectType = EProjectType::Any) const;

	/**
	 * Queries whether a platform has any valid configurations
	 */
	bool IsValidPlatform(const FString& PlatformName, EProjectType ProjectType = EProjectType::Any) const;

	/**
	 * Queries whether a platform and configuration combination is valid
	 */
	bool IsValidPlatformAndConfiguration(const EBuildConfigurations::Type Configuration, const FString& PlatformName, EProjectType ProjectType = EProjectType::Any) const;

	bool CanDisplayPlatform(const FString& PlatformName, EProjectType ProjectType) const;

private:
	/**
	 * Constructor
	 */
	FInstalledPlatformInfo();

	/**
	 * Parse platform configuration info from a config file entry
	 */
	void ParsePlatformConfiguration(FString PlatformConfiguration);

	/** List of installed platform configuration combinations */
	TArray<FInstalledPlatformConfiguration> InstalledPlatformConfigurations;
};
