// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWMessageBox.h"
#include "SUWindowsStyle.h"

void SUWMessageBox::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	FVector2D ViewportSize;
	PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);

	FVector2D WindowSize = FVector2D(250, 150);	// PICK OUT OF MY BUTT -- Should look fine :)
	FVector2D WindowPosition = FVector2D( (ViewportSize.X * 0.5), (ViewportSize.Y * 0.5));

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Canvas,SCanvas)
			+SCanvas::Slot()
			.Position(WindowPosition)
			.Size(WindowSize)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
					]
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(0.0f,5.0f,0.0f,5.0f)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(InArgs._MessageTitle)
					]
					+SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(InArgs._MessageText)
						.AutoWrapText(true)
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Bottom)
					.HAlign(HAlign_Right)
					.Padding(5.0f,5.0f,5.0f,5.0f)
					[
						BuildButtonBar(InArgs._ButtonsMask)
					]

				]
			]
		];
}

void SUWMessageBox::BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32 &ButtonCount)
{
	if (Bar.IsValid())
	{
		Bar->AddSlot(ButtonCount,0)
			.HAlign(HAlign_Fill)
//			.Padding(0.0f,0.0f,5.0f,0.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
				.Text(ButtonText.ToString())
				.OnClicked(this, &SUWMessageBox::OnButtonClick, ButtonID)
			];

		ButtonCount++;
	};
}

TSharedRef<class SWidget> SUWMessageBox::BuildButtonBar(uint16 ButtonMask)
{
	uint32 ButtonCount = 0;

	TSharedPtr<SUniformGridPanel> Bar;
	SAssignNew(Bar,SUniformGridPanel)
		.SlotPadding(FMargin(0.0f,0.0f, 10.0f, 10.0f));

	if ( Bar.IsValid() )
	{
		if (ButtonMask & UTDIALOG_BUTTON_OK) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","OKButton","OK"), UTDIALOG_BUTTON_OK,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_CANCEL) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","CancelButton","Cancel"), UTDIALOG_BUTTON_CANCEL,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_YES) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","YesButton","Yes"), UTDIALOG_BUTTON_YES,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_NO) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","NoButton","No"), UTDIALOG_BUTTON_NO,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_HELP) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","HelpButton","Help"), UTDIALOG_BUTTON_HELP,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_RECONNECT) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","ReconnectButton","Reconnect"), UTDIALOG_BUTTON_RECONNECT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_EXIT) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","ExitButton","Exit"), UTDIALOG_BUTTON_EXIT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_QUIT) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","QuitButton","Quit"), UTDIALOG_BUTTON_QUIT,ButtonCount);
	}

	return Bar.ToSharedRef();
}

FReply SUWMessageBox::OnButtonClick(uint16 ButtonID)
{
	PlayerOwner->MessageBoxDialogResult(ButtonID);
	return FReply::Handled();

}

/******************** ALL OF THE HACKS NEEDED TO MAINTAIN WINDOW FOCUS *********************************/
	
void SUWMessageBox::OnDialogOpened()
{
	GameViewportWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}

void SUWMessageBox::OnDialogClosed()
{
	FSlateApplication::Get().SetKeyboardFocus(GameViewportWidget);
}


bool SUWMessageBox::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUWMessageBox::OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	return FReply::Handled()
				.ReleaseMouseCapture()
				.LockMouseToWidget(SharedThis(this));

}

FReply SUWMessageBox::OnKeyUp( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}


FReply SUWMessageBox::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	return FReply::Handled();
}


FReply SUWMessageBox::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Handled();
}

FReply SUWMessageBox::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Handled();
}

