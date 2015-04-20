// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWDialog.h"
#include "SUWindowsStyle.h"
#include "Engine/UserInterfaceSettings.h"


#if !UE_SERVER

void SUWDialog::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	checkSlow(PlayerOwner != NULL);

	TabStop = 0;

	OnDialogResult = InArgs._OnDialogResult;

	// Calculate the size.  If the size is relative, then scale it against the designed by resolution as the 
	// DPI Scale planel will take care of the rest.
	FVector2D DesignedRez(1920,1080);
	ActualSize = InArgs._DialogSize;
	if (InArgs._bDialogSizeIsRelative)
	{
		ActualSize *= DesignedRez;
	}

	// Now we have to center it.  The tick here is we have to scale the current viewportSize UP by scale used in the DPI panel other
	// we can't position properly.
	
	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	float DPIScale = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
	ViewportSize = ViewportSize / DPIScale;
	ActualPosition = (ViewportSize * InArgs._DialogPosition) - (ActualSize * InArgs._DialogAnchorPoint);

	TSharedPtr<SWidget> FinalContent;

	if ( InArgs._IsScrollable )
	{
		SAssignNew(FinalContent, SScrollBox)
		.Style(SUWindowsStyle::Get(),"UT.ScrollBox.Borderless")
		+ SScrollBox::Slot()

		.Padding(FMargin(0.0f, 15.0f, 0.0f, 15.0f))
		[
			// Add an Overlay
			SAssignNew(DialogContent, SOverlay)
		];
	}
	else
	{
		FinalContent = SAssignNew(DialogContent, SOverlay);
	}

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[

		SNew(SOverlay)
		+SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SImage)
			.Image(InArgs._bShadow ? SUWindowsStyle::Get().GetBrush("UT.TopMenu.Shadow") : new FSlateNoResource)
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[

			SAssignNew(Canvas, SCanvas)

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
						.Image(SUWindowsStyle::Get().GetBrush("UT.DialogBox.Background"))
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
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SAssignNew(DialogTitle, STextBlock)
						.Text(InArgs._DialogTitle)
						.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.TitleTextStyle")
					]

					// The content section
					+ SVerticalBox::Slot()										
					.Padding(InArgs._ContentPadding.X, InArgs._ContentPadding.Y, InArgs._ContentPadding.X, InArgs._ContentPadding.Y)
					[
						FinalContent.ToSharedRef()
					]

					// The ButtonBar
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Bottom)
					.Padding(5.0f, 5.0f, 5.0f, 5.0f)
					[
						SNew(SBox)
						.HeightOverride(48)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.FillWidth(1.0)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.HAlign(HAlign_Right)
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UT.Dialog.RightButtonBackground"))
										]
									]
									+ SOverlay::Slot()
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.HAlign(HAlign_Right)
										[
											BuildButtonBar(InArgs._ButtonMask)
										]

									]
								]

							]
							+ SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Left)
								.Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									BuildCustomButtonBar()
								]
							]
						]
					]
				]
			]
		]
	];
}



void SUWDialog::BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32 &ButtonCount)
{
	TSharedPtr<SButton> Button;
	if (Bar.IsValid())
	{
		Bar->AddSlot(ButtonCount,0)
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HeightOverride(48)
				[
					SAssignNew(Button, SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
					.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.Text(ButtonText.ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
					.OnClicked(this, &SUWDialog::OnButtonClick, ButtonID)
				]
			];

		ButtonCount++;
		if (Button.IsValid())
		{
			TabTable.AddUnique(Button);
			ButtonMap.Add(ButtonID, Button);
		}
	};
}

TSharedRef<class SWidget> SUWDialog::BuildButtonBar(uint16 ButtonMask)
{
	uint32 ButtonCount = 0;

	ButtonMap.Empty();

	SAssignNew(ButtonBar,SUniformGridPanel)
		.SlotPadding(FMargin(0.0f,0.0f, 10.0f, 10.0f));

	if ( ButtonBar.IsValid() )
	{
		if (ButtonMask & UTDIALOG_BUTTON_OK)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","OKButton","OK"),					UTDIALOG_BUTTON_OK,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_PLAY)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","PlayButton","PLAY"),				UTDIALOG_BUTTON_PLAY,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_LAN)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","LanButton","START LAN GAME"),		UTDIALOG_BUTTON_LAN,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_CANCEL)	BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","CancelButton","CANCEL"),			UTDIALOG_BUTTON_CANCEL,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_YES)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","YesButton", "YES"),				UTDIALOG_BUTTON_YES, ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_NO)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","NoButton","NO"),					UTDIALOG_BUTTON_NO,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_YESCLEAR)	BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","YesClearButton","CLEAR DUPES"),	UTDIALOG_BUTTON_YESCLEAR, ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_HELP)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","HelpButton","HELP"),				UTDIALOG_BUTTON_HELP,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_RECONNECT) BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","ReconnectButton","RECONNECT"),	UTDIALOG_BUTTON_RECONNECT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_EXIT)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","ExitButton","EXIT"),				UTDIALOG_BUTTON_EXIT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_QUIT)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","QuitButton","QUIT"),				UTDIALOG_BUTTON_QUIT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_VIEW)		BuildButton(ButtonBar, NSLOCTEXT("SUWDialog","ViewButton","VIEW"),				UTDIALOG_BUTTON_VIEW,ButtonCount);
	}

	return ButtonBar.ToSharedRef();
}

TSharedRef<class SWidget> SUWDialog::BuildCustomButtonBar()
{
	TSharedPtr<SCanvas> C;
	SAssignNew(C,SCanvas);
	return C.ToSharedRef();
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
	FSlateApplication::Get().ClearKeyboardFocus();
	FSlateApplication::Get().ClearUserFocus(0);
}

bool SUWDialog::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUWDialog::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent)
{
	return FReply::Handled()
		.ReleaseMouseCapture();
}

FReply SUWDialog::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnButtonClick(UTDIALOG_BUTTON_CANCEL);
	}

	return FReply::Unhandled();
}

FReply SUWDialog::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
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
			.Text(*InItem.Get())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		];
}

FReply SUWDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		int32 NewStop = (InKeyEvent.IsLeftShiftDown() || InKeyEvent.IsRightShiftDown()) ? -1 : 1;
		if (TabStop + NewStop < 0)
		{
			TabStop = TabTable.Num() - 1;
		}
		else
		{
			TabStop = (TabStop + NewStop) % TabTable.Num();
		}

		FSlateApplication::Get().SetKeyboardFocus(TabTable[TabStop], EKeyboardFocusCause::Keyboard);	
	}
	return FReply::Handled();


}


#endif