// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "UMGDetailKeyframeHandler.h"
#include "ISequencer.h"
#include "PropertyHandle.h"
#include "WidgetBlueprintEditor.h"

FUMGDetailKeyframeHandler::FUMGDetailKeyframeHandler(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: BlueprintEditor( InBlueprintEditor )
{}

bool FUMGDetailKeyframeHandler::IsPropertyKeyable(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	return BlueprintEditor.Pin()->GetSequencer()->CanKeyProperty(FCanKeyPropertyParams(InObjectClass, InPropertyHandle));
}

bool FUMGDetailKeyframeHandler::IsPropertyKeyingEnabled() const
{
	UMovieSceneSequence* Sequence = BlueprintEditor.Pin()->GetSequencer()->GetRootMovieSceneSequence();
	return Sequence != nullptr && Sequence != UWidgetAnimation::GetNullAnimation();
}

void FUMGDetailKeyframeHandler::OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle)
{
	TArray<UObject*> Objects;
	KeyedPropertyHandle.GetOuterObjects( Objects );

	FKeyPropertyParams KeyPropertyParams(Objects, KeyedPropertyHandle);
	KeyPropertyParams.KeyParams.bCreateHandleIfMissing = true;
	KeyPropertyParams.KeyParams.bCreateTrackIfMissing = true;
	KeyPropertyParams.KeyParams.bCreateKeyIfUnchanged = true;
	KeyPropertyParams.KeyParams.bCreateKeyIfEmpty = true;
	KeyPropertyParams.KeyParams.bCreateKeyOnlyWhenAutoKeying = false;

	BlueprintEditor.Pin()->GetSequencer()->KeyProperty(KeyPropertyParams);
}
