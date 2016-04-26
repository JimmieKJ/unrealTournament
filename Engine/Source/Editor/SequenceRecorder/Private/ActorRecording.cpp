// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "ActorRecording.h"
#include "SequenceRecorderSettings.h"
#include "SequenceRecorderUtils.h"
#include "IMovieScenePropertyRecorder.h"
#include "MovieSceneSpawnPropertyRecorder.h"
#include "MovieScene3DTransformPropertyRecorder.h"
#include "MovieSceneVisibilityPropertyRecorder.h"
#include "MovieSceneAnimationPropertyRecorder.h"
#include "MovieSceneParticleTrackPropertyRecorder.h"
#include "MovieScene3DAttachPropertyRecorder.h"
#include "AssetSelection.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "MovieSceneSpawnTrack.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieScene3DTransformSection.h"
#include "MovieSceneBoolSection.h"
#include "KismetEditorUtilities.h"
#include "BlueprintEditorUtils.h"
#include "MovieSceneFolder.h"
#include "CameraRig_Crane.h"
#include "CameraRig_Rail.h"
#include "Animation/SkeletalMeshActor.h"

static const FName SequencerActorTag(TEXT("SequencerActor"));

UActorRecording::UActorRecording(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWasSpawnedPostRecord = false;
	MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	bEnableUpdateRateOptimizations = false;
	Guid.Invalidate();
	bNewComponentAddedWhileRecording = false;

	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
		AnimationSettings = Settings->DefaultAnimationSettings;
	}
}

bool UActorRecording::IsRelevantForRecording(AActor* Actor)
{
	// don't record actors that sequencer has spawned itself!
	if(Actor->ActorHasTag(SequencerActorTag))
	{
		return false;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(Actor);

	for(USceneComponent* SceneComponent : SceneComponents)
	{
		if(SceneComponent->IsA<USkeletalMeshComponent>() ||
			SceneComponent->IsA<UStaticMeshComponent>() ||
			SceneComponent->IsA<UParticleSystemComponent>() || 
			SceneComponent->IsA<ULightComponent>())
		{
			return true;
		}
	}

	return false;
}

bool UActorRecording::StartRecording(ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	bNewComponentAddedWhileRecording = false;

	if(ActorToRecord.IsValid())
	{
		if(CurrentSequence != nullptr)
		{
			StartRecordingActorProperties(CurrentSequence, CurrentSequenceTime);
		}
		else
		{
			TSharedPtr<FMovieSceneAnimationPropertyRecorder> AnimationRecorder = MakeShareable(new FMovieSceneAnimationPropertyRecorder(AnimationSettings, TargetAnimation.Get()));
			AnimationRecorder->CreateSection(ActorToRecord.Get(), nullptr, FGuid(), 0.0f, true);
			PropertyRecorders.Add(AnimationRecorder);			
		}
	}

	return true;
}

static FString GetUniqueSpawnableName(UMovieScene* MovieScene, const FString& BaseName)
{
	FString BlueprintName = BaseName;
	auto DuplName = [&](FMovieSceneSpawnable& InSpawnable)
	{
		return InSpawnable.GetName() == BlueprintName;
	};

	int32 Index = 2;
	FString UniqueString;
	while (MovieScene->FindSpawnable(DuplName))
	{
		BlueprintName.RemoveFromEnd(UniqueString);
		UniqueString = FString::Printf(TEXT(" (%d)"), Index++);
		BlueprintName += UniqueString;
	}

	return BlueprintName;
}

void UActorRecording::GetSceneComponents(TArray<USceneComponent*>& OutArray, bool bIncludeNonCDO/*=true*/)
{
	// it is not enough to just go through the owned components array here
	// we need to traverse the scene component hierarchy as well, as some components may be 
	// owned by other actors (e.g. for pooling) and some may not be part of the hierarchy
	if(ActorToRecord.IsValid())
	{
		USceneComponent* RootComponent = ActorToRecord->GetRootComponent();
		if(RootComponent)
		{
			// note: GetChildrenComponents clears array!
			RootComponent->GetChildrenComponents(true, OutArray);
			OutArray.Add(RootComponent);
		}

		// add owned components that are *not* part of the hierarchy
		TInlineComponentArray<USceneComponent*> OwnedComponents(ActorToRecord.Get());
		for(USceneComponent* OwnedComponent : OwnedComponents)
		{
			if(OwnedComponent->AttachParent == nullptr && OwnedComponent != RootComponent)
			{
				OutArray.Add(OwnedComponent);
			}
		}

		if(!bIncludeNonCDO)
		{
			AActor* CDO = Cast<AActor>(ActorToRecord->GetClass()->GetDefaultObject());

			auto ShouldRemovePredicate = [&](UActorComponent* PossiblyRemovedComponent)
				{
					// try to find a component with this name in the CDO
					UActorComponent* const* FoundComponent = CDO->GetComponents().FindByPredicate([&](UActorComponent* SearchComponent) 
					{ 
						return SearchComponent->GetClass() == PossiblyRemovedComponent->GetClass() &&
							   SearchComponent->GetFName() == PossiblyRemovedComponent->GetFName(); 
					} );

					// remove if its not found
					return FoundComponent == nullptr;
				};

			OutArray.RemoveAllSwap(ShouldRemovePredicate);
		}
	}
}

void UActorRecording::SyncTrackedComponents(bool bIncludeNonCDO/*=true*/)
{
	TArray<USceneComponent*> NewComponentArray;
	GetSceneComponents(NewComponentArray, bIncludeNonCDO);

	TrackedComponents.Reset(NewComponentArray.Num());
	for(USceneComponent* SceneComponent : NewComponentArray)
	{
		TrackedComponents.Add(SceneComponent);
	}
}

void UActorRecording::InvalidateObjectToRecord()
{
	ActorToRecord = nullptr;
	for(auto& PropertyRecorder : PropertyRecorders)
	{
		PropertyRecorder->InvalidateObjectToRecord();
	}
}

bool UActorRecording::ValidComponent(USceneComponent* SceneComponent) const
{
	if(SceneComponent != nullptr)
	{
		const bool bIsValidComponent = 
			SceneComponent->IsA<USkeletalMeshComponent>() ||
			SceneComponent->IsA<UStaticMeshComponent>() ||
			SceneComponent->IsA<UParticleSystemComponent>();

		return bIsValidComponent;
	}

	return false;
}

void UActorRecording::FindOrAddFolder(UMovieScene* MovieScene)
{
	check(ActorToRecord.IsValid());

	FName FolderName(NAME_None);
	if(ActorToRecord.Get()->IsA<ACharacter>() || ActorToRecord.Get()->IsA<ASkeletalMeshActor>())
	{
		FolderName = TEXT("Characters");
	}
	else if(ActorToRecord.Get()->IsA<ACameraActor>() || ActorToRecord.Get()->IsA<ACameraRig_Crane>() || ActorToRecord.Get()->IsA<ACameraRig_Rail>())
	{
		FolderName = TEXT("Cameras");
	}
	else
	{
		FolderName = TEXT("Misc");
	}

	// look for a folder to put us in
	UMovieSceneFolder* FolderToUse = nullptr;
	for(UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
	{
		if(Folder->GetFolderName() == FolderName)
		{
			FolderToUse = Folder;
			break;
		}
	}

	if(FolderToUse == nullptr)
	{
		FolderToUse = NewObject<UMovieSceneFolder>(MovieScene, NAME_None, RF_Transactional);
		FolderToUse->SetFolderName(FolderName);
		MovieScene->GetRootFolders().Add(FolderToUse);
	}

	FolderToUse->AddChildObjectBinding(Guid);
}

void UActorRecording::StartRecordingActorProperties(ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	if(CurrentSequence != nullptr)
	{
		// set up our spawnable for this actor
		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();

		AActor* Actor = ActorToRecord.Get();
		FString BlueprintName = GetUniqueSpawnableName(MovieScene, Actor->GetName());

		UClass* ActorClass = Actor->GetClass();
			
		UBlueprint* Blueprint = nullptr;

		if(ActorClass->IsChildOf<AWorldSettings>())
		{
			// we need to create our own substitute blueprint for world settings actors as they are not blueprintable by default
			Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), MovieScene, *BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_Name);
		}
		else
		{
			UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(ActorClass, MovieScene, *BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_Name);
			if(NewBlueprint)
			{
				// if the duplicated BP has any skel mesh components, set them to single anim instance
				AActor* CDO = CastChecked<AActor>(NewBlueprint->GeneratedClass->ClassDefaultObject);
				if(CDO)
				{
					TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshComponents;
					CDO->GetComponents(SkeletalMeshComponents);	
					for(USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
					{
						SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
						SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
						SkeletalMeshComponent->ForcedLodModel = 1;
					}
				}

				Blueprint = NewBlueprint;
			}
		}

		if(Blueprint)
		{
			Guid = MovieScene->AddSpawnable(BlueprintName, Blueprint);
		}

		// now add tracks to record
		if(Guid.IsValid())
		{
			// add our folder
			FindOrAddFolder(MovieScene);

			// force set recording to record translations as we need this with no animation
			ActorSettings.bRecordTransforms = true;

			// grab components so we can track attachments
			// don't include non-CDO here as they wont be part of our initial BP (duplicated above)
			// we will catch these 'extra' components on the first tick
			const bool bIncludeNonCDO = false;
			SyncTrackedComponents(bIncludeNonCDO);

			TInlineComponentArray<USceneComponent*> SceneComponents(ActorToRecord.Get());

			// check if components need recording
			TInlineComponentArray<USceneComponent*> ValidSceneComponents;
			for(TWeakObjectPtr<USceneComponent>& SceneComponent : TrackedComponents)
			{
				if(ValidComponent(SceneComponent.Get()))
				{
					ValidSceneComponents.Add(SceneComponent.Get());

					// add all parent components too
					TArray<USceneComponent*> ParentComponents;
					SceneComponent->GetParentComponents(ParentComponents);
					for(USceneComponent* ParentComponent : ParentComponents)
					{	
						ValidSceneComponents.AddUnique(ParentComponent);
					}
				}
			}

			TSharedPtr<FMovieSceneAnimationPropertyRecorder> FirstAnimRecorder = nullptr;
			for(USceneComponent* SceneComponent : ValidSceneComponents)
			{
				TSharedPtr<FMovieSceneAnimationPropertyRecorder> AnimRecorder = StartRecordingComponentProperties(SceneComponent->GetFName(), SceneComponent, ActorToRecord.Get(), CurrentSequence, CurrentSequenceTime);
				if(!FirstAnimRecorder.IsValid() && AnimRecorder.IsValid())
				{
					FirstAnimRecorder = AnimRecorder;
				}
			}

			// add a spawn track so we actually get spawned
			TSharedPtr<FMovieSceneSpawnPropertyRecorder> SpawnRecorder = MakeShareable(new FMovieSceneSpawnPropertyRecorder);
			SpawnRecorder->CreateSection(ActorToRecord.Get(), MovieScene, Guid, CurrentSequenceTime, true);
			PropertyRecorders.Add(SpawnRecorder);

			// we need to create a transform track even if we arent recording transforms
			TSharedPtr<FMovieScene3DTransformPropertyRecorder> TransformRecorder = MakeShareable(new FMovieScene3DTransformPropertyRecorder(FirstAnimRecorder));
			TransformRecorder->CreateSection(ActorToRecord.Get(), MovieScene, Guid, CurrentSequenceTime, ActorSettings.bRecordTransforms);
			PropertyRecorders.Add(TransformRecorder);

			// add a visibility track 
			TSharedPtr<FMovieSceneVisibilityPropertyRecorder> VisibilityRecorder = MakeShareable(new FMovieSceneVisibilityPropertyRecorder);
			VisibilityRecorder->CreateSection(ActorToRecord.Get(), MovieScene, Guid, CurrentSequenceTime, ActorSettings.bRecordVisibility);
			PropertyRecorders.Add(VisibilityRecorder);

			// add an attachment track
			TSharedPtr<FMovieScene3DAttachPropertyRecorder> AttachRecorder = MakeShareable(new FMovieScene3DAttachPropertyRecorder);
			AttachRecorder->CreateSection(ActorToRecord.Get(), MovieScene, Guid, CurrentSequenceTime, true);
			PropertyRecorders.Add(AttachRecorder);			
		}
	}
}

static FString GetUniquePosessableName(UMovieScene* MovieScene, const FString& BaseName)
{
	FString BlueprintName = BaseName;
	auto DuplName = [&](FMovieScenePossessable& InPosessable)
	{
		return InPosessable.GetName() == BlueprintName;
	};

	int32 Index = 2;
	FString UniqueString;
	while (MovieScene->FindPossessable(DuplName))
	{
		BlueprintName.RemoveFromEnd(UniqueString);
		UniqueString = FString::Printf(TEXT(" (%d)"), Index++);
		BlueprintName += UniqueString;
	}

	return BlueprintName;
}

TSharedPtr<FMovieSceneAnimationPropertyRecorder> UActorRecording::StartRecordingComponentProperties(const FName& BindingName, USceneComponent* SceneComponent, UObject* BindingContext, ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	// first create a possessable for this component to be controlled by
	UMovieScene* OwnerMovieScene = CurrentSequence->GetMovieScene();

	FString Name = GetUniquePosessableName(OwnerMovieScene, BindingName.ToString());

	const FGuid PossessableGuid = OwnerMovieScene->AddPossessable(Name, SceneComponent->GetClass());

	// Set up parent/child guids for possessables within spawnables
	FMovieScenePossessable* ChildPossessable = OwnerMovieScene->FindPossessable(PossessableGuid);
	if (ensure(ChildPossessable))
	{
		ChildPossessable->SetParent(Guid);
	}

	FMovieSceneSpawnable* ParentSpawnable = OwnerMovieScene->FindSpawnable(Guid);
	if (ParentSpawnable)
	{
		ParentSpawnable->AddChildPossessable(PossessableGuid);
	}

	FLevelSequenceObjectReference ObjectReference(FUniqueObjectGuid(), BindingName.ToString());

	CurrentSequence->BindPossessableObject(PossessableGuid, ObjectReference);

	TSharedPtr<FMovieSceneAnimationPropertyRecorder> AnimationRecorder;
	if(SceneComponent->IsA<USkeletalMeshComponent>())
	{
		AnimationRecorder = MakeShareable(new FMovieSceneAnimationPropertyRecorder(AnimationSettings, TargetAnimation.Get()));
		AnimationRecorder->CreateSection(SceneComponent, OwnerMovieScene, PossessableGuid, CurrentSequenceTime, true);
		PropertyRecorders.Add(AnimationRecorder);
	}
	else if(SceneComponent->IsA<UParticleSystemComponent>())
	{
		TSharedPtr<FMovieSceneParticleTrackPropertyRecorder> ParticleTrackRecorder = MakeShareable(new FMovieSceneParticleTrackPropertyRecorder());
		ParticleTrackRecorder->CreateSection(SceneComponent, OwnerMovieScene, PossessableGuid, CurrentSequenceTime, true);
		PropertyRecorders.Add(ParticleTrackRecorder);
	}

	// Dont record the root component transforms as this will be taken into account by the actor transform track
	// Also dont record transforms of skeletal mesh components as they will be taken into account in the actor transform
	bool bIsCharacterSkelMesh = false;
	if(SceneComponent->IsA<USkeletalMeshComponent>() && SceneComponent->GetOwner()->IsA<ACharacter>())
	{
		ACharacter* Character = CastChecked<ACharacter>(SceneComponent->GetOwner());
		bIsCharacterSkelMesh = SceneComponent == Character->GetMesh();
	}

	if(SceneComponent != SceneComponent->GetOwner()->GetRootComponent() && !bIsCharacterSkelMesh)
	{
		TSharedPtr<FMovieScene3DTransformPropertyRecorder> TransformRecorder = MakeShareable(new FMovieScene3DTransformPropertyRecorder);
		TransformRecorder->CreateSection(SceneComponent, OwnerMovieScene, PossessableGuid, CurrentSequenceTime, true);
		PropertyRecorders.Add(TransformRecorder);
	}

	TSharedPtr<FMovieSceneVisibilityPropertyRecorder> VisibilityRecorder = MakeShareable(new FMovieSceneVisibilityPropertyRecorder);
	VisibilityRecorder->CreateSection(SceneComponent, OwnerMovieScene, PossessableGuid, CurrentSequenceTime, true);
	PropertyRecorders.Add(VisibilityRecorder);	

	return AnimationRecorder;
}

void UActorRecording::Tick(float DeltaSeconds, ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	if (IsRecording())
	{
		if(CurrentSequence)
		{
			// check our components to see if they have changed
			static TArray<USceneComponent*> SceneComponents;
			GetSceneComponents(SceneComponents);

			if(TrackedComponents.Num() != SceneComponents.Num())
			{
				StartRecordingNewComponents(CurrentSequence, CurrentSequenceTime);
			}
		}

		for(auto& PropertyRecorder : PropertyRecorders)
		{
			PropertyRecorder->Record(CurrentSequenceTime);
		}
	}
}

bool UActorRecording::StopRecording(ULevelSequence* CurrentSequence)
{
	FString ActorName;
	if(CurrentSequence)
	{
		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
		check(MovieScene);

		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Guid);
		if(Spawnable)
		{
			ActorName = Spawnable->GetName();
		}
	}

	FScopedSlowTask SlowTask((float)PropertyRecorders.Num() + 1.0f, FText::Format(NSLOCTEXT("SequenceRecorder", "ProcessingActor", "Processing Actor {0}"), FText::FromString(ActorName)));

	// stop property recorders
	for(auto& PropertyRecorder : PropertyRecorders)
	{
		SlowTask.EnterProgressFrame();

		PropertyRecorder->FinalizeSection();
	}

	SlowTask.EnterProgressFrame();

	PropertyRecorders.Empty();

	if(bNewComponentAddedWhileRecording && CurrentSequence)
	{
		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
		check(MovieScene);

		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Guid);
		if(Spawnable)
		{
			UBlueprint* Blueprint = CastChecked<UBlueprint>(Spawnable->GetClass()->ClassGeneratedBy);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}

	return true;
}

bool UActorRecording::IsRecording() const
{
	return ActorToRecord.IsValid() && PropertyRecorders.Num() > 0;
}

static FName FindParentComponentOwnerClassName(USceneComponent* SceneComponent, UBlueprint* Blueprint)
{
	if(SceneComponent->AttachParent)
	{
		FName AttachName = SceneComponent->AttachParent->GetFName();

		// see if we can find this component in the BP inheritance hierarchy
		while(Blueprint)
		{
			if(Blueprint->SimpleConstructionScript->FindSCSNode(AttachName) != nullptr)
			{
				return Blueprint->GetFName();
			}

			Blueprint = Cast<UBlueprint>(Blueprint->GeneratedClass->GetSuperClass()->ClassGeneratedBy);
		}
	}

	return NAME_None;
}

void UActorRecording::StartRecordingNewComponents(ULevelSequence* CurrentSequence, float CurrentSequenceTime)
{
	if (ActorToRecord.IsValid())
	{
		// find the new component(s)
		TArray<USceneComponent*> NewComponents;
		TArray<USceneComponent*> SceneComponents;
		GetSceneComponents(SceneComponents);
		for(USceneComponent* SceneComponent : SceneComponents)
		{
			if(ValidComponent(SceneComponent))
			{
				TWeakObjectPtr<USceneComponent> WeakSceneComponent(SceneComponent);
				int32 FoundIndex = TrackedComponents.Find(WeakSceneComponent);
				if(FoundIndex == INDEX_NONE)
				{
					// new component!
					NewComponents.Add(SceneComponent);
				}
			}
		}

		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
		check(MovieScene);

		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Guid);
		check(Spawnable);

		UBlueprint* Blueprint = CastChecked<UBlueprint>(Spawnable->GetClass()->ClassGeneratedBy);

		for(USceneComponent* SceneComponent : NewComponents)
		{
			// new component, so we need to add this to our BP if it didn't come from SCS
			FName NewName;
			if(SceneComponent->CreationMethod != EComponentCreationMethod::SimpleConstructionScript)
			{
				NewName = *FString::Printf(TEXT("Dynamic%s"), *SceneComponent->GetFName().ToString());
				USCS_Node* NewSCSNode = Blueprint->SimpleConstructionScript->CreateNode(SceneComponent->GetClass(), NewName); 
				UEditorEngine::CopyPropertiesForUnrelatedObjects(SceneComponent, NewSCSNode->ComponentTemplate);
				NewSCSNode->ComponentTemplate->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;

				// look for a similar attach parent in the current structure
				bool bAdded = false;
				if(SceneComponent->AttachParent != nullptr)
				{
					FName AttachName = SceneComponent->AttachParent->GetFName();
					NewSCSNode->ParentComponentOrVariableName = AttachName;
					NewSCSNode->bIsParentComponentNative = SceneComponent->AttachParent->CreationMethod == EComponentCreationMethod::Native;
					NewSCSNode->ParentComponentOwnerClassName = FindParentComponentOwnerClassName(SceneComponent, Blueprint);

					USCS_Node* ParentSCSNode = Blueprint->SimpleConstructionScript->FindSCSNode(AttachName);
					if(ParentSCSNode)
					{
						ParentSCSNode->AddChildNode(NewSCSNode);
						bAdded = true;
					}
				}
			
				// nothing found - add to root
				if(!bAdded)
				{
					Blueprint->SimpleConstructionScript->AddNode(NewSCSNode);
				}

				if(SceneComponent->AttachSocketName != NAME_None)
				{
					NewSCSNode->AttachToName = SceneComponent->AttachSocketName;
				}
			}
			else
			{
				NewName = SceneComponent->GetFName();
			}

			StartRecordingComponentProperties(NewName, SceneComponent, ActorToRecord.Get(), CurrentSequence, CurrentSequenceTime);

			bNewComponentAddedWhileRecording = true;
		}
		
		SyncTrackedComponents();
	}
}
