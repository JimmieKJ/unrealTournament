// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


// Includes.
#include "UnrealEd.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Engine/Selection.h"

#include "LandscapeProxy.h"
#include "LandscapeComponent.h"

uint32			EngineThreadId;
const TCHAR*	GItem;
const TCHAR*	GValue;
TCHAR*			GCommand;

extern int GLastScroll;

// Misc.
UEngine* Engine;

/*-----------------------------------------------------------------------------
	Editor hook exec.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::NotifyPreChange( UProperty* PropertyAboutToChange )
{
}
void UUnrealEdEngine::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged )
{
	// Notify all active modes of actor property changes.
	TArray<FEdMode*> ActiveModes;
	GLevelEditorModeTools().GetActiveModes( ActiveModes );

	for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes[ModeIndex]->ActorPropChangeNotify();
	}
}

void UUnrealEdEngine::UpdateFloatingPropertyWindows(bool bForceRefresh)
{
	TArray<UObject*> SelectedObjects;

	bool bProcessed = false;
	if (GetSelectedActorCount() == 1)
	{
		FSelectionIterator It( GetSelectedActorIterator() );
		if ( It )
		{
			ALandscapeProxy* LP = Cast<ALandscapeProxy>(*It);
			if (LP != NULL && !LP->IsPendingKill())
			{
				ULandscapeInfo* Info = LP->GetLandscapeInfo(false);
				if (Info && Info->bCurrentlyEditing)
				{
					for (ULandscapeComponent* LandscapeComponent : Info->GetSelectedComponents())
					{
						SelectedObjects.Add(LandscapeComponent);
					}
					bProcessed = true;
				}
			}
		}
	}
	
	if (!bProcessed)
	{
		// Assemble a set of valid selected actors.
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->IsPendingKill() )
			{
				SelectedObjects.Add( Actor );
			}
		}
	}

	UpdateFloatingPropertyWindowsFromActorList(SelectedObjects, bForceRefresh);
}


void UUnrealEdEngine::UpdateFloatingPropertyWindowsFromActorList(const TArray< UObject *>& ActorList, bool bForceRefresh)
{
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	LevelEditor.BroadcastActorSelectionChanged(ActorList, bForceRefresh);
}
