// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "AutoReimportUtilities.h"
#include "AutoReimport/AutoReimportManager.h"
#include "ContentDirectoryMonitor.h"

#include "ObjectTools.h"
#include "ARFilter.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "ReimportFeedbackContext.h"
#include "MessageLogModule.h"

#define LOCTEXT_NAMESPACE "AutoReimportManager"
#define yield if (TimeLimit.Exceeded()) return

DEFINE_LOG_CATEGORY_STATIC(LogAutoReimportManager, Log, All);


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
	Idle, PendingResponse, ProcessAdditions, SavePackages, ProcessModifications, ProcessDeletions
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
	TArray<FPathAndMountPoint> GetMonitoredDirectories() const;
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
	TOptional<ECurrentState> Idle(const FTimeLimit& Limit);
	
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

	/** Cached string of extensions that are supported by all available factories */
	FString SupportedExtensions;

	/** Array of objects that detect changes to directories */
	TArray<FContentDirectoryMonitor> DirectoryMonitors;

	/** A list of packages to save when we've added a bunch of assets */
	TArray<UPackage*> PackagesToSave;

	/** Set when we are processing changes to avoid reentrant logic for asset deletions etc */
	bool bIsProcessingChanges;

	/** The time when the last change to the cache was reported */
	FTimeLimit StartProcessingDelay;

	/** The cached number of unprocessed changes we currently have to process */
	int32 CachedNumUnprocessedChanges;
};

FAutoReimportManager::FAutoReimportManager()
	: StateMachine(ECurrentState::Idle)
	, bIsProcessingChanges(false)
	, StartProcessingDelay(0.5)
{
	// @todo: arodham: update this when modules are reloaded or new factory types are available?
	SupportedExtensions = GetAllFactoryExtensions();

	auto* Settings = GetMutableDefault<UEditorLoadingSavingSettings>();
	Settings->OnSettingChanged().AddRaw(this, &FAutoReimportManager::HandleLoadingSavingSettingChanged);

	FPackageName::OnContentPathMounted().AddRaw(this, &FAutoReimportManager::OnContentPathChanged);
	FPackageName::OnContentPathDismounted().AddRaw(this, &FAutoReimportManager::OnContentPathChanged);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	if (!MessageLogModule.IsRegisteredLogListing("AssetReimport"))
	{
		MessageLogModule.RegisterLogListing("AssetReimport", LOCTEXT("AssetReimportLabel", "Asset Reimport"));
	}

	// Only set this up for content directories if the user has this enabled
	if (Settings->bMonitorContentDirectories)
	{
		SetUpDirectoryMonitors();
	}

	StateMachine.Add(ECurrentState::Idle,					EStateMachineNode::CallOnce,	[this](const FTimeLimit& T){ return this->Idle(T);					});
	StateMachine.Add(ECurrentState::PendingResponse,		EStateMachineNode::CallOnce,	[this](const FTimeLimit& T){ return this->PendingResponse();		});
	StateMachine.Add(ECurrentState::ProcessAdditions,		EStateMachineNode::CallMany,	[this](const FTimeLimit& T){ return this->ProcessAdditions(T);		});
	StateMachine.Add(ECurrentState::SavePackages,			EStateMachineNode::CallOnce,	[this](const FTimeLimit& T){ return this->SavePackages();			});
	StateMachine.Add(ECurrentState::ProcessModifications,	EStateMachineNode::CallMany,	[this](const FTimeLimit& T){ return this->ProcessModifications(T);	});
	StateMachine.Add(ECurrentState::ProcessDeletions,		EStateMachineNode::CallMany,	[this](const FTimeLimit& T){ return this->ProcessDeletions();		});
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

void FAutoReimportManager::Destroy()
{
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
		const int32 Total = Utils::Reduce(DirectoryMonitors, [](const FContentDirectoryMonitor& Monitor, int32 Total){
			return Total + Monitor.GetTotalWork();
		}, 0);

		Args.Add(Total);
	}

	return FText::Format(LOCTEXT("ProcessingChanges", "Processing outstanding content changes ({0} of {1})..."), Args);
}

TOptional<ECurrentState> FAutoReimportManager::ProcessAdditions(const FTimeLimit& TimeLimit)
{
	// Override the global feedback context while we do this to avoid popping up dialogs
	TGuardValue<FFeedbackContext*> ScopedContextOverride(GWarn, FeedbackContextOverride.Get());

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

	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.ProcessAdditions(PackagesToSave, TimeLimit, Factories, *FeedbackContextOverride);
		yield TOptional<ECurrentState>();
	}

	return ECurrentState::SavePackages;
}

TOptional<ECurrentState> FAutoReimportManager::SavePackages()
{
	// We don't override the context specifically when saving packages so the user gets proper feedback
	//TGuardValue<FFeedbackContext*> ScopedContextOverride(GWarn, FeedbackContextOverride.Get());

	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	if (PackagesToSave.Num() > 0)
	{
		const bool bAlreadyCheckedOut = false;
		const bool bCheckDirty = false;
		const bool bPromptToSave = false;
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave, nullptr, bAlreadyCheckedOut);

		PackagesToSave.Empty();
	}

	return ECurrentState::ProcessModifications;
}

TOptional<ECurrentState> FAutoReimportManager::ProcessModifications(const FTimeLimit& TimeLimit)
{
	// Override the global feedback context while we do this to avoid popping up dialogs
	TGuardValue<FFeedbackContext*> ScopedContextOverride(GWarn, FeedbackContextOverride.Get());

	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.ProcessModifications(Registry, TimeLimit, *FeedbackContextOverride);
		yield TOptional<ECurrentState>();
	}

	return ECurrentState::ProcessDeletions;
}

TOptional<ECurrentState> FAutoReimportManager::ProcessDeletions()
{
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());

	TArray<FAssetData> AssetsToDelete;

	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.ExtractAssetsToDelete(Registry, AssetsToDelete);
	}

	if (AssetsToDelete.Num() > 0)
	{
		ObjectTools::DeleteAssets(AssetsToDelete);
		for (const auto& AssetData : AssetsToDelete)
		{
			FeedbackContextOverride->AddMessage(EMessageSeverity::Info, FText::Format(LOCTEXT("Success_DeletedAsset", "Deleted {0}."), FText::FromName(AssetData.AssetName)));
		}
	}

	Cleanup();
	return ECurrentState::Idle;
}

TOptional<ECurrentState> FAutoReimportManager::PendingResponse()
{
	FeedbackContextOverride->GetContent()->SetMainText(GetProgressText());
	return TOptional<ECurrentState>();
}

TOptional<ECurrentState> FAutoReimportManager::Idle(const FTimeLimit& TimeLimit)
{
	for (auto& Monitor : DirectoryMonitors)
	{
		Monitor.Tick(TimeLimit);
		yield TOptional<ECurrentState>();
	}

	const int32 NumUnprocessedChanges = GetNumUnprocessedChanges();
	if (NumUnprocessedChanges > 0)
	{
		// Check if we have any more unprocessed changes. If we have, we wait a delay before processing them.
		if (NumUnprocessedChanges != CachedNumUnprocessedChanges)
		{
			CachedNumUnprocessedChanges = NumUnprocessedChanges;
			StartProcessingDelay.Reset();
		}
		else
		{
			if (StartProcessingDelay.Exceeded())
			{
				// Deal with changes to the file system second
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
		}
	}

	return TOptional<ECurrentState>();
}

void FAutoReimportManager::Cleanup()
{
	CachedNumUnprocessedChanges = 0;

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
		PropertyName == GET_MEMBER_NAME_CHECKED(UEditorLoadingSavingSettings, AutoReimportDirectories))
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
	TArray<FPathAndMountPoint> CollapsedDirectories;

	// Build an array of directory / mounted path so we can match up absolute paths that the user has typed in
	TArray<TPair<FString, FString>> MountedPaths;
	{
		TArray<FString> RootContentPaths;
		FPackageName::QueryRootContentPaths( RootContentPaths );
		for (FString& RootPath : RootContentPaths)
		{
			FString ContentFolder = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(RootPath));
			MountedPaths.Add( TPairInitializer<FString, FString>(MoveTemp(ContentFolder), MoveTemp(RootPath)) );
		}
	}


	auto& FileManager = IFileManager::Get();
	for (const FString& Path : GetDefault<UEditorLoadingSavingSettings>()->AutoReimportDirectories)
	{
		FPathAndMountPoint ThisPath;

		const FName MountPoint = FPackageName::GetPackageMountPoint(Path);
		if (!MountPoint.IsNone())
		{
			ThisPath.Path = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Path / TEXT(""))) / "";
		}
		else
		{
			ThisPath.Path = FPaths::ConvertRelativePathToFull(Path) / "";
		}

		if (FileManager.DirectoryExists(*ThisPath.Path))
		{
			// Set the mounted path if necessary
			auto* Pair = MountedPaths.FindByPredicate([&](const TPair<FString, FString>& Pair){
				return ThisPath.Path.StartsWith(Pair.Key);
			});

			if (Pair)
			{
				ThisPath.MountPoint = Pair->Value / ThisPath.Path.RightChop(Pair->Key.Len());
			}

			CollapsedDirectories.Add(ThisPath);
		}
	}

	Utils::RemoveDuplicates(CollapsedDirectories, [](const FPathAndMountPoint& A, const FPathAndMountPoint& B){
		return A.Path.StartsWith(B.Path, ESearchCase::IgnoreCase) ||
			   B.Path.StartsWith(A.Path, ESearchCase::IgnoreCase);
	});

	for (const auto& Dir : CollapsedDirectories)
	{
		DirectoryMonitors.Emplace(Dir.Path, SupportedExtensions, Dir.MountPoint);
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

