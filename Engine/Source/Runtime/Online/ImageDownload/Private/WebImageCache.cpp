// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ImageDownloadPCH.h"
#include "WebImageCache.h"

FWebImageCache::FWebImageCache()
: DefaultStandInBrush(FCoreStyle::Get().GetDefaultBrush())
{
}

TSharedRef<const FWebImage> FWebImageCache::Download(const FString& Url)
{
	// canonicalize URL (we don't currently have code to do this so just treat the URL as opaque)
	const FString& CanonicalUrl = Url;

	// see if there's a cached copy
	TWeakPtr<FWebImage>* ImageFind = UrlToImageMap.Find(CanonicalUrl);
	TSharedPtr<FWebImage> ImagePtr;
	if (ImageFind != nullptr)
	{
		ImagePtr = ImageFind->Pin();
		if (!ImagePtr.IsValid())
		{
			UrlToImageMap.Remove(CanonicalUrl);
		}
	}

	// return any pinned version
	if (ImagePtr.IsValid())
	{
		// if it is done and we failed, and it's being requested again, queue up another try
		if (ImagePtr->DidDownloadFail())
		{
			ImagePtr->SetStandInBrush(DefaultStandInBrush);
			ImagePtr->BeginDownload(ImagePtr->GetUrl());
		}

		// return the image ptr
		TSharedRef<FWebImage> ImageRef = ImagePtr.ToSharedRef();
		StrongRefCache.AddUnique(ImageRef);
		return ImageRef;
	}

	// make a new one
	TSharedRef<FWebImage> WebImage = MakeShareable(new FWebImage());
	WebImage->SetStandInBrush(DefaultStandInBrush);
	WebImage->BeginDownload(CanonicalUrl);

	// add it to the cache
	StrongRefCache.Add(WebImage);
	UrlToImageMap.Add(CanonicalUrl, WebImage);

	return WebImage;
}

void FWebImageCache::RelinquishUnusedImages()
{
	StrongRefCache.Empty();
	for (auto It = UrlToImageMap.CreateConstIterator(); It;)
	{
		if (!It.Value().IsValid())
		{
			auto RemoveMe = It;
			++It;
			UrlToImageMap.Remove(RemoveMe.Key());
		}
		else
		{
			++It;
		}
	}
}
