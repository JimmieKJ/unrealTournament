// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ImageDownloadPCH.h"
#include "WebImage.h"

static const FString IMAGE_MIME_PNG(TEXT("image/png"));
static const FString IMAGE_MIME_PNG_ALT(TEXT("image/x-png"));
static const FString IMAGE_MIME_JPEG(TEXT("image/jpeg"));
static const FString IMAGE_MIME_BMP(TEXT("image/bmp"));

FWebImage::FWebImage()
: StandInBrush(FCoreStyle::Get().GetDefaultBrush())
, bDownloadSuccess(false)
{
}

TAttribute< const FSlateBrush* > FWebImage::Attr() const
{
	return TAttribute< const FSlateBrush* >(AsShared(), &FWebImage::GetBrush);
}

bool FWebImage::BeginDownload(const FString& UrlIn, const FOnImageDownloaded& DownloadCb)
{
	CancelDownload();

	// store the url
	Url = UrlIn;

	// make a new request
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetURL(Url);
	HttpRequest->SetHeader(TEXT("Accept"), FString::Printf(TEXT("%s, %s, %s; q=0.8, %s; q=0.5"), *IMAGE_MIME_PNG, *IMAGE_MIME_PNG_ALT, *IMAGE_MIME_JPEG, *IMAGE_MIME_BMP));
	HttpRequest->OnProcessRequestComplete().BindSP(this, &FWebImage::HttpRequestComplete, DownloadCb);

	// queue the request
	PendingRequest = HttpRequest;
	if (!HttpRequest->ProcessRequest())
	{
		// clear our local handle
		PendingRequest.Reset();
		return false;
	}
	return true;
}

void FWebImage::HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnImageDownloaded DownloadCb)
{
	// clear our handle to the request
	PendingRequest.Reset();

	// get the request URL
	check(HttpRequest.IsValid()); // this should be valid, we did just send a request...
	bool bSuccess = ProcessHttpResponse(HttpRequest->GetURL(), bSucceeded ? HttpResponse : FHttpResponsePtr());

	// save this info
	bDownloadSuccess = bSuccess;
	DownloadTimeUtc = FDateTime::UtcNow();

	// fire the response delegate
	if (DownloadCb.IsBound())
	{
		DownloadCb.Execute(bSuccess);
	}
}

bool FWebImage::ProcessHttpResponse(const FString& RequestUrl, FHttpResponsePtr HttpResponse)
{
	// check for successful response
	if (!HttpResponse.IsValid())
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Connection Failed. url=%s"), *RequestUrl);
		return false;
	}

	// check status code
	int32 StatusCode = HttpResponse->GetResponseCode();
	if (StatusCode / 100 != 2)
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: HTTP response %d. url=%s"), StatusCode, *RequestUrl);
		return false;
	}

	// get the content type and figure out the image format from it
	FString ContentType = HttpResponse->GetContentType();
	EImageFormat::Type ImageFormat;
	if (ContentType.Equals(IMAGE_MIME_PNG, ESearchCase::IgnoreCase) || ContentType.Equals(IMAGE_MIME_PNG_ALT, ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::PNG;
	}
	else if (ContentType.Equals(IMAGE_MIME_JPEG, ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::JPEG;
	}
	else if (ContentType.Equals(IMAGE_MIME_BMP, ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::BMP;
	}
	else
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Invalid Content-Type '%s'. url=%s"), *ContentType, *RequestUrl);
		return false;
	}

	// build an image wrapper for this type
	static const FName MODULE_IMAGE_WRAPPER("ImageWrapper");
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(MODULE_IMAGE_WRAPPER);
	IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Unable to make image wrapper for %s"), *ContentType);
		return false;
	}

	// parse the content
	const TArray<uint8>& Content = HttpResponse->GetContent();
	if (!ImageWrapper->SetCompressed(Content.GetData(), Content.Num()))
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Unable to parse %s from %s"), *ContentType, *RequestUrl);
		return false;
	}

	// get the raw image data
	const TArray<uint8>* RawImageData = nullptr;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawImageData) || RawImageData == nullptr)
	{
		UE_LOG(LogImageDownload, Error, TEXT("Image Download: Unable to convert %s to BGRA 8"), *ContentType);
		return false;
	}

	// make a dynamic image
	FName ResourceName(*RequestUrl);
	DownloadedBrush = FSlateDynamicImageBrush::CreateWithImageData(ResourceName, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight()), *RawImageData);
	return DownloadedBrush.IsValid();
}

void FWebImage::CancelDownload()
{
	if (PendingRequest.IsValid())
	{
		FHttpRequestPtr Request = PendingRequest;
		PendingRequest.Reset();
		Request->CancelRequest();
	}
	bDownloadSuccess = false;
}
