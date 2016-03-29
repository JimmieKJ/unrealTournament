// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorPCH.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/ILevelViewport.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "ClassIconFinder.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneMaterialTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieSceneSequenceInstance.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "SDockTab.h"

// @todo sequencer: hack: setting defaults for transform tracks
#include "MovieScene3DTransformSection.h"
#include "MovieScene3DTransformTrack.h"

#include "SequencerSpawnRegister.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditor"


/* Local constants
 *****************************************************************************/

const FName FLevelSequenceEditorToolkit::SequencerMainTabId(TEXT("Sequencer_SequencerMain"));

namespace SequencerDefs
{
	static const FName SequencerAppIdentifier(TEXT("SequencerApp"));
}


static TArray<FLevelSequenceEditorToolkit*> OpenToolkits;

void FLevelSequenceEditorToolkit::IterateOpenToolkits(TFunctionRef<bool(FLevelSequenceEditorToolkit&)> Iter)
{
	for (FLevelSequenceEditorToolkit* Toolkit : OpenToolkits)
	{
		if (!Iter(*Toolkit))
		{
			return;
		}
	}
}

FLevelSequenceEditorToolkit::FLevelSequenceEditorToolkitOpened& FLevelSequenceEditorToolkit::OnOpened()
{
	static FLevelSequenceEditorToolkitOpened OnOpenedEvent;
	return OnOpenedEvent;
}

/* FLevelSequenceEditorToolkit structors
 *****************************************************************************/

FLevelSequenceEditorToolkit::FLevelSequenceEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{
	// register sequencer menu extenders
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	int32 NewIndex = SequencerModule.GetMenuExtensibilityManager()->GetExtenderDelegates().Add(
		FAssetEditorExtender::CreateRaw(this, &FLevelSequenceEditorToolkit::HandleMenuExtensibilityGetExtender));
	SequencerExtenderHandle = SequencerModule.GetMenuExtensibilityManager()->GetExtenderDelegates()[NewIndex].GetHandle();

	OpenToolkits.Add(this);
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

void FLevelSequenceEditorToolkit::Initialize(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, ULevelSequence* InLevelSequence, bool bEditWithinLevelEditor)
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

	TSharedRef<FLevelSequenceEditorSpawnRegister> SpawnRegister = MakeShareable(new TSequencerSpawnRegister<FLevelSequenceEditorSpawnRegister>);

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
	SpawnRegister->SetSequencer(Sequencer);
	Sequencer->OnActorAddedToSequencer().AddSP(this, &FLevelSequenceEditorToolkit::HandleActorAddedToSequencer);

	if (bEditWithinLevelEditor)
	{
		// @todo remove when world-centric mode is added
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

		LevelEditorModule.AttachSequencer(Sequencer->GetSequencerWidget(), SharedThis(this));

		// We need to find out when the user loads a new map, because we might need to re-create puppet actors
		// when previewing a MovieScene
		LevelEditorModule.OnMapChanged().AddRaw(this, &FLevelSequenceEditorToolkit::HandleMapChanged);
	}

	FLevelSequenceEditorToolkit::OnOpened().Broadcast(*this);
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

void FLevelSequenceEditorToolkit::HandleAddComponentActionExecute(UActorComponent* Component)
{
	Sequencer->GetHandleToObject(Component);
}


void FLevelSequenceEditorToolkit::HandleAddComponentMaterialActionExecute(UPrimitiveComponent* Component, int32 MaterialIndex)
{
	FGuid ObjectHandle = Sequencer->GetHandleToObject(Component);
	for (const FMovieSceneBinding& Binding : Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetBindings())
	{
		if (Binding.GetObjectGuid() == ObjectHandle)
		{
			bool bHasMaterialTrack = false;
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				UMovieSceneComponentMaterialTrack* MaterialTrack = Cast<UMovieSceneComponentMaterialTrack>(Track);
				if (MaterialTrack != nullptr && MaterialTrack->GetMaterialIndex() == MaterialIndex)
				{
					bHasMaterialTrack = true;
					break;
				}
			}
			if (bHasMaterialTrack == false)
			{
				const FScopedTransaction Transaction(LOCTEXT("AddComponentMaterialTrack", "Add component material track"));

				UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
				{
					FocusedMovieScene->Modify();
				}

				UMovieSceneComponentMaterialTrack* MaterialTrack = Cast<UMovieSceneComponentMaterialTrack>(FocusedMovieScene->AddTrack<UMovieSceneComponentMaterialTrack>(ObjectHandle));
				{
					MaterialTrack->Modify();
					MaterialTrack->SetMaterialIndex(MaterialIndex);
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
			FGuid Binding = Sequencer->CreateBinding(*ThisActor, ThisActor->GetActorLabel());

			Sequencer->UpdateRuntimeInstances();

			AddDefaultTracksForActor(*ThisActor, Binding);
		}

		GEditor->SelectActor(ThisActor, true, true);
		InActors++;
	}

	Sequencer->NotifyMovieSceneDataChanged();
}

void FLevelSequenceEditorToolkit::HandleActorAddedToSequencer(AActor* Actor, const FGuid Binding)
{
	AddDefaultTracksForActor(*Actor, Binding);
}

void FLevelSequenceEditorToolkit::AddDefaultTracksForActor(AActor& Actor, const FGuid Binding)
{
	// get focused movie scene
	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();

	if (Sequence == nullptr)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	// add default tracks
	for (const FLevelSequenceTrackSettings& TrackSettings : GetDefault<ULevelSequenceEditorSettings>()->TrackSettings)
	{
		UClass* MatchingActorClass = TrackSettings.MatchingActorClass.ResolveClass();

		if ((MatchingActorClass == nullptr) || !Actor.IsA(MatchingActorClass))
		{
			continue;
		}

		// add tracks by type
		for (const FStringClassReference& DefaultTrack : TrackSettings.DefaultTracks)
		{
			UClass* TrackClass = DefaultTrack.ResolveClass();

			if (TrackClass != nullptr)
			{
				UMovieSceneTrack* NewTrack = MovieScene->AddTrack(TrackClass, Binding);

				// Create a section for any property tracks
				if (Cast<UMovieScenePropertyTrack>(NewTrack))
				{
					UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
					NewTrack->AddSection(*NewSection);

					// @todo sequencer: hack: setting defaults for transform tracks
					if (NewTrack->IsA(UMovieScene3DTransformTrack::StaticClass()))
					{
						auto TransformSection = Cast<UMovieScene3DTransformSection>(NewSection);

						FVector Location = Actor.GetActorLocation();
						FRotator Rotation = Actor.GetActorRotation();
						FVector Scale = Actor.GetActorScale();

						if (Actor.GetRootComponent())
						{
							FTransform ActorRelativeTransform = Actor.GetRootComponent()->GetRelativeTransform();

							Location = ActorRelativeTransform.GetTranslation();
							Rotation = ActorRelativeTransform.GetRotation().Rotator();
							Scale = ActorRelativeTransform.GetScale3D();
						}

						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::X, Location.X, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::Y, Location.Y, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::Z, Location.Z, false /*bUnwindRotation*/));

						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::X, Rotation.Euler().X, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Y, Rotation.Euler().Y, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Z, Rotation.Euler().Z, false /*bUnwindRotation*/));

						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::X, Scale.X, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::Y, Scale.Y, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::Z, Scale.Z, false /*bUnwindRotation*/));
					}

					NewSection->SetIsInfinite(GetSequencer()->GetInfiniteKeyAreas());
				}

				Sequencer->UpdateRuntimeInstances();
			}
		}

		// add tracks by property
		for (const FLevelSequencePropertyTrackSettings& PropertyTrackSettings : TrackSettings.DefaultPropertyTracks)
		{
			TArray<UProperty*> PropertyPath;
			UObject* PropertyOwner = &Actor;

			// determine object hierarchy
			TArray<FString> ComponentNames;
			PropertyTrackSettings.ComponentPath.ParseIntoArray(ComponentNames, TEXT("."));

			for (const FString& ComponentName : ComponentNames)
			{
				PropertyOwner = FindObjectFast<UObject>(PropertyOwner, *ComponentName);

				if (PropertyOwner == nullptr)
				{
					return;
				}
			}

			UStruct* PropertyOwnerClass = PropertyOwner->GetClass();

			// determine property path
			TArray<FString> PropertyNames;
			PropertyTrackSettings.PropertyPath.ParseIntoArray(PropertyNames, TEXT("."));

			for (const FString& PropertyName : PropertyNames)
			{
				UProperty* Property = PropertyOwnerClass->FindPropertyByName(*PropertyName);

				if (Property != nullptr)
				{
					PropertyPath.Add(Property);
				}

				UStructProperty* StructProperty = Cast<UStructProperty>(Property);

				if (StructProperty != nullptr)
				{
					PropertyOwnerClass = StructProperty->Struct;
					continue;
				}

				UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);

				if (ObjectProperty != nullptr)
				{
					PropertyOwnerClass = ObjectProperty->PropertyClass;
					continue;
				}

				break;
			}

			if (!Sequencer->CanKeyProperty(FCanKeyPropertyParams(Actor.GetClass(), PropertyPath)))
			{
				continue;
			}

			// key property
			FKeyPropertyParams KeyPropertyParams(TArrayBuilder<UObject*>().Add(PropertyOwner), PropertyPath);
			{
				KeyPropertyParams.KeyParams.bCreateTrackIfMissing = true;
				KeyPropertyParams.KeyParams.bCreateHandleIfMissing = true;
				KeyPropertyParams.KeyParams.bCreateKeyIfUnchanged = false;
				KeyPropertyParams.KeyParams.bCreateKeyIfEmpty = false;
				KeyPropertyParams.KeyParams.bCreateKeyOnlyWhenAutoKeying = false;
			}

			Sequencer->KeyProperty(KeyPropertyParams);
			Sequencer->UpdateRuntimeInstances();
		}
	}
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
		InitOptions.Filters->AddFilterPredicate(FActorFilterPredicate::CreateLambda(IsActorValidForPossession));
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew(SVerticalBox)
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


TSharedRef<FExtender> FLevelSequenceEditorToolkit::HandleMenuExtensibilityGetExtender(const TSharedRef<FUICommandList> CommandList, const TArray<UObject*> ContextSensitiveObjects)
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


void FLevelSequenceEditorToolkit::HandleTrackMenuExtensionAddTrack(FMenuBuilder& AddTrackMenuBuilder, TArray<UObject*> ContextObjects)
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
						FText AddComponentMaterialToolTip = FText::Format(LOCTEXT("ComponentMaterialIndexToolTipFormat", "Add material element {0}"), FText::AsNumber(MaterialIndex));
						AddTrackMenuBuilder.AddMenuEntry(AddComponentMaterialLabel, AddComponentMaterialToolTip, FSlateIcon(), AddComponentMaterialAction);
					}
				}
				AddTrackMenuBuilder.EndSection();
			}
		}
	}
}

bool FLevelSequenceEditorToolkit::OnRequestClose()
{
	OpenToolkits.Remove(this);
	OnClosedEvent.Broadcast();
	return true;
}

#undef LOCTEXT_NAMESPACE
