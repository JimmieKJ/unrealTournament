// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene.h"
#include "WidgetAnimation.h"

UObject* FWidgetAnimationBinding::FindRuntimeObject( UWidgetTree& WidgetTree ) const
{
	UObject* FoundObject = FindObject<UObject>(&WidgetTree, *WidgetName.ToString());
	if(FoundObject)
	{
		if(SlotWidgetName != NAME_None)
		{
			// if we were animating the slot, look up the slot that contains the widget 
			UWidget* WidgetObject = Cast<UWidget>(FoundObject);
			if(WidgetObject->Slot)
			{
				FoundObject = WidgetObject->Slot;
			}
		}
	}

	return FoundObject;
}

UWidgetAnimation::UWidgetAnimation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR

UWidgetAnimation* UWidgetAnimation::GetNullAnimation()
{
	static UWidgetAnimation* NullAnimation = nullptr;
	if( !NullAnimation )
	{
		NullAnimation = NewObject<UWidgetAnimation>(GetTransientPackage(), NAME_None, RF_RootSet);
		NullAnimation->MovieScene = NewObject<UMovieScene>(NullAnimation, FName("No Animation"), RF_RootSet);
	}

	return NullAnimation;
}

#endif

float UWidgetAnimation::GetStartTime() const
{
	return MovieScene->GetTimeRange().GetLowerBoundValue();
}

float UWidgetAnimation::GetEndTime() const
{
	return MovieScene->GetTimeRange().GetUpperBoundValue();
}
