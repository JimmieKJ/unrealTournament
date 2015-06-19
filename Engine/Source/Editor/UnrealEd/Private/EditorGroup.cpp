// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "ScopedTransaction.h"
#include "MainFrame.h"
#include "MeshUtilities.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Engine/Selection.h"


void UUnrealEdEngine::edactRegroupFromSelected()
{
	if(GEditor->bGroupingActive)
	{
		TArray<AActor*> ActorsToAdd;

		ULevel* ActorLevel = NULL;

		bool bActorsInSameLevel = true;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
		{
			AActor* Actor = CastChecked<AActor>(*It);
			if( !ActorLevel )
			{
				ActorLevel = Actor->GetLevel();
			}
			else if( ActorLevel != Actor->GetLevel() )
			{
				bActorsInSameLevel = false;
				break;
			}

			if ( Actor->IsA(AActor::StaticClass()) && !Actor->IsA(AGroupActor::StaticClass()) )
			{
				// Add each selected actor to our new group
				// Adding an actor will remove it from any existing groups.
				ActorsToAdd.Add( Actor );

			}
		}

		if( bActorsInSameLevel )
		{
			if( ActorsToAdd.Num() > 1 )
			{
				check(ActorLevel);
				// Store off the current level and make the level that contain the actors to group as the current level
				UWorld* World = ActorLevel->OwningWorld;
				check( World );
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "Group_Regroup", "Regroup Ctrl+G") );

					FActorSpawnParameters SpawnInfo;
					SpawnInfo.OverrideLevel = ActorLevel;
					AGroupActor* SpawnedGroupActor = World->SpawnActor<AGroupActor>( SpawnInfo );

					for( int32 ActorIndex = 0; ActorIndex < ActorsToAdd.Num(); ++ActorIndex )
					{
						SpawnedGroupActor->Add( *ActorsToAdd[ActorIndex] );
					}

					SpawnedGroupActor->CenterGroupLocation();
					SpawnedGroupActor->Lock();
				}
			}
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Group_CantCreateGroupMultipleLevels", "Can't group the selected actors because they are in different levels.") );
		}
	}
}


void UUnrealEdEngine::edactUngroupFromSelected()
{
	if(GEditor->bGroupingActive)
	{
		TArray<AGroupActor*> OutermostGroupActors;
		
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );

			// Get the outermost locked group
			AGroupActor* OutermostGroup = AGroupActor::GetRootForActor( Actor, true );
			if( OutermostGroup == NULL )
			{
				// Failed to find locked root group, try to find the immediate parent
				OutermostGroup = AGroupActor::GetParentForActor( Actor );
			}
			
			if( OutermostGroup )
			{
				OutermostGroupActors.AddUnique( OutermostGroup );
			}
		}

		if( OutermostGroupActors.Num() )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "Group_Disband", "Disband Group") );
			for( int32 GroupIndex = 0; GroupIndex < OutermostGroupActors.Num(); ++GroupIndex )
			{
				AGroupActor* GroupActor = OutermostGroupActors[GroupIndex];
				GroupActor->ClearAndRemove();
			}
		}
	}
}


void UUnrealEdEngine::edactLockSelectedGroups()
{
	if(GEditor->bGroupingActive)
	{
		AGroupActor::LockSelectedGroups();
	}
}


void UUnrealEdEngine::edactUnlockSelectedGroups()
{
	if(GEditor->bGroupingActive)
	{
		AGroupActor::UnlockSelectedGroups();
	}
}


void UUnrealEdEngine::edactAddToGroup()
{
	if(GEditor->bGroupingActive)
	{
		AGroupActor::AddSelectedActorsToSelectedGroup();
	}
}


void UUnrealEdEngine::edactRemoveFromGroup()
{
	if(GEditor->bGroupingActive)
	{
		TArray<AActor*> ActorsToRemove;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			// See if an entire group is being removed
			AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
			if(GroupActor == NULL)
			{
				// See if the actor selected belongs to a locked group, if so remove the group in lieu of the actor
				GroupActor = AGroupActor::GetParentForActor(Actor);
				if(GroupActor && !GroupActor->IsLocked())
				{
					GroupActor = NULL;
				}
			}

			if(GroupActor)
			{
				// If the GroupActor has no parent, do nothing, otherwise just add the group for removal
				if(AGroupActor::GetParentForActor(GroupActor))
				{
					ActorsToRemove.AddUnique(GroupActor);
				}
			}
			else
			{
				ActorsToRemove.AddUnique(Actor);
			}
		}

		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "Group_Remove", "Remove from Group") );
		for ( int32 ActorIndex = 0; ActorIndex < ActorsToRemove.Num(); ++ActorIndex )
		{
			AActor* Actor = ActorsToRemove[ActorIndex];
			AGroupActor* ActorGroup = AGroupActor::GetParentForActor(Actor);

			if(ActorGroup)
			{
				AGroupActor* ActorGroupParent = AGroupActor::GetParentForActor(ActorGroup);
				if(ActorGroupParent)
				{
					ActorGroupParent->Add(*Actor);
					ActorGroupParent->CenterGroupLocation();
				}
				else
				{
					ActorGroup->Remove(*Actor);
					ActorGroup->CenterGroupLocation();
				}
			}
		}
		// Do a re-selection of each actor, to maintain group selection rules
		GEditor->SelectNone(true, true);
		for ( int32 ActorIndex = 0; ActorIndex < ActorsToRemove.Num(); ++ActorIndex )
		{
			GEditor->SelectActor( ActorsToRemove[ActorIndex], true, false);
		}
	}
}

