// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTLoginDialog.h"
#include "SUTMessageBoxDialog.h"
#include "../SUTStyle.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

#include "SUTStyle.h"
#include "SlateBasics.h"
#include "SlateExtras.h"

const float MIN_LOGIN_VIEW_TIME=0.75f;

void SUTLoginDialog::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;

	LoginStartTime = PlayerOwner->GetWorld()->GetRealTimeSeconds();
	HideDelayTime = 0.0f;

	checkSlow(PlayerOwner != NULL);
	FVector2D ViewportSize;
	PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);

	FString UserID = InArgs._UserIDText;
	if (UserID.IsEmpty())
	{
		// Attempt to lookup the current user id.
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
						.Image(SUTStyle::Get().GetBrush("UT.Login.Dialog.Background"))
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
							.Image(SUTStyle::Get().GetBrush("UT.Login.EpicLogo"))
						]
					]
				]
			]

			+SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Center)
			[
				SAssignNew(InfoBox,SVerticalBox)
				.Visibility(this, &SUTLoginDialog::GetInfoVis)
				+ SVerticalBox::Slot()
				.Padding(10.0f,150.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Login","TeaserText","Enter the Field of Battle!"))
					.TextStyle(SUTStyle::Get(), "UT.Login.TextStyle")
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
								.Image(SUTStyle::Get().GetBrush("UT.Login.Editbox.Background"))
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
									.Style(SUTStyle::Get(), "UT.Login.Editbox")
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
									.TextStyle(SUTStyle::Get(), "UT.Login.Label.TextStyle")
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
							SAssignNew(ErrorText, SRichTextBlock)
							.TextStyle(SUTStyle::Get(), "UT.Login.Error.TextStyle")
							.Justification(ETextJustify::Center)
							.DecoratorStyleSet( &SUTStyle::Get() )
							.AutoWrapText( true )
							.WrapTextAt(DesignedSize.X * 0.7)
							.Text(FText::GetEmpty())
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
								.Image(SUTStyle::Get().GetBrush("UT.Login.Editbox.Background"))
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
									.OnTextCommitted(this, &SUTLoginDialog::OnTextCommited)
									.MinDesiredWidth(425)
									.Style(SUTStyle::Get(), "UT.Login.Editbox")
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
									.TextStyle(SUTStyle::Get(), "UT.Login.Label.TextStyle")
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
							.ButtonStyle(SUTStyle::Get(), "UT.Login.EmptyButton")
							.OnClicked(this, &SUTLoginDialog::OnForgotPasswordClick)
							.ContentPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Login", "PasswordRecovery", "Forgot Your Password?"))
								.TextStyle(SUTStyle::Get(), "UT.Login.EmptyButton.TextStyle")
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
						.HeightOverride(56.0)
						[
							SNew(SButton)
							.ButtonStyle(SUTStyle::Get(), "UT.Login.Button")
							.OnClicked(this, &SUTLoginDialog::OnSignInClick)
							.ContentPadding(FMargin(25.0, 0.0, 25.0, 5.0))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("Login", "LoginButtonText", "Sign In"))
									.TextStyle(SUTStyle::Get(), "UT.Login.Button.TextStyle")
								]
							]
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(SUTStyle::Get(), "UT.Login.EmptyButton")
						.OnClicked(this, &SUTLoginDialog::OnCloseClick)
						.ContentPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("Login", "PlayOffline", "Play Offline"))
							.TextStyle(SUTStyle::Get(), "UT.Login.Offline.TextStyle")
						]
					]
				]

				// New Account

				+SVerticalBox::Slot()
				.Padding(10.0f, 66.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Login", "NoAccountMsg", "Need an Epic Games Account?"))
						.TextStyle(SUTStyle::Get(), "UT.Login.TextStyle")
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
						.ButtonStyle(SUTStyle::Get(), "UT.Login.EmptyButton")
						.OnClicked(this, &SUTLoginDialog::OnNewAccountClick)
						.ContentPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("Login", "SignUp", "Sign Up!"))
							.TextStyle(SUTStyle::Get(), "UT.Login.EmptyButton.TextStyle")
						]
					]
				]
			]
			+SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Center)
			[
				SAssignNew(LoadingBox,SVerticalBox)
				.Visibility(this, &SUTLoginDialog::GetLoadingVis)
				+ SVerticalBox::Slot()
				.Padding(10.0f,150.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Login","TeaserText","Enter the Field of Battle!"))
					.TextStyle(SUTStyle::Get(), "UT.Login.TextStyle")
				]
				+ SVerticalBox::Slot()
				.Padding(10.0f,150.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SUTLoginDialog::GetLoginPhaseMessage)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Link")
				]
				+ SVerticalBox::Slot()
				.Padding(10.0f,150.0f, 10.0f, 0.0f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SThrobber)
				]

			]
		]
	];

}
void SUTLoginDialog::SetInitialFocus()
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


FString SUTLoginDialog::GetEpicID()
{
	return UserEditBox->GetText().ToString();
}

FString SUTLoginDialog::GetPassword()
{
	return PassEditBox->GetText().ToString();
}


FReply SUTLoginDialog::OnNewAccountClick()
{
	FString Error;
	FPlatformProcess::LaunchURL(TEXT("https://forums.unrealtournament.com/download.php?return=http://www.unrealtournament.com"), NULL, &Error);
	if (Error.Len() > 0)
	{
		PlayerOwner->MessageBox(NSLOCTEXT("SUTMenuBase", "HTTPBrowserError", "Error Launching Browser"), FText::FromString(Error));
	}
	return FReply::Handled(); 
}

FReply SUTLoginDialog::OnForgotPasswordClick()
{
	FString Error;
	FPlatformProcess::LaunchURL(TEXT("https://accounts.epicgames.com/requestPasswordReset"), NULL, &Error);
	if (Error.Len() > 0)
	{
		PlayerOwner->MessageBox(NSLOCTEXT("SUTMenuBase", "HTTPBrowserError", "Error Launching Browser"),FText::FromString(Error) );
	}
	return FReply::Handled();

	
}

FReply SUTLoginDialog::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCloseClick();
	}

	return FReply::Unhandled();
}


FReply SUTLoginDialog::OnCloseClick()
{
	PlayerOwner->ClearPendingLoginUserName();
	PlayerOwner->bPlayingOffline = true;
	PlayerOwner->LoginPhase = ELoginPhase::Offline;
	PlayerOwner->CloseAuth();
	
	return FReply::Handled();
}


void SUTLoginDialog::OnTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		OnSignInClick();
	}
} 

FReply SUTLoginDialog::OnSignInClick()
{

	if ( GetEpicID() != TEXT("") && GetPassword() != TEXT("") )  
	{
		PlayerOwner->AttemptLogin();
	}

	return FReply::Handled();
}


FReply SUTLoginDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
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

EVisibility SUTLoginDialog::GetInfoVis() const
{
	return (PlayerOwner.IsValid() && PlayerOwner->LoginPhase == ELoginPhase::InDialog) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SUTLoginDialog::GetLoadingVis() const
{
	return (PlayerOwner.IsValid() && PlayerOwner->LoginPhase == ELoginPhase::InDialog ) ? EVisibility::Collapsed : EVisibility::Visible;
}


void SUTLoginDialog::SetErrorText(FText NewErrorText)
{
	// Parse out the error text.

	FString ErrorAsString = NewErrorText.ToString();
	int32 ErrorCodePos = INDEX_NONE;
	if ( ErrorAsString.FindLastChar(TEXT('='),ErrorCodePos) )
	{
		ErrorAsString.RemoveAt(0,ErrorCodePos+1);
		ErrorAsString = ErrorAsString.Trim();
		if ( ErrorAsString == TEXT("18031") )
		{
			NewErrorText = NSLOCTEXT("SUTLoginDialog","BadCredentials","The Username or Password you have entered was not found.");
		}
	}

	ErrorText->SetText(NewErrorText);
}

void SUTLoginDialog::BeginLogin()
{
	bIsLoggingIn = true;
	LoginStartTime = PlayerOwner->GetWorld()->GetRealTimeSeconds();
}

void SUTLoginDialog::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Failsafe
	if (PlayerOwner->LoginPhase == ELoginPhase::Offline || PlayerOwner->LoginPhase == ELoginPhase::LoggedIn)
	{
		UE_LOG(UT,Log,TEXT("Login Required the failsafe! %i"), int32(PlayerOwner->LoginPhase));
		EndLogin(true);
	}
	else if (bRequestingClose)
	{
		HideDelayTime -= InDeltaTime;
		if (HideDelayTime <= 0.0f)
		{
			PlayerOwner->CloseAuth();
		}
	}
}

void SUTLoginDialog::EndLogin(bool bClose)
{
	if (bClose)
	{
		float LogoutTime = PlayerOwner->GetWorld()->GetRealTimeSeconds();
		if (LogoutTime - LoginStartTime < MIN_LOGIN_VIEW_TIME)
		{
			bRequestingClose = true;
			HideDelayTime = MIN_LOGIN_VIEW_TIME - (LogoutTime - LoginStartTime);
		}
		else
		{
			PlayerOwner->CloseAuth();
		}
	}
	else
	{
		bIsLoggingIn = false;
	}
}

FText SUTLoginDialog::GetLoginPhaseMessage() const
{
	if (PlayerOwner.IsValid())
	{
		if (PlayerOwner->LoginPhase == ELoginPhase::NotLoggedIn)
		{
			return NSLOCTEXT("Login","LoadingNotLoggedIn",".. Awaiting Authentication .. ");
		}
		else if (PlayerOwner->LoginPhase == ELoginPhase::Offline)
		{
			return NSLOCTEXT("Login","LoadingOffline",".. Offline .. ");
		}
		else if (PlayerOwner->LoginPhase == ELoginPhase::Auth)
		{
			return NSLOCTEXT("Login","LoadingAuth",".. Looking up Account .. ");
		}
		else if (PlayerOwner->LoginPhase == ELoginPhase::GettingProfile)
		{
			return NSLOCTEXT("Login","LoadingProifile",".. Loading Settings .. ");
		}
		else if (PlayerOwner->LoginPhase == ELoginPhase::GettingProgression)
		{
			return NSLOCTEXT("Login","LoadingProgression",".. Loading Progression .. ");
		}
		else if (PlayerOwner->LoginPhase == ELoginPhase::GettingMMR)
		{
			return NSLOCTEXT("Login","LoadingMMR",".. Loading MMR .. ");
		}
		else if (PlayerOwner->LoginPhase == ELoginPhase::LoggedIn)
		{
			return NSLOCTEXT("Login","LoadingLoggedIn",".. Finalizing .. ");
		}
	}

	return NSLOCTEXT("Login","Loading",".. Contact Epic .. ");
}



#endif