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

#define LOCTEXT_NAMESPACE "UMGSequencerObjectBindingManager"

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
	// Slot guids are tracked by their content.
	UPanelSlot* Slot = Cast<UPanelSlot>(&Object);
	if (Slot != nullptr)
	{
		return SlotContentPreviewObjectToGuidMap.FindRef(Slot->Content);
	}
	return PreviewObjectToGuidMap.FindRef( &Object );
}

bool FUMGSequencerObjectBindingManager::CanPossessObject( UObject& Object ) const
{
	// Can't possess empty slots.
	UPanelSlot* Slot = Cast<UPanelSlot>(&Object);
	if (Slot != nullptr && Slot->Content == nullptr)
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor.GetWidgetBlueprintObj();

	// Only preview widgets in this blueprint can be possessed
	UUserWidget* PreviewWidget = WidgetBlueprintEditor.GetPreview();

	return Object.IsA<UVisual>() && Object.IsIn( PreviewWidget );
}

void FUMGSequencerObjectBindingManager::BindPossessableObject( const FGuid& PossessableGuid, UObject& PossessedObject )
{
	UPanelSlot* PossessedSlot = Cast<UPanelSlot>(&PossessedObject);
	if ( PossessedSlot != nullptr && PossessedSlot->Content != nullptr )
	{
		SlotContentPreviewObjectToGuidMap.Add(PossessedSlot->Content, PossessableGuid);
		GuidToSlotContentPreviewObjectsMap.Add(PossessableGuid, PossessedSlot->Content);

		// Save the name of the widget containing the slots.  This is the object to look up that contains the slot itself(the thing we are animating)
		FWidgetAnimationBinding NewBinding;
		NewBinding.AnimationGuid = PossessableGuid;
		NewBinding.SlotWidgetName = PossessedSlot->GetFName();
		NewBinding.WidgetName = PossessedSlot->Content->GetFName();

		WidgetAnimation->AnimationBindings.Add(NewBinding);
	}
	else if( PossessedSlot == nullptr )
	{
		PreviewObjectToGuidMap.Add(&PossessedObject, PossessableGuid);
		GuidToPreviewObjectsMap.Add(PossessableGuid, &PossessedObject);

		FWidgetAnimationBinding NewBinding;
		NewBinding.AnimationGuid = PossessableGuid;
		NewBinding.WidgetName = PossessedObject.GetFName();

		WidgetAnimation->AnimationBindings.Add(NewBinding);
	}
}

void FUMGSequencerObjectBindingManager::UnbindPossessableObjects( const FGuid& PossessableGuid )
{
	TArray<TWeakObjectPtr<UObject>> PreviewObjects;
	GuidToPreviewObjectsMap.MultiFind(PossessableGuid, PreviewObjects);
	for (TWeakObjectPtr<UObject>& PreviewObject : PreviewObjects)
	{
		PreviewObjectToGuidMap.Remove(PreviewObject);
	}
	GuidToPreviewObjectsMap.Remove(PossessableGuid);

	TArray<TWeakObjectPtr<UObject>> SlotContentPreviewObjects;
	GuidToSlotContentPreviewObjectsMap.MultiFind(PossessableGuid, SlotContentPreviewObjects);
	for (TWeakObjectPtr<UObject>& SlotContentPreviewObject : SlotContentPreviewObjects)
	{
		SlotContentPreviewObjectToGuidMap.Remove(SlotContentPreviewObject);
	}
	GuidToSlotContentPreviewObjectsMap.Remove(PossessableGuid);

	UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor.GetWidgetBlueprintObj();

	WidgetAnimation->Modify();
	WidgetAnimation->AnimationBindings.RemoveAll( [&]( const FWidgetAnimationBinding& Binding ) { return Binding.AnimationGuid == PossessableGuid; } );

}

void FUMGSequencerObjectBindingManager::GetRuntimeObjects( const TSharedRef<FMovieSceneInstance>& MovieSceneInstance, const FGuid& ObjectGuid, TArray<UObject*>& OutRuntimeObjects ) const
{
	TArray<TWeakObjectPtr<UObject>> PreviewObjects;
	GuidToPreviewObjectsMap.MultiFind(ObjectGuid, PreviewObjects);
	for (TWeakObjectPtr<UObject>& PreviewObject : PreviewObjects)
	{
		OutRuntimeObjects.Add(PreviewObject.Get());
	}

	TArray<TWeakObjectPtr<UObject>> SlotContentPreviewObjects;
	GuidToSlotContentPreviewObjectsMap.MultiFind(ObjectGuid, SlotContentPreviewObjects);
	for (TWeakObjectPtr<UObject>& SlotContentPreviewObject : SlotContentPreviewObjects)
	{
		UWidget* ContentWidget = Cast<UWidget>(SlotContentPreviewObject.Get());
		OutRuntimeObjects.Add(ContentWidget->Slot);
	}
}

bool FUMGSequencerObjectBindingManager::TryGetObjectBindingDisplayName(const FGuid& ObjectGuid, FText& DisplayName) const
{
	// TODO: This gets called every frame for every bound object and could be a potential performance issue for a really complicated animation.
	TArray<TWeakObjectPtr<UObject>> BindingObjects;
	GuidToPreviewObjectsMap.MultiFind(ObjectGuid, BindingObjects);
	TArray<TWeakObjectPtr<UObject>> SlotContentBindingObjects;
	GuidToSlotContentPreviewObjectsMap.MultiFind(ObjectGuid, SlotContentBindingObjects);
	if (BindingObjects.Num() == 0 && SlotContentBindingObjects.Num() == 0)
	{
		DisplayName = LOCTEXT("NoBoundObjects", "No bound objects");
	}
	else if (BindingObjects.Num() + SlotContentBindingObjects.Num() > 1)
	{
		DisplayName = LOCTEXT("Multiple bound objects", "Multilple bound objects");
	}
	else if (BindingObjects.Num() == 1)
	{
		DisplayName = FText::FromString(BindingObjects[0].Get()->GetName());
	}
	else // SlotContentBindingObjects.Num() == 1
	{
		UWidget* SlotContent = Cast<UWidget>(SlotContentBindingObjects[0].Get());
		FText PanelName = SlotContent->Slot != nullptr && SlotContent->Slot->Parent != nullptr
			? FText::FromString(SlotContent->Slot->Parent->GetName())
			: LOCTEXT("InvalidPanel", "Invalid Panel");
		FText ContentName = FText::FromString(SlotContent->GetName());
		DisplayName = FText::Format(LOCTEXT("SlotObject", "{0} ({1} Slot)"), ContentName, PanelName);
	}
	return true;
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
			if ( FoundObject )
			{
				UPanelSlot* FoundSlot = Cast<UPanelSlot>(FoundObject);
				if (FoundSlot == nullptr)
				{
					PreviewObjectToGuidMap.Add(FoundObject, Binding.AnimationGuid);
					GuidToPreviewObjectsMap.Add(Binding.AnimationGuid, FoundObject);
				}
				else
				{
					SlotContentPreviewObjectToGuidMap.Add(FoundSlot->Content, Binding.AnimationGuid);
					GuidToSlotContentPreviewObjectsMap.Add(Binding.AnimationGuid, FoundSlot->Content);
				}
			}
		}
	}
}

void FUMGSequencerObjectBindingManager::OnWidgetPreviewUpdated()
{
	PreviewObjectToGuidMap.Empty();
	GuidToPreviewObjectsMap.Empty();
	SlotContentPreviewObjectToGuidMap.Empty();
	GuidToSlotContentPreviewObjectsMap.Empty();

	InitPreviewObjects();

	WidgetBlueprintEditor.GetSequencer()->UpdateRuntimeInstances();
}

#undef LOCTEXT_NAMESPACE