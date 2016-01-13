// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTYoutubeConsentDialog.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTYoutubeConsentDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(InArgs._DialogTitle)
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(InArgs._ButtonMask)
		.OnDialogResult(InArgs._OnDialogResult)
		);
	
	// ClientID UT Youtube app on PLK google account
	FString ClientID = TEXT("465724645978-10npjjgfbb03p4ko12ku1vq1ioshts24.apps.googleusercontent.com");
	FString ConsentURL = TEXT("https://accounts.google.com/o/oauth2/auth?client_id=") + ClientID + TEXT("&redirect_uri=urn:ietf:wg:oauth:2.0:oob&scope=https://www.googleapis.com/auth/youtube.upload&response_type=code&access_type=offline");

	if (!InArgs._RequestURL.IsEmpty())
	{
		ConsentURL = InArgs._RequestURL;
	}

	FVector2D WebBrowserSize = ActualSize;
	WebBrowserSize.X -= 30;
	WebBrowserSize.Y -= 30;

	if (DialogContent.IsValid())
	{		
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[
				SAssignNew(ConsentBrowser, SWebBrowser)
				.InitialURL(ConsentURL)
				.ShowControls(false)
				.ViewportSize(WebBrowserSize)
				.OnTitleChanged(FOnTextChanged::CreateSP(this, &SUTYoutubeConsentDialog::OnTitleChanged))
			]
		];
	}
}

void SUTYoutubeConsentDialog::OnTitleChanged(const FText& NewText)
{
	FString TitleString = NewText.ToString();
	if (TitleString.Contains(TEXT("code=")))
	{
		int32 EqualIndex;
		TitleString.FindChar(TEXT('='), EqualIndex);
		
		UniqueCode = TitleString.RightChop(EqualIndex + 1);

		OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_OK);
		GetPlayerOwner()->CloseDialog(SharedThis(this));
	}
	else if (TitleString.Contains(TEXT("error=")))
	{
		OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
		GetPlayerOwner()->CloseDialog(SharedThis(this));
	}
}

FReply SUTYoutubeConsentDialog::OnButtonClick(uint16 ButtonID)
{
	// cancel the request
	ConsentBrowser->LoadURL(TEXT("http://google.com"));

	OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

void SUTYoutubeConsentDialog::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SUTDialogBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

#endif