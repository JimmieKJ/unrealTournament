// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTRedirectDialog.h"
#include "../SUWindowsStyle.h"
#include "AssetData.h"
#include "UTGameEngine.h"

#if !UE_SERVER

void SUTRedirectDialog::Construct(const FArguments& InArgs)
{
	LastETATime = 0.0f;
	LastDownloadedAmount = 0;
	SecondsRemaining = 0.0f;
	NumSamples = 0;
	CurrentSample = 0;

	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}

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
					SNew(STextBlock).Text(NSLOCTEXT("SUTRedirectDialog", "Downloading", "Downloading "))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(20, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SUTRedirectDialog::GetFileName)
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
					.Percent(this, &SUTRedirectDialog::GetProgressFile)
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
					.Text(this, &SUTRedirectDialog::GetProgressFileText)
				]
			]
		];

	}

	DownloadFile(RedirectToURL);
}

FReply SUTRedirectDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_CANCEL)
	{
		CancelDownload();
	}
	return FReply::Handled();
}

void SUTRedirectDialog::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!RedirectToURL.IsEmpty())
	{
		if (HttpRequest.IsValid() && HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
		{
			HttpRequest->Tick(InDeltaTime);

			//Update the download speeds and ETA
			if (LastETATime < InCurrentTime - 1)
			{
				LastETATime = InCurrentTime;
				UpdateETA();
			}
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

	return SUTDialogBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}


void SUTRedirectDialog::HttpRequestProgress(FHttpRequestPtr InHttpRequest, int32 NumBytesSent, int32 NumBytesRecv)
{
	if (InHttpRequest.IsValid())
	{
		AssetsTotalSize = InHttpRequest->GetResponse()->GetContentLength();
		AssetsDownloadedAmount = NumBytesRecv;
	}
}

void SUTRedirectDialog::HttpRequestComplete(FHttpRequestPtr InHttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	//If the download was successful save it to disk
	if (bSucceeded && InHttpRequest.IsValid() && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
	{
		if (HttpResponse->GetContent().Num() > 0)
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		
			FString Path = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Paks"), TEXT("DownloadedPaks"));
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
					FCoreDelegates::OnMountPak.Execute(FullFilePath, 0, nullptr);
					UTEngine->MountedDownloadedContentChecksums.Add(BaseFilename, MD5);
					UTEngine->AddAssetRegistry(BaseFilename);
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


bool SUTRedirectDialog::DownloadFile(FString URL)
{
	HttpRequest = FHttpModule::Get().CreateRequest();

	if (HttpRequest.IsValid())
	{
		HttpRequest->SetURL(URL);
		HttpRequest->OnProcessRequestComplete().BindRaw(this, &SUTRedirectDialog::HttpRequestComplete);
		HttpRequest->OnRequestProgress().BindRaw(this, &SUTRedirectDialog::HttpRequestProgress);

		FString NeedAuthUrl1 = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/cloudstorage/user/");
		FString NeedAuthUrl2 = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/cloudstorage/user/");
		FString NeedAuthUrl3 = TEXT("https://ut-public-service-publictest-prod12.ol.epicgames.com/ut/api/cloudstorage/user/");

		if (OnlineIdentityInterface.IsValid())
		{
			if (URL.StartsWith(NeedAuthUrl1) || URL.StartsWith(NeedAuthUrl2) || URL.StartsWith(NeedAuthUrl3))
			{
				FString AuthToken = OnlineIdentityInterface->GetAuthToken(0);
				HttpRequest->SetHeader(TEXT("Authorization"), FString(TEXT("bearer ")) + AuthToken);
			}
		}

		HttpRequest->SetVerb("GET");
		return HttpRequest->ProcessRequest();
	}
	return false;
}

void SUTRedirectDialog::CancelDownload()
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

FText SUTRedirectDialog::GetProgressFileText() const
{
	if (AssetsTotalSize == 0 && SecondsRemaining <= 0.0f)
	{
		return NSLOCTEXT("SUTRedirectDialog", "Connecting", "Connecting...");
	}

	FFormatNamedArguments Args;
	Args.Add("AssetsDownloadedAmount", BytesToText(AssetsDownloadedAmount));
	Args.Add("AssetsTotalSize", BytesToText(AssetsTotalSize));
	Args.Add("DownloadSpeed", BytesToText(GetAverageBytes()));
	Args.Add("PerSecond", NSLOCTEXT("SUTRedirectDialog", "PerSecond", "/s"));
	Args.Add("RemainingTime", SecondsToText(SecondsRemaining));
	return FText::Format(FText::FromString(TEXT("{AssetsDownloadedAmount} / {AssetsTotalSize}             {DownloadSpeed}{PerSecond}             {RemainingTime}")), Args);
}

FText SUTRedirectDialog::BytesToText(int32 Bytes) const
{
	FText Txt = NSLOCTEXT("SUTRedirectDialog", "Byte", "B");
	int32 Divisor = 1;
	if (Bytes > 0x40000000)
	{
		Txt = NSLOCTEXT("SUTRedirectDialog", "GiB", "GiB");
		Divisor = 0x40000000;
	}
	else if (Bytes > 0x100000)
	{
		Txt = NSLOCTEXT("SUTRedirectDialog", "MiB", "MiB");
		Divisor = 0x100000;
	}
	else if (Bytes > 0x400)
	{
		Txt = NSLOCTEXT("SUTRedirectDialog", "KiB", "KiB");
		Divisor = 0x400;
	}

	FNumberFormattingOptions NumberFormat;
	NumberFormat.MinimumFractionalDigits = Divisor == 1 ? 0 : 2;
	NumberFormat.MaximumFractionalDigits = Divisor == 1 ? 0 : 2;
	return FText::Format(FText::FromString(TEXT("{0} {1}")), FText::AsNumber((float)Bytes / (float)Divisor, &NumberFormat), Txt);
}

FText SUTRedirectDialog::SecondsToText(float Seconds) const
{
	if (Seconds > 0.0f)
	{
		FFormatNamedArguments Args;
		Args.Add("Time", FText::AsTimespan(FTimespan::FromSeconds(Seconds)));
		return FText::Format(NSLOCTEXT("SUTRedirectDialog", "TimeRemaining", "{Time} remaining"), Args);
	}
	return NSLOCTEXT("SUTRedirectDialog", "CalculatingETA", "Calculating ETA...");
}

void SUTRedirectDialog::UpdateETA()
{
	AddSample(AssetsDownloadedAmount - LastDownloadedAmount);
	LastDownloadedAmount = AssetsDownloadedAmount;

	float Average = GetAverageBytes();
	SecondsRemaining = (Average > 0.0f) ? (float)(AssetsTotalSize - AssetsDownloadedAmount) / (float)GetAverageBytes() : 0.0f;
}

void SUTRedirectDialog::AddSample(int32 Sample)
{
	ByteSamples[CurrentSample] = Sample;
	CurrentSample = (CurrentSample < NUM_REDIRECT_SAMPLES - 1) ? CurrentSample + 1 : 0;
	if (NumSamples < NUM_REDIRECT_SAMPLES)
	{
		NumSamples++;
	}
}

float SUTRedirectDialog::GetAverageBytes() const
{
	int32 Total = 0;
	for (int32 i = 0; i < NumSamples; i++)
	{
		Total += ByteSamples[i];
	}
	return (NumSamples > 0) ? (float)Total / (float)NumSamples : 0.0f;
}
#endif