// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "UMGDetailKeyframeHandler.h"
#include "ISequencer.h"
#include "PropertyHandle.h"
#include "WidgetBlueprintEditor.h"

FUMGDetailKeyframeHandler::FUMGDetailKeyframeHandler(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: BlueprintEditor( InBlueprintEditor )
{}

bool FUMGDetailKeyframeHandler::IsPropertyKeyable(const UClass& InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	return BlueprintEditor.Pin()->GetSequencer()->CanKeyProperty( InObjectClass, InPropertyHandle );
}

bool FUMGDetailKeyframeHandler::IsPropertyKeyingEnabled() const
{
	UMovieScene* MovieScene = BlueprintEditor.Pin()->GetSequencer()->GetRootMovieScene();
	return MovieScene != nullptr && MovieScene != UWidgetAnimation::GetNullAnimation()->MovieScene;
}

void FUMGDetailKeyframeHandler::OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle)
{
	TArray<UObject*> Objects;
	KeyedPropertyHandle.GetOuterObjects( Objects );

	BlueprintEditor.Pin()->GetSequencer()->KeyProperty( Objects, KeyedPropertyHandle );
}
