// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorPCH.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/ILevelViewport.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "SDockTab.h"
#include "MovieSceneBinding.h"
#include "MovieScene.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneMaterialTrack.h"
#include "ScopedTransaction.h"
#include "ClassIconFinder.h"
#include "LevelSequenceSpawnRegister.h"
#include "ISequencerObjectChangeListener.h"

#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"


#define LOCTEXT_NAMESPACE "LevelSequenceEditor"

/**
 * Spawn register used in the editor to add some usability features like maintaining selection states, and projecting spawned state onto spawnable defaults
 */
class FLevelSequenceEditorSpawnRegister : public FLevelSequenceSpawnRegister
{
public:
	/** Weak pointer to the active sequencer */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Constructor */
	FLevelSequenceEditorSpawnRegister()
	{
		bShouldClearSelectionCache = true;

		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnActorSelectionChanged);

		OnActorMovedHandle = GEditor->OnActorMoved().AddLambda([=](AActor* Actor){
			OnSpawnedObjectPropertyChanged(*Actor);
		});
	}

	~FLevelSequenceEditorSpawnRegister()
	{
		GEditor->OnActorMoved().Remove(OnActorMovedHandle);
		if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
		{
			LevelEditor->OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		}
	}

private:

	/** Called to spawn an object */
	virtual UObject* SpawnObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player) override
	{
		TGuardValue<bool> Guard(bShouldClearSelectionCache, false);

		UObject* NewObject = FLevelSequenceSpawnRegister::SpawnObject(BindingId, SequenceInstance, Player);

		// Add an object listener for the spawned object to propagate changes back onto the spawnable default
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid() && NewObject)
		{
			Sequencer->GetObjectChangeListener().GetOnAnyPropertyChanged(*NewObject).AddSP(this, &FLevelSequenceEditorSpawnRegister::OnSpawnedObjectPropertyChanged);

			// Select the actor if we think it should be selected
			AActor* Actor = Cast<AActor>(NewObject);
			if (Actor && SelectedSpawnedObjects.Contains(FMovieSceneSpawnRegisterKey(BindingId, SequenceInstance)))
			{
				GEditor->SelectActor(Actor, true /*bSelected*/, true /*bNotify*/);
			}
		}

		return NewObject;
	}

	/** Called right before an object is about to be destroyed */
	virtual void PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance) override
	{
		TGuardValue<bool> Guard(bShouldClearSelectionCache, false);

		// Cache its selection state
		AActor* Actor = Cast<AActor>(&Object);
		if (Actor && GEditor->GetSelectedActors()->IsSelected(Actor))
		{
			SelectedSpawnedObjects.Add(FMovieSceneSpawnRegisterKey(BindingId, SequenceInstance));
			GEditor->SelectActor(Actor, false /*bSelected*/, true /*bNotify*/);
		}

		// Remove our object listener
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			Sequencer->GetObjectChangeListener().ReportObjectDestroyed(Object);
		}

		FLevelSequenceSpawnRegister::PreDestroyObject(Object, BindingId, SequenceInstance);
	}

	/** Populate a map of properties that are keyed on a particular object */
	void PopulateKeyedPropertyMap(AActor& SpawnedObject, TMap<UObject*, TSet<UProperty*>>& OutKeyedPropertyMap)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

		Sequencer->GetAllKeyedProperties(SpawnedObject, OutKeyedPropertyMap.FindOrAdd(&SpawnedObject));

		for (UActorComponent* Component : SpawnedObject.GetComponents())
		{
			Sequencer->GetAllKeyedProperties(*Component, OutKeyedPropertyMap.FindOrAdd(Component));
		}
	}

	/** Called when the editor selection has changed */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
	{
		if (bShouldClearSelectionCache)
		{
			SelectedSpawnedObjects.Reset();
		}
	}

	/** Called when a property on a spawned object changes */
	void OnSpawnedObjectPropertyChanged(UObject& SpawnedObject)
	{
		using namespace EditorUtilities;

		AActor* Actor = CastChecked<AActor>(&SpawnedObject);
		if (!Actor)
		{
			return;
		}

		TMap<UObject*, TSet<UProperty*>> ObjectToKeyedProperties;
		PopulateKeyedPropertyMap(*Actor, ObjectToKeyedProperties);

		// Copy any changed actor properties onto the default actor, provided they are not keyed
		FCopyOptions Options(ECopyOptions::PropagateChangesToArchetypeInstances);

		// Set up a property filter so only stuff that is not keyed gets copied onto the default
		Options.PropertyFilter = [&](const UProperty& Property, const UObject& Object) -> bool {
			const TSet<UProperty*>* ExcludedProperties = ObjectToKeyedProperties.Find(const_cast<UObject*>(&Object));

			return !ExcludedProperties || !ExcludedProperties->Contains(const_cast<UProperty*>(&Property));
		};

		// Now copy the actor properties
		AActor* DefaultActor = Actor->GetClass()->GetDefaultObject<AActor>();
		EditorUtilities::CopyActorProperties(Actor, DefaultActor, Options);

		// The above function call explicitly doesn't copy the root component transform (so the default actor is always at 0,0,0)
		// But in sequencer, we want the object to have a default transform if it doesn't have a transform track
		static FName RelativeLocation = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation);
		static FName RelativeRotation = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation);
		static FName RelativeScale3D = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D);

		bool bHasKeyedTransform = false;
		for (UProperty* Property : *ObjectToKeyedProperties.Find(Actor))
		{
			FName PropertyName = Property->GetFName();
			bHasKeyedTransform = PropertyName == RelativeLocation || PropertyName == RelativeRotation || PropertyName == RelativeScale3D;
			if (bHasKeyedTransform)
			{
				break;
			}
		}

		// Set the default transform if it's not keyed
		USceneComponent* RootComponent = Actor->GetRootComponent();
		USceneComponent* DefaultRootComponent = DefaultActor->GetRootComponent();

		if (!bHasKeyedTransform && RootComponent && DefaultRootComponent)
		{
			DefaultRootComponent->RelativeLocation = RootComponent->RelativeLocation;
			DefaultRootComponent->RelativeRotation = RootComponent->RelativeRotation;
			DefaultRootComponent->RelativeScale3D = RootComponent->RelativeScale3D;
		}
	}

private:

	/** Handles for delegates that we've bound to */
	FDelegateHandle OnActorMovedHandle, OnActorSelectionChangedHandle;

	/** Set of spawn register keys for objects that should be selected if they are spawned */
	TSet<FMovieSceneSpawnRegisterKey> SelectedSpawnedObjects;

	/** True if we should clear the above selection cache when the editor selection has been changed */
	bool bShouldClearSelectionCache;
};


/* Local constants
 *****************************************************************************/

const FName FLevelSequenceEditorToolkit::SequencerMainTabId(TEXT("Sequencer_SequencerMain"));

namespace SequencerDefs
{
	static const FName SequencerAppIdentifier(TEXT("SequencerApp"));
}


FLevelSequenceEditorToolkit::FLevelSequenceEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{
	// register sequencer menu extenders
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	int32 NewIndex = SequencerModule.GetMenuExtensibilityManager()->GetExtenderDelegates().Add(
		FAssetEditorExtender::CreateRaw(this, &FLevelSequenceEditorToolkit::HandleMenuExtensibilityGetExtender));
	SequencerExtenderHandle = SequencerModule.GetMenuExtensibilityManager()->GetExtenderDelegates()[NewIndex].GetHandle();
}


FLevelSequenceEditorToolkit::~FLevelSequenceEditorToolkit()
{
	Sequencer->Close();

	// unregister delegates
	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		auto& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.OnMapChanged().RemoveAll(this);
	}

	// unregister sequencer menu extenders
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerModule.GetMenuExtensibilityManager()->GetExtenderDelegates().RemoveAll([this](const FAssetEditorExtender& Extender)
	{
		return SequencerExtenderHandle == Extender.GetHandle();
	});
}


/* FLevelSequenceEditorToolkit interface
 *****************************************************************************/

void FLevelSequenceEditorToolkit::Initialize( const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, ULevelSequence* InLevelSequence, bool bEditWithinLevelEditor )
{
	// create tab layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_LevelSequenceEditor")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->Split
				(
					FTabManager::NewStack()
						->AddTab(SequencerMainTabId, ETabState::OpenedTab)
				)
		);

	LevelSequence = InLevelSequence;

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = false;

	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, SequencerDefs::SequencerAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, LevelSequence);

	TSharedRef<FLevelSequenceEditorSpawnRegister> SpawnRegister = MakeShareable(new FLevelSequenceEditorSpawnRegister);

	// initialize sequencer
	FSequencerInitParams SequencerInitParams;
	{
		SequencerInitParams.RootSequence = LevelSequence;
		SequencerInitParams.bEditWithinLevelEditor = bEditWithinLevelEditor;
		SequencerInitParams.ToolkitHost = InitToolkitHost;
		SequencerInitParams.SpawnRegister = SpawnRegister;

		TSharedRef<FExtender> AddMenuExtender = MakeShareable(new FExtender);

		AddMenuExtender->AddMenuExtension("AddTracks", EExtensionHook::Before, nullptr,
			FMenuExtensionDelegate::CreateLambda([=](FMenuBuilder& MenuBuilder){

				MenuBuilder.AddSubMenu(
					LOCTEXT("AddActor_Label", "Actor To Sequencer"),
					LOCTEXT("AddActor_ToolTip", "Allow sequencer to possess an actor that already exists in the current level"),
					FNewMenuDelegate::CreateRaw(this, &FLevelSequenceEditorToolkit::AddPosessActorMenuExtensions),
					false /*bInOpenSubMenuOnClick*/,
					FSlateIcon("LevelSequenceEditorStyle", "LevelSequenceEditor.PossessNewActor")
					);

			})
		);

		SequencerInitParams.ViewParams.AddMenuExtender = AddMenuExtender;
		SequencerInitParams.ViewParams.UniqueName = "LevelSequenceEditor";
	}

	Sequencer = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer").CreateSequencer(SequencerInitParams);
	
	SpawnRegister->WeakSequencer = Sequencer;

	if (bEditWithinLevelEditor)
	{
		// @todo remove when world-centric mode is added
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

		LevelEditorModule.AttachSequencer(Sequencer->GetSequencerWidget(), SharedThis(this));

		// We need to find out when the user loads a new map, because we might need to re-create puppet actors
		// when previewing a MovieScene
		LevelEditorModule.OnMapChanged().AddRaw(this, &FLevelSequenceEditorToolkit::HandleMapChanged);
	}
}


/* IToolkit interface
 *****************************************************************************/

FText FLevelSequenceEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Level Sequence Editor");
}


FName FLevelSequenceEditorToolkit::GetToolkitFName() const
{
	static FName SequencerName("LevelSequenceEditor");
	return SequencerName;
}


FLinearColor FLevelSequenceEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.7, 0.0f, 0.0f, 0.5f);
}


FString FLevelSequenceEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Sequencer ").ToString();
}


void FLevelSequenceEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	if (IsWorldCentricAssetEditor())
	{
		return;
	}

	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_SequencerAssetEditor", "Sequencer"));

	TabManager->RegisterTabSpawner(SequencerMainTabId, FOnSpawnTab::CreateSP(this, &FLevelSequenceEditorToolkit::HandleTabManagerSpawnTab))
		.SetDisplayName(LOCTEXT("SequencerMainTab", "Sequencer"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "LevelSequenceEditor.Tabs.Sequencer"));
}


void FLevelSequenceEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	if (!IsWorldCentricAssetEditor())
	{
		TabManager->UnregisterTabSpawner(SequencerMainTabId);
	}

	// @todo remove when world-centric mode is added
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.AttachSequencer(SNullWidget::NullWidget, nullptr);
}


/* FLevelSequenceEditorToolkit callbacks
 *****************************************************************************/

void FLevelSequenceEditorToolkit::HandleAddComponentActionExecute( UActorComponent* Component )
{
	Sequencer->GetHandleToObject( Component );
}


void FLevelSequenceEditorToolkit::HandleAddComponentMaterialActionExecute( UPrimitiveComponent* Component, int32 MaterialIndex )
{
	FGuid ObjectHandle = Sequencer->GetHandleToObject( Component );
	for ( const FMovieSceneBinding& Binding : Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetBindings() )
	{
		if ( Binding.GetObjectGuid() == ObjectHandle )
		{
			bool bHasMaterialTrack = false;
			for ( UMovieSceneTrack* Track : Binding.GetTracks() )
			{
				UMovieSceneComponentMaterialTrack* MaterialTrack = Cast<UMovieSceneComponentMaterialTrack>( Track );
				if ( MaterialTrack != nullptr && MaterialTrack->GetMaterialIndex() == MaterialIndex )
				{
					bHasMaterialTrack = true;
					break;
				}
			}
			if ( bHasMaterialTrack == false )
			{
				const FScopedTransaction Transaction( LOCTEXT( "AddComponentMaterialTrack", "Add component material track" ) );

				UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
				{
					FocusedMovieScene->Modify();
				}

				UMovieSceneComponentMaterialTrack* MaterialTrack = Cast<UMovieSceneComponentMaterialTrack>(FocusedMovieScene->AddTrack<UMovieSceneComponentMaterialTrack>(ObjectHandle));
				{
					MaterialTrack->Modify();
					MaterialTrack->SetMaterialIndex( MaterialIndex );
				}
				
				Sequencer->NotifyMovieSceneDataChanged();
			}
		}
	}
}

void FLevelSequenceEditorToolkit::AddActorsToSequencer(AActor*const* InActors, int32 NumActors)
{
	const FScopedTransaction Transaction(LOCTEXT("UndoPossessingObject", "Possess Object(s) with Sequencer"));

	UMovieSceneSequence* FocussedSequence = Sequencer->GetFocusedMovieSceneSequence();
	UMovieScene* FocussedMovieScene = FocussedSequence->GetMovieScene();

	FocussedSequence->Modify();

	GEditor->SelectNone(true, true);
	while (NumActors--)
	{
		AActor* ThisActor = *InActors;
		if (!Sequencer->GetFocusedMovieSceneSequenceInstance()->FindObjectId(*ThisActor).IsValid())
		{
			Sequencer->CreateBinding(*ThisActor, ThisActor->GetActorLabel());
		}

		GEditor->SelectActor(ThisActor, true, true);

		InActors++;
	}

	Sequencer->NotifyMovieSceneDataChanged();
}

void FLevelSequenceEditorToolkit::AddPosessActorMenuExtensions(FMenuBuilder& MenuBuilder)
{
	auto IsActorValidForPossession = [=](const AActor* Actor){
		bool bCreateHandleIfMissing = false;
		if (Sequencer.IsValid())
		{
			return !Sequencer->GetHandleToObject((UObject*)Actor, bCreateHandleIfMissing).IsValid();
		}
		return true;
	};

	// Set up a menu entry to add the selected actor(s) to the sequencer
	TArray<AActor*> ActorsValidForPossession;
	GEditor->GetSelectedActors()->GetSelectedObjects(ActorsValidForPossession);
	ActorsValidForPossession.RemoveAll([&](AActor* In){ return !IsActorValidForPossession(In); });

	FText SelectedLabel;
	FSlateIcon ActorIcon(FEditorStyle::GetStyleSetName(), FClassIconFinder::FindIconNameForClass(AActor::StaticClass()));
	if (ActorsValidForPossession.Num() == 1)
	{
		SelectedLabel = FText::Format(LOCTEXT("AddSpecificActor", "Add '{0}'"), FText::FromString(ActorsValidForPossession[0]->GetActorLabel()));
		FName IconName = FClassIconFinder::FindIconNameForActor(ActorsValidForPossession[0]);
		ActorIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), FClassIconFinder::FindIconNameForClass(ActorsValidForPossession[0]->GetClass()));
	}
	else if (ActorsValidForPossession.Num() > 1)
	{
		SelectedLabel = FText::Format(LOCTEXT("AddSpecificActor", "Add Current Selection ({0} actors)"), FText::AsNumber(ActorsValidForPossession.Num()));
	}

	if (!SelectedLabel.IsEmpty())
	{
		// Copy the array into the lambda - probably not that big a deal
		MenuBuilder.AddMenuEntry(SelectedLabel, FText(), ActorIcon, FExecuteAction::CreateLambda([=]{
			FSlateApplication::Get().DismissAllMenus();
			AddActorsToSequencer(ActorsValidForPossession.GetData(), ActorsValidForPossession.Num());
		}));
	}
	
	MenuBuilder.BeginSection("ChooseActorSection", LOCTEXT("ChooseActor", "Choose Actor:"));

	using namespace SceneOutliner;

	// Set up a menu entry to add any arbitrary actor to the sequencer
	FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display actors that are not possessed already
		InitOptions.Filters->AddFilterPredicate( FActorFilterPredicate::CreateLambda( IsActorValidForPossession ) );
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateLambda([=](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					AddActorsToSequencer(&Actor, 1);
				})
			)
		];
	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	MenuBuilder.EndSection();
}


void FLevelSequenceEditorToolkit::HandleMapChanged(class UWorld* NewWorld, EMapChangeType MapChangeType)
{
	Sequencer->NotifyMapChanged(NewWorld, MapChangeType);
}


TSharedRef<FExtender> FLevelSequenceEditorToolkit::HandleMenuExtensibilityGetExtender( const TSharedRef<FUICommandList> CommandList, const TArray<UObject*> ContextSensitiveObjects )
{
	TSharedRef<FExtender> AddTrackMenuExtender(new FExtender());
	AddTrackMenuExtender->AddMenuExtension(
		SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection,
		EExtensionHook::Before,
		CommandList,
		FMenuExtensionDelegate::CreateRaw(this, &FLevelSequenceEditorToolkit::HandleTrackMenuExtensionAddTrack, ContextSensitiveObjects));

	return AddTrackMenuExtender;
}


TSharedRef<SDockTab> FLevelSequenceEditorToolkit::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args)
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	if (Args.GetTabId() == SequencerMainTabId)
	{
		TabWidget = Sequencer->GetSequencerWidget();
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("SequencerMainTitle", "Sequencer"))
		.TabColorScale(GetTabColorScale())
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}


void FLevelSequenceEditorToolkit::HandleTrackMenuExtensionAddTrack( FMenuBuilder& AddTrackMenuBuilder, TArray<UObject*> ContextObjects )
{
	if (ContextObjects.Num() != 1)
	{
		return;
	}

	AActor* Actor = Cast<AActor>(ContextObjects[0]);
	if (Actor != nullptr)
	{
		AddTrackMenuBuilder.BeginSection("Components", LOCTEXT("ComponentsSection", "Components"));
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				FUIAction AddComponentAction(FExecuteAction::CreateSP(this, &FLevelSequenceEditorToolkit::HandleAddComponentActionExecute, Component));
				FText AddComponentLabel = FText::FromString(Component->GetName());
				FText AddComponentToolTip = FText::Format(LOCTEXT("ComponentToolTipFormat", "Add {0} component"), FText::FromString(Component->GetName()));
				AddTrackMenuBuilder.AddMenuEntry(AddComponentLabel, AddComponentToolTip, FSlateIcon(), AddComponentAction);
			}
		}
		AddTrackMenuBuilder.EndSection();
	}
	else
	{
		UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(ContextObjects[0]);
		if (Component != nullptr)
		{
			int32 NumMaterials = Component->GetNumMaterials();
			if (NumMaterials > 0)
			{
				AddTrackMenuBuilder.BeginSection("Materials", LOCTEXT("MaterialSection", "Materials"));
				{
					for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++)
					{
						FUIAction AddComponentMaterialAction(FExecuteAction::CreateSP(this, &FLevelSequenceEditorToolkit::HandleAddComponentMaterialActionExecute, Component, MaterialIndex));
						FText AddComponentMaterialLabel = FText::Format(LOCTEXT("ComponentMaterialIndexLabelFormat", "Element {0}"), FText::AsNumber(MaterialIndex));
						FText AddComponentMaterialToolTip = FText::Format(LOCTEXT("ComponentMaterialIndexToolTipFormat", "Add material element {0}" ), FText::AsNumber(MaterialIndex));
						AddTrackMenuBuilder.AddMenuEntry( AddComponentMaterialLabel, AddComponentMaterialToolTip, FSlateIcon(), AddComponentMaterialAction );
					}
				}
				AddTrackMenuBuilder.EndSection();
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
