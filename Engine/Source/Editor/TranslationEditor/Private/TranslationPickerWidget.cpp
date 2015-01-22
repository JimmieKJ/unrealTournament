// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"
#include "TranslationPickerWidget.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

STranslationWidgetPicker::~STranslationWidgetPicker()
{
	// kill the picker window as well if this widget is going away - that way we dont get dangling refs to the property
	if (PickerWindow.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RequestDestroyWindow(PickerWindow.Pin().ToSharedRef());
		PickerWindow.Reset();
		PickerWindowWidget.Reset();
	}
}

void STranslationWidgetPicker::Construct(const FArguments& InArgs)
{
	// Mimicking a toolbar button look

	// Icon for the picker widget button
	TSharedRef< SWidget > IconWidget =
		SNew(SImage)
		.Image(FEditorStyle::GetBrush("TranslationEditor.TranslationPicker"));

	// Style settings
	FName StyleSet = FEditorStyle::GetStyleSetName();
	FName StyleName = "Toolbar";

	FText ToolTip = LOCTEXT("TranslationPickerTooltip", "Open the Translation Picker");

	// Create the content for our button
	TSharedRef< SWidget > ButtonContent =

		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			// Icon image
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)	// Center the icon horizontally, so that large labels don't stretch out the artwork
			[
				IconWidget
			]

			// Label text
			+ SVerticalBox::Slot().AutoHeight()
				.HAlign(HAlign_Center)	// Center the label text horizontally
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TranslationPicker", "Translation Picker"))
					.TextStyle(FEditorStyle::Get(), FName("ToolBar.Label"))	// Smaller font for tool tip labels
					.ShadowOffset(FVector2D::UnitVector)
				]
		];

	FName CheckboxStyle = ISlateStyle::Join(StyleName, ".SToolBarButtonBlock.CheckBox.Padding");

	ChildSlot
		[
			// Create a check box
			SNew(SCheckBox)

			// Use the tool bar style for this check box
			.Style(FEditorStyle::Get(), "ToolBar.ToggleButton")

			// User will have set the focusable attribute for the block, honor it
			.IsFocusable(false)

			// Pass along the block's tool-tip string
			.ToolTip(SNew(SToolTip).Text(ToolTip))
			[
				ButtonContent
			]

			// Bind the button's "on checked" event to our object's method for this
			.OnCheckStateChanged(this, &STranslationWidgetPicker::OnCheckStateChanged)


				// Bind the check box's "checked" state to our user interface action
				.IsChecked(this, &STranslationWidgetPicker::OnIsChecked)

				.Padding(FEditorStyle::Get().GetMargin(CheckboxStyle))
		];
}

FReply STranslationWidgetPicker::OnClicked()
{
	// Not picking previously, launch a picker window
	if (!PickerWindow.IsValid())
	{
		TSharedRef<SWindow> NewWindow = SWindow::MakeCursorDecorator();
		NewWindow->MoveWindowTo(FSlateApplication::Get().GetCursorPos());
		PickerWindow = NewWindow;

		NewWindow->SetContent(
			SAssignNew(PickerWindowWidget, STranslationPickerFloatingWindow)
			.ParentWindow(NewWindow)
			);

		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		check(RootWindow.IsValid());
		FSlateApplication::Get().AddWindowAsNativeChild(NewWindow, RootWindow.ToSharedRef());
	}
	// Was already picking previously, destroy picker window
	else
	{
		FSlateApplication::Get().RequestDestroyWindow(PickerWindow.Pin().ToSharedRef());
		PickerWindow.Pin().Reset();
	}

	return FReply::Handled();
}

bool STranslationWidgetPicker::IsPicking() const
{
	if (PickerWindowWidget.IsValid())
	{
		return true;
	}

	return false;
}

ECheckBoxState STranslationWidgetPicker::OnIsChecked() const
{
	return IsPicking() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void STranslationWidgetPicker::OnCheckStateChanged(const ECheckBoxState NewCheckedState)
{
	OnClicked();
}




#undef LOCTEXT_NAMESPACE



