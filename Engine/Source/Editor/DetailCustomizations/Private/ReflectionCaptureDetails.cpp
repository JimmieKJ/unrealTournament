// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ReflectionCaptureDetails.h"

#define LOCTEXT_NAMESPACE "ReflectionCaptureDetails"



TSharedRef<IDetailCustomization> FReflectionCaptureDetails::MakeInstance()
{
	return MakeShareable( new FReflectionCaptureDetails );
}

void FReflectionCaptureDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetDetailsView().GetSelectedObjects();

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			AReflectionCapture* CurrentCaptureActor = Cast<AReflectionCapture>(CurrentObject.Get());
			if (CurrentCaptureActor != NULL)
			{
				ReflectionCapture = CurrentCaptureActor;
				break;
			}
		}
	}

	DetailLayout.EditCategory("ReflectionCapture")
	.AddCustomRow(NSLOCTEXT("ReflectionCaptureDetails", "UpdateReflectionCaptures", "Update Captures"))
	.NameContent()
	[
		SNullWidget::NullWidget
	]
	.ValueContent()
	[
		SNew(SBox)
		.WidthOverride(125)
		[
			SNew(SButton)
			.ContentPadding(3)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FReflectionCaptureDetails::OnUpdateReflectionCaptures)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ReflectionCaptureDetails", "UpdateReflectionCaptures", "Update Captures"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	];
}

FReply FReflectionCaptureDetails::OnUpdateReflectionCaptures()
{
	if( ReflectionCapture.IsValid() )
	{
		GEditor->UpdateReflectionCaptures();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
