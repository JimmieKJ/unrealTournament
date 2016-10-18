// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene.h"
#include "WidgetAnimation.h"
#include "WidgetTree.h"


#define LOCTEXT_NAMESPACE "UWidgetAnimation"


/* UWidgetAnimation structors
 *****************************************************************************/

UWidgetAnimation::UWidgetAnimation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
{ }


/* UWidgetAnimation interface
 *****************************************************************************/

#if WITH_EDITOR

UWidgetAnimation* UWidgetAnimation::GetNullAnimation()
{
	static UWidgetAnimation* NullAnimation = nullptr;

	if (!NullAnimation)
	{
		NullAnimation = NewObject<UWidgetAnimation>(GetTransientPackage(), NAME_None);
		NullAnimation->AddToRoot();
		NullAnimation->MovieScene = NewObject<UMovieScene>(NullAnimation, FName("No Animation"));
		NullAnimation->MovieScene->AddToRoot();
	}

	return NullAnimation;
}

#endif


float UWidgetAnimation::GetStartTime() const
{
	return MovieScene->GetPlaybackRange().GetLowerBoundValue();
}


float UWidgetAnimation::GetEndTime() const
{
	return MovieScene->GetPlaybackRange().GetUpperBoundValue();
}


void UWidgetAnimation::Initialize(UUserWidget* InPreviewWidget)
{
	PreviewWidget = InPreviewWidget;

	// clear object maps
	PreviewObjectToIds.Empty();
	IdToPreviewObjects.Empty();
	IdToSlotContentPreviewObjects.Empty();
	SlotContentPreviewObjectToIds.Empty();

	if (PreviewWidget == nullptr)
	{
		return;
	}

	// populate object maps
	UWidgetTree* WidgetTree = PreviewWidget->WidgetTree;

	for (const FWidgetAnimationBinding& Binding : AnimationBindings)
	{
		UObject* FoundObject = Binding.FindRuntimeObject(*WidgetTree, *InPreviewWidget);

		if (FoundObject == nullptr)
		{
			continue;
		}

		UPanelSlot* FoundSlot = Cast<UPanelSlot>(FoundObject);

		if (FoundSlot == nullptr)
		{
			IdToPreviewObjects.Add(Binding.AnimationGuid, FoundObject);
			PreviewObjectToIds.Add(FoundObject, Binding.AnimationGuid);
		}
		else
		{
			IdToSlotContentPreviewObjects.Add(Binding.AnimationGuid, FoundSlot->Content);
			SlotContentPreviewObjectToIds.Add(FoundSlot->Content, Binding.AnimationGuid);
		}
	}
}


/* UMovieSceneAnimation overrides
 *****************************************************************************/


void UWidgetAnimation::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	// If it's the Root Widget
	if (&PossessedObject == PreviewWidget.Get())
	{
		PreviewObjectToIds.Add(&PossessedObject, ObjectId);
		IdToPreviewObjects.Add(ObjectId, &PossessedObject);

		FWidgetAnimationBinding NewBinding;
		{
			NewBinding.AnimationGuid = ObjectId;
			NewBinding.WidgetName = PossessedObject.GetFName();
			NewBinding.bIsRootWidget = true;
		}

		AnimationBindings.Add(NewBinding);
		return;
	}
	
	UPanelSlot* PossessedSlot = Cast<UPanelSlot>(&PossessedObject);

	if ((PossessedSlot != nullptr) && (PossessedSlot->Content != nullptr))
	{
		SlotContentPreviewObjectToIds.Add(PossessedSlot->Content, ObjectId);
		IdToSlotContentPreviewObjects.Add(ObjectId, PossessedSlot->Content);

		// Save the name of the widget containing the slots. This is the object
		// to look up that contains the slot itself (the thing we are animating).
		FWidgetAnimationBinding NewBinding;
		{
			NewBinding.AnimationGuid = ObjectId;
			NewBinding.SlotWidgetName = PossessedSlot->GetFName();
			NewBinding.WidgetName = PossessedSlot->Content->GetFName();
			NewBinding.bIsRootWidget = false;
		}

		AnimationBindings.Add(NewBinding);
	}
	else if (PossessedSlot == nullptr)
	{
		PreviewObjectToIds.Add(&PossessedObject, ObjectId);
		IdToPreviewObjects.Add(ObjectId, &PossessedObject);

		FWidgetAnimationBinding NewBinding;
		{
			NewBinding.AnimationGuid = ObjectId;
			NewBinding.WidgetName = PossessedObject.GetFName();
			NewBinding.bIsRootWidget = false;
		}

		AnimationBindings.Add(NewBinding);
	}
}


bool UWidgetAnimation::CanPossessObject(UObject& Object) const
{
	if (&Object == PreviewWidget.Get())
	{
		return true;
	}

	UPanelSlot* Slot = Cast<UPanelSlot>(&Object);

	if ((Slot != nullptr) && (Slot->Content == nullptr))
	{
		// can't possess empty slots.
		return false;
	}

	return (Object.IsA<UVisual>() && Object.IsIn(PreviewWidget.Get()));
}


UObject* UWidgetAnimation::FindPossessableObject(const FGuid& ObjectId, UObject* Context) const
{
	TWeakObjectPtr<UObject> PreviewObject = IdToPreviewObjects.FindRef(ObjectId);

	if (PreviewObject.IsValid())
	{
		return PreviewObject.Get();
	}

	TWeakObjectPtr<UObject> SlotContentPreviewObject = IdToSlotContentPreviewObjects.FindRef(ObjectId);

	if (SlotContentPreviewObject.IsValid())
	{
		UWidget* Widget = Cast<UWidget>(SlotContentPreviewObject.Get());
		if ( Widget != nullptr )
		{
			return Widget->Slot;
		}
	}

	return nullptr;
}


FGuid UWidgetAnimation::FindPossessableObjectId(UObject& Object) const
{
	UPanelSlot* Slot = Cast<UPanelSlot>(&Object);

	if (Slot != nullptr)
	{
		// slot guids are tracked by their content.
		return SlotContentPreviewObjectToIds.FindRef(Slot->Content);
	}

	return PreviewObjectToIds.FindRef(&Object);
}


UMovieScene* UWidgetAnimation::GetMovieScene() const
{
	return MovieScene;
}


UObject* UWidgetAnimation::GetParentObject(UObject* Object) const
{
	UPanelSlot* Slot = Cast<UPanelSlot>(Object);

	if (Slot != nullptr)
	{
		// The slot is actually the child of the panel widget in the hierarchy,
		// but we want it to show up as a sub-object of the widget it contains
		// in the timeline so we return the content's GUID.
		return Slot->Content;
	}

	return nullptr;
}


void UWidgetAnimation::UnbindPossessableObjects(const FGuid& ObjectId)
{
	// unbind widgets
	TArray<TWeakObjectPtr<UObject>> PreviewObjects;
	{
		IdToPreviewObjects.MultiFind(ObjectId, PreviewObjects);

		for (TWeakObjectPtr<UObject>& PreviewObject : PreviewObjects)
		{
			PreviewObjectToIds.Remove(PreviewObject);
		}

		IdToPreviewObjects.Remove(ObjectId);
	}

	// unbind slots
	TArray<TWeakObjectPtr<UObject>> SlotContentPreviewObjects;
	{
		IdToSlotContentPreviewObjects.MultiFind(ObjectId, SlotContentPreviewObjects);

		for (TWeakObjectPtr<UObject>& SlotContentPreviewObject : SlotContentPreviewObjects)
		{
			SlotContentPreviewObjectToIds.Remove(SlotContentPreviewObject);
		}

		IdToSlotContentPreviewObjects.Remove(ObjectId);
	}

	// mark dirty
	Modify();

	// remove animation bindings
	AnimationBindings.RemoveAll([&](const FWidgetAnimationBinding& Binding) {
		return Binding.AnimationGuid == ObjectId;
	});
}

void UWidgetAnimation::ReplacePossessableObject(const FGuid& OldId, const FGuid& NewId, UObject& OldObject, UObject& NewObject)
{
	TArray<TWeakObjectPtr<UObject>> PreviewObjects;
	{
		IdToPreviewObjects.MultiFind(OldId, PreviewObjects);

		for (TWeakObjectPtr<UObject>& PreviewObject : PreviewObjects)
		{
			PreviewObjectToIds.Remove(PreviewObject);
			PreviewObjectToIds.Add(&NewObject, NewId);
		}

		IdToPreviewObjects.Remove(OldId);
		IdToPreviewObjects.Add(NewId, &NewObject);
	}

	// slots
	FGuid SlotContentId = SlotContentPreviewObjectToIds.FindRef(&OldObject);
	TArray<TWeakObjectPtr<UObject>> SlotContentPreviewObjects;
	{
		IdToSlotContentPreviewObjects.MultiFind(SlotContentId, SlotContentPreviewObjects);
		
		for (TWeakObjectPtr<UObject>& SlotContentPreviewObject : SlotContentPreviewObjects)
		{
			SlotContentPreviewObjectToIds.Remove(SlotContentPreviewObject);
			SlotContentPreviewObjectToIds.Add(&NewObject, SlotContentId);
		}

		IdToSlotContentPreviewObjects.Remove(SlotContentId);
		IdToSlotContentPreviewObjects.Add(SlotContentId, &NewObject);
	}
}


#undef LOCTEXT_NAMESPACE
