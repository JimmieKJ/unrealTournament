// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MatineeModule.h"
#include "Matinee.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"

void FMatinee::BuildPropertyWindow()
{
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyWindow = PropertyModule.CreateDetailView( Args );
}


/**
 * Updates the contents of the property window based on which groups or tracks are selected if any. 
 */
void FMatinee::UpdatePropertyWindow()
{
	TArray<UObject*> Objects;

	if( HasATrackSelected() )
	{
		TArray<UInterpTrack*> SelectedTracks;
		GetSelectedTracks(SelectedTracks);

		// We are guaranteed at least one selected track.
		check( SelectedTracks.Num() );

		// Send tracks to the property window
		for (int32 Index=0; Index < SelectedTracks.Num(); Index++)
		{
			Objects.Add(SelectedTracks[Index]);
		}
	}
	else if( HasAGroupSelected() )
	{
		TArray<UInterpGroup*> SelectedGroups;
		GetSelectedGroups(SelectedGroups);

		// We are guaranteed at least one selected group.
		check( SelectedGroups.Num() );

		// Send groups to the property window
		for (int32 Index=0; Index < SelectedGroups.Num(); Index++)
		{
			Objects.Add(SelectedGroups[Index]);
		}
	}
	// else send nothing
	
	PropertyWindow->SetObjects(Objects);
}