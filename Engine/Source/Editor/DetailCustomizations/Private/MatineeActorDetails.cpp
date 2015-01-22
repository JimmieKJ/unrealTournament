// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "Matinee/MatineeActor.h"
#include "MatineeActorDetails.h"

#define LOCTEXT_NAMESPACE "MatineeActorDetails"



TSharedRef<IDetailCustomization> FMatineeActorDetails::MakeInstance()
{
	return MakeShareable( new FMatineeActorDetails );
}

void FMatineeActorDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetDetailsView().GetSelectedObjects();

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			AMatineeActor* CurrentMatineeActor = Cast<AMatineeActor>(CurrentObject.Get());
			if (CurrentMatineeActor != NULL)
			{
				MatineeActor = CurrentMatineeActor;
				break;
			}
		}
	}
	
	DetailLayout.EditCategory( "MatineeActor", NSLOCTEXT("MatineeActorDetails", "MatineeActor", "Matinee Actor"), ECategoryPriority::Important )
	.AddCustomRow( NSLOCTEXT("MatineeActorDetails", "OpenMatinee", "Open Matinee") )
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0, 5, 10, 5)
		[
			SNew(SButton)
			.ContentPadding(3)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked( this, &FMatineeActorDetails::OnOpenMatineeForActor )
			.Text( NSLOCTEXT("MatineeActorDetails", "OpenMatinee", "Open Matinee") )
		]
	];
}

FReply FMatineeActorDetails::OnOpenMatineeForActor()
{
	if( MatineeActor.IsValid() )
	{
		GEditor->OpenMatinee(MatineeActor.Get());
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
