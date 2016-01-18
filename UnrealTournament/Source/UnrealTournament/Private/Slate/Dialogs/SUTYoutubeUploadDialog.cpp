// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTYoutubeUploadDialog.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER
void SUTYoutubeUploadDialog::Construct(const FArguments& InArgs)
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

	VideoFilename = InArgs._VideoFilename;
	AccessToken = InArgs._AccessToken;
	UploadFailMessage.Empty();

	VideoTitle = InArgs._VideoTitle;
	if (VideoTitle.IsEmpty())
	{
		VideoTitle = TEXT("UT Automated Upload");
	}

	YoutubeUploadRequest = nullptr;
	BytesUploaded = 0;

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
					SNew(STextBlock).Text(NSLOCTEXT("SUTYoutubeUploadDialog", "Uploading", "Uploading..."))
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
					.Percent(this, &SUTYoutubeUploadDialog::GetProgressCompression)
				]
			]
		];
	}

	// Verify file size so we don't try to upload over what Youtube allows
	FileSizeInBytes = IFileManager::Get().FileSize(*VideoFilename);
	if (FileSizeInBytes == -1)
	{
		UE_LOG(UT, Warning, TEXT("Failed to open video file %s"), *VideoFilename);
		OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
		GetPlayerOwner()->CloseDialog(SharedThis(this));
		return;
	}

	YoutubeUploadBinaryData.Empty(FileSizeInBytes);
	YoutubeUploadBinaryData.AddZeroed(FileSizeInBytes);

	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);

	TSharedPtr<FJsonObject> SnippetJson = MakeShareable(new FJsonObject);
	SnippetJson->SetStringField(TEXT("title"), VideoTitle);
	SnippetJson->SetStringField(TEXT("description"), TEXT("Unreal Tournament PreAlpha Gameplay Footage"));
	TArray< TSharedPtr<FJsonValue> > TagsJson;
	TagsJson.Add(MakeShareable(new FJsonValueString(TEXT("UnrealTournament"))));
	TagsJson.Add(MakeShareable(new FJsonValueString(TEXT("UT"))));
	TagsJson.Add(MakeShareable(new FJsonValueString(TEXT("boblife"))));
	SnippetJson->SetArrayField(TEXT("tags"), TagsJson);
	SnippetJson->SetNumberField(TEXT("categoryId"), 20);

	TSharedPtr<FJsonObject> StatusJson = MakeShareable(new FJsonObject);
	StatusJson->SetStringField(TEXT("privacyStatus"), TEXT("public"));
	StatusJson->SetBoolField(TEXT("embeddable"), true);
	StatusJson->SetStringField(TEXT("license"), TEXT("youtube"));

	RequestJson->SetObjectField(TEXT("snippet"), SnippetJson);
	RequestJson->SetObjectField(TEXT("status"), StatusJson);

	TArray<uint8> RequestPayload;
	FString OutputJsonString;
	TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > Writer = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&OutputJsonString);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);
	{
		FMemoryWriter MemoryWriter(RequestPayload);
		FTCHARToUTF8 ConvertToUtf8(*OutputJsonString);
		MemoryWriter.Serialize((void*)ConvertToUtf8.Get(), ConvertToUtf8.Length());
	}

	FString RequestUrl = TEXT("https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status,contentDetails");

	FHttpRequestPtr YoutubeResumableRequest = FHttpModule::Get().CreateRequest();
	YoutubeResumableRequest->SetVerb(TEXT("POST"));
	YoutubeResumableRequest->SetURL(RequestUrl);
	YoutubeResumableRequest->OnProcessRequestComplete().BindRaw(this, &SUTYoutubeUploadDialog::YoutubeResumableSessionRequestComplete);
	YoutubeResumableRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
	//YoutubeResumableRequest->SetHeader(TEXT("Content-Length"), FString::FromInt(RequestPayload.Num()));
	YoutubeResumableRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	YoutubeResumableRequest->SetHeader(TEXT("X-Upload-Content-Length"), FString::FromInt(FileSizeInBytes));
	YoutubeResumableRequest->SetHeader(TEXT("X-Upload-Content-Type"), TEXT("video/webm"));

	YoutubeResumableRequest->SetContent(RequestPayload);

	//UE_LOG(UT, Log, TEXT("%s"), *OutputJsonString);

	YoutubeResumableRequest->ProcessRequest();
}

void SUTYoutubeUploadDialog::YoutubeResumableSessionRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (HttpResponse.IsValid())
	{
		if (HttpResponse->GetResponseCode() == 200)
		{
			// Location HTTP header section is where we upload to
			FString UploadUrl = HttpResponse->GetHeader(TEXT("Location"));
			FArchive* VideoTempFile = IFileManager::Get().CreateFileReader(*VideoFilename);
			VideoTempFile->Serialize(YoutubeUploadBinaryData.GetData(), YoutubeUploadBinaryData.Num());

			YoutubeUploadRequest = FHttpModule::Get().CreateRequest();
			YoutubeUploadRequest->SetVerb(TEXT("PUT"));
			YoutubeUploadRequest->SetURL(UploadUrl);
			YoutubeUploadRequest->OnProcessRequestComplete().BindRaw(this, &SUTYoutubeUploadDialog::YoutubeFileUploadRequestComplete);
			HttpRequest->OnRequestProgress().BindRaw(this, &SUTYoutubeUploadDialog::YoutubeFileUploadProgress);
			YoutubeUploadRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
			YoutubeUploadRequest->SetHeader(TEXT("Content-Length"), FString::FromInt(YoutubeUploadBinaryData.Num()));
			YoutubeUploadRequest->SetHeader(TEXT("Content-Type"), TEXT("video/webm"));

			YoutubeUploadRequest->SetContent(YoutubeUploadBinaryData);
			YoutubeUploadRequest->ProcessRequest();
		}
		else
		{
			UploadFailMessage = *HttpResponse->GetContentAsString();
			UE_LOG(UT, Warning, TEXT("Resumable session failed\n%s"), *UploadFailMessage);
			OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
			GetPlayerOwner()->CloseDialog(SharedThis(this));
		}
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Failed to make http request"));
		OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
		GetPlayerOwner()->CloseDialog(SharedThis(this));
	}
}

void SUTYoutubeUploadDialog::YoutubeFileUploadProgress(FHttpRequestPtr InHttpRequest, int32 NumBytesSent, int32 NumBytesRecv)
{
	BytesUploaded = NumBytesSent;
}

void SUTYoutubeUploadDialog::YoutubeFileUploadRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (HttpResponse.IsValid())
	{
		// The docs say 201, but I get 200s
		if (HttpResponse->GetResponseCode() == 200 || HttpResponse->GetResponseCode() == 201)
		{
			UE_LOG(UT, Log, TEXT("Upload succeeded\n%s"), *HttpResponse->GetContentAsString());
			OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_OK);
			GetPlayerOwner()->CloseDialog(SharedThis(this));
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("%s"), *HttpResponse->GetContentAsString());
			OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
			GetPlayerOwner()->CloseDialog(SharedThis(this));
		}
	}
}

TOptional<float> SUTYoutubeUploadDialog::GetProgressCompression() const
{
	if (FileSizeInBytes > 0)
	{
		return (float)BytesUploaded / (float)FileSizeInBytes;
	}

	return 0;
}

FReply SUTYoutubeUploadDialog::OnButtonClick(uint16 ButtonID)
{
	OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}
#endif