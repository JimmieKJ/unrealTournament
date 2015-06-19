// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerActorBindingManager.h"
#include "Sequencer.h"
#include "MovieScene.h"
#include "Toolkits/IToolkitHost.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_PlayMovieScene.h"
#include "MovieSceneInstance.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Editor/Kismet/Public/BlueprintEditorModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "Sequencer"

FSequencerActorBindingManager::FSequencerActorBindingManager( UWorld* InWorld, TSharedRef<ISequencerObjectChangeListener> InObjectChangeListener, TSharedRef<FSequencer> InSequencer )
	: ActorWorld( InWorld )
	, PlayMovieSceneNode( nullptr )
	, Sequencer( InSequencer )
	, ObjectChangeListener( InObjectChangeListener )
{
	// Register to be notified when object changes should be propagated
	InObjectChangeListener->GetOnPropagateObjectChanges().AddRaw( this, &FSequencerActorBindingManager::OnPropagateObjectChanges );
}


FSequencerActorBindingManager::~FSequencerActorBindingManager()
{
	if( ObjectChangeListener.IsValid() )
	{
		ObjectChangeListener.Pin()->GetOnPropagateObjectChanges().RemoveAll( this );
	}
	
	if( PlayMovieSceneNode.IsValid() )
	{
		PlayMovieSceneNode->OnBindingsChanged().RemoveAll( this );
		PlayMovieSceneNode.Reset();
	}
}

FGuid FSequencerActorBindingManager::FindGuidForObject( const UMovieScene& MovieScene, UObject& Object ) const
{
	FGuid ObjectGuid = FindSpawnableGuidForPuppetObject( &Object );
	if( !ObjectGuid.IsValid() )
	{
		// Spawnable
		// Is this a game preview object?
		const bool bIsGamePreviewObject = !!( Object.GetOutermost()->PackageFlags & PKG_PlayInEditor );
		if( bIsGamePreviewObject )
		{
			// OK, so someone is asking for a handle to an object from a game preview session, probably because
			// they want to capture keys during live simulation.

			// Check to see if we already have a puppet that was generated from a recording of this game preview object
			// @todo sequencer livecapture: We could support recalling counterpart by full name instead of weak pointer, to allow "overdubbing" of previously recorded actors, when the new actors in the current play session have the same path name
			// @todo sequencer livecapture: Ideally we could capture from editor-world actors that are "puppeteered" as well (real time)
			const FMovieSceneSpawnable* FoundSpawnable = MovieScene.FindSpawnableForCounterpart( &Object );
			if( FoundSpawnable != NULL )
			{
				ObjectGuid = FoundSpawnable->GetGuid();
			}
		}
		else
		{
			BindToPlayMovieSceneNode( false );

			// Possessable
			// When editing within the level editor, make sure we're bound to the level script node which contains data about possessables.
			if( PlayMovieSceneNode.IsValid() )
			{
				ObjectGuid = PlayMovieSceneNode->FindGuidForObject( &Object );
			}
		}

	}
	
	return ObjectGuid;
}

void FSequencerActorBindingManager::SpawnOrDestroyObjectsForInstance( TSharedRef<FMovieSceneInstance> MovieSceneInstance, const bool bDestroyAll )
{
	bool bAnyLevelActorsChanged = false;

	// Get the list of puppet objects for the movie scene
	TArray< TSharedRef<FPuppetActorInfo> >& PuppetObjects = InstanceToPuppetObjectsMap.FindOrAdd( MovieSceneInstance );

	UMovieScene* MovieScene = MovieSceneInstance->GetMovieScene();

	// Remove any puppet objects that we no longer need
	{
		for( auto PuppetObjectIndex = 0; PuppetObjectIndex < PuppetObjects.Num(); ++PuppetObjectIndex )
		{
			if( PuppetObjects[ PuppetObjectIndex ]->GetType() == EPuppetObjectType::Actor )
			{
				TSharedRef< FPuppetActorInfo > PuppetActorInfo = StaticCastSharedRef< FPuppetActorInfo >( PuppetObjects[ PuppetObjectIndex ] );

				// Figure out if we still need this puppet actor
				bool bShouldDestroyActor = true;
				if( !bDestroyAll )
				{
					for( auto SpawnableIndex = 0; SpawnableIndex < MovieScene->GetSpawnableCount(); ++SpawnableIndex )
					{
						auto& Spawnable = MovieScene->GetSpawnable( SpawnableIndex );
						if( Spawnable.GetGuid() == PuppetActorInfo->SpawnableGuid )
						{
							bShouldDestroyActor = false;
							break;
						}
					}
				}

				if( bShouldDestroyActor )
				{
					AActor* PuppetActor = PuppetActorInfo->PuppetActor.Get();
					if( PuppetActor != NULL )
					{
						UWorld* PuppetWorld = PuppetActor->GetWorld();
						if( ensure( PuppetWorld != NULL ) )
						{
							// Destroy this actor
							{
								// Ignored unless called while game is running
								const bool bNetForce = false;

								// We don't want to dirty the level for puppet actor changes
								const bool bShouldModifyLevel = false;

								if( !bAnyLevelActorsChanged )
								{
									DeselectAllPuppetObjects();
								}

								// Actor should never be selected in the editor at this point.  We took care of that up above.
								ensure( !PuppetActor->IsSelected() );

								const bool bWasDestroyed = PuppetWorld->DestroyActor( PuppetActor, bNetForce, bShouldModifyLevel );

								if( bWasDestroyed )
								{
									bAnyLevelActorsChanged = true;
									PuppetObjects.RemoveAt( PuppetObjectIndex-- );
								}
								else
								{
									// @todo sequencer: At least one puppet couldn't be cleaned up!
								}
							}
						}
					}
					else
					{
						// Actor is no longer valid (probably the world was destroyed)
					}
				}
			}
			else
			{
				check(0);	// Unhandled type
			}
		}
	}

	if( !bDestroyAll )
	{
		for( auto SpawnableIndex = 0; SpawnableIndex < MovieScene->GetSpawnableCount(); ++SpawnableIndex )
		{
			FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable( SpawnableIndex );

			// Must have a valid world for us to be able to do this
			if( ActorWorld != NULL )
			{
				// Do we already have a puppet for this spawnable?
				bool bIsAlreadySpawned = false;
				for( auto PuppetIndex = 0; PuppetIndex < PuppetObjects.Num(); ++PuppetIndex )
				{
					auto& PuppetObject = PuppetObjects[ PuppetIndex ];
					if( PuppetObject->SpawnableGuid == Spawnable.GetGuid() )
					{
						bIsAlreadySpawned = true;
						break;
					}
				}

				if( !bIsAlreadySpawned )
				{
					UClass* GeneratedClass = Spawnable.GetClass();
					if ( GeneratedClass != NULL && GeneratedClass->IsChildOf(AActor::StaticClass()))
					{
						AActor* ActorCDO = CastChecked< AActor >( GeneratedClass->ClassDefaultObject );

						const FVector SpawnLocation = ActorCDO->GetRootComponent()->RelativeLocation;
						const FRotator SpawnRotation = ActorCDO->GetRootComponent()->RelativeRotation;

						// @todo sequencer: We should probably spawn these in a specific sub-level!
						// World->CurrentLevel = ???;

						const FName PuppetActorName = NAME_None;

						// Override the object flags so that RF_Transactional is not set.  Puppet actors are never transactional
						// @todo sequencer: These actors need to avoid any transaction history.  However, RF_Transactional can currently be set on objects on the fly!
						const EObjectFlags ObjectFlags = RF_Transient;		// NOTE: We are omitting RF_Transactional intentionally

						// @todo sequencer livecapture: Consider using SetPlayInEditorWorld() and RestoreEditorWorld() here instead
						
						// @todo sequencer actors: We need to make sure puppet objects aren't copied into PIE/SIE sessions!  They should be omitted from that duplication!

						// Spawn the puppet actor
						FActorSpawnParameters SpawnInfo;
						SpawnInfo.Name = PuppetActorName;
						SpawnInfo.ObjectFlags = ObjectFlags;
						AActor* NewActor = ActorWorld->SpawnActor( GeneratedClass, &SpawnLocation, &SpawnRotation, SpawnInfo );
				
						if( NewActor )
						{
							// @todo sequencer: We're naming the actor based off of the spawnable's name.  Is that really what we want?
							FActorLabelUtilities::SetActorLabelUnique(NewActor, Spawnable.GetName());

							// Actor was spawned OK!

							// Keep track of this actor
							TSharedRef< FPuppetActorInfo > NewPuppetInfo( new FPuppetActorInfo() );
							NewPuppetInfo->SpawnableGuid = Spawnable.GetGuid();
							NewPuppetInfo->PuppetActor = NewActor;
							PuppetObjects.Add( NewPuppetInfo );
						}
						else
						{
							// Actor failed to spawn
							// @todo sequencer: What should we do when this happens to one or more actors?
						}
					}
				}
			}
		}
	}
}

void FSequencerActorBindingManager::DestroyAllSpawnedObjects()
{
	const bool bDestroyAll = true;
	for( auto MovieSceneIter = InstanceToPuppetObjectsMap.CreateConstIterator(); MovieSceneIter; ++MovieSceneIter )
	{
		SpawnOrDestroyObjectsForInstance( MovieSceneIter.Key().Pin().ToSharedRef(), bDestroyAll );
	}

	InstanceToPuppetObjectsMap.Empty();
}

bool FSequencerActorBindingManager::CanPossessObject( UObject& Object ) const
{
	return Object.IsA<AActor>();
}

void FSequencerActorBindingManager::BindPossessableObject( const FGuid& PossessableGuid, UObject& PossessedObject )
{
	 // When editing within the level editor, make sure we're bound to the level script node which contains data about possessables.
	 const bool bCreateIfNotFound = true;
	 BindToPlayMovieSceneNode( bCreateIfNotFound );
	
	 //const FString& PossessableName = Object->GetName();
	 //const FGuid PossessableGuid = FocusedMovieScene->AddPossessable( PossessableName, Object->GetClass() );
	 
	 //if (IsShotFilteringOn())
	 //{
	 //	 AddUnfilterableObject(PossessableGuid);
	 // }
	 
	 // Bind the object to the handle
	 TArray< UObject* > Objects;
	 Objects.Add( &PossessedObject );
	 PlayMovieSceneNode->BindPossessableToObjects( PossessableGuid, Objects );
	 
	 // A possessable was created so we need to respawn its puppet
	 // SpawnOrDestroyPuppetObjects( FocusedMovieSceneInstance );
}

void FSequencerActorBindingManager::UnbindPossessableObjects( const FGuid& PossessableGuid )
{
	// @todo Sequencer: Should we remove the pin and unlink?
}

void FSequencerActorBindingManager::GetRuntimeObjects( const TSharedRef<FMovieSceneInstance>& MovieSceneInstance, const FGuid& ObjectGuid, TArray<UObject*>& OutRuntimeObjects ) const
{
	UObject* FoundSpawnedObject = FindPuppetObjectForSpawnableGuid( MovieSceneInstance, ObjectGuid );
	if( FoundSpawnedObject )
	{
		// Spawnable
		OutRuntimeObjects.Reset();
		OutRuntimeObjects.Add( FoundSpawnedObject );
	}
	else if( PlayMovieSceneNode.IsValid() )
	{
		// Possessable
		OutRuntimeObjects = PlayMovieSceneNode->FindBoundObjects( ObjectGuid );
	}
}

UK2Node_PlayMovieScene* FSequencerActorBindingManager::FindPlayMovieSceneNodeInLevelScript( const UMovieScene* MovieScene ) const
{
	// Grab the world object for this editor
	check( MovieScene != NULL );
	
	// Search all levels in the specified world
	for( TArray<ULevel*>::TConstIterator LevelIter( ActorWorld->GetLevels().CreateConstIterator() ); LevelIter; ++LevelIter )
	{
		auto* Level = *LevelIter;
		if( Level != NULL )
		{
			// We don't want to create a level script if one doesn't exist yet.  We just want to grab the one
			// that we already have, if one exists.
			const bool bDontCreate = true;
			ULevelScriptBlueprint* LSB = Level->GetLevelScriptBlueprint( bDontCreate );
			if( LSB != NULL )
			{
				TArray<UK2Node_PlayMovieScene*> EventNodes;
				FBlueprintEditorUtils::GetAllNodesOfClass( LSB, EventNodes );
				for( auto FoundNodeIter( EventNodes.CreateIterator() ); FoundNodeIter; ++FoundNodeIter )
				{
					UK2Node_PlayMovieScene* FoundNode = *FoundNodeIter;
					if( FoundNode->GetMovieScene() == MovieScene )
					{
						return FoundNode;
					}
				}
			}
		}
	}
	
	return NULL;
}


UK2Node_PlayMovieScene* FSequencerActorBindingManager::CreateNewPlayMovieSceneNode( UMovieScene* MovieScene ) const
{
	// Grab the world object for this editor
	check( MovieScene != NULL );
	
	ULevel* Level = ActorWorld->GetCurrentLevel();
	check( Level != NULL );
	
	// Here, we'll create a level script if one does not yet exist.
	const bool bDontCreate = false;
	ULevelScriptBlueprint* LSB = Level->GetLevelScriptBlueprint( bDontCreate );
	if( LSB != NULL )
	{
		UEdGraph* TargetGraph = NULL;
		if( LSB->UbergraphPages.Num() > 0 )
		{
			TargetGraph = LSB->UbergraphPages[0]; // Just use the first graph
		}
		
		if( ensure( TargetGraph != NULL ) )
		{
			// Figure out a decent place to stick the node
			const FVector2D NewNodePos = TargetGraph->GetGoodPlaceForNewNode();
			
			// Create a new node
			UK2Node_PlayMovieScene* TemplateNode = NewObject<UK2Node_PlayMovieScene>();
			TemplateNode->SetMovieScene( MovieScene );
			return FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_PlayMovieScene>(TargetGraph, TemplateNode, NewNodePos);;
		}
	}
	
	return NULL;
}

UK2Node_PlayMovieScene* FSequencerActorBindingManager::BindToPlayMovieSceneNode( const bool bCreateIfNotFound ) const
{
	auto* MovieScene = Sequencer.Pin()->GetFocusedMovieScene();
	
	// Update level script
	UK2Node_PlayMovieScene* FoundPlayMovieSceneNode = FindPlayMovieSceneNodeInLevelScript( MovieScene );
	if( FoundPlayMovieSceneNode == NULL )
	{
		if( bCreateIfNotFound )
		{
			// Couldn't find an existing node that uses this MovieScene, so we'll create one now.
			FoundPlayMovieSceneNode = CreateNewPlayMovieSceneNode( MovieScene );
			if( FoundPlayMovieSceneNode != NULL )
			{
				// Let the user know that we plopped down a new PlayMovieScene event in their level script graph
				{
					FNotificationInfo Info( LOCTEXT("AddedPlayMovieSceneEventToLevelScriptGraph", "A new event for this MovieScene was added to this level's script") );
					Info.bFireAndForget = true;
					Info.bUseThrobber = false;
					Info.bUseSuccessFailIcons = false;
					Info.bUseLargeFont = false;
					Info.ExpireDuration = 5.0f;		// Stay visible for awhile so the user has time to click "Show Graph" if they want to
					
					// @todo sequencer: Needs better artwork (see DefaultStyle.cpp)
					Info.Image = FEditorStyle::GetBrush( TEXT( "Sequencer.NotificationImage_AddedPlayMovieSceneEvent" ) );
					
					struct Local
					{
						/**
						 * Called by our notification's hyperlink to open the Level Script editor and navigate to our newly-created PlayMovieScene node
						 *
						 * @param	Sequencer			The Sequencer we're using
						 * @param	PlayMovieSceneNode	The node that we need to jump to
						 */
						static void NavigateToPlayMovieSceneNode( UK2Node_PlayMovieScene* PlayMovieSceneNode )
						{
							check( PlayMovieSceneNode != NULL );
							
							IBlueprintEditor* BlueprintEditor = NULL;
							
							auto* LSB = CastChecked< ULevelScriptBlueprint >( PlayMovieSceneNode->GetBlueprint() );
							
							// @todo sequencer: Support using world-centric editing here?  (just need to set EToolkitMode::WorldCentric)
							FAssetEditorManager::Get().OpenEditorForAsset( LSB );
							
							if( BlueprintEditor != NULL )
							{
								const bool bRequestRename = false;
								BlueprintEditor->JumpToHyperlink( PlayMovieSceneNode, false );
							}
							else
							{
								UE_LOG( LogSequencer, Warning, TEXT( "Unable to open Blueprint Editor to edit newly-created PlayMovieScene event" ) );
							}
						}
					};
					
					Info.Hyperlink = FSimpleDelegate::CreateStatic( &Local::NavigateToPlayMovieSceneNode, FoundPlayMovieSceneNode );
					Info.HyperlinkText = LOCTEXT("AddedPlayMovieSceneEventToLevelScriptGraph_Hyperlink", "Show Graph");
					
					TSharedPtr< SNotificationItem > NewNotification = FSlateNotificationManager::Get().AddNotification(Info);
				}
			}
		}
	}
	
	if( PlayMovieSceneNode.Get() != FoundPlayMovieSceneNode )
	{
		if( PlayMovieSceneNode.IsValid() )
		{
			// Unhook from old node
			PlayMovieSceneNode.Get()->OnBindingsChanged().RemoveAll( this );
		}
		
		PlayMovieSceneNode = FoundPlayMovieSceneNode;
		
		if( PlayMovieSceneNode.IsValid() )
		{
			// Bind to the new node
			PlayMovieSceneNode.Get()->OnBindingsChanged().AddSP( this, &FSequencerActorBindingManager::OnPlayMovieSceneBindingsChanged );
		}
	}
	
	return PlayMovieSceneNode.Get();
}

void FSequencerActorBindingManager::OnPlayMovieSceneBindingsChanged()
{
	const bool bDestroyAll = false;
	SpawnOrDestroyObjectsForInstance( Sequencer.Pin()->GetFocusedMovieSceneInstance(), bDestroyAll );
}

void FSequencerActorBindingManager::DeselectAllPuppetObjects()
{
	TArray< AActor* > ActorsToDeselect;

	for( auto MovieSceneIter( InstanceToPuppetObjectsMap.CreateConstIterator() ); MovieSceneIter; ++MovieSceneIter ) 
	{
		const TArray< TSharedRef<FPuppetActorInfo> >& PuppetObjects = MovieSceneIter.Value();

		for( auto PuppetObjectIter( PuppetObjects.CreateConstIterator() ); PuppetObjectIter; ++PuppetObjectIter )
		{
			if( PuppetObjectIter->Get().GetType() == EPuppetObjectType::Actor )
			{
				TSharedRef< FPuppetActorInfo > PuppetActorInfo = StaticCastSharedRef< FPuppetActorInfo >( *PuppetObjectIter );
				AActor* PuppetActor = PuppetActorInfo->PuppetActor.Get();
				if( PuppetActor != NULL )
				{
					ActorsToDeselect.Add( PuppetActor );
				}
			}
		}
	}

	if( ActorsToDeselect.Num() > 0 )
	{
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();
		GEditor->GetSelectedActors()->MarkBatchDirty();
		for( auto ActorIt( ActorsToDeselect.CreateIterator() ); ActorIt; ++ActorIt )
		{
			GEditor->GetSelectedActors()->Deselect( *ActorIt );
		}
		GEditor->GetSelectedActors()->EndBatchSelectOperation();
	}
}


void FSequencerActorBindingManager::OnPropagateObjectChanges( UObject* Object )
{
	// @todo sequencer major: Many other types of changes to the puppet actor may occur which should be propagated (or disallowed?):
	//		- Deleting the puppet actor (Del key) should probably delete the reference actor and remove it from the MovieScene
	//		- Attachment changes
	//		- Replace/Convert operations?

	// We only care about actor objects for this type of spawner.  Note that sometimes we can get component objects passed in, but we'll
	// rely on the caller also calling OnObjectPropertyChange for the outer Actor if a component property changes.
	if( Object->IsA<AActor>() )
	{
		for( auto MovieSceneIter( InstanceToPuppetObjectsMap.CreateConstIterator() ); MovieSceneIter; ++MovieSceneIter ) 
		{
			const TArray< TSharedRef<FPuppetActorInfo> >& PuppetObjects = MovieSceneIter.Value();

			// Is this an object that we care about?
			for( auto PuppetObjectIter( PuppetObjects.CreateConstIterator() ); PuppetObjectIter; ++PuppetObjectIter )
			{
				if( PuppetObjectIter->Get().GetType() == EPuppetObjectType::Actor )
				{
					TSharedRef< FPuppetActorInfo > PuppetActorInfo = StaticCastSharedRef< FPuppetActorInfo >( *PuppetObjectIter );
					AActor* PuppetActor = PuppetActorInfo->PuppetActor.Get();
					if( PuppetActor != NULL )
					{
						if( PuppetActor == Object )
						{
							// @todo sequencer propagation: Don't propagate changes that are being auto-keyed or key-adjusted!

							// Our puppet actor was modified.
							PropagatePuppetActorChanges( PuppetActorInfo );
						}
					}
				}
			}
		}
	}
}



void FSequencerActorBindingManager::PropagatePuppetActorChanges( const TSharedRef< FPuppetActorInfo > PuppetActorInfo )
{
	AActor* PuppetActor = PuppetActorInfo->PuppetActor.Get();
	AActor* TargetActor = NULL;

	{
		// Find the spawnable for this puppet actor
		FMovieSceneSpawnable* FoundSpawnable = NULL;
		TArray< UMovieScene* > MovieScenesBeingEdited = Sequencer.Pin()->GetMovieScenesBeingEdited();
		for( auto CurMovieSceneIt( MovieScenesBeingEdited.CreateIterator() ); CurMovieSceneIt; ++CurMovieSceneIt )
		{
			auto CurMovieScene = *CurMovieSceneIt;
			FoundSpawnable = CurMovieScene->FindSpawnable( PuppetActorInfo->SpawnableGuid );
			if( FoundSpawnable != NULL )
			{
				break;
			}
		}
		
		if (ensure( FoundSpawnable != NULL && PuppetActor != NULL ))
		{
			UClass* ActorClass = FoundSpawnable->GetClass();

			// The puppet actor's class should always be the blueprint that it was spawned from!
			UClass* SpawnedActorClass = PuppetActor->GetClass();
			check( ActorClass == SpawnedActorClass );

			// We'll be copying properties into the class default object of the Blueprint's generated class
			TargetActor = CastChecked<AActor>( ActorClass->GetDefaultObject() );
		}
	}


	if( PuppetActor != NULL && TargetActor != NULL )
	{
		Sequencer.Pin()->CopyActorProperties( PuppetActor, TargetActor );
	}
}


FGuid FSequencerActorBindingManager::FindSpawnableGuidForPuppetObject( UObject* Object ) const
{
	for( auto MovieSceneIter( InstanceToPuppetObjectsMap.CreateConstIterator() ); MovieSceneIter; ++MovieSceneIter ) 
	{
		const TArray< TSharedRef<FPuppetActorInfo> >& PuppetObjects = MovieSceneIter.Value();

		// Is this an object that we care about?
		for( auto PuppetObjectIter( PuppetObjects.CreateConstIterator() ); PuppetObjectIter; ++PuppetObjectIter )
		{
			if( PuppetObjectIter->Get().GetType() == EPuppetObjectType::Actor )
			{
				TSharedRef< FPuppetActorInfo > PuppetActorInfo = StaticCastSharedRef< FPuppetActorInfo >( *PuppetObjectIter );
				AActor* PuppetActor = PuppetActorInfo->PuppetActor.Get();
				if( PuppetActor != NULL )
				{
					if( PuppetActor == Object )
					{
						// Found it!
						return PuppetActorInfo->SpawnableGuid;
					}
				}
			}
		}
	}

	// Not found
	return FGuid();
}


UObject* FSequencerActorBindingManager::FindPuppetObjectForSpawnableGuid( const TSharedRef<FMovieSceneInstance>& MovieSceneInstance, const FGuid& Guid ) const
{
	const TArray< TSharedRef<FPuppetActorInfo> >& PuppetObjects = InstanceToPuppetObjectsMap.FindChecked( MovieSceneInstance );

	for( auto PuppetObjectIter( PuppetObjects.CreateConstIterator() ); PuppetObjectIter; ++PuppetObjectIter )
	{
		if( PuppetObjectIter->Get().GetType() == EPuppetObjectType::Actor )
		{
			TSharedRef< FPuppetActorInfo > PuppetActorInfo = StaticCastSharedRef< FPuppetActorInfo >( *PuppetObjectIter );
			if( PuppetActorInfo->SpawnableGuid == Guid )
			{
				return PuppetActorInfo->PuppetActor.Get();
			}
		}
	}

	// Not found
	return NULL;
}


#undef LOCTEXT_NAMESPACE