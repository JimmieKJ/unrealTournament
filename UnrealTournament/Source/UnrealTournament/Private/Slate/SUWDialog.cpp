// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWDialog.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWDialog::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	checkSlow(PlayerOwner != NULL);

	OnDialogResult = InArgs._OnDialogResult;

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	// Calculate the action position;
	ActualSize = InArgs._DialogSize;
	if (InArgs._bDialogSizeIsRelative)
	{
		ActualSize *= ViewportSize;
	}
	FVector2D Pos = ViewportSize * InArgs._DialogPosition;
	ActualPosition = Pos - (ActualSize * InArgs._DialogAnchorPoint);

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Canvas,SCanvas)

			// We use a Canvas Slot to position and size the dialog.  
			+SCanvas::Slot()
			.Position(ActualPosition)
			.Size(ActualSize)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[


				// This is our primary overlay.  It controls all of the various elements of the dialog.  This is not
				// the content overlay.  This comes below.
				SNew(SOverlay)				

				// this is the background image
				+SOverlay::Slot()							
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Dialog.Background"))
					]
				]

				// This will define a vertical box that holds the various components of the dialog box.
				+ SOverlay::Slot()							
				[
					SNew(SVerticalBox)

					// The title bar
					+ SVerticalBox::Slot()						
					.Padding(0.0f, 5.0f, 0.0f, 5.0f)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(InArgs._DialogTitle)
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.Title.TextStyle")
					]

					// The content section
					+ SVerticalBox::Slot()													
					.Padding(InArgs._ContentPadding.X, InArgs._ContentPadding.Y, InArgs._ContentPadding.X, InArgs._ContentPadding.Y)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
						[
							// Add an Overlay
							SAssignNew(DialogContent, SOverlay)
						]
					]

					// The ButtonBar
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Bottom)
					.HAlign(HAlign_Right)
					.Padding(5.0f, 5.0f, 5.0f, 5.0f)
					[
						BuildButtonBar(InArgs._ButtonMask)
					]
				]
			]
		];
}


void SUWDialog::BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32 &ButtonCount)
{
	if (Bar.IsValid())
	{
		Bar->AddSlot(ButtonCount,0)
			.HAlign(HAlign_Fill)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
				.Text(ButtonText.ToString())
				.OnClicked(this, &SUWDialog::OnButtonClick, ButtonID)
			];

		ButtonCount++;
	};
}

TSharedRef<class SWidget> SUWDialog::BuildButtonBar(uint16 ButtonMask)
{
	uint32 ButtonCount = 0;

	SAssignNew(ButtonBar,SUniformGridPanel)
		.SlotPadding(FMargin(0.0f,0.0f, 10.0f, 10.0f));

	if ( ButtonBar.IsValid() )
	{
		if (ButtonMask & UTDIALOG_BUTTON_OK)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","OKButton","OK"),					UTDIALOG_BUTTON_OK,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_CANCEL)	BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","CancelButton","Cancel"),			UTDIALOG_BUTTON_CANCEL,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_YES)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","YesButton","Yes"),				UTDIALOG_BUTTON_YES,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_NO)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","NoButton","No"),					UTDIALOG_BUTTON_NO,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_HELP)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","HelpButton","Help"),				UTDIALOG_BUTTON_HELP,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_RECONNECT) BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","ReconnectButton","Reconnect"),	UTDIALOG_BUTTON_RECONNECT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_EXIT)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","ExitButton","Exit"),				UTDIALOG_BUTTON_EXIT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_QUIT)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","QuitButton","Quit"),				UTDIALOG_BUTTON_QUIT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_VIEW)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","ViewButton","View"),				UTDIALOG_BUTTON_QUIT,ButtonCount);
		
	}

	return ButtonBar.ToSharedRef();
}


FReply SUWDialog::OnButtonClick(uint16 ButtonID)
{
	OnDialogResult.ExecuteIfBound(SharedThis(this), ButtonID);
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}


/******************** ALL OF THE HACKS NEEDED TO MAINTAIN WINDOW FOCUS *********************************/

void SUWDialog::OnDialogOpened()
{
	GameViewportWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}

void SUWDialog::OnDialogClosed()
{
	FSlateApplication::Get().SetKeyboardFocus(GameViewportWidget);
}

bool SUWDialog::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUWDialog::OnKeyboardFocusReceived(const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent)
{
	return FReply::Handled()
		.ReleaseMouseCapture()
		.LockMouseToWidget(SharedThis(this));

}

FReply SUWDialog::OnKeyUp(const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent)
{
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		OnButtonClick(UTDIALOG_BUTTON_CANCEL);
	}

	return FReply::Unhandled();
}

FReply SUWDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent)
{
	return FReply::Handled();
}


FReply SUWDialog::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SUWDialog::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

TSharedRef<SWidget> SUWDialog::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(*InItem.Get())
		];
}

#endif