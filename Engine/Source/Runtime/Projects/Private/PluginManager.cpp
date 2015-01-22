// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectsPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC( LogPluginManager, Log, All );

#define LOCTEXT_NAMESPACE "PluginManager"


namespace PluginSystemDefs
{
	/** File extension of plugin descriptor files.
	    NOTE: This constant exists in UnrealBuildTool code as well. */
	static const TCHAR PluginDescriptorFileExtension[] = TEXT( ".uplugin" );

	/** Relative path to the plugin's 128x128 icon resource file */
	static const FString RelativeIcon128FilePath( TEXT( "Resources/Icon128.png" ) );

}

FPluginInstance::FPluginInstance(const FString& InFileName, const FPluginDescriptor& InDescriptor, EPluginLoadedFrom::Type InLoadedFrom)
	: Name(FPaths::GetBaseFilename(InFileName))
	, FileName(InFileName)
	, Descriptor(InDescriptor)
	, LoadedFrom(InLoadedFrom)
	, bEnabled(false)
{
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

void FPluginManager::DiscoverAllPlugins()
{
	struct Local
	{

	private:
		/**
		 * Recursively searches for plugins and generates a list of plugin descriptors
		 *
		 * @param	PluginsDirectory	Directory we're currently searching
		 * @param	LoadedFrom			Where we're loading these plugins from (game, engine, etc)
		 * @param	Plugins				The array to be filled in with new plugins including descriptors
		 */
		static void FindPluginsRecursively( const FString& PluginsDirectory, const EPluginLoadedFrom::Type LoadedFrom, TArray< TSharedRef<FPluginInstance> >& Plugins )
		{
			// NOTE: The logic in this function generally matches that of the C# code for FindPluginsRecursively
			//       in UnrealBuildTool.  These routines should be kept in sync.

			// This directory scanning needs to be fast because it will happen every time at startup!  We don't
			// want to blindly recurse down every subdirectory, because plugin content and code directories could
			// contain a lot of files in total!

			// Each sub-directory is possibly a plugin.  If we find that it contains a plugin, we won't recurse any
			// further -- you can't have plugins within plugins.  If we didn't find a plugin, we'll keep recursing.
			TArray<FString> PossiblePluginDirectories;
			{
				class FPluginDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
				{
				public:
					FPluginDirectoryVisitor( TArray<FString>& InitFoundDirectories )
						: FoundDirectories( InitFoundDirectories )
					{
					}

					virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
					{
						if( bIsDirectory )
						{
							FoundDirectories.Add( FString( FilenameOrDirectory ) );
						}

						const bool bShouldContinue = true;
						return bShouldContinue;
					}

					TArray<FString>& FoundDirectories;
				};

				FPluginDirectoryVisitor PluginDirectoryVisitor( PossiblePluginDirectories );
				FPlatformFileManager::Get().GetPlatformFile().IterateDirectory( *PluginsDirectory, PluginDirectoryVisitor );
			}
				
			for( auto PossiblePluginDirectoryIter( PossiblePluginDirectories.CreateConstIterator() ); PossiblePluginDirectoryIter; ++PossiblePluginDirectoryIter )
			{
				const FString PossiblePluginDirectory = *PossiblePluginDirectoryIter;

				FString PluginDescriptorFilename;
				{
					// Usually the plugin descriptor is named the same as the directory.  It doesn't have to match the directory
					// name but we'll check that first because its much faster than scanning!
					const FString ProbablePluginDescriptorFilename = PossiblePluginDirectory / (FPaths::GetCleanFilename(PossiblePluginDirectory) + PluginSystemDefs::PluginDescriptorFileExtension);
					if( FPlatformFileManager::Get().GetPlatformFile().FileExists( *ProbablePluginDescriptorFilename ) )
					{
						PluginDescriptorFilename = ProbablePluginDescriptorFilename;
					}
					else
					{
						// Scan the directory for a plugin descriptor.
						TArray<FString> PluginDescriptorFilenames;
						{
							class FPluginDescriptorFileVisitor : public IPlatformFile::FDirectoryVisitor
							{
							public:
								FPluginDescriptorFileVisitor( const FWildcardString& InitSearchPattern, TArray<FString>& InitFoundPluginDescriptorFilenames )
									: SearchPattern( InitSearchPattern ),
									  FoundPluginDescriptorFilenames( InitFoundPluginDescriptorFilenames )
								{
								}

								virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
								{
									bool bShouldContinue = true;
									if( !bIsDirectory )
									{
										if( SearchPattern.IsMatch( FilenameOrDirectory ) )
										{
											FoundPluginDescriptorFilenames.Add( FString( FilenameOrDirectory ) );
											bShouldContinue = false;
										}
									}
									return bShouldContinue;
								}

								const FWildcardString& SearchPattern;
								TArray<FString>& FoundPluginDescriptorFilenames;
							};

							const FWildcardString PluginDescriptorSearchString = PossiblePluginDirectory / FString(TEXT("*")) + PluginSystemDefs::PluginDescriptorFileExtension;

							FPluginDescriptorFileVisitor PluginDescriptorFileVisitor( PluginDescriptorSearchString, PluginDescriptorFilenames );
							FPlatformFileManager::Get().GetPlatformFile().IterateDirectory( *PossiblePluginDirectory, PluginDescriptorFileVisitor );
						}

						// Did we find any plugin descriptor files?
						if( PluginDescriptorFilenames.Num() > 0 )
						{
							PluginDescriptorFilename = PluginDescriptorFilenames[ 0 ];;
						}
					}
				}

				if( !PluginDescriptorFilename.IsEmpty() )
				{
					// Found a plugin directory!  No need to recurse any further.
					FPluginDescriptor Descriptor;

					// Load the descriptor
					FText FailureReason;
					if(Descriptor.Load(PluginDescriptorFilename, FailureReason))
					{
						TSharedRef< FPluginInstance > NewPlugin = MakeShareable( new FPluginInstance(PluginDescriptorFilename, Descriptor, LoadedFrom) );

						if (FPaths::IsProjectFilePathSet())
						{
							FString GameProjectFolder = FPaths::GetPath(FPaths::GetProjectFilePath());
							if (PluginDescriptorFilename.StartsWith(GameProjectFolder))
							{
								FString PluginBinariesFolder = FPaths::Combine(*FPaths::GetPath(PluginDescriptorFilename), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
								FModuleManager::Get().AddBinariesDirectory(*PluginBinariesFolder, (LoadedFrom == EPluginLoadedFrom::GameProject));
							}
						}

						Plugins.Add(NewPlugin);
					}
					else
					{
						// NOTE: Even though loading of this plugin failed, we'll keep processing other plugins
						UE_LOG(LogPluginManager, Error, TEXT("%s (%s)"), *FailureReason.ToString(), *PluginDescriptorFilename);
					}
				}
				else
				{
					// Didn't find a plugin in this directory.  Continue to look in subfolders.
					FindPluginsRecursively( PossiblePluginDirectory, LoadedFrom, Plugins );
				}
			}
		}


	public:

		/**
		 * Searches for plugins and generates a list of plugin descriptors
		 *
		 * @param	PluginsDirectory	The base directory to search for plugins
		 * @param	LoadedFrom			Where we're loading these plugins from (game, engine, etc)
		 * @param	Plugins				The array to be filled in with loaded plugin descriptors
		 */
		static void FindPluginsIn( const FString& PluginsDirectory, const EPluginLoadedFrom::Type LoadedFrom, TArray< TSharedRef<FPluginInstance> >& Plugins )
		{
			// Make sure the directory even exists
			if( FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *PluginsDirectory ) )
			{
				FindPluginsRecursively( PluginsDirectory, LoadedFrom, Plugins );
			}
		}
	};


	{
		TArray< TSharedRef<FPluginInstance> > Plugins;

#if (WITH_ENGINE && !IS_PROGRAM) || WITH_PLUGIN_SUPPORT
		// Find "built-in" plugins.  That is, plugins situated right within the Engine directory.
		Local::FindPluginsIn( FPaths::EnginePluginsDir(), EPluginLoadedFrom::Engine, Plugins );

		// Find plugins in the game project directory (<MyGameProject>/Plugins)
		if( FApp::HasGameName() )
		{
			Local::FindPluginsIn( FPaths::GamePluginsDir(), EPluginLoadedFrom::GameProject, Plugins );
		}
#endif		// (WITH_ENGINE && !IS_PROGRAM) || WITH_PLUGIN_SUPPORT


		// Create plugin objects for all of the plugins that we found
		ensure( AllPlugins.Num() == 0 );		// Should not have already been initialized!
		AllPlugins = Plugins;
		for( auto PluginIt( Plugins.CreateConstIterator() ); PluginIt; ++PluginIt )
		{
			const TSharedRef<FPluginInstance>& Plugin = *PluginIt;

			// Add the plugin binaries directory
			const FString PluginBinariesPath = FPaths::Combine(*FPaths::GetPath(Plugin->FileName), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
			FModuleManager::Get().AddBinariesDirectory(*PluginBinariesPath, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject);
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
					FString Description = (Plugin.Description.Len() > 0)? FString::Printf(TEXT("\n\n%s"), *Plugin.Description) : FString();
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("PluginMissingError", "This project requires the {0} plugin. {1}"), FText::FromString(Plugin.Name), FText::FromString(Description)), &Caption);
					return false;
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
		for( const TSharedRef< FPluginInstance > Plugin : AllPlugins )
		{
			if ( AllEnabledPlugins.Contains(Plugin->Name) )
			{
				Plugin->bEnabled = true;
			}
		}

		ContentFolders.Empty();
		for(const TSharedRef<FPluginInstance>& Plugin: AllPlugins)
		{
			if (Plugin->bEnabled)
			{
				// Build the list of content folders
				if (Plugin->Descriptor.bCanContainContent)
				{
					FPluginContentFolder ContentFolder;
					ContentFolder.Name = Plugin->Name;
					ContentFolder.RootPath = FString::Printf(TEXT("/%s/"), *Plugin->Name);
					ContentFolder.ContentPath = FPaths::GetPath(Plugin->FileName) / TEXT("Content");
					ContentFolders.Emplace(ContentFolder);

					if (auto EngineConfigFile = GConfig->Find(GEngineIni, false))
					{
						if (auto CoreSystemSection = EngineConfigFile->Find(TEXT("Core.System")))
						{
							CoreSystemSection->AddUnique("Paths", ContentFolder.ContentPath);
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
		if( ContentFolders.Num() > 0 && ensure( RegisterMountPointDelegate.IsBound() ) )
		{
			for(const FPluginContentFolder& ContentFolder: ContentFolders)
			{
				RegisterMountPointDelegate.Execute(ContentFolder.RootPath, ContentFolder.ContentPath);

				// Pak files are loaded from <PluginName>/Content/Paks/<PlatformName>
				if (FPlatformProperties::RequiresCookedData())
				{
					FoundPaks.Reset();
					PlatformFile.IterateDirectoryRecursively(*(ContentFolder.ContentPath / TEXT("Paks") / FPlatformProperties::PlatformName()), PakVisitor);
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

TSharedPtr<FPluginInstance> FPluginManager::FindPluginInstance(const FString& Name)
{
	TSharedPtr<FPluginInstance> Result;
	for(const TSharedRef<FPluginInstance>& Instance : AllPlugins)
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

	// Load plugins!
	for( const TSharedRef< FPluginInstance > Plugin : AllPlugins )
	{
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
	for (TArray< TSharedRef< FPluginInstance > >::TConstIterator Iter(AllPlugins); Iter; ++Iter)
	{
		const TSharedRef< FPluginInstance > &Plugin = *Iter;
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

TArray< FPluginStatus > FPluginManager::QueryStatusForAllPlugins() const
{
	TArray< FPluginStatus > PluginStatuses;

	for( auto PluginIt( AllPlugins.CreateConstIterator() ); PluginIt; ++PluginIt )
	{
		const TSharedRef< FPluginInstance >& Plugin = *PluginIt;
		const FPluginDescriptor& PluginInfo = Plugin->Descriptor;
		
		FPluginStatus PluginStatus;
		PluginStatus.Name = Plugin->Name;
		PluginStatus.FriendlyName = PluginInfo.FriendlyName;
		PluginStatus.Version = PluginInfo.Version;
		PluginStatus.VersionName = PluginInfo.VersionName;
		PluginStatus.Description = PluginInfo.Description;
		PluginStatus.CreatedBy = PluginInfo.CreatedBy;
		PluginStatus.CreatedByURL = PluginInfo.CreatedByURL;
		PluginStatus.CategoryPath = PluginInfo.Category;
		PluginStatus.DocsURL = PluginInfo.DocsURL;
		PluginStatus.PluginDirectory = FPaths::GetPath(Plugin->FileName);
		PluginStatus.bIsEnabled = Plugin->bEnabled;
		PluginStatus.bIsBuiltIn = ( Plugin->LoadedFrom == EPluginLoadedFrom::Engine );
		PluginStatus.bIsEnabledByDefault = PluginInfo.bEnabledByDefault;
		PluginStatus.bIsBetaVersion = PluginInfo.bIsBetaVersion;
		PluginStatus.bHasContentFolder = PluginInfo.bCanContainContent;

		// @todo plugedit: Maybe we should do the FileExists check ONCE at plugin load time and not at query time
		const FString Icon128FilePath = FPaths::GetPath(Plugin->FileName) / PluginSystemDefs::RelativeIcon128FilePath;
		if( FPlatformFileManager::Get().GetPlatformFile().FileExists( *Icon128FilePath ) )
		{
			PluginStatus.Icon128FilePath = Icon128FilePath;
		}

		PluginStatuses.Add( PluginStatus );
	}

	return PluginStatuses;
}

const TArray<FPluginContentFolder>& FPluginManager::GetPluginContentFolders() const
{
	return ContentFolders;
}

#undef LOCTEXT_NAMESPACE
