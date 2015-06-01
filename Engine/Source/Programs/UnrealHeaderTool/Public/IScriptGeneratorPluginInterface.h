// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ModuleManager.h"

/** Build module type, mirrored in UEBuildModule.cs, enum UEBUildModuletype */
struct EBuildModuleType
{
	enum Type
	{
		Unknown,
		Runtime,
		Developer,
		Editor,
		ThirdParty,
		Program,
		Game,
		// NOTE: If you add a new value, make sure to update the ToString() method below!

		Max
	};


	/**
	* Converts a string literal into EModuleType::Type value
	*
	* @param	The string to convert to EModuleType::Type
	*
	* @return	The enum value corresponding to the name
	*/
	inline static const EBuildModuleType::Type Parse(const TCHAR* Value)
	{
		if (FCString::Stricmp(Value, TEXT("Unknown")) == 0)
		{
			return Unknown;
		}
		else if (FCString::Stricmp(Value, TEXT("Runtime")) == 0)
		{
			return Runtime;
		}
		else if (FCString::Stricmp(Value, TEXT("Developer")) == 0)
		{
			return Developer;
		}
		else if (FCString::Stricmp(Value, TEXT("Editor")) == 0)
		{
			return Editor;
		}
		else if (FCString::Stricmp(Value, TEXT("ThirdParty")) == 0)
		{
			return ThirdParty;
		}
		else if (FCString::Stricmp(Value, TEXT("Program")) == 0)
		{
			return Program;
		}
		else if (FCString::Stricmp(Value, TEXT("Game")) == 0)
		{
			return Game;
		}
		else
		{
			FError::Throwf(TEXT("Unrecognized EBuildModuleType name: %s"), Value);
			return Unknown;
		}
	}
};

/**
 * The public interface to script generator plugins.
 */
class IScriptGeneratorPluginInterface : public IModuleInterface, public IModularFeature
{
public:

	/** Name of module that is going to be compiling generated script glue */
	virtual FString GetGeneratedCodeModuleName() const = 0;
	/** Returns true if this plugin supports exporting scripts for the specified target. This should handle game as well as editor target names */
	virtual bool SupportsTarget(const FString& TargetName) const = 0;
	/** Returns true if this plugin supports exporting scripts for the specified module */
	virtual bool ShouldExportClassesForModule(const FString& ModuleName, EBuildModuleType::Type ModuleType, const FString& ModuleGeneratedIncludeDirectory) const = 0;
	/** Initializes this plugin with build information */
	virtual void Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& IncludeBase) = 0;
	/** Exports a single class. May be called multiple times for the same class (as UHT processes the entire hierarchy inside modules. */
	virtual void ExportClass(class UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) = 0;
	/** Called once all classes have been exported */
	virtual void FinishExport() = 0;
	/** Name of the generator plugin, mostly for debuggind purposes */
	virtual FString GetGeneratorName() const = 0;
};

