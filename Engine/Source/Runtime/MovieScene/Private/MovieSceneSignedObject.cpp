// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSignedObject.h"
#include "Templates/Casts.h"

UMovieSceneSignedObject::UMovieSceneSignedObject(const FObjectInitializer& Init)
	: Super(Init)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		MarkAsChanged();
	}
}

void UMovieSceneSignedObject::MarkAsChanged()
{
	Signature = FGuid::NewGuid();
	OnSignatureChangedEvent.Broadcast();
	
	UObject* Outer = GetOuter();
	while (Outer)
	{
		UMovieSceneSignedObject* TypedOuter = Cast<UMovieSceneSignedObject>(Outer);
		if (TypedOuter)
		{
			TypedOuter->MarkAsChanged();
			break;
		}
		Outer = Outer->GetOuter();
	}
}

bool UMovieSceneSignedObject::Modify(bool bAlwaysMarkDirty)
{
	bool bModified = Super::Modify(bAlwaysMarkDirty);
	MarkAsChanged();
	return bModified;
}

#if WITH_EDITOR

void UMovieSceneSignedObject::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MarkAsChanged();
}

void UMovieSceneSignedObject::PostEditUndo()
{
	Super::PostEditUndo();
	MarkAsChanged();
}

void UMovieSceneSignedObject::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	Super::PostEditUndo(TransactionAnnotation);
	MarkAsChanged();
}

#endif
