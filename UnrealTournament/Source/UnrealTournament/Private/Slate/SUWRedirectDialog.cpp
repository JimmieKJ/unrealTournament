// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWRedirectDialog.h"
#include "SUWindowsStyle.h"
#include "AssetData.h"
#include "UTGameEngine.h"

#if !UE_SERVER

void SUWRedirectDialog::Construct(const FArguments& InArgs)
{
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}

	SUWDialog::Construct(SUWDialog::FArguments()
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

	bDownloadCanceled = false;
	RedirectToURL = InArgs._RedirectToURL;
	RedirectURLs = InArgs._RedirectURLs;

	if (RedirectToURL.IsEmpty() && RedirectURLs.Num() > 0)
	{
		RedirectToURL = RedirectURLs[0];
		RedirectURLs.RemoveAt(0);
	}

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);
	FVector2D ResolutionScale(ViewportSize.X / 1280.0f, ViewportSize.Y / 720.0f);

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	
	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock).Text(NSLOCTEXT("SUWRedirectDialog", "Downloading", "Downloading "))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(20, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SUWRedirectDialog::GetFileName)
				]
			]
			+ SVerticalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			//.AutoHeight()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SProgressBar)
					.Percent(this, &SUWRedirectDialog::GetProgressFile)
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(20, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SUWRedirectDialog::GetProgressFileText)
				]
			]
		];

	}

	DownloadFile(RedirectToURL);
}

FReply SUWRedirectDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_CANCEL)
	{
		CancelDownload();
	}
	return FReply::Handled();
}

void SUWRedirectDialog::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!RedirectToURL.IsEmpty())
	{
		if (HttpRequest.IsValid() && HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
		{
			HttpRequest->Tick(InDeltaTime);
		}
		else if (HttpRequest->GetStatus() == EHttpRequestStatus::Failed)
		{
			CancelDownload();
		}
		else
		{
			if (RedirectURLs.Num() > 0)
			{
				RedirectToURL = RedirectURLs[0];
				RedirectURLs.RemoveAt(0);
				DownloadFile(RedirectToURL);
			}
			else
			{
				OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_OK);
				GetPlayerOwner()->CloseDialog(SharedThis(this));
			}
		}
	}
	else
	{
		GetPlayerOwner()->CloseDialog(SharedThis(this));
	}

	return SUWDialog::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}


void SUWRedirectDialog::HttpRequestProgress(FHttpRequestPtr HttpRequest, int32 NumBytes)
{
	if (HttpRequest.IsValid())
	{
		AssetsTotalSize = HttpRequest->GetResponse()->GetContentLength();
		AssetsDownloadedAmount = NumBytes;
	}
}

void SUWRedirectDialog::HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	//If the download was successful save it to disk
	if (bSucceeded && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
	{
		if (HttpResponse->GetContent().Num() > 0)
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		
			FString Path = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("DownloadedPaks"));
			if (!PlatformFile.DirectoryExists(*Path))
			{
				PlatformFile.CreateDirectoryTree(*Path);
			}
			FString FullFilePath = FPaths::Combine(*Path, *FPaths::GetCleanFilename(RedirectToURL));
			bSucceeded = FFileHelper::SaveArrayToFile(HttpResponse->GetContent(), *FullFilePath);

			UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
			if (UTEngine)
			{
				FString MD5 = UTEngine->MD5Sum(HttpResponse->GetContent());
				FString BaseFilename = FPaths::GetBaseFilename(RedirectToURL);
				UTEngine->DownloadedContentChecksums.Add(BaseFilename, MD5);

				if (FCoreDelegates::OnMountPak.IsBound())
				{
					FCoreDelegates::OnMountPak.Execute(FullFilePath, 0);
					UTEngine->MountedDownloadedContentChecksums.Add(BaseFilename, MD5);
				}
			}
		}
	}
	else
	{
		if (HttpResponse.IsValid())
		{
			UE_LOG(UT, Warning, TEXT("HTTP Error: %d"), HttpResponse->GetResponseCode());
			FString ErrorContent = HttpResponse->GetContentAsString();
			UE_LOG(UT, Log, TEXT("%s"), *ErrorContent);
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("HTTP Error"));
		}

		CancelDownload();
	}
}


bool SUWRedirectDialog::DownloadFile(FString URL)
{
	HttpRequest = FHttpModule::Get().CreateRequest();

	if (HttpRequest.IsValid())
	{
		HttpRequest->SetURL(URL);
		HttpRequest->OnProcessRequestComplete().BindRaw(this, &SUWRedirectDialog::HttpRequestComplete);
		HttpRequest->OnRequestProgress().BindRaw(this, &SUWRedirectDialog::HttpRequestProgress);

		if (URL.Contains(TEXT("epicgames")) && OnlineIdentityInterface.IsValid())
		{
			FString AuthToken = OnlineIdentityInterface->GetAuthToken(0);
			HttpRequest->SetHeader(TEXT("Authorization"), FString(TEXT("bearer ")) + AuthToken);
		}

		HttpRequest->SetVerb("GET");
		return HttpRequest->ProcessRequest();
	}
	return false;
}

void SUWRedirectDialog::CancelDownload()
{
	if (!bDownloadCanceled)
	{
		bDownloadCanceled = true;
		if (HttpRequest.IsValid() && HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
		{
			HttpRequest->CancelRequest();
		}

		if (GetPlayerOwner() != nullptr)
		{
			OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
			GetPlayerOwner()->CloseDialog(SharedThis(this));
		}
	}
}

#endif