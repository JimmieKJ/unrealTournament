// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"
#include "TranslationPickerFloatingWindow.h"
#include "Editor/Documentation/Public/SDocumentationToolTip.h"
#include "TranslationPickerEditWindow.h"
#include "Editor/TranslationEditor/Private/TranslationPickerWidget.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

void STranslationPickerFloatingWindow::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	WindowContents = SNew(SToolTip);

	ChildSlot
	[
		WindowContents.ToSharedRef()
	];
}

void STranslationPickerFloatingWindow::GetTextFromChildWidgets(TSharedRef<SWidget> Widget)
{
	FChildren* Children = Widget->GetChildren();

	TArray<TSharedRef<SWidget>> ChildrenArray;
	for (int ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
	{
		ChildrenArray.Add(Children->GetChildAt(ChildIndex));
	}

	for (int ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
	{
		// Pull out any FText from this child widget
		GetTextFromWidget(Children->GetChildAt(ChildIndex));
		// Recursively search this child's children for their FText
		GetTextFromChildWidgets(Children->GetChildAt(ChildIndex));
	}
}

void STranslationPickerFloatingWindow::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(FSlateApplication::Get().GetCursorPos(), FSlateApplication::Get().GetInteractiveTopLevelWindows(), true);

	if (Path.IsValid())
	{
		// If the path of widgets we're hovering over changed since last time (or if this is the first tick and LastTickHoveringWidgetPath hasn't been set yet)
		if (!LastTickHoveringWidgetPath.IsValid() || LastTickHoveringWidgetPath.ToWidgetPath().ToString() != Path.ToString())
		{
			// Clear all previous text and widgets
			PickedTexts.Empty();

			// Search everything under the cursor for any FText we know how to parse
			for (int32 PathIndex = Path.Widgets.Num() - 1; PathIndex >= 0; PathIndex--)
			{
				// General Widget case
				TSharedRef<SWidget> PathWidget = Path.Widgets[PathIndex].Widget;
				GetTextFromWidget(PathWidget);

				// Tooltip case
				TSharedPtr<IToolTip> Tooltip = PathWidget->GetToolTip();
				//FText TooltipDescription = FText::GetEmpty();

				if (Tooltip.IsValid() && !Tooltip->IsEmpty())
				{
					GetTextFromWidget(Tooltip->AsWidget());
				}

				// Currently LocateWindowUnderMouse doesn't return hit-test invisible widgets
				// So recursively search all child widgets of the deepest widget in our path in case there is hit-test invisible text in the deeper children
				// GetTextFromWidget prevents adding the same FText twice, so shouldn't be any harm
				if (PathIndex == (Path.Widgets.Num() - 1))
				{
					GetTextFromChildWidgets(PathWidget);
				}
			}

			TSharedRef<SVerticalBox> TextsBox = SNew(SVerticalBox);

			// Add a new Translation Picker Edit Widget for each picked text
			for (FText PickedText : PickedTexts)
			{
				TextsBox->AddSlot()
					.AutoHeight()
					.Padding(FMargin(5))
					[
						SNew(SBorder)
						[
							SNew(STranslationPickerEditWidget)
							.PickedText(PickedText)
							.bAllowEditing(false)
						]
					];
			}

			WindowContents->SetContentWidget(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0)
				.AutoHeight()
				.Padding(FMargin(5))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FInternationalization::Get().GetCurrentCulture()->GetDisplayName()))
					.Justification(ETextJustify::Center)
				]
			+ SVerticalBox::Slot()
				.Padding(0)
				.FillHeight(1)
				.Padding(FMargin(5))
				[
					SNew(SScrollBox)
					.Orientation(EOrientation::Orient_Vertical)
					.ScrollBarAlwaysVisible(true)
					+SScrollBox::Slot()
						.Padding(FMargin(0))
						[
									TextsBox
						]
				]
			+ SVerticalBox::Slot()
				.Padding(0)
				.AutoHeight()
				.Padding(FMargin(5))
				[
					SNew(STextBlock)
					.Text(PickedTexts.Num() >= 1 ? LOCTEXT("TranslationPickerEscToEdit", "Press Esc to edit translation(s)") : LOCTEXT("TranslationPickerHoverToViewEditEscToQuit", "Hover over text to view/edit translation, or press Esc to quit"))
					.Justification(ETextJustify::Center)
				]
			);
		}
	}

	// kind of a hack, but we need to maintain keyboard focus otherwise we wont get our keypress to 'pick'
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::SetDirectly);
	if (ParentWindow.IsValid())
	{
		FVector2D WindowSize = ParentWindow.Pin()->GetSizeInScreen();
		FVector2D DesiredPosition = FSlateApplication::Get().GetCursorPos();
		DesiredPosition.X -= FSlateApplication::Get().GetCursorSize().X;
		DesiredPosition.Y += FSlateApplication::Get().GetCursorSize().Y;

		// Move to opposite side of the cursor than the tool tip, so they don't overlaps
		DesiredPosition.X -= WindowSize.X;

		// also kind of a hack, but this is the only way at the moment to get a 'cursor decorator' without using the drag-drop code path
		ParentWindow.Pin()->MoveWindowTo(DesiredPosition);
	}

	LastTickHoveringWidgetPath = FWeakWidgetPath(Path);
}

FText STranslationPickerFloatingWindow::GetTextFromWidget(TSharedRef<SWidget> Widget)
{
	FText OriginalText = FText::GetEmpty();

	STextBlock* TextBlock = (STextBlock*)&Widget.Get();

	// Have to parse the various widget types to find the FText
	if ((Widget->GetTypeAsString() == "STextBlock" && TextBlock))
	{
		OriginalText = TextBlock->GetText();
	}
	else if (Widget->GetTypeAsString() == "SToolTip")
	{
		SToolTip* ToolTipWidget = ((SToolTip*)&Widget.Get());
		if (ToolTipWidget != nullptr)
		{
			OriginalText = GetTextFromWidget(ToolTipWidget->GetContentWidget());
			if (OriginalText.IsEmpty())
			{
				OriginalText = ToolTipWidget->GetTextTooltip();
			}
		}
	}
	else if (Widget->GetTypeAsString() == "SDocumentationToolTip")
	{
		SDocumentationToolTip* DocumentationToolTip = (SDocumentationToolTip*)&Widget.Get();
		if (DocumentationToolTip != nullptr)
		{
			OriginalText = DocumentationToolTip->GetTextTooltip();
		}
	}
	else if (Widget->GetTypeAsString() == "SEditableText")
	{
		SEditableText* EditableText = (SEditableText*)&Widget.Get();
		if (EditableText != nullptr)
		{
			// Always return the hint text because that's the only thing that will be translatable
			OriginalText = EditableText->GetHintText();
		}
	}
	else if (Widget->GetTypeAsString() == "SRichTextBlock")
	{
		SRichTextBlock* RichTextBlock = (SRichTextBlock*)&Widget.Get();
		if (RichTextBlock != nullptr)
		{
			OriginalText = RichTextBlock->GetText();
		}
	}
	else if (Widget->GetTypeAsString() == "SMultiLineEditableText")
	{
		SMultiLineEditableText* MultiLineEditableText = (SMultiLineEditableText*)&Widget.Get();
		if (MultiLineEditableText != nullptr)
		{
			// Always return the hint text because that's the only thing that will be translatable
			OriginalText = MultiLineEditableText->GetHintText();
		}
	}
	else if (Widget->GetTypeAsString() == "SMultiLineEditableTextBox")
	{
		SMultiLineEditableTextBox* MultiLineEditableTextBox = (SMultiLineEditableTextBox*)&Widget.Get();
		if (MultiLineEditableTextBox != nullptr)
		{
			OriginalText = MultiLineEditableTextBox->GetText();
		}
	}
	else if (Widget->GetTypeAsString() == "SButton")
	{
		SButton* Button = (SButton*)&Widget.Get();

		// It seems like the LocateWindowUnderMouse() function will sometimes return an SButton but not the FText inside the button?
		// So try to find the first FText child of a button just in case
		FChildren* Children = Button->GetChildren();
		for (int ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			TSharedRef<SWidget> ChildWidget = Children->GetChildAt(ChildIndex);
			OriginalText = GetTextFromWidget(ChildWidget);
			if (!OriginalText.IsEmpty())
			{
				break;
			}
		}
	}
	
	if (!OriginalText.IsEmpty())
	{
		// Search the text from this widget's FText::Format history to find any source text
		TArray<FText> FormattingSourceTexts;
		OriginalText.GetSourceTextsFromFormatHistory(FormattingSourceTexts);

		for (FText FormattingSourceText : FormattingSourceTexts)
		{
			// Don't show the same text twice
			bool bAlreadyPicked = false;
			for (FText PickedText : PickedTexts)
			{
				if (FormattingSourceText.EqualTo(PickedText))
				{
					bAlreadyPicked = true;
					break;
				}
			}

			if (!bAlreadyPicked)
			{
				PickedTexts.Add(FormattingSourceText);
			}
		}
	}

	return OriginalText;
}

FReply STranslationPickerFloatingWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (PickedTexts.Num() > 0)
		// Open a different window to allow editing of the translation
		{
			TSharedRef<SWindow> NewWindow = SNew(SWindow)
				.Title(LOCTEXT("TranslationPickerEditWindowTitle", "Edit Translation(s)"))
				.CreateTitleBar(true)
				.SizingRule(ESizingRule::UserSized);

			TSharedRef<STranslationPickerEditWindow> EditWindow = SNew(STranslationPickerEditWindow)
				.ParentWindow(NewWindow)
				.PickedTexts(PickedTexts);

			NewWindow->SetContent(EditWindow);

			// Make this roughly the same size as the Edit Window, so when you press Esc to edit, the window is in basically the same size
			NewWindow->Resize(FVector2D(STranslationPickerEditWindow::DefaultEditWindowWidth, STranslationPickerEditWindow::DefaultEditWindowHeight));

			TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
			if (RootWindow.IsValid())
			{
				FSlateApplication::Get().AddWindowAsNativeChild(NewWindow, RootWindow.ToSharedRef());
			}
			else
			{
				FSlateApplication::Get().AddWindow(NewWindow);
			}

			FVector2D WindowSize = ParentWindow.Pin()->GetSizeInScreen();
			FVector2D DesiredPosition = FSlateApplication::Get().GetCursorPos();
			DesiredPosition.X -= FSlateApplication::Get().GetCursorSize().X;
			DesiredPosition.Y += FSlateApplication::Get().GetCursorSize().Y;

			// Open this new Edit window in the same position
			DesiredPosition.X -= WindowSize.X;

			NewWindow->MoveWindowTo(DesiredPosition);
		}

		TranslationPickerManager::ClosePickerWindow();
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE