// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWLoginDialog.h"
#include "SUWMessageBox.h"
#include "SUWindowsStyle.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

void SUWLoginDialog::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	checkSlow(PlayerOwner != NULL);
	FVector2D ViewportSize;
	PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);

	FString UserID = InArgs._UserIDText;
	if (UserID.IsEmpty())
	{
		// Attemtp to lookup the current user id.
		UserID = PlayerOwner->GetAccountName();
	}

	OnDialogResult = InArgs._OnDialogResult;

	FVector2D DesignedRez(1920,1080);
	FVector2D DesignedSize(500, 800);
	FVector2D Pos = (DesignedRez * 0.5f) - (DesignedSize * 0.5f);
	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(Canvas, SCanvas)

		// We use a Canvas Slot to position and size the dialog.  
		+ SCanvas::Slot()
		.Position(Pos)
		.Size(DesignedSize)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		[
			// This is our primary overlay.  It controls all of the various elements of the dialog.  This is not
			// the content overlay.  This comes below.
			SNew(SOverlay)

			// this is the background image
			+ SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(48)
					[
						SNew(SCanvas)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(DesignedSize.Y - 48)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UT.Login.Dialog.Background"))
					]
				]

			]

			// This will define a vertical box that holds the various components of the dialog box.
			+ SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(110)
						.HeightOverride(126)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.Login.EpicLogo"))
						]
					]
				]
			]

			+ SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Right)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(10.0f, 56.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.Login.EmptyButton")
						.OnClicked(this, &SUWLoginDialog::OnCloseClick)
						[
							SNew(SBox)
							.WidthOverride(32)
							.HeightOverride(32)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Exit"))
							]
						]
					]
				]
			]

			+SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(10.0f,150.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Login","TeaserText","Enter the Field of Battle!"))
					.TextStyle(SUWindowsStyle::Get(), "UT.Login.TextStyle")
				]

				// User Name

				+SVerticalBox::Slot()
				.Padding(10.0f, 48.0f,10.0f,0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(425.0f)
						.HeightOverride(80.0)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							.VAlign(VAlign_Fill)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Login.Editbox.Background"))
							]
							+ SOverlay::Slot()
							.VAlign(VAlign_Bottom)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.Padding(10.0f,0.0f,10.0f,0.0f)
								.AutoHeight()
								[
									SAssignNew(UserEditBox, SEditableTextBox)
									.MinDesiredWidth(425)
									.Text(FText::FromString(UserID))
									.Style(SUWindowsStyle::Get(), "UT.Login.Editbox")
								]
							]
							+ SOverlay::Slot()
							.VAlign(VAlign_Top)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("Login", "Email", "Email"))
									.TextStyle(SUWindowsStyle::Get(), "UT.Login.Label.TextStyle")
								]
							]
						]
					]
				]

				// Error Message
				+SVerticalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(10.0f,0.0f,10.f,0.0f)
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(DesignedSize.X * 0.9)
						[
							SNew(SRichTextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Login.Error.TextStyle")
							.Justification(ETextJustify::Center)
							.DecoratorStyleSet( &SUWindowsStyle::Get() )
							.AutoWrapText( true )
							.WrapTextAt(DesignedSize.X * 0.9)
							.Text(InArgs._ErrorText)
						]
					]
				]

				// Password

				+SVerticalBox::Slot()
				.Padding(10.0f, 40.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(425.0f)
						.HeightOverride(80.0)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							.VAlign(VAlign_Fill)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Login.Editbox.Background"))
							]
							+ SOverlay::Slot()
							.VAlign(VAlign_Bottom)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								.AutoHeight()
								[
									SAssignNew(PassEditBox, SEditableTextBox)
									.IsPassword(true)
									.OnTextCommitted(this, &SUWLoginDialog::OnTextCommited)
									.MinDesiredWidth(425)
									.Style(SUWindowsStyle::Get(), "UT.Login.Editbox")
								]
							]
							+ SOverlay::Slot()
							.VAlign(VAlign_Top)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("Login", "Password", "Password"))
									.TextStyle(SUWindowsStyle::Get(), "UT.Login.Label.TextStyle")
								]
							]
						]
					]
				]


				// Password Recovery

				+SVerticalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Login.EmptyButton")
							.OnClicked(this, &SUWLoginDialog::OnForgotPasswordClick)
							.ContentPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Login", "PasswordRecovery", "Forgot Your Password?"))
								.TextStyle(SUWindowsStyle::Get(), "UT.Login.EmptyButton.TextStyle")
							]
						]
					]
				]

				// Sign Button

				+SVerticalBox::Slot()
				.Padding(10.0f, 32.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(390.0f)
						.HeightOverride(44.0)
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Login.Button")
							.OnClicked(this, &SUWLoginDialog::OnSignInClick)
							.ContentPadding(FMargin(25.0, 0.0, 25.0, 5.0))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("Login", "LoginButtonText", "Sign In"))
									.TextStyle(SUWindowsStyle::Get(), "UT.Login.Button.TextStyle")
								]
							]
						]
					]
				]

				// New Account

				+SVerticalBox::Slot()
				.Padding(10.0f, 16.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Login", "NoAccountMsg", "Need an Epic Games Account?"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Login.TextStyle")
					]
				]

				// Click Here

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.Login.EmptyButton")
						.OnClicked(this, &SUWLoginDialog::OnNewAccountClick)
						.ContentPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("Login", "SignUp", "Sign Up!"))
							.TextStyle(SUWindowsStyle::Get(), "UT.Login.EmptyButton.TextStyle")
						]
					]
				]
			]
		]
	];

}
void SUWLoginDialog::SetInitialFocus()
{
	if (UserEditBox->GetText().IsEmpty())
	{
		FSlateApplication::Get().SetKeyboardFocus(UserEditBox, EKeyboardFocusCause::SetDirectly);
	}
	else
	{
		FSlateApplication::Get().SetKeyboardFocus(PassEditBox, EKeyboardFocusCause::SetDirectly);
	}

}


FString SUWLoginDialog::GetEpicID()
{
	return UserEditBox->GetText().ToString();
}

FString SUWLoginDialog::GetPassword()
{
	return PassEditBox->GetText().ToString();
}


FReply SUWLoginDialog::OnNewAccountClick()
{
	FString Error;
	FPlatformProcess::LaunchURL(TEXT("https://forums.unrealtournament.com/download.php?return=http://www.unrealtournament.com"), NULL, &Error);
	if (Error.Len() > 0)
	{
		PlayerOwner->MessageBox(NSLOCTEXT("SUWindowsDesktop", "HTTPBrowserError", "Error Launching Browser"), FText::FromString(Error));
	}
	return FReply::Handled();
}

FReply SUWLoginDialog::OnForgotPasswordClick()
{
	FString Error;
	FPlatformProcess::LaunchURL(TEXT("https://accounts.unrealtournament.com/requestPasswordReset"), NULL, &Error);
	if (Error.Len() > 0)
	{
		PlayerOwner->MessageBox(NSLOCTEXT("SUWindowsDesktop", "HTTPBrowserError", "Error Launching Browser"),FText::FromString(Error) );
	}
	return FReply::Handled();

	
}

FReply SUWLoginDialog::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCloseClick();
	}

	return FReply::Unhandled();
}


FReply SUWLoginDialog::OnCloseClick()
{
	OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
	return FReply::Handled();
}


void SUWLoginDialog::OnTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		OnSignInClick();
	}
}

FReply SUWLoginDialog::OnSignInClick()
{

	if ( GetEpicID() != TEXT("") && GetPassword() != TEXT("") )  
	{
		OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_OK);
	}

	return FReply::Handled();
}


FReply SUWLoginDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		if ( UserEditBox->HasKeyboardFocus() )
		{
			FSlateApplication::Get().SetKeyboardFocus(PassEditBox, EKeyboardFocusCause::SetDirectly);
		}
		else
		{
			FSlateApplication::Get().SetKeyboardFocus(UserEditBox, EKeyboardFocusCause::SetDirectly);
		}
	}
	return FReply::Handled();


}


#endif