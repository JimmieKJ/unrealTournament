// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectsPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC( LogProjectManager, Log, All );

#define LOCTEXT_NAMESPACE "ProjectManager"

FProjectManager::FProjectManager()
	: bIsCurrentProjectDirty(false)
{
}

const FProjectDescriptor* FProjectManager::GetCurrentProject() const
{
	return CurrentProject.Get();
}

bool FProjectManager::LoadProjectFile( const FString& InProjectFile )
{
	// Try to load the descriptor
	FText FailureReason;
	TSharedPtr<FProjectDescriptor> Descriptor = MakeShareable(new FProjectDescriptor());
	if(Descriptor->Load(InProjectFile, FailureReason))
	{
		// Create the project
		CurrentProject = Descriptor;
		return true;
	}
	
#if PLATFORM_IOS
    FString UpdatedMessage = FString::Printf(TEXT("%s\n%s"), *FailureReason.ToString(), TEXT("For troubleshooting, please go to https://docs.unrealengine.com/latest/INT/Platforms/iOS/GettingStarted/index.html"));
    FailureReason = FText::FromString(UpdatedMessage);
#endif
	UE_LOG(LogProjectManager, Error, TEXT("%s"), *FailureReason.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
    
	return false;
}


bool FProjectManager::LoadModulesForProject( const ELoadingPhase::Type LoadingPhase )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Game Modules"), STAT_GameModule, STATGROUP_LoadTime);

	bool bSuccess = true;

	if ( CurrentProject.IsValid() )
	{
		TMap<FName, EModuleLoadResult> ModuleLoadFailures;
		FModuleDescriptor::LoadModulesForPhase(LoadingPhase, CurrentProject->Modules, ModuleLoadFailures);

		if ( ModuleLoadFailures.Num() > 0 )
		{
			FText FailureMessage;
			for ( auto FailureIt = ModuleLoadFailures.CreateConstIterator(); FailureIt; ++FailureIt )
			{
				const EModuleLoadResult FailureReason = FailureIt.Value();

				if( FailureReason != EModuleLoadResult::Success )
				{
					const FText TextModuleName = FText::FromName(FailureIt.Key());

					if ( FailureReason == EModuleLoadResult::FileNotFound )
					{
						FailureMessage = FText::Format( LOCTEXT("PrimaryGameModuleNotFound", "The game module '{0}' could not be found. Please ensure that this module exists and that it is compiled."), TextModuleName );
					}
					else if ( FailureReason == EModuleLoadResult::FileIncompatible )
					{
						FailureMessage = FText::Format( LOCTEXT("PrimaryGameModuleIncompatible", "The game module '{0}' does not appear to be up to date. This may happen after updating the engine. Please recompile this module and try again."), TextModuleName );
					}
					else if ( FailureReason == EModuleLoadResult::FailedToInitialize )
					{
						FailureMessage = FText::Format( LOCTEXT("PrimaryGameModuleFailedToInitialize", "The game module '{0}' could not be successfully initialized after it was loaded."), TextModuleName );
					}
					else if ( FailureReason == EModuleLoadResult::CouldNotBeLoadedByOS )
					{
						FailureMessage = FText::Format( LOCTEXT("PrimaryGameModuleCouldntBeLoaded", "The game module '{0}' could not be loaded. There may be an operating system error or the module may not be properly set up."), TextModuleName );
					}
					else 
					{
						ensure(0);	// If this goes off, the error handling code should be updated for the new enum values!
						FailureMessage = FText::Format( LOCTEXT("PrimaryGameModuleGenericLoadFailure", "The game module '{0}' failed to load for an unspecified reason.  Please report this error."), TextModuleName );
					}

					// Just report the first error
					break;
				}
			}

			FMessageDialog::Open(EAppMsgType::Ok, FailureMessage);
			bSuccess = false;
		}
	}

	return bSuccess;
}

bool FProjectManager::CheckModuleCompatibility(TArray<FString>& OutIncompatibleModules)
{
	return !CurrentProject.IsValid() || FModuleDescriptor::CheckModuleCompatibility(CurrentProject->Modules, true, OutIncompatibleModules);
}

const FString& FProjectManager::GetAutoLoadProjectFileName()
{
	static FString RecentProjectFileName = FPaths::Combine(*FPaths::GameAgnosticSavedDir(), TEXT("AutoLoadProject.txt"));
	return RecentProjectFileName;
}

bool FProjectManager::SignSampleProject(const FString& FilePath, const FString& Category, FText& OutFailReason)
{
	FProjectDescriptor Descriptor;
	if(!Descriptor.Load(FilePath, OutFailReason))
	{
		return false;
	}

	Descriptor.Sign(FilePath);
	Descriptor.Category = Category;
	return Descriptor.Save(FilePath, OutFailReason);
}

bool FProjectManager::QueryStatusForProject(const FString& FilePath, FProjectStatus& OutProjectStatus) const
{
	FText FailReason;
	FProjectDescriptor Descriptor;
	if(!Descriptor.Load(FilePath, FailReason))
	{
		return false;
	}

	QueryStatusForProjectImpl(Descriptor, FilePath, OutProjectStatus);
	return true;
}

bool FProjectManager::QueryStatusForCurrentProject(FProjectStatus& OutProjectStatus) const
{
	if ( !CurrentProject.IsValid() )
	{
		return false;
	}

	QueryStatusForProjectImpl(*CurrentProject, FPaths::GetProjectFilePath(), OutProjectStatus);
	return true;
}

void FProjectManager::QueryStatusForProjectImpl(const FProjectDescriptor& ProjectInfo, const FString& FilePath, FProjectStatus& OutProjectStatus)
{
	OutProjectStatus.Name = FPaths::GetBaseFilename(FilePath);
	OutProjectStatus.Description = ProjectInfo.Description;
	OutProjectStatus.Category = ProjectInfo.Category;
	OutProjectStatus.bCodeBasedProject = ProjectInfo.Modules.Num() > 0;
	OutProjectStatus.bSignedSampleProject = ProjectInfo.IsSigned(FilePath);
	OutProjectStatus.bRequiresUpdate = ProjectInfo.FileVersion < EProjectDescriptorVersion::Latest;
	OutProjectStatus.TargetPlatforms = ProjectInfo.TargetPlatforms;
}

void FProjectManager::UpdateSupportedTargetPlatformsForProject(const FString& FilePath, const FName& InPlatformName, const bool bIsSupported)
{
	FProjectDescriptor NewProject;

	FText FailReason;
	if ( !NewProject.Load(FilePath, FailReason) )
	{
		return;
	}

	if ( bIsSupported )
	{
		NewProject.TargetPlatforms.AddUnique(InPlatformName);
	}
	else
	{
		NewProject.TargetPlatforms.Remove(InPlatformName);
	}

	NewProject.Save(FilePath, FailReason);

	// Call OnTargetPlatformsForCurrentProjectChangedEvent if this project is the same as the one we currently have loaded
	const FString CurrentProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString InProjectPath = FPaths::ConvertRelativePathToFull(FilePath);
	if ( CurrentProjectPath == InProjectPath )
	{
		OnTargetPlatformsForCurrentProjectChangedEvent.Broadcast();
	}
}

void FProjectManager::UpdateSupportedTargetPlatformsForCurrentProject(const FName& InPlatformName, const bool bIsSupported)
{
	if ( !CurrentProject.IsValid() )
	{
		return;
	}

	CurrentProject->UpdateSupportedTargetPlatforms(InPlatformName, bIsSupported);

	FText FailReason;
	CurrentProject->Save(FPaths::GetProjectFilePath(), FailReason);

	OnTargetPlatformsForCurrentProjectChangedEvent.Broadcast();
}

void FProjectManager::ClearSupportedTargetPlatformsForProject(const FString& FilePath)
{
	FProjectDescriptor Descriptor;

	FText FailReason;
	if ( !Descriptor.Load(FilePath, FailReason) )
	{
		return;
	}

	Descriptor.TargetPlatforms.Empty();
	Descriptor.Save(FilePath, FailReason);

	// Call OnTargetPlatformsForCurrentProjectChangedEvent if this project is the same as the one we currently have loaded
	const FString CurrentProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString InProjectPath = FPaths::ConvertRelativePathToFull(FilePath);
	if ( CurrentProjectPath == InProjectPath )
	{
		OnTargetPlatformsForCurrentProjectChangedEvent.Broadcast();
	}
}

void FProjectManager::ClearSupportedTargetPlatformsForCurrentProject()
{
	if ( !CurrentProject.IsValid() )
	{
		return;
	}

	CurrentProject->TargetPlatforms.Empty();

	FText FailReason;
	CurrentProject->Save(FPaths::GetProjectFilePath(), FailReason);

	OnTargetPlatformsForCurrentProjectChangedEvent.Broadcast();
}

void FProjectManager::GetEnabledPlugins(TArray<FString>& OutPluginNames) const
{
	// Get the default list of plugin names
	GetDefaultEnabledPlugins(OutPluginNames, true);

	// Modify that with the list of plugins in the project file
	const FProjectDescriptor *Project = GetCurrentProject();
	if(Project != NULL)
	{
		for(const FPluginReferenceDescriptor& Plugin: Project->Plugins)
		{
			if(Plugin.IsEnabledForPlatform(FPlatformMisc::GetUBTPlatform()))
			{
				OutPluginNames.AddUnique(Plugin.Name);
			}
			else
			{
				OutPluginNames.Remove(Plugin.Name);
			}
		}
	}
}

bool FProjectManager::IsNonDefaultPluginEnabled() const
{
	TArray<FString> EnabledPlugins;
	GetEnabledPlugins(EnabledPlugins);

	for(const FPluginStatus& Plugin: IPluginManager::Get().QueryStatusForAllPlugins())
	{
		if ((Plugin.LoadedFrom == EPluginLoadedFrom::GameProject || !Plugin.Descriptor.bEnabledByDefault || Plugin.Descriptor.bInstalled) && EnabledPlugins.Contains(Plugin.Name))
		{
			return true;
		}
	}

	return false;
}

bool FProjectManager::SetPluginEnabled(const FString& PluginName, bool bEnabled, FText& OutFailReason, const FString& MarketplaceURL)
{
	// Don't go any further if there's no project loaded
	if(!CurrentProject.IsValid())
	{
		OutFailReason = LOCTEXT("NoProjectLoaded", "No project is currently loaded");
		return false;
	}

	// Find or create the index of any existing reference in the project descriptor
	int PluginRefIdx = 0;
	for(;;PluginRefIdx++)
	{
		if(PluginRefIdx == CurrentProject->Plugins.Num())
		{
			PluginRefIdx = CurrentProject->Plugins.Add(FPluginReferenceDescriptor(PluginName, bEnabled, MarketplaceURL));
			break;
		}
		else if(CurrentProject->Plugins[PluginRefIdx].Name == PluginName)
		{
			CurrentProject->Plugins[PluginRefIdx].bEnabled = bEnabled;
			break;
		}
	}

	// If the current plugin reference is the default, just remove it from the list
	const FPluginReferenceDescriptor* PluginRef = &CurrentProject->Plugins[PluginRefIdx];
	if(PluginRef->WhitelistPlatforms.Num() == 0 && PluginRef->BlacklistPlatforms.Num() == 0)
	{
		// We alway need to be explicit about installed plugins, because they'll be auto-enabled again if we're not.
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if(!Plugin.IsValid() || !Plugin->GetDescriptor().bInstalled)
		{
			// Get the default list of enabled plugins
			TArray<FString> DefaultEnabledPlugins;
			GetDefaultEnabledPlugins(DefaultEnabledPlugins, false);

			// Check the enabled state is the same in that
			if(DefaultEnabledPlugins.Contains(PluginName) == bEnabled)
			{
				CurrentProject->Plugins.RemoveAt(PluginRefIdx);
				PluginRefIdx = INDEX_NONE;
			}
		}
	}

	// Mark project as dirty
	bIsCurrentProjectDirty = true;

	return true;
}

void FProjectManager::GetDefaultEnabledPlugins(TArray<FString>& OutPluginNames, bool bIncludeInstalledPlugins)
{
	// Add all the game plugins and everything marked as enabled by default
	TArray<FPluginStatus> PluginStatuses = IPluginManager::Get().QueryStatusForAllPlugins();
	for(const FPluginStatus& PluginStatus: PluginStatuses)
	{
		if(PluginStatus.Descriptor.bEnabledByDefault || PluginStatus.LoadedFrom == EPluginLoadedFrom::GameProject)
		{
			if(bIncludeInstalledPlugins || !PluginStatus.Descriptor.bInstalled)
			{
				OutPluginNames.AddUnique(PluginStatus.Name);
			}
		}
	}
}

bool FProjectManager::IsCurrentProjectDirty() const
{
	return bIsCurrentProjectDirty;
}

bool FProjectManager::SaveCurrentProjectToDisk(FText& OutFailReason)
{
	if (CurrentProject->Save(FPaths::GetProjectFilePath(), OutFailReason))
	{
		bIsCurrentProjectDirty = false;
		return true;
	}
	return false;
}


IProjectManager& IProjectManager::Get()
{
	// Single instance of manager, allocated on demand and destroyed on program exit.
	static FProjectManager* ProjectManager = NULL;
	if( ProjectManager == NULL )
	{
		ProjectManager = new FProjectManager();
	}
	return *ProjectManager;
}

#undef LOCTEXT_NAMESPACE