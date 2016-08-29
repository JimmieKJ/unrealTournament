// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ProjectsPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC( LogPluginManager, Log, All );

#define LOCTEXT_NAMESPACE "PluginManager"


namespace PluginSystemDefs
{
	/** File extension of plugin descriptor files.
	    NOTE: This constant exists in UnrealBuildTool code as well. */
	static const TCHAR PluginDescriptorFileExtension[] = TEXT( ".uplugin" );

	/**
	 * Parsing the command line and loads any foreign plugins that were
	 * specified using the -PLUGIN= command.
	 *
	 * @param  CommandLine    The commandline used to launch the editor.
	 * @param  SearchPathsOut 
	 * @return The number of plugins that were specified using the -PLUGIN param.
	 */
	static int32 GetAdditionalPluginPaths(TSet<FString>& PluginPathsOut)
	{
		const TCHAR* SwitchStr = TEXT("PLUGIN=");
		const int32  SwitchLen = FCString::Strlen(SwitchStr);

		int32 PluginCount = 0;

		const TCHAR* SearchStr = FCommandLine::Get();
		do
		{
			FString PluginPath;

			SearchStr = FCString::Strfind(SearchStr, SwitchStr);
			if (FParse::Value(SearchStr, SwitchStr, PluginPath))
			{
				FString PluginDir = FPaths::GetPath(PluginPath);
				PluginPathsOut.Add(PluginDir);

				++PluginCount;
				SearchStr += SwitchLen + PluginPath.Len();
			}
			else
			{
				break;
			}
		} while (SearchStr != nullptr);

#if IS_PROGRAM
		// For programs that have the project dir set, look for plugins under the project directory
		const FProjectDescriptor *Project = IProjectManager::Get().GetCurrentProject();
		if (Project != nullptr)
		{
			PluginPathsOut.Add(FPaths::GetPath(FPaths::GetProjectFilePath()) / TEXT("Plugins"));
		}
#endif
		return PluginCount;
	}
}

FPlugin::FPlugin(const FString& InFileName, const FPluginDescriptor& InDescriptor, EPluginLoadedFrom InLoadedFrom)
	: Name(FPaths::GetBaseFilename(InFileName))
	, FileName(InFileName)
	, Descriptor(InDescriptor)
	, LoadedFrom(InLoadedFrom)
	, bEnabled(false)
{

}

FPlugin::~FPlugin()
{
}

FString FPlugin::GetName() const
{
	return Name;
}

FString FPlugin::GetDescriptorFileName() const
{
	return FileName;
}

FString FPlugin::GetBaseDir() const
{
	return FPaths::GetPath(FileName);
}

FString FPlugin::GetContentDir() const
{
	return FPaths::GetPath(FileName) / TEXT("Content");
}

FString FPlugin::GetMountedAssetPath() const
{
	return FString::Printf(TEXT("/%s/"), *Name);
}

bool FPlugin::IsEnabled() const
{
	return bEnabled;
}

bool FPlugin::CanContainContent() const
{
	return Descriptor.bCanContainContent;
}

EPluginLoadedFrom FPlugin::GetLoadedFrom() const
{
	return LoadedFrom;
}

const FPluginDescriptor& FPlugin::GetDescriptor() const
{
	return Descriptor;
}

bool FPlugin::UpdateDescriptor(const FPluginDescriptor& NewDescriptor, FText& OutFailReason)
{
	if(!NewDescriptor.Save(FileName, OutFailReason))
	{
		return false;
	}

	Descriptor = NewDescriptor;
	return true;
}

FPluginManager::FPluginManager()
	: bHaveConfiguredEnabledPlugins(false)
	, bHaveAllRequiredPlugins(false)
{
	DiscoverAllPlugins();
}

FPluginManager::~FPluginManager()
{
	// NOTE: All plugins and modules should be cleaned up or abandoned by this point

	// @todo plugin: Really, we should "reboot" module manager's unloading code so that it remembers at which startup phase
	//  modules were loaded in, so that we can shut groups of modules down (in reverse-load order) at the various counterpart
	//  shutdown phases.  This will fix issues where modules that are loaded after game modules are shutdown AFTER many engine
	//  systems are already killed (like GConfig.)  Currently the only workaround is to listen to global exit events, or to
	//  explicitly unload your module somewhere.  We should be able to handle most cases automatically though!
}

void FPluginManager::RefreshPluginsList()
{
	// Read a new list of all plugins
	TMap<FString, TSharedRef<FPlugin>> NewPlugins;
	ReadAllPlugins(NewPlugins, PluginDiscoveryPaths);

	// Build a list of filenames for plugins which are enabled, and remove the rest
	TArray<FString> EnabledPluginFileNames;
	for(TMap<FString, TSharedRef<FPlugin>>::TIterator Iter(AllPlugins); Iter; ++Iter)
	{
		const TSharedRef<FPlugin>& Plugin = Iter.Value();
		if(Plugin->bEnabled)
		{
			EnabledPluginFileNames.Add(Plugin->FileName);
		}
		else
		{
			Iter.RemoveCurrent();
		}
	}

	// Add all the plugins which aren't already enabled
	for(const TPair<FString, TSharedRef<FPlugin>>& NewPluginPair: NewPlugins)
	{
		const TSharedRef<FPlugin>& NewPlugin = NewPluginPair.Value;
		if(!EnabledPluginFileNames.Contains(NewPlugin->FileName))
		{
			AllPlugins.Add(NewPlugin->GetName(), NewPlugin);
		}
	}
}

void FPluginManager::DiscoverAllPlugins()
{
	ensure( AllPlugins.Num() == 0 );		// Should not have already been initialized!

	PluginSystemDefs::GetAdditionalPluginPaths(PluginDiscoveryPaths);
	ReadAllPlugins(AllPlugins, PluginDiscoveryPaths);
}

void FPluginManager::ReadAllPlugins(TMap<FString, TSharedRef<FPlugin>>& Plugins, const TSet<FString>& ExtraSearchPaths)
{
#if (WITH_ENGINE && !IS_PROGRAM) || WITH_PLUGIN_SUPPORT
	// Find "built-in" plugins.  That is, plugins situated right within the Engine directory.
	ReadPluginsInDirectory(FPaths::EnginePluginsDir(), EPluginLoadedFrom::Engine, Plugins);

	// Find plugins in the game project directory (<MyGameProject>/Plugins). If there are any engine plugins matching the name of a game plugin,
	// assume that the game plugin version is preferred.
	if( FApp::HasGameName() )
	{
		ReadPluginsInDirectory(FPaths::GamePluginsDir(), EPluginLoadedFrom::GameProject, Plugins);
	}

	for (const FString& ExtraSearchPath : ExtraSearchPaths)
	{
		ReadPluginsInDirectory(ExtraSearchPath, EPluginLoadedFrom::GameProject, Plugins);
	}
#endif
}

void FPluginManager::ReadPluginsInDirectory(const FString& PluginsDirectory, const EPluginLoadedFrom LoadedFrom, TMap<FString, TSharedRef<FPlugin>>& Plugins)
{
	// Make sure the directory even exists
	if(FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*PluginsDirectory))
	{
		TArray<FString> FileNames;
		FindPluginsInDirectory(PluginsDirectory, FileNames);

		for(const FString& FileName: FileNames)
		{
			FPluginDescriptor Descriptor;
			FText FailureReason;
			if ( Descriptor.Load(FileName, FailureReason) )
			{
				TSharedRef<FPlugin> Plugin = MakeShareable(new FPlugin(FileName, Descriptor, LoadedFrom));
				
				FString FullPath = FPaths::ConvertRelativePathToFull(FileName);
				UE_LOG(LogPluginManager, Log, TEXT("Loaded Plugin %s, From %s"), *Plugin->GetName(), *FullPath);

				const TSharedRef<FPlugin>* ExistingPlugin = Plugins.Find(Plugin->GetName());
				if (ExistingPlugin == nullptr)
				{
					Plugins.Add(Plugin->GetName(), Plugin);
				}
				else if ((*ExistingPlugin)->LoadedFrom == EPluginLoadedFrom::Engine && LoadedFrom == EPluginLoadedFrom::GameProject)
				{
					Plugins[Plugin->GetName()] = Plugin;
					UE_LOG(LogPluginManager, Log, TEXT("Replacing engine version of '%s' plugin with game version"), *Plugin->GetName());
				}
				else if( (*ExistingPlugin)->LoadedFrom != EPluginLoadedFrom::GameProject || LoadedFrom != EPluginLoadedFrom::Engine)
				{
					UE_LOG(LogPluginManager, Warning, TEXT("Plugin '%s' exists at '%s' and '%s' - second location will be ignored"), *Plugin->GetName(), *(*ExistingPlugin)->FileName, *Plugin->FileName);
				}
			}
			else
			{
				// NOTE: Even though loading of this plugin failed, we'll keep processing other plugins
				FString FullPath = FPaths::ConvertRelativePathToFull(FileName);
				FText FailureMessage = FText::Format(LOCTEXT("FailureFormat", "{0} ({1})"), FailureReason, FText::FromString(FullPath));
				FText DialogTitle = LOCTEXT("PluginFailureTitle", "Failed to load Plugin");
				UE_LOG(LogPluginManager, Error, TEXT("%s"), *FailureMessage.ToString());
				FMessageDialog::Open(EAppMsgType::Ok, FailureMessage, &DialogTitle);
			}
		}
	}
}

void FPluginManager::FindPluginsInDirectory(const FString& PluginsDirectory, TArray<FString>& FileNames)
{
	// Class to enumerate the contents of a directory, and find all sub-directories and plugin descriptors within it
	struct FPluginDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString> SubDirectories;
		TArray<FString> PluginDescriptors;

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			FString FilenameOrDirectoryStr = FilenameOrDirectory;
			if(bIsDirectory)
			{
				SubDirectories.Add(FilenameOrDirectoryStr);
			}
			else if(FilenameOrDirectoryStr.EndsWith(TEXT(".uplugin")))
			{
				PluginDescriptors.Add(FilenameOrDirectoryStr);
			}
			return true;
		}
	};

	// Enumerate the contents of the current directory
	FPluginDirectoryVisitor Visitor;
	FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*PluginsDirectory, Visitor);

	// If there's no plugins in this directory, recurse through all the child directories
	if(Visitor.PluginDescriptors.Num() == 0)
	{
		for(const FString& SubDirectory: Visitor.SubDirectories)
		{
			FindPluginsInDirectory(SubDirectory, FileNames);
		}
	}
	else
	{
		for(const FString& PluginDescriptor: Visitor.PluginDescriptors)
		{
			FileNames.Add(PluginDescriptor);
		}
	}
}

// Helper class to find all pak files.
class FPakFileSearchVisitor : public IPlatformFile::FDirectoryVisitor
{
	TArray<FString>& FoundFiles;
public:
	FPakFileSearchVisitor(TArray<FString>& InFoundFiles)
		: FoundFiles(InFoundFiles)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			if (Filename.MatchesWildcard(TEXT("*.pak")))
			{
				FoundFiles.Add(Filename);
			}
		}
		return true;
	}
};

bool FPluginManager::IsPluginSupportedByCurrentTarget(TSharedRef<FPlugin> Plugin) const
{
	bool bSupported = false;
	if (Plugin->GetDescriptor().Modules.Num())
	{
		for (const FModuleDescriptor& Module : Plugin->GetDescriptor().Modules)
		{
			// Programs support only program type plugins
			// Non-program targets don't support from plugins
#if IS_PROGRAM
			if (Module.Type == EHostType::Program)
			{
				bSupported = true;
			}
#else
			if (Module.Type != EHostType::Program)
			{
				bSupported = true;
			}
#endif
		}
	}
	else
	{
		bSupported = true;
	}
	return bSupported;
}

bool FPluginManager::ConfigureEnabledPlugins()
{
	if(!bHaveConfiguredEnabledPlugins)
	{
		// Don't need to run this again
		bHaveConfiguredEnabledPlugins = true;

		// If a current project is set, check that we know about any plugin that's explicitly enabled
		const FProjectDescriptor *Project = IProjectManager::Get().GetCurrentProject();
		const bool bHasProjectFile = Project != nullptr;

		// Get all the enabled plugin names
		TArray< FString > EnabledPluginNames;
#if IS_PROGRAM
		// Programs can also define the list of enabled plugins in ini
		GConfig->GetArray(TEXT("Plugins"), TEXT("ProgramEnabledPlugins"), EnabledPluginNames, GEngineIni);
#endif
#if !IS_PROGRAM || HACK_HEADER_GENERATOR
		if (!FParse::Param(FCommandLine::Get(), TEXT("NoEnginePlugins")))
		{
			FProjectManager::Get().GetEnabledPlugins(EnabledPluginNames);
		}
#endif

		// Build a set from the array
		TSet< FString > AllEnabledPlugins;
		AllEnabledPlugins.Append(MoveTemp(EnabledPluginNames));

		// Enable all the plugins by name
		for (const TPair<FString, TSharedRef< FPlugin >> PluginPair : AllPlugins)
		{
			const TSharedRef<FPlugin>& Plugin = PluginPair.Value;
			if (AllEnabledPlugins.Contains(Plugin->Name))
			{
#if IS_PROGRAM
				Plugin->bEnabled = !bHasProjectFile || IsPluginSupportedByCurrentTarget(Plugin);
				if (!Plugin->bEnabled)
				{
					AllEnabledPlugins.Remove(Plugin->Name);
				}
#else
				Plugin->bEnabled = true;
#endif
			}
		}

		if (bHasProjectFile)
		{
			// Take a copy of the Project's plugins as we may remove some
			TArray<FPluginReferenceDescriptor> PluginsCopy = Project->Plugins;
			for(const FPluginReferenceDescriptor& Plugin: PluginsCopy)
			{
				bool bShouldBeEnabled = Plugin.bEnabled && Plugin.IsEnabledForPlatform(FPlatformMisc::GetUBTPlatform()) && Plugin.IsEnabledForTarget(FPlatformMisc::GetUBTTarget());
				if (bShouldBeEnabled && !FindPluginInstance(Plugin.Name).IsValid() && !Plugin.bOptional
#if IS_PROGRAM
					 && AllEnabledPlugins.Contains(Plugin.Name) // skip if this is a program and the plugin is not enabled
#endif
					)
				{
					if (FApp::IsUnattended())
					{
						UE_LOG(LogPluginManager, Error, TEXT("This project requires the '%s' plugin. Install it and try again, or remove it from the project's required plugin list."), *Plugin.Name);
						return false;
					}

					FText Caption(LOCTEXT("PluginMissingCaption", "Plugin missing"));
					if(Plugin.MarketplaceURL.Len() > 0)
					{
						if(FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("PluginMissingError", "This project requires the {0} plugin.\n\nWould you like to download it from the the Marketplace?"), FText::FromString(Plugin.Name)), &Caption) == EAppReturnType::Yes)
						{
							FString Error;
							FPlatformProcess::LaunchURL(*Plugin.MarketplaceURL, nullptr, &Error);
							if(Error.Len() > 0) FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
							return false;
						}
					}
					else
					{
						FString Description = (Plugin.Description.Len() > 0) ? FString::Printf(TEXT("\n\n%s"), *Plugin.Description) : FString();
						FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("PluginRequiredError", "This project requires the {0} plugin. {1}"), FText::FromString(Plugin.Name), FText::FromString(Description)), &Caption);
						
						if (FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("PluginMissingDisable", "Would you like to disable {0}? You will no longer be able to open any assets created using it."), FText::FromString(Plugin.Name)), &Caption) == EAppReturnType::No)
						{
							return false;
						}

						FText FailReason;
						if (!IProjectManager::Get().SetPluginEnabled(*Plugin.Name, false, FailReason))
						{
							FMessageDialog::Open(EAppMsgType::Ok, FailReason);
						}
					}
				}
			}
		}

		// If we made it here, we have all the required plugins
		bHaveAllRequiredPlugins = true;

		for(const TPair<FString, TSharedRef<FPlugin>>& PluginPair: AllPlugins)
		{
			const TSharedRef<FPlugin>& Plugin = PluginPair.Value;
			if (Plugin->bEnabled)
			{
				// Add the plugin binaries directory
				const FString PluginBinariesPath = FPaths::Combine(*FPaths::GetPath(Plugin->FileName), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
				FModuleManager::Get().AddBinariesDirectory(*PluginBinariesPath, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject);

#if !IS_MONOLITHIC
				// Only check this when in a non-monolithic build where modules could be in separate binaries
				if (Project != NULL && Project->Modules.Num() == 0)
				{
					// Content only project - check whether any plugins are incompatible and offer to disable instead of trying to build them later
					TArray<FString> IncompatibleFiles;
					if (!FModuleDescriptor::CheckModuleCompatibility(Plugin->Descriptor.Modules, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject, IncompatibleFiles))
					{
						// Ask whether to disable plugin if incompatible
						FText Caption(LOCTEXT("IncompatiblePluginCaption", "Plugin missing or incompatible"));
						if (FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("IncompatiblePluginText", "Missing or incompatible modules in {0} plugin - would you like to disable it? You will no longer be able to open any assets created using it."), FText::FromString(Plugin->Name)), &Caption) == EAppReturnType::No)
						{
							return false;
						}

						FText FailReason;
						if (!IProjectManager::Get().SetPluginEnabled(*Plugin->Name, false, FailReason))
						{
							FMessageDialog::Open(EAppMsgType::Ok, FailReason);
						}
					}
				}
#endif //!IS_MONOLITHIC

			// Build the list of content folders
				if (Plugin->Descriptor.bCanContainContent)
				{
					if (FConfigFile* EngineConfigFile = GConfig->Find(GEngineIni, false))
					{
						if (FConfigSection* CoreSystemSection = EngineConfigFile->Find(TEXT("Core.System")))
						{
							CoreSystemSection->AddUnique("Paths", Plugin->GetContentDir());
						}
					}
				}

				// Load Default<PluginName>.ini config file if it exists
				FString PluginConfigDir = FPaths::GetPath(Plugin->FileName) / TEXT("Config/");
				FConfigFile PluginConfig;
				FConfigCacheIni::LoadExternalIniFile(PluginConfig, *Plugin->Name, *FPaths::EngineConfigDir(), *PluginConfigDir, true);
				if (PluginConfig.Num() > 0)
				{
					FString PlaformName = FPlatformProperties::PlatformName();
					FString PluginConfigFilename = FString::Printf(TEXT("%s%s/%s.ini"), *FPaths::GeneratedConfigDir(), *PlaformName, *Plugin->Name);
					FConfigFile& NewConfigFile = GConfig->Add(PluginConfigFilename, FConfigFile());
					NewConfigFile.AddMissingProperties(PluginConfig);
					NewConfigFile.Write(PluginConfigFilename);
				}
			}
		}
		
		// Mount all the plugin content folders and pak files
		TArray<FString>	FoundPaks;
		FPakFileSearchVisitor PakVisitor(FoundPaks);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		for(TSharedRef<IPlugin> Plugin: GetEnabledPlugins())
		{
			if (Plugin->CanContainContent() && ensure(RegisterMountPointDelegate.IsBound()))
			{
				FString ContentDir = Plugin->GetContentDir();
				RegisterMountPointDelegate.Execute(Plugin->GetMountedAssetPath(), ContentDir);

				// Pak files are loaded from <PluginName>/Content/Paks/<PlatformName>
				if (FPlatformProperties::RequiresCookedData())
				{
					FoundPaks.Reset();
					PlatformFile.IterateDirectoryRecursively(*(ContentDir / TEXT("Paks") / FPlatformProperties::PlatformName()), PakVisitor);
					for (const FString& PakPath : FoundPaks)
					{
						if (FCoreDelegates::OnMountPak.IsBound())
						{
							FCoreDelegates::OnMountPak.Execute(PakPath, 0, nullptr);
						}
					}
				}
			}
		}
	}
	return bHaveAllRequiredPlugins;
}

TSharedPtr<FPlugin> FPluginManager::FindPluginInstance(const FString& Name)
{
	const TSharedRef<FPlugin>* Instance = AllPlugins.Find(Name);
	if (Instance == nullptr)
	{
		return TSharedPtr<FPlugin>();
	}
	else
	{
		return TSharedPtr<FPlugin>(*Instance);
	}
}

bool FPluginManager::LoadModulesForEnabledPlugins( const ELoadingPhase::Type LoadingPhase )
{
	// Figure out which plugins are enabled
	if(!ConfigureEnabledPlugins())
	{
		return false;
	}

	FScopedSlowTask SlowTask(AllPlugins.Num());

	// Load plugins!
	for( const TPair<FString, TSharedRef< FPlugin >> PluginPair : AllPlugins )
	{
		const TSharedRef<FPlugin> &Plugin = PluginPair.Value;
		SlowTask.EnterProgressFrame(1);

		if ( Plugin->bEnabled )
		{
			TMap<FName, EModuleLoadResult> ModuleLoadFailures;
			FModuleDescriptor::LoadModulesForPhase(LoadingPhase, Plugin->Descriptor.Modules, ModuleLoadFailures);

			FText FailureMessage;
			for( auto FailureIt( ModuleLoadFailures.CreateConstIterator() ); FailureIt; ++FailureIt )
			{
				const FName ModuleNameThatFailedToLoad = FailureIt.Key();
				const EModuleLoadResult FailureReason = FailureIt.Value();

				if( FailureReason != EModuleLoadResult::Success )
				{
					const FText PluginNameText = FText::FromString(Plugin->Name);
					const FText TextModuleName = FText::FromName(FailureIt.Key());

					if ( FailureReason == EModuleLoadResult::FileNotFound )
					{
						FailureMessage = FText::Format( LOCTEXT("PluginModuleNotFound", "Plugin '{0}' failed to load because module '{1}' could not be found.  Please ensure the plugin is properly installed, otherwise consider disabling the plugin for this project."), PluginNameText, TextModuleName );
					}
					else if ( FailureReason == EModuleLoadResult::FileIncompatible )
					{
						FailureMessage = FText::Format( LOCTEXT("PluginModuleIncompatible", "Plugin '{0}' failed to load because module '{1}' does not appear to be compatible with the current version of the engine.  The plugin may need to be recompiled."), PluginNameText, TextModuleName );
					}
					else if ( FailureReason == EModuleLoadResult::CouldNotBeLoadedByOS )
					{
						FailureMessage = FText::Format( LOCTEXT("PluginModuleCouldntBeLoaded", "Plugin '{0}' failed to load because module '{1}' could not be loaded.  There may be an operating system error or the module may not be properly set up."), PluginNameText, TextModuleName );
					}
					else if ( FailureReason == EModuleLoadResult::FailedToInitialize )
					{
						FailureMessage = FText::Format( LOCTEXT("PluginModuleFailedToInitialize", "Plugin '{0}' failed to load because module '{1}' could be initialized successfully after it was loaded."), PluginNameText, TextModuleName );
					}
					else 
					{
						ensure(0);	// If this goes off, the error handling code should be updated for the new enum values!
						FailureMessage = FText::Format( LOCTEXT("PluginGenericLoadFailure", "Plugin '{0}' failed to load because module '{1}' could not be loaded for an unspecified reason.  This plugin's functionality will not be available.  Please report this error."), PluginNameText, TextModuleName );
					}

					// Don't need to display more than one module load error per plugin that failed to load
					break;
				}
			}

			if( !FailureMessage.IsEmpty() )
			{
				FMessageDialog::Open(EAppMsgType::Ok, FailureMessage);
				return false;
			}
		}
	}
	return true;
}

void FPluginManager::GetLocalizationPathsForEnabledPlugins( TArray<FString>& OutLocResPaths )
{
	// Figure out which plugins are enabled
	if (!ConfigureEnabledPlugins())
	{
		return;
	}

	// Gather the paths from all plugins that have localization targets that are loaded based on the current runtime environment
	for (const TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		const TSharedRef<FPlugin>& Plugin = PluginPair.Value;
		if (!Plugin->bEnabled || Plugin->GetDescriptor().LocalizationTargets.Num() == 0)
		{
			continue;
		}
		
		const FString PluginLocDir = Plugin->GetContentDir() / TEXT("Localization");
		for (const FLocalizationTargetDescriptor& LocTargetDesc : Plugin->GetDescriptor().LocalizationTargets)
		{
			if (LocTargetDesc.ShouldLoadLocalizationTarget())
			{
				OutLocResPaths.Add(PluginLocDir / LocTargetDesc.Name);
			}
		}
	}
}

void FPluginManager::SetRegisterMountPointDelegate( const FRegisterMountPointDelegate& Delegate )
{
	RegisterMountPointDelegate = Delegate;
}

bool FPluginManager::AreRequiredPluginsAvailable()
{
	return ConfigureEnabledPlugins();
}

bool FPluginManager::CheckModuleCompatibility(TArray<FString>& OutIncompatibleModules)
{
	if(!ConfigureEnabledPlugins())
	{
		return false;
	}

	bool bResult = true;
	for(const TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		const TSharedRef< FPlugin > &Plugin = PluginPair.Value;
		if (Plugin->bEnabled && !FModuleDescriptor::CheckModuleCompatibility(Plugin->Descriptor.Modules, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject, OutIncompatibleModules))
		{
			bResult = false;
		}
	}
	return bResult;
}

IPluginManager& IPluginManager::Get()
{
	// Single instance of manager, allocated on demand and destroyed on program exit.
	static FPluginManager* PluginManager = NULL;
	if( PluginManager == NULL )
	{
		PluginManager = new FPluginManager();
	}
	return *PluginManager;
}

TSharedPtr<IPlugin> FPluginManager::FindPlugin(const FString& Name)
{
	const TSharedRef<FPlugin>* Instance = AllPlugins.Find(Name);
	if (Instance == nullptr)
	{
		return TSharedPtr<IPlugin>();
	}
	else
	{
		return TSharedPtr<IPlugin>(*Instance);
	}
}

TArray<TSharedRef<IPlugin>> FPluginManager::GetEnabledPlugins()
{
	TArray<TSharedRef<IPlugin>> Plugins;
	for(TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		TSharedRef<FPlugin>& PossiblePlugin = PluginPair.Value;
		if(PossiblePlugin->bEnabled)
		{
			Plugins.Add(PossiblePlugin);
		}
	}
	return Plugins;
}

TArray<TSharedRef<IPlugin>> FPluginManager::GetDiscoveredPlugins()
{
	TArray<TSharedRef<IPlugin>> Plugins;
	for (TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		Plugins.Add(PluginPair.Value);
	}
	return Plugins;
}

TArray< FPluginStatus > FPluginManager::QueryStatusForAllPlugins() const
{
	TArray< FPluginStatus > PluginStatuses;

	for( const TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins )
	{
		const TSharedRef< FPlugin >& Plugin = PluginPair.Value;
		
		FPluginStatus PluginStatus;
		PluginStatus.Name = Plugin->Name;
		PluginStatus.PluginDirectory = FPaths::GetPath(Plugin->FileName);
		PluginStatus.bIsEnabled = Plugin->bEnabled;
		PluginStatus.Descriptor = Plugin->Descriptor;
		PluginStatus.LoadedFrom = Plugin->LoadedFrom;

		PluginStatuses.Add( PluginStatus );
	}

	return PluginStatuses;
}

void FPluginManager::AddPluginSearchPath(const FString& ExtraDiscoveryPath, bool bRefresh)
{
	PluginDiscoveryPaths.Add(ExtraDiscoveryPath);
	if (bRefresh)
	{
		RefreshPluginsList();
	}
}

#undef LOCTEXT_NAMESPACE
