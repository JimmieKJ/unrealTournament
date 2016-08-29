// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorPCH.h"
#include "ISequencer.h"
#include "ISequencerObjectChangeListener.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "MovieScene.h"
#include "AssetSelection.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditorSpawnRegister"

/* FLevelSequenceEditorSpawnRegister structors
 *****************************************************************************/

FLevelSequenceEditorSpawnRegister::FLevelSequenceEditorSpawnRegister()
{
	bShouldClearSelectionCache = true;

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddRaw(this, &FLevelSequenceEditorSpawnRegister::HandleActorSelectionChanged);

	FAreObjectsEditable AreObjectsEditable = FAreObjectsEditable::CreateRaw(this, &FLevelSequenceEditorSpawnRegister::AreObjectsEditable);
	OnAreObjectsEditableHandle = AreObjectsEditable.GetHandle();
	LevelEditor.AddEditableObjectPredicate(AreObjectsEditable);

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnObjectsReplaced);
#endif
}


FLevelSequenceEditorSpawnRegister::~FLevelSequenceEditorSpawnRegister()
{
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditor->OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		LevelEditor->RemoveEditableObjectPredicate(OnAreObjectsEditableHandle);
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->OnPreSave().RemoveAll(this);
		Sequencer->OnActivateSequence().RemoveAll(this);
	}

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().RemoveAll(this);
#endif
}


/* FLevelSequenceSpawnRegister interface
 *****************************************************************************/

UObject* FLevelSequenceEditorSpawnRegister::SpawnObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);
	AActor* NewObject = Cast<AActor>(FLevelSequenceSpawnRegister::SpawnObject(BindingId, SequenceInstance, Player));
	
	if (NewObject)
	{
		// Cache the spawned object, and editable state first
		SpawnedObjects.FindOrAdd(SequenceInstance.GetInstanceId()).Add(NewObject);

		// Select the actor if we think it should be selected
		if (SelectedSpawnedObjects.Contains(FMovieSceneSpawnRegisterKey(BindingId, SequenceInstance)))
		{
			GEditor->SelectActor(NewObject, true /*bSelected*/, true /*bNotify*/);
		}
	}

	return NewObject;
}


void FLevelSequenceEditorSpawnRegister::PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);
	
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	// We only save default state for the currently focussed movie scene sequence instance
	if (Sequencer.IsValid() && &Sequencer->GetFocusedMovieSceneSequenceInstance().Get() == &SequenceInstance)
	{
		SaveDefaultSpawnableState(BindingId, SequenceInstance);
	}

	// Cache its selection state
	AActor* Actor = Cast<AActor>(&Object);
	if (Actor && GEditor->GetSelectedActors()->IsSelected(Actor))
	{
		SelectedSpawnedObjects.Add(FMovieSceneSpawnRegisterKey(BindingId, SequenceInstance));
		GEditor->SelectActor(Actor, false /*bSelected*/, true /*bNotify*/);
	}
	
	// Remove the spawned object (and anything that's null) from our cache
	TSet<FObjectKey>* ExistingObjects = SpawnedObjects.Find(SequenceInstance.GetInstanceId());
	if (ExistingObjects)
	{
		ExistingObjects->Remove(FObjectKey(&Object));
		if (ExistingObjects->Num() == 0)
		{
			SpawnedObjects.Remove(SequenceInstance.GetInstanceId());
		}
	}

	FLevelSequenceSpawnRegister::PreDestroyObject(Object, BindingId, SequenceInstance);
}

void FLevelSequenceEditorSpawnRegister::SaveDefaultSpawnableState(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	UObject* Object = SequenceInstance.FindObject(BindingId, *Sequencer);
	if (!Object)
	{
		return;
	}

	// Find the spawnable definition
	FMovieSceneSpawnable* Spawnable = SequenceInstance.GetSequence()->GetMovieScene()->FindSpawnable(BindingId);
	if (Spawnable)
	{
		SequenceInstance.RestoreSpecificState(BindingId, *Sequencer);
		Spawnable->CopyObjectTemplate(*Object, *SequenceInstance.GetSequence());
	}
}

void FLevelSequenceEditorSpawnRegister::OnPreSaveMovieScene(ISequencer& InSequencer)
{
	// We're about to save the movie scene(s), so we need to save default spawnable state for the currently focused movie scene sequence instance

	TSharedPtr<FMovieSceneSequenceInstance> Instance = InSequencer.GetFocusedMovieSceneSequenceInstance();
	UMovieSceneSequence* Sequence = Instance->GetSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	for (int32 SpawnableIndex = 0; SpawnableIndex < MovieScene->GetSpawnableCount(); ++SpawnableIndex)
	{
		FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(SpawnableIndex);
		SaveDefaultSpawnableState(Spawnable.GetGuid(), *Instance);
	}
}

void FLevelSequenceEditorSpawnRegister::OnSequenceInstanceActivated(FMovieSceneSequenceInstance& ActiveInstance)
{
	ActiveSequence = ActiveInstance.GetInstanceId();
}

/* FLevelSequenceEditorSpawnRegister implementation
 *****************************************************************************/

void FLevelSequenceEditorSpawnRegister::SetSequencer(const TSharedPtr<ISequencer>& Sequencer)
{
	WeakSequencer = Sequencer;
	Sequencer->OnPreSave().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnPreSaveMovieScene);
	Sequencer->OnActivateSequence().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnSequenceInstanceActivated);

	ActiveSequence = Sequencer->GetFocusedMovieSceneSequenceInstance()->GetInstanceId();
}


/* FLevelSequenceEditorSpawnRegister callbacks
 *****************************************************************************/

void FLevelSequenceEditorSpawnRegister::HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (bShouldClearSelectionCache)
	{
		SelectedSpawnedObjects.Reset();
	}
}

bool FLevelSequenceEditorSpawnRegister::AreObjectsEditable(const TArray<TWeakObjectPtr<UObject>>& InObjects) const
{
	// Check that none of the objects specified are (or belong to) a spawned actor
	for (const TWeakObjectPtr<UObject>& WeakObject : InObjects)
	{
		UObject* Object = WeakObject.Get();
		if (!Object)
		{
			continue;
		}

		AActor* SourceActor = Cast<AActor>(Object);
		if (!SourceActor)
		{
			UActorComponent* ActorComponent = Cast<UActorComponent>(Object);
			if (ActorComponent)
			{
				SourceActor = ActorComponent->GetOwner();
			}
		}

		if (!SourceActor)
		{
			continue;
		}

		FObjectKey ThisObject(SourceActor);
		for (auto& Pair : SpawnedObjects)
		{
			if (Pair.Key != ActiveSequence && Pair.Value.Contains(ThisObject))
			{
				return false;
			}
		}
	}
	return true;
}

void FLevelSequenceEditorSpawnRegister::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	for (auto& Pair : Register)
	{
		TWeakObjectPtr<>& WeakObject = Pair.Value.Object;
		UObject* SpawnedObject = WeakObject.Get();
		if (UObject* NewObject = OldToNewInstanceMap.FindRef(SpawnedObject))
		{
			WeakObject = NewObject;
			// It's a spawnable, so ensure it's transient
			NewObject->SetFlags(RF_Transient);
			TSharedPtr<FMovieSceneSequenceInstance> Instance = Pair.Key.SequenceInstance.Pin();
			if (Instance.IsValid())
			{
				Instance->OnObjectSpawned(Pair.Key.BindingId, *NewObject, *Sequencer);
			}
		}
	}
}

#if WITH_EDITOR

TValueOrError<FNewSpawnable, FText> FLevelSequenceEditorSpawnRegister::CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene)
{
	FNewSpawnable NewSpawnable(nullptr, FName::NameToDisplayString(SourceObject.GetName(), false));

	const FName TemplateName = MakeUniqueObjectName(&OwnerMovieScene, UObject::StaticClass(), SourceObject.GetFName());

	// First off, deal with creating a spawnable from a class
	if (UClass* InClass = Cast<UClass>(&SourceObject))
	{
		if (!InClass->IsChildOf(AActor::StaticClass()))
		{
			FText ErrorText = FText::Format(LOCTEXT("NotAnActorClass", "Unable to add spawnable for class of type '{0}' since it is not a valid actor class."), FText::FromString(InClass->GetName()));
			return MakeError(ErrorText);
		}

		NewSpawnable.ObjectTemplate = NewObject<UObject>(&OwnerMovieScene, InClass, TemplateName);
	}

	// Deal with creating a spawnable from an instance of an actor
	else if (AActor* Actor = Cast<AActor>(&SourceObject))
	{
		NewSpawnable.ObjectTemplate = StaticDuplicateObject(Actor, &OwnerMovieScene, TemplateName, RF_AllFlags & ~RF_Transactional);
		NewSpawnable.Name = Actor->GetActorLabel();
	}

	// If it's a blueprint, we need some special handling
	else if (UBlueprint* SourceBlueprint = Cast<UBlueprint>(&SourceObject))
	{
		NewSpawnable.ObjectTemplate = NewObject<UObject>(&OwnerMovieScene, SourceBlueprint->GeneratedClass, TemplateName);
	}

	// At this point we have to assume it's an asset
	else
	{
		// @todo sequencer: Add support for forcing specific factories for an asset?
		UActorFactory* FactoryToUse = FActorFactoryAssetProxy::GetFactoryForAssetObject(&SourceObject);
		if (!FactoryToUse)
		{
			FText ErrorText = FText::Format(LOCTEXT("CouldNotFindFactory", "Unable to create spawnable from  asset '{0}' - no valid factory could be found."), FText::FromString(SourceObject.GetName()));
			return MakeError(ErrorText);
		}

		FText ErrorText;
		if (!FactoryToUse->CanCreateActorFrom(FAssetData(&SourceObject), ErrorText))
		{
			if (!ErrorText.IsEmpty())
			{
				return MakeError(FText::Format(LOCTEXT("CannotCreateActorFromAsset_Ex", "Unable to create spawnable from  asset '{0}'. {1}."), FText::FromString(SourceObject.GetName()), ErrorText));
			}
			else
			{
				return MakeError(FText::Format(LOCTEXT("CannotCreateActorFromAsset", "Unable to create spawnable from  asset '{0}'."), FText::FromString(SourceObject.GetName())));
			}
		}

		AActor* Instance = FactoryToUse->CreateActor(&SourceObject, GWorld->PersistentLevel, FTransform(), RF_NoFlags, TemplateName );

		NewSpawnable.ObjectTemplate = StaticDuplicateObject(Instance, &OwnerMovieScene, TemplateName);

		GWorld->DestroyActor(Instance);
	}

	if (!NewSpawnable.ObjectTemplate || !NewSpawnable.ObjectTemplate->IsA<AActor>())
	{
		FText ErrorText = FText::Format(LOCTEXT("UnknownClassError", "Unable to create a new spawnable object from {0}."), FText::FromString(SourceObject.GetName()));
		return MakeError(ErrorText);
	}

	return MakeValue(NewSpawnable);
}

#endif


#undef LOCTEXT_NAMESPACE