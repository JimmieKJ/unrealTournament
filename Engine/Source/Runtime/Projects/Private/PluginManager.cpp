// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectsPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC( LogPluginManager, Log, All );

#define LOCTEXT_NAMESPACE "PluginManager"


namespace PluginSystemDefs
{
	/** File extension of plugin descriptor files.
	    NOTE: This constant exists in UnrealBuildTool code as well. */
	static const TCHAR PluginDescriptorFileExtension[] = TEXT( ".uplugin" );

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
	TArray<TSharedRef<FPlugin>> NewPlugins;
	ReadAllPlugins(NewPlugins);

	// Build a list of filenames for plugins which are enabled, and remove the rest
	TArray<FString> EnabledPluginFileNames;
	for(int32 Idx = 0; Idx < AllPlugins.Num(); Idx++)
	{
		const TSharedRef<FPlugin>& Plugin = AllPlugins[Idx];
		if(Plugin->bEnabled)
		{
			EnabledPluginFileNames.Add(Plugin->FileName);
		}
		else
		{
			AllPlugins.RemoveAt(Idx--);
		}
	}

	// Add all the plugins which aren't already enabled
	for(TSharedRef<FPlugin>& NewPlugin: NewPlugins)
	{
		if(!EnabledPluginFileNames.Contains(NewPlugin->FileName))
		{
			AllPlugins.Add(NewPlugin);
		}
	}
}

void FPluginManager::DiscoverAllPlugins()
{
	ensure( AllPlugins.Num() == 0 );		// Should not have already been initialized!
	ReadAllPlugins(AllPlugins);

	// Add the plugin binaries directory
	for(const TSharedRef<FPlugin>& Plugin: AllPlugins)
	{
		const FString PluginBinariesPath = FPaths::Combine(*FPaths::GetPath(Plugin->FileName), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
		FModuleManager::Get().AddBinariesDirectory(*PluginBinariesPath, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject);
	}
}

void FPluginManager::ReadAllPlugins(TArray<TSharedRef<FPlugin>>& Plugins)
{
#if (WITH_ENGINE && !IS_PROGRAM) || WITH_PLUGIN_SUPPORT
	// Find "built-in" plugins.  That is, plugins situated right within the Engine directory.
	ReadPluginsInDirectory(FPaths::EnginePluginsDir(), EPluginLoadedFrom::Engine, Plugins);

	// Find plugins in the game project directory (<MyGameProject>/Plugins)
	if( FApp::HasGameName() )
	{
		ReadPluginsInDirectory(FPaths::GamePluginsDir(), EPluginLoadedFrom::GameProject, Plugins);
	}
#endif
}

void FPluginManager::ReadPluginsInDirectory(const FString& PluginsDirectory, const EPluginLoadedFrom LoadedFrom, TArray<TSharedRef<FPlugin>>& Plugins)
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
			if(Descriptor.Load(FileName, FailureReason))
			{
				Plugins.Add(MakeShareable(new FPlugin(FileName, Descriptor, LoadedFrom)));
			}
			else
			{
				// NOTE: Even though loading of this plugin failed, we'll keep processing other plugins
				UE_LOG(LogPluginManager, Error, TEXT("%s (%s)"), *FailureReason.ToString(), *FileName);
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

bool FPluginManager::ConfigureEnabledPlugins()
{
	if(!bHaveConfiguredEnabledPlugins)
	{
		// Don't need to run this again
		bHaveConfiguredEnabledPlugins = true;

		// If a current project is set, check that we know about any plugin that's explicitly enabled
		const FProjectDescriptor *Project = IProjectManager::Get().GetCurrentProject();
		if(Project != nullptr)
		{
			for(const FPluginReferenceDescriptor& Plugin: Project->Plugins)
			{
				if(Plugin.bEnabled && !FindPluginInstance(Plugin.Name).IsValid())
				{
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
					else if (Plugin.bNotRequired)
					{
						FText PluginFailReason = FText::Format(LOCTEXT("PluginMissingNotRequiredError", "This project is missing the {0} plugin, disabling it as it is not required."), FText::FromString(Plugin.Name));
						IProjectManager::Get().SetPluginEnabled(*Plugin.Name, false, PluginFailReason);
					}
					else
					{
						FString Description = (Plugin.Description.Len() > 0) ? FString::Printf(TEXT("\n\n%s"), *Plugin.Description) : FString();
						FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("PluginMissingError", "This project requires the {0} plugin. {1}"), FText::FromString(Plugin.Name), FText::FromString(Description)), &Caption);
						
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

		// Get all the enabled plugin names
		TArray< FString > EnabledPluginNames;
		#if IS_PROGRAM
			GConfig->GetArray(TEXT("Plugins"), TEXT("ProgramEnabledPlugins"), EnabledPluginNames, GEngineIni);
		#else
			FProjectManager::Get().GetEnabledPlugins(EnabledPluginNames);
		#endif

		// Build a set from the array
		TSet< FString > AllEnabledPlugins;
		AllEnabledPlugins.Append( MoveTemp(EnabledPluginNames) );

		// Enable all the plugins by name
		for( const TSharedRef< FPlugin > Plugin : AllPlugins )
		{
			if ( AllEnabledPlugins.Contains(Plugin->Name) )
			{
				Plugin->bEnabled = true;
			}
		}

		for(const TSharedRef<FPlugin>& Plugin: AllPlugins)
		{
			if (Plugin->bEnabled)
			{
				// Build the list of content folders
				if (Plugin->Descriptor.bCanContainContent)
				{
					if (auto EngineConfigFile = GConfig->Find(GEngineIni, false))
					{
						if (auto CoreSystemSection = EngineConfigFile->Find(TEXT("Core.System")))
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
			if(Plugin->CanContainContent() && ensure(RegisterMountPointDelegate.IsBound()))
			{
				FString ContentDir = Plugin->GetContentDir();
				RegisterMountPointDelegate.Execute(Plugin->GetMountedAssetPath(), ContentDir);

				// Pak files are loaded from <PluginName>/Content/Paks/<PlatformName>
				if (FPlatformProperties::RequiresCookedData())
				{
					FoundPaks.Reset();
					PlatformFile.IterateDirectoryRecursively(*(ContentDir / TEXT("Paks") / FPlatformProperties::PlatformName()), PakVisitor);
					for (const auto& PakPath : FoundPaks)
					{
						if (FCoreDelegates::OnMountPak.IsBound())
						{
							FCoreDelegates::OnMountPak.Execute(PakPath, 0);
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
	TSharedPtr<FPlugin> Result;
	for(const TSharedRef<FPlugin>& Instance : AllPlugins)
	{
		if(Instance->Name == Name)
		{
			Result = Instance;
			break;
		}
	}
	return Result;
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
	for( const TSharedRef< FPlugin > Plugin : AllPlugins )
	{
		SlowTask.EnterProgressFrame(1);

		if ( Plugin->bEnabled )
		{
			TMap<FName, EModuleLoadResult> ModuleLoadFailures;
			FModuleDescriptor::LoadModulesForPhase(LoadingPhase, Plugin->Descriptor.Modules, ModuleLoadFailures);

			FText FailureMessage;
			for( auto FailureIt( ModuleLoadFailures.CreateConstIterator() ); FailureIt; ++FailureIt )
			{
				const auto ModuleNameThatFailedToLoad = FailureIt.Key();
				const auto FailureReason = FailureIt.Value();

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
	for (TArray< TSharedRef< FPlugin > >::TConstIterator Iter(AllPlugins); Iter; ++Iter)
	{
		const TSharedRef< FPlugin > &Plugin = *Iter;
		if (Plugin->bEnabled && !FModuleDescriptor::CheckModuleCompatbility(Plugin->Descriptor.Modules, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject, OutIncompatibleModules))
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
	TSharedPtr<IPlugin> Plugin;
	for(TSharedRef<FPlugin>& PossiblePlugin : AllPlugins)
	{
		if(PossiblePlugin->Name == Name)
		{
			Plugin = PossiblePlugin;
			break;
		}
	}
	return Plugin;
}

TArray<TSharedRef<IPlugin>> FPluginManager::GetEnabledPlugins()
{
	TArray<TSharedRef<IPlugin>> Plugins;
	for(TSharedRef<FPlugin>& PossiblePlugin : AllPlugins)
	{
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
	for(TSharedRef<FPlugin>& Plugin : AllPlugins)
	{
		Plugins.Add(Plugin);
	}
	return Plugins;
}

TArray< FPluginStatus > FPluginManager::QueryStatusForAllPlugins() const
{
	TArray< FPluginStatus > PluginStatuses;

	for( auto PluginIt( AllPlugins.CreateConstIterator() ); PluginIt; ++PluginIt )
	{
		const TSharedRef< FPlugin >& Plugin = *PluginIt;
		
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

#undef LOCTEXT_NAMESPACE
