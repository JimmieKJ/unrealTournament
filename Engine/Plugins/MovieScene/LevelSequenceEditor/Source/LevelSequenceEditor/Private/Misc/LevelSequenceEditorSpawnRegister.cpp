// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorPCH.h"
#include "ISequencer.h"
#include "ISequencerObjectChangeListener.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "MovieScene.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetSelection.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditorSpawnRegister"

/* FLevelSequenceEditorSpawnRegister structors
 *****************************************************************************/

FLevelSequenceEditorSpawnRegister::FLevelSequenceEditorSpawnRegister()
{
	bShouldClearSelectionCache = true;

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddRaw(this, &FLevelSequenceEditorSpawnRegister::HandleActorSelectionChanged);

	OnActorMovedHandle = GEditor->OnActorMoved().AddLambda([=](AActor* Actor){
		if (SpawnedObjects.Contains(Actor))
		{
			HandleAnyPropertyChanged(*Actor);
		}
	});
}


FLevelSequenceEditorSpawnRegister::~FLevelSequenceEditorSpawnRegister()
{
	GEditor->OnActorMoved().Remove(OnActorMovedHandle);
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditor->OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
	}
}


/* FLevelSequenceSpawnRegister interface
 *****************************************************************************/

UObject* FLevelSequenceEditorSpawnRegister::SpawnObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);
	UObject* NewObject = FLevelSequenceSpawnRegister::SpawnObject(BindingId, SequenceInstance, Player);
	
	if (NewObject)
	{
		SpawnedObjects.Add(NewObject);

		// Add an object listener for the spawned object to propagate changes back onto the spawnable default
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		AActor* Actor = Cast<AActor>(NewObject);

		if (Sequencer.IsValid() && Actor)
		{
			Sequencer->GetObjectChangeListener().GetOnAnyPropertyChanged(*NewObject).AddSP(this, &FLevelSequenceEditorSpawnRegister::HandleAnyPropertyChanged);

			// Select the actor if we think it should be selected
			if (Actor && SelectedSpawnedObjects.Contains(FMovieSceneSpawnRegisterKey(BindingId, SequenceInstance)))
			{
				GEditor->SelectActor(Actor, true /*bSelected*/, true /*bNotify*/);
			}
		}
	}

	return NewObject;
}


void FLevelSequenceEditorSpawnRegister::PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);

	// Cache its selection state
	AActor* Actor = Cast<AActor>(&Object);
	if (Actor && GEditor->GetSelectedActors()->IsSelected(Actor))
	{
		SelectedSpawnedObjects.Add(FMovieSceneSpawnRegisterKey(BindingId, SequenceInstance));
		GEditor->SelectActor(Actor, false /*bSelected*/, true /*bNotify*/);
	}
			
	SpawnedObjects.Remove(&Object);

	// Remove our object listener
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->GetObjectChangeListener().ReportObjectDestroyed(Object);
	}

	FLevelSequenceSpawnRegister::PreDestroyObject(Object, BindingId, SequenceInstance);
}


/* FLevelSequenceEditorSpawnRegister implementation
 *****************************************************************************/

void FLevelSequenceEditorSpawnRegister::PopulateKeyedPropertyMap(AActor& SpawnedObject, TMap<UObject*, TSet<UProperty*>>& OutKeyedPropertyMap)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	Sequencer->GetAllKeyedProperties(SpawnedObject, OutKeyedPropertyMap.FindOrAdd(&SpawnedObject));

	for (UActorComponent* Component : SpawnedObject.GetComponents())
	{
		Sequencer->GetAllKeyedProperties(*Component, OutKeyedPropertyMap.FindOrAdd(Component));
	}
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


void FLevelSequenceEditorSpawnRegister::HandleAnyPropertyChanged(UObject& SpawnedObject)
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


#if WITH_EDITOR

TValueOrError<FNewSpawnable, FText> FLevelSequenceEditorSpawnRegister::CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene)
{
	FNewSpawnable NewSpawnable(nullptr, FName::NameToDisplayString(SourceObject.GetName(), false));

	const FName BlueprintName = MakeUniqueObjectName(&OwnerMovieScene, UBlueprint::StaticClass(), SourceObject.GetFName());

	// First off, deal with creating a spawnable from a class
	if (UClass* InClass = Cast<UClass>(&SourceObject))
	{
		if (!InClass->IsChildOf(AActor::StaticClass()))
		{
			FText ErrorText = FText::Format(LOCTEXT("NotAnActorClass", "Unable to add spawnable for class of type '{0}' since it is not a valid actor class."), FText::FromString(InClass->GetName()));
			return MakeError(ErrorText);
		}

		NewSpawnable.Blueprint = FKismetEditorUtilities::CreateBlueprint( InClass, &OwnerMovieScene, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass() );
	}

	// Deal with creating a spawnable from an instance of an actor
	else if (AActor* Actor = Cast<AActor>(&SourceObject))
	{
		using namespace EditorUtilities;

		UBlueprint* ActorBlueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy);
		if (ActorBlueprint)
		{
			// Create a new blueprint out of this actor's parent blueprint
			NewSpawnable.Blueprint = FKismetEditorUtilities::CreateBlueprint(ActorBlueprint->GeneratedClass, &OwnerMovieScene, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		}
		else
		{
			NewSpawnable.Blueprint = FKismetEditorUtilities::CreateBlueprint(Actor->GetClass(), &OwnerMovieScene, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("CreateFromActor"));
		}

		// Use the actor name
		NewSpawnable.Name = Actor->GetActorLabel();
	}

	// If it's a blueprint, we need some special handling
	else if (UBlueprint* SourceBlueprint = Cast<UBlueprint>(&SourceObject))
	{
		UClass* SourceParentClass = SourceBlueprint->GeneratedClass;

		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(SourceParentClass))
		{
			FText ErrorText = FText::Format(LOCTEXT("UnableToAddSpawnableBlueprint", "Unable to add spawnable for class of type '{0}' since it is not a valid blueprint parent class."), FText::FromString(SourceParentClass->GetName()));
			return MakeError(ErrorText);
		}

		NewSpawnable.Blueprint = FKismetEditorUtilities::CreateBlueprint(SourceParentClass, &OwnerMovieScene, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
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

		NewSpawnable.Blueprint = FactoryToUse->CreateBlueprint( &SourceObject, &OwnerMovieScene, BlueprintName );
	}


	if (!NewSpawnable.Blueprint)
	{
		FText ErrorText = FText::Format(LOCTEXT("UnknownClassError", "Unable to create a new spawnable object from {0}."), FText::FromString(SourceObject.GetName()));
		return MakeError(ErrorText);
	}

	FKismetEditorUtilities::CompileBlueprint(NewSpawnable.Blueprint);
	return MakeValue(NewSpawnable);
}

#endif


#undef LOCTEXT_NAMESPACE