// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MatineeModule.h"
#include "MatineeFilterButton.h"

void SMatineeFilterButton::Construct(const FArguments& InArgs)
{
	OnCheckStateChanged = InArgs._OnCheckStateChanged;
	OnContextMenuOpening = InArgs._OnContextMenuOpening;

	ChildSlot
	[
		SNew(SCheckBox)
		.IsChecked(InArgs._IsChecked)
		.OnCheckStateChanged(InArgs._OnCheckStateChanged)
		.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
		.Padding(3.0f)
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "Matinee.Filters.Text")
			.Text(InArgs._Text)
		]
	];
}

FReply SMatineeFilterButton::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		// Pretend as if the checkbox was clicked so that the current tab will be selected
		OnCheckStateChanged.ExecuteIfBound( ECheckBoxState::Checked );

		if ( OnContextMenuOpening.IsBound() )
		{
			// Get the context menu content. If null, don't open a menu.
			TSharedPtr<SWidget> MenuContent = OnContextMenuOpening.Execute();
			if( MenuContent.IsValid() )
			{
				FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

				FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent.ToSharedRef(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}
