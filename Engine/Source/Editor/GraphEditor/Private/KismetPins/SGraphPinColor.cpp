// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "NodeFactory.h"
#include "KismetPins/SGraphPinColor.h"
#include "SColorPicker.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"


void SGraphPinColor::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinColor::GetDefaultValueWidget()
{
	return SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("FilledBorder") )
		.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
		.Padding(1)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SColorBlock )
				.Color( this, &SGraphPinColor::GetColor )
				.IgnoreAlpha(true)
				.OnMouseButtonDown( this, &SGraphPinColor::OnColorBoxClicked )
			]
		];
}

FReply SGraphPinColor::OnColorBoxClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		SelectedColor = GetColor();
		TArray<FLinearColor*> LinearColorArray;
		LinearColorArray.Add(&SelectedColor);

		FColorPickerArgs PickerArgs;
		PickerArgs.bIsModal = true;
		PickerArgs.ParentWidget = AsShared();
		PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
		PickerArgs.LinearColorArray = &LinearColorArray;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SGraphPinColor::OnColorCommitted);
		PickerArgs.bUseAlpha = true;

		OpenColorPicker(PickerArgs);

		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FLinearColor SGraphPinColor::GetColor() const
{
	FString ColorString = GraphPinObj->GetDefaultAsString();
	FLinearColor PinColor;

	// Ensure value is sensible
	if (!PinColor.InitFromString(ColorString)) 
	{
		PinColor = FLinearColor::Black;
	}
	return PinColor;
}

void SGraphPinColor::OnColorCommitted(FLinearColor InColor)
{
	// Update pin object
	FString ColorString = InColor.ToString();

	if(GraphPinObj->GetDefaultAsString() != ColorString)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeColorPinValue", "Change Color Pin Value" ) );
		GraphPinObj->Modify();
		
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, ColorString);
		OwnerNodePtr.Pin()->UpdateGraphNode();
	}
}