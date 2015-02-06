// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "UMGSequencerObjectBindingManager.h"
#include "WidgetBlueprintEditor.h"
#include "ISequencer.h"
#include "MovieScene.h"
#include "WidgetBlueprintEditor.h"
#include "Components/PanelSlot.h"
#include "Components/Visual.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "WidgetBlueprint.h"

FUMGSequencerObjectBindingManager::FUMGSequencerObjectBindingManager( FWidgetBlueprintEditor& InWidgetBlueprintEditor, UWidgetAnimation& InAnimation )
	: WidgetAnimation( &InAnimation )
	, WidgetBlueprintEditor( InWidgetBlueprintEditor )
{
	WidgetBlueprintEditor.GetOnWidgetPreviewUpdated().AddRaw( this, &FUMGSequencerObjectBindingManager::OnWidgetPreviewUpdated );

}

FUMGSequencerObjectBindingManager::~FUMGSequencerObjectBindingManager()
{
	WidgetBlueprintEditor.GetOnWidgetPreviewUpdated().RemoveAll( this );
}

void FUMGSequencerObjectBindingManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(WidgetAnimation);
}

FGuid FUMGSequencerObjectBindingManager::FindGuidForObject( const UMovieScene& MovieScene, UObject& Object ) const
{
	return PreviewObjectToGuidMap.FindRef( &Object );
}

bool FUMGSequencerObjectBindingManager::CanPossessObject( UObject& Object ) const
{
	UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor.GetWidgetBlueprintObj();

	// Only preview widgets in this blueprint can be possessed
	UUserWidget* PreviewWidget = WidgetBlueprintEditor.GetPreview();

	return Object.IsA<UVisual>() && Object.IsIn( PreviewWidget );
}

void FUMGSequencerObjectBindingManager::BindPossessableObject( const FGuid& PossessableGuid, UObject& PossessedObject )
{
	PreviewObjectToGuidMap.Add( &PossessedObject, PossessableGuid );

	FWidgetAnimationBinding NewBinding;
	NewBinding.AnimationGuid = PossessableGuid;

	UPanelSlot* Slot = Cast<UPanelSlot>( &PossessedObject );
	if( Slot && Slot->Content )
	{
		// Save the name of the widget containing the slots.  This is the object to look up that contains the slot itself(the thing we are animating)
		NewBinding.SlotWidgetName = Slot->GetFName();
		NewBinding.WidgetName = Slot->Content->GetFName();


		WidgetAnimation->AnimationBindings.Add(NewBinding);
	}
	else if( !Slot )
	{
		NewBinding.WidgetName = PossessedObject.GetFName();

		WidgetAnimation->AnimationBindings.Add(NewBinding);
	}

}

void FUMGSequencerObjectBindingManager::UnbindPossessableObjects( const FGuid& PossessableGuid )
{
	for( auto It = PreviewObjectToGuidMap.CreateIterator(); It; ++It )
	{
		if( It.Value() == PossessableGuid )
		{
			It.RemoveCurrent();
		}
	}

	UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor.GetWidgetBlueprintObj();

	WidgetAnimation->AnimationBindings.RemoveAll( [&]( const FWidgetAnimationBinding& Binding ) { return Binding.AnimationGuid == PossessableGuid; } );

}

void FUMGSequencerObjectBindingManager::GetRuntimeObjects( const TSharedRef<FMovieSceneInstance>& MovieSceneInstance, const FGuid& ObjectGuid, TArray<UObject*>& OutRuntimeObjects ) const
{
	for( auto It = PreviewObjectToGuidMap.CreateConstIterator(); It; ++It )
	{
		UObject* Object = It.Key().Get();
		if( Object && It.Value() == ObjectGuid )
		{
			OutRuntimeObjects.Add( Object );
		}
	}
}

bool FUMGSequencerObjectBindingManager::HasValidWidgetAnimation() const
{
	UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor.GetWidgetBlueprintObj();
	return WidgetAnimation != nullptr && WidgetBlueprint->Animations.Contains( WidgetAnimation );
}

void FUMGSequencerObjectBindingManager::InitPreviewObjects()
{
	// Populate preview object to guid map
	UUserWidget* PreviewWidget = WidgetBlueprintEditor.GetPreview();

	if( PreviewWidget )
	{
		UWidgetTree* WidgetTree = PreviewWidget->WidgetTree;

		for(const FWidgetAnimationBinding& Binding : WidgetAnimation->AnimationBindings)
		{
			UObject* FoundObject = Binding.FindRuntimeObject( *WidgetTree );
			if( FoundObject )
			{
				PreviewObjectToGuidMap.Add(FoundObject, Binding.AnimationGuid);
			}
		}
	}
}

void FUMGSequencerObjectBindingManager::OnWidgetPreviewUpdated()
{
	PreviewObjectToGuidMap.Empty();

	InitPreviewObjects();

	WidgetBlueprintEditor.GetSequencer()->UpdateRuntimeInstances();
}