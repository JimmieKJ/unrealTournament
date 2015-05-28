// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "AutoReimportUtilities.h"
#include "AutoReimport/AutoReimportManager.h"
#include "ContentDirectoryMonitor.h"

#include "PackageTools.h"
#include "ObjectTools.h"
#include "ARFilter.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "ReimportFeedbackContext.h"
#include "MessageLogModule.h"

#define LOCTEXT_NAMESPACE "AutoReimportManager"
#define yield if (TimeLimit.Exceeded()) return

enum class EStateMachineNode { CallOnce, CallMany };

/** A simple state machine class that calls functions mapped on enum values. If any function returns a new enum type, it moves onto that function */
template<typename TState>
struct FStateMachine
{
	typedef TFunction<TOptional<TState>(const FTimeLimit&)> FunctionType;

	/** Constructor that specifies the initial state of the machine */
	FStateMachine(TState InitialState) : CurrentState(InitialState){}

	/** Add an enum->function mapping for this state machine */
	void Add(TState State, EStateMachineNode NodeType, const FunctionType& Function)
	{
		Nodes.Add(State, FStateMachineNode(NodeType, Function));
	}

	/** Set a new state for this machine */
	void SetState(TState NewState)
	{
		CurrentState = NewState;
	}

	/** Tick this state machine with the given time limit. Will continuously enumerate the machine until TimeLimit is reached */
	void Tick(const FTimeLimit& TimeLimit)
	{
		while (!TimeLimit.Exceeded())
		{
			const auto& State = Nodes[CurrentState];
			TOptional<TState> NewState = State.Endpoint(TimeLimit);
			if (NewState.IsSet())
			{
				CurrentState = NewState.GetValue();
			}
			else if (State.Type == EStateMachineNode::CallOnce)
			{
				break;
			}
		}
	}

private:

	/** The current state of this machine */
	TState CurrentState;

private:

	struct FStateMachineNode
	{
		FStateMachineNode(EStateMachineNode InType, const FunctionType& InEndpoint) : Endpoint(InEndpoint), Type(InType) {}

		/** The function endpoint for this node */
		FunctionType Endpoint;

		/** Whether this endpoint should be called multiple times in a frame, or just once */
		EStateMachineNode Type;
	};

	/** A map of enum value -> callback information */
	TMap<TState, FStateMachineNode> Nodes;
};

/** Enum and value to specify the current state of our processing */
enum class ECurrentState
{
	Idle, PendingResponse, ProcessAdditions, ProcessModifications, ProcessDeletions, SavePackages
};
uint32 GetTypeHash(ECurrentState State)
{
	return (int32)State;
}

/* Deals with auto reimporting of objects when the objects file on disk is modified*/
class FAutoReimportManager : public FTickableEditorObject, public FGCObject, public TSharedFromThis<FAutoReimportManager>
{
public:

	FAutoReimportManager();

	/** Get a list of currently monitored directories */
	TArray<FPathAndMountPoint> GetMonitoredDirectories() const;

	/** Report an external change to the manager, such that a subsequent equal change reported by the os be ignored */
	void IgnoreNewFile(const FString& Filename);
	void IgnoreFileModification(const FString& Filename);
	void IgnoreMovedFile(const FString& SrcFilename, const FString& DstFilename);
	void IgnoreDeletedFile(const FString& Filename);

	/** Destroy this manager */
	void Destroy();

private:

	/** FTickableEditorObject interface*/
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;

	/** FGCObject interface*/
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:

	/** Called when a new asset path has been mounted or unmounted */
	void OnContentPathChanged(const FString& InAssetPath, const FString& FileSystemPath);

	/** Called when an asset has been renamed */
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldPath);

private:

	/** Callback for when an editor user setting has changed */
	void HandleLoadingSavingSettingChanged(FName PropertyName);

	/** Set up monitors to all the monitored content directories */
	void SetUpDirectoryMonitors();

	/** Retrieve a semi-colon separated string of file extensions supported by all available editor import factories */
	static FString GetAllFactoryExtensions();

private:
	
	/** Get the number of unprocessed changes that are not part of the current processing operation */
	int32 GetNumUnprocessedChanges() const;

	/** Get the progress text to display on the context notification */
	FText GetProgressText() const;

private:

	/** A state machine holding information about the current state of the manager */
	FStateMachine<ECurrentState> StateMachine;

private:

	/** Idle processing */
	TOptional<ECurrentState> Idle();
	
	/** Process any remaining pending additions we have */
	TOptional<ECurrentState> ProcessAdditions(const FTimeLimit& TimeLimit);

	/** Save any packages that were created inside ProcessAdditions */
	TOptional<ECurrentState> SavePackages();

	/** Process any remaining pending modifications we have */
	TOptional<ECurrentState> ProcessModifications(const FTimeLimit& TimeLimit);

	/** Process any remaining pending deletions we have */
	TOptional<ECurrentState> ProcessDeletions();

	/** Wait for a user's input. Just updates the progress text for now */
	TOptional<ECurrentState> PendingResponse();

	/** Cleanup an operation that just processed some changes */
	void Cleanup();

private:

	/** Feedback context that can selectively override the global context to consume progress events for saving of assets */
	TSharedPtr<FReimportFeedbackContext> FeedbackContextOverride;

	/** Array of objects that detect changes to directories */
	TArray<FContentDirectoryMonitor> DirectoryMonitors;

	/** A list of packages to save when we've added a bunch of assets */
	TArray<UPackage*> PackagesToSave;

	/** Reentracy guard for when we are making changes to assets */
	bool bGuardAssetChanges;

	/** A timeout used to refresh directory monitors when the user has made an interactive change to the settings */
	FTimeLimit ResetMonitorsTimeout;
};

FAutoReimportManager::FAutoReimportManager()
	: StateMachine(ECurrentState::Idle)
	, bGuardAssetChanges(false)
{
	auto* Settings = GetMutableDefault<UEditorLoadingSavingSettings>();
	Settings->OnSettingChanged().AddRaw(this, &FAutoReimportManager::HandleLoadingSavingSettingChanged);

	FPackageName::OnContentPathMounted().AddRaw(this, &FAutoReimportManager::OnContentPathChanged);
	FPackageName::OnContentPathDismounted().AddRaw(this, &FAutoReimportManager::OnContentPathChanged);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	if (!MessageLogModule.IsRegisteredLogListing("AssetReimport"))
	{
		MessageLogModule.RegisterLogListing("AssetReimport", LOCTEXT("AssetReimportLabel", "Asset Reimport"));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnAssetRenamed().AddRaw(this, &FAutoReimportManager::OnAssetRenamed);

	// Only set this up for content directories if the user has this enabled
	if (Settings->bMonitorContentDirectories)
	{
		SetUpDirectoryMonitors();
	}

	StateMachine.Add(ECurrentState::Idle,					EStateMachineNode::CallOnce,	[this](const FTimeLimit& T){ return this->Idle();					});
	StateMachine.Add(ECurrentState::PendingResponse,		EStateMachineNode::CallOnce,	[this](const FTimeLimit& T){ return this->PendingResponse();		});
	StateMachine.Add(ECurrentState::ProcessAdditions,		EStateMachineNode::CallMany,	[this](const FTimeLimit& T){ return this->ProcessAdditions(T);		});
	StateMachine.Add(ECurrentState::ProcessModifications,	EStateMachineNode::CallMany,	[this](const FTimeLimit& T){ return this->ProcessModifications(T);	});
	StateMachine.Add(ECurrentState::ProcessDeletions,		EStateMachineNode::CallMany,	[this](const FTimeLimit& T){ return this->ProcessDeletions();		});
	StateMachine.Add(ECurrentState::SavePackages,			EStateMachineNode::CallOnce,	[this](const FTimeLimit& T){ return this->SavePackages();			});
}

TArray<FPathAndMountPoint> FAutoReimportManager::GetMonitoredDirectories() const
{
	TArray<FPathAndMountPoint> Dirs;
	for (const auto& Monitor : DirectoryMonitors)
	{
		Dirs.Emplace(Monitor.GetDirectory(), Monitor.GetMountPoint());
	}
	return Dirs;
}

void FAutoReimportManager::IgnoreNewFile(const FString& Filename)
{
	for (auto& Monitor : DirectoryMonitors)
	{
		if (Filename.StartsWith(Monitor.GetDirectory()))
		{
			Monitor.IgnoreNewFile(Filename);
		}
	}
}

void FAutoReimportManager::IgnoreFileModification(const FString& Filename)
{
	for (auto& Monitor : DirectoryMonitors)
	{
		if (Filename.StartsWith(Monitor.GetDirectory()))
		{
			Monitor.IgnoreFileModification(Filename);
		}
	}
}

void FAutoReimportManager::IgnoreMovedFile(const FString& SrcFilename, const FString& DstFilename)
{
	for (auto& Monitor : DirectoryMonitors)
	{
		const bool bSrcInFolder = SrcFilename.StartsWith(Monitor.GetDirectory());
		const bool bDstInFolder = DstFilename.StartsWith(Monitor.GetDirectory());

		if (bSrcInFolder && bDstInFolder)
		{
			Monitor.IgnoreMovedFile(SrcFilename, DstFilename);
		}
		else if (bSrcInFolder)
		{
			Monitor.IgnoreDeletedFile(SrcFilename);
		}
		else if (bDstInFolder)
		{
			Monitor.IgnoreNewFile(DstFilename);
		}
	}
}

void FAutoReimportManager::IgnoreDeletedFile(const FString& Filename)
{
	for (auto& Monitor : DirectoryMonitors)
	{
		if (Filename.StartsWith(Monitor.GetDirectory()))
		{
			Monitor.IgnoreDeletedFile(Filename);
		}
	}
}

void FAutoReimportManager::Destroy()
{
	if (auto* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		AssetRegistryModule->Get().OnAssetRenamed().RemoveAll(this);
	}
	
	if (auto* Settings = GetMutableDefault<UEditorLoadingSavingSettings>())
	{
		Settings->OnSettingChanged().RemoveAll(this);
	}

	FPackageName::OnContentPathMounted().RemoveAll(this);
	FPackageName::OnContentPathDismounted().RemoveAll(this);

	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		AssetRegistryModule->Get().OnInMemoryAssetDeleted().RemoveAll(this);
	}

	// Force a save of all the caches
	DirectoryMonitors.Empty();
}

void FAutoReimportManager::OnAssetRenamed(const FAssetData& AssetData, const FString& OldPath)
{
	if (bGuardAssetChanges)
	{
		return;
	}

	// This code moves a source content file that reside alongside assets when the assets are renamed. We do this under the following conditions:
	// 	1. The sourcefile is solely referenced from the the asset that has been moved
	//	2. Said asset only references a single file
	//	3. The source file resides in the same folder as the asset
	//
	// Additionally, we rename the source file if it matched the name of the asset before the rename/move.
	//	- If we rename the source file, then we also update the reimport paths for the asset

	TArray<FString> SourceFilesRelativeToOldPath;
	for (const auto& Pair : AssetData.TagsAndValues)
	{
		if (Pair.Key.IsEqual(UObject::SourceFileTagName(), ENameCase::IgnoreCase, false))
		{
			SourceFilesRelativeToOldPath.Add(Pair.Value);
		}
	}

	// We move the file with the asset provided it is the only file referenced, and sits right beside the uasset file
	if (SourceFilesRelativeToOldPath.Num() == 1 && !SourceFilesRelativeToOldPath[0].GetCharArray().ContainsByPredicate([](const TCHAR Char) { return Char == '/' || Char == '\\'; }))
	{
		const FString AbsoluteSrcPath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(FPackageName::GetLongPackagePath(OldPath) / TEXT("")));
		const FString AbsoluteDstPath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(AssetData.PackagePath.ToString() / TEXT("")));

		const FString OldAssetName = FPackageName::GetLongPackageAssetName(FPackageName::ObjectPathToPackageName(OldPath));
		FString NewFileName = FPaths::GetBaseFilename(SourceFilesRelativeToOldPath[0]);

		bool bRequireReimportPathUpdate = false;
		if (PackageTools::SanitizePackageName(NewFileName) == OldAssetName)
		{
			NewFileName = AssetData.AssetName.ToString();
			bRequireReimportPathUpdate = true;
		}

		const FString SrcFile = AbsoluteSrcPath / SourceFilesRelativeToOldPath[0];
		const FString DstFile = AbsoluteDstPath / NewFileName + TEXT(".") + FPaths::GetExtension(SourceFilesRelativeToOldPath[0]);

		// We can't do this if multiple assets reference the same file. We should be checking for > 1 referencing asset, but the asset registry
		// filter lookup won't return the recently renamed package because it will be Empty by now, so we check for *anything* referencing the asset (assuming that we'll never find *this* asset).
		const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		if (Utils::FindAssetsPertainingToFile(Registry, SrcFile).Num() > 0)
		{
			return;
		}

		if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*DstFile) &&
			IFileManager::Get().Move(*DstFile, *SrcFile, false /*bReplace */, false, true /* attributes */, true /* don't retry */))
		{
			IgnoreMovedFile(SrcFile, DstFile);

			if (bRequireReimportPathUpdate)
			{
				TArray<FString> Paths;
				Paths.Add(DstFile);

				// Update the reimport file names
				FReimportManager::Instance()->UpdateReimportPaths(AssetData.GetAsset(), Paths);
			}
		}
	}
}

int32 FAutoReimportManager::GetNumUnprocessedChanges() const
{
	return Utils::Reduce(DirectoryMonitors, [](const FContentDirectoryMonitor& Monitor, int32 Total){
		return Total + Monitor.GetNumUnprocessedChanges();
	}, 0);
}

FText FAutoReimportManager::GetProgressText() const
{
	FFormatOrderedArguments Args;
	{
		const int32 Progress = Utils::Reduce(DirectoryMonitors, [](const FContentDirectoryMonitor& Monitor, int32 Total){
			return Total + Monitor.GetWorkProgress();
		}, 0);

		Args.Add(Progress);
	}

	{
		const int32 Total = Utils::Reduce(DirectoryMonitors, [](const FContentDirectoryMonitor& Monitor, int32 InTotal){
			return InTotal + Monitor.GetTotalWork();
		}, 0);

		Args.Add(Total);
	}

	return FText::Format(LOCTEXT("ProcessingChanges", "Processing outstanding content changes ({0} of {1})..."), Args);
}

TOptional<ECurrentState> FAutoReimportManager::ProcessAdditions(const FTimeLimit& TimeLimit)
{
	// Override the global feedback context while we do this to avoid popping up dialogs
	TGuardValue<FFeedbackContext*> ScopedContextOverride(GWarn, FeedbackContextOverride.Get());
	TGuardValue<bool> ScopedAssetChangesGuard(bGuardAssetChanges, true);

	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	TMap<FString, TArray<UFactory*>> Factories;

	TArray<FString> FactoryExtensions;
	FactoryExtensions.Reserve(16);

	// Get the list of valid factories
	for (TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* CurrentClass = (*It);

		if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
			if (Factory->bEditorImport && Factory->ImportPriority >= 0)
			{
				FactoryExtensions.Reset();
				Factory->GetSupportedFileExtensions(FactoryExtensions);

				for (const auto& Ext : FactoryExtensions)
				{
					auto& Array = Factories.FindOrAdd(Ext);
					Array.Add(Factory);
				}
			}
		}
	}

	for (auto& Pair : Factories)
	{
		Pair.Value.Sort([](const UFactory& A, const UFactory& B) { return A.ImportPriority > B.ImportPriority; });
	}

	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.ProcessAdditions(Registry, TimeLimit, PackagesToSave, Factories, *FeedbackContextOverride);
		yield TOptional<ECurrentState>();
	}

	return ECurrentState::ProcessModifications;
}

TOptional<ECurrentState> FAutoReimportManager::ProcessModifications(const FTimeLimit& TimeLimit)
{
	// Override the global feedback context while we do this to avoid popping up dialogs
	TGuardValue<FFeedbackContext*> ScopedContextOverride(GWarn, FeedbackContextOverride.Get());
	TGuardValue<bool> ScopedAssetChangesGuard(bGuardAssetChanges, true);

	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.ProcessModifications(Registry, TimeLimit, PackagesToSave, *FeedbackContextOverride);
		yield TOptional<ECurrentState>();
	}

	return ECurrentState::ProcessDeletions;
}

TOptional<ECurrentState> FAutoReimportManager::ProcessDeletions()
{
	TGuardValue<bool> ScopedAssetChangesGuard(bGuardAssetChanges, true);

	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	TArray<FAssetData> AssetsToDelete;

	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.ExtractAssetsToDelete(Registry, AssetsToDelete);
	}

	if (AssetsToDelete.Num() > 0)
	{
		for (const auto& AssetData : AssetsToDelete)
		{
			FeedbackContextOverride->AddMessage(EMessageSeverity::Info, FText::Format(LOCTEXT("Success_DeletedAsset", "Attempting to delete {0} (its source file has been removed)."), FText::FromName(AssetData.AssetName)));
		}

		ObjectTools::DeleteAssets(AssetsToDelete);
	}

	return ECurrentState::SavePackages;
}

TOptional<ECurrentState> FAutoReimportManager::SavePackages()
{
	// We don't override the context specifically when saving packages so the user gets proper feedback
	//TGuardValue<FFeedbackContext*> ScopedContextOverride(GWarn, FeedbackContextOverride.Get());

	TGuardValue<bool> ScopedAssetChangesGuard(bGuardAssetChanges, true);

	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	if (PackagesToSave.Num() > 0)
	{
		const bool bAlreadyCheckedOut = false;
		const bool bCheckDirty = false;
		const bool bPromptToSave = false;
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave, nullptr, bAlreadyCheckedOut);

		PackagesToSave.Empty();
	}

	// Make sure the progress text is up to date before we close the notification
	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	Cleanup();
	return ECurrentState::Idle;
}

TOptional<ECurrentState> FAutoReimportManager::PendingResponse()
{
	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());
	return TOptional<ECurrentState>();
}

TOptional<ECurrentState> FAutoReimportManager::Idle()
{
	// Check whether we need to reset the monitors or not
	if (ResetMonitorsTimeout.Exceeded())
	{
		const auto* Settings = GetDefault<UEditorLoadingSavingSettings>();
		if (Settings->bMonitorContentDirectories)
		{
			DirectoryMonitors.Empty();
			SetUpDirectoryMonitors();
		}
		else
		{
			// Destroy all the existing monitors, including their file caches
			for (auto& Monitor : DirectoryMonitors)
			{
				Monitor.Destroy();
			}
			DirectoryMonitors.Empty();
		}

		ResetMonitorsTimeout = FTimeLimit();
		return TOptional<ECurrentState>();
	}

	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.Tick();
	}

	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	if (AssetRegistry.IsLoadingAssets())
	{
		return TOptional<ECurrentState>();		
	}

	if (GetNumUnprocessedChanges() > 0)
	{
		// We have some changes so kick off the process
		for (auto& Monitor : DirectoryMonitors)
		{
			Monitor.StartProcessing();
		}

		// Check that we actually have anything to do before kicking off a process
		const int32 WorkTotal = Utils::Reduce(DirectoryMonitors, [](const FContentDirectoryMonitor& Monitor, int32 Total){
			return Total + Monitor.GetTotalWork();
		}, 0);

		if (WorkTotal > 0)
		{
			// Create a new feedback context override
			FeedbackContextOverride = MakeShareable(new FReimportFeedbackContext);
			FeedbackContextOverride->Initialize(SNew(SReimportFeedback, GetProgressText()));

			return ECurrentState::ProcessAdditions;
		}
	}

	return TOptional<ECurrentState>();
}

void FAutoReimportManager::Cleanup()
{
	FeedbackContextOverride->Destroy();
	FeedbackContextOverride = nullptr;
}

void FAutoReimportManager::Tick(float DeltaTime)
{
	// Never spend more than a 60fps frame doing this work (meaning we shouldn't drop below 30fps), we can do more if we're throttling CPU usage (ie editor running in background)
	const FTimeLimit TimeLimit(GEditor->ShouldThrottleCPUUsage() ? 1 / 6.f : 1.f / 60.f);
	StateMachine.Tick(TimeLimit);
}

TStatId FAutoReimportManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAutoReimportManager, STATGROUP_Tickables);
}

void FAutoReimportManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto* Package : PackagesToSave)
	{
		Collector.AddReferencedObject(Package);
	}
}

void FAutoReimportManager::HandleLoadingSavingSettingChanged(FName PropertyName)
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UEditorLoadingSavingSettings, bMonitorContentDirectories) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UEditorLoadingSavingSettings, AutoReimportDirectorySettings))
	{
		ResetMonitorsTimeout = FTimeLimit(5.f);
	}
}

void FAutoReimportManager::OnContentPathChanged(const FString& InAssetPath, const FString& FileSystemPath)
{
	const auto* Settings = GetDefault<UEditorLoadingSavingSettings>();
	if (Settings->bMonitorContentDirectories)
	{
		DirectoryMonitors.Empty();
		SetUpDirectoryMonitors();
	}
}

void FAutoReimportManager::SetUpDirectoryMonitors()
{
	struct FParsedSettings
	{
		FString SourceDirectory;
		FString MountPoint;
		FMatchRules Rules;
	};

	TArray<FParsedSettings> FinalArray;
	auto SupportedExtensions = GetAllFactoryExtensions();
	for (const auto& Setting : GetDefault<UEditorLoadingSavingSettings>()->AutoReimportDirectorySettings)
	{
		FParsedSettings NewMapping;
		NewMapping.SourceDirectory = Setting.SourceDirectory;
		NewMapping.MountPoint = Setting.MountPoint;

		if (!FAutoReimportDirectoryConfig::ParseSourceDirectoryAndMountPoint(NewMapping.SourceDirectory, NewMapping.MountPoint))
		{
			continue;
		}

		// Only include extensions that match a factory
		NewMapping.Rules.SetApplicableExtensions(SupportedExtensions);
		for (const auto& WildcardConfig : Setting.Wildcards)
		{
			NewMapping.Rules.AddWildcardRule(WildcardConfig.Wildcard, WildcardConfig.bInclude);
		}
		
		FinalArray.Add(NewMapping);
	}

	for (int32 Index = 0; Index < FinalArray.Num(); ++Index)
	{
		const auto& Mapping = FinalArray[Index];

		// We only create a directory monitor if there are no other's watching parent directories of this one
		for (int32 OtherIndex = Index + 1; OtherIndex < FinalArray.Num(); ++OtherIndex)
		{
			if (FinalArray[Index].SourceDirectory.StartsWith(FinalArray[OtherIndex].SourceDirectory))
			{
				UE_LOG(LogAutoReimportManager, Warning, TEXT("Unable to watch directory %s as it will conflict with another watching %s."), *FinalArray[Index].SourceDirectory, *FinalArray[OtherIndex].SourceDirectory);
				goto next;
			}
		}

		DirectoryMonitors.Emplace(Mapping.SourceDirectory, Mapping.Rules, Mapping.MountPoint);

	next:
		continue;
	}
}

FString FAutoReimportManager::GetAllFactoryExtensions()
{
	FString AllExtensions;

	// Use a scratch buffer to avoid unnecessary re-allocation
	FString Scratch;
	Scratch.Reserve(32);

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class->IsChildOf(UFactory::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			UFactory* Factory = Cast<UFactory>(Class->GetDefaultObject());
			if (Factory->bEditorImport)
			{
				for (const FString& Format : Factory->Formats)
				{
					int32 Index = INDEX_NONE;
					if (Format.FindChar(';', Index) && Index > 0)
					{
						Scratch.GetCharArray().Reset();
						// Include the ;
						Scratch.AppendChars(*Format, Index + 1);

						if (AllExtensions.Find(Scratch) == INDEX_NONE)
						{
							AllExtensions += Scratch;
						}
					}
				}
			}
		}
	}

	return AllExtensions;
}

UAutoReimportManager::UAutoReimportManager(const FObjectInitializer& Init)
	: Super(Init)
{
}

UAutoReimportManager::~UAutoReimportManager()
{
}

void UAutoReimportManager::Initialize()
{
	Implementation = MakeShareable(new FAutoReimportManager);
}

void UAutoReimportManager::IgnoreNewFile(const FString& Filename)
{
	Implementation->IgnoreNewFile(Filename);
}

void UAutoReimportManager::IgnoreFileModification(const FString& Filename)
{
	Implementation->IgnoreFileModification(Filename);
}

void UAutoReimportManager::IgnoreMovedFile(const FString& SrcFilename, const FString& DstFilename)
{
	Implementation->IgnoreMovedFile(SrcFilename, DstFilename);
}

void UAutoReimportManager::IgnoreDeletedFile(const FString& Filename)
{
	Implementation->IgnoreDeletedFile(Filename);
}

TArray<FPathAndMountPoint> UAutoReimportManager::GetMonitoredDirectories() const
{
	return Implementation->GetMonitoredDirectories();
}

void UAutoReimportManager::BeginDestroy()
{
	Super::BeginDestroy();
	if (Implementation.IsValid())
	{
		Implementation->Destroy();
		Implementation = nullptr;
	}
}


#undef CONDITIONAL_YIELD
#undef LOCTEXT_NAMESPACE

