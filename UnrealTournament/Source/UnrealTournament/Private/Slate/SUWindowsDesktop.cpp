// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsStyle.h"

void SUWindowsDesktop::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;

	TWeakPtr<SMenuAnchor> MenuAnchorPtr;
		
	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.MaxHeight(26)
			.HAlign(HAlign_Fill)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
					]
				]
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(10.0f,0.0f,0.0f,0.0f))
					.HAlign(HAlign_Fill)
					[
						SAssignNew(FileHeader, SComboButton)
						.Method(SMenuAnchor::UseCurrentWindow)
						.HasDownArrow(false)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_File","File").ToString())
						]
						.MenuContent()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SButton)
								.ContentPadding(FMargin(10.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_OpenIP", "Open IP").ToString())
								.OnClicked(this, &SUWindowsDesktop::OnOpenClicked)
						
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SButton)
								.ContentPadding(FMargin(10.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_ExitGame", "Exit Game").ToString())
								.OnClicked(this, &SUWindowsDesktop::OnExitClicked)
							]
						]
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(10.0f,0.0f,0.0f,0.0f))
					.HAlign(HAlign_Fill)
					[
						SAssignNew(OptionsHeader, SComboButton)
						.Method(SMenuAnchor::UseCurrentWindow)
						.HasDownArrow(false)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_Options","Options").ToString())
						]
						.MenuContent()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SButton)
								.ContentPadding(FMargin(40.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_Video", "Video").ToString())
								.OnClicked(this, &SUWindowsDesktop::OnVideoOptionsClicked)
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SButton)
								.ContentPadding(FMargin(10.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_Audio", "Audio").ToString())
								.OnClicked(this, &SUWindowsDesktop::OnAudioOptionsClicked)
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SButton)
								.ContentPadding(FMargin(10.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_Controls", "Controls").ToString())
								.OnClicked(this, &SUWindowsDesktop::OnControlOptionsClicked)
							]

						]
					]

				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(STextBlock)
				.ShadowColorAndOpacity(FLinearColor::Black)
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FIntPoint(-1, 1))
				.Font(FSlateFontInfo("Veranda", 16)) 
				.Text(FText::FromString("Second Line...."))
			]
		];
}

void SUWindowsDesktop::OnMenuOpened()
{
	GameViewportWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}

void SUWindowsDesktop::OnMenuClosed()
{
	FSlateApplication::Get().SetKeyboardFocus(GameViewportWidget);
}

bool SUWindowsDesktop::SupportsKeyboardFocus() const
{
	return true;
}



FReply SUWindowsDesktop::OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	return FReply::Handled()
				.ReleaseMouseCapture()
				.LockMouseToWidget(SharedThis(this));

}

FReply SUWindowsDesktop::OnKeyUp( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		CloseMenus();
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SUWindowsDesktop::CloseMenus()
{
	if (PlayerOwner != NULL)
	{
		PlayerOwner->HideMenu();
	}

}


FReply SUWindowsDesktop::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	return FReply::Handled();
}


FReply SUWindowsDesktop::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Handled();
}

FReply SUWindowsDesktop::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	CloseMenus();
	return FReply::Handled();
}

FReply SUWindowsDesktop::OnOpenClicked()
{
	if ( FileHeader.IsValid() ) FileHeader->SetIsOpen(false,false);
	CloseMenus();
	return FReply::Handled();
}

FReply SUWindowsDesktop::OnExitClicked()
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		PlayerOwner->PlayerController->ConsoleCommand(TEXT("quit"));
	}
	return FReply::Handled();
}


FReply SUWindowsDesktop::OnVideoOptionsClicked()
{
	if ( OptionsHeader.IsValid() ) OptionsHeader->SetIsOpen(false,false);
	CloseMenus();
	return FReply::Handled();
}

FReply SUWindowsDesktop::OnAudioOptionsClicked()
{
	if ( OptionsHeader.IsValid() ) OptionsHeader->SetIsOpen(false,false);
	CloseMenus();
	return FReply::Handled();
}

FReply SUWindowsDesktop::OnControlOptionsClicked()
{
	if ( OptionsHeader.IsValid() ) OptionsHeader->SetIsOpen(false,false);
	CloseMenus();
	return FReply::Handled();
}
