// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "WebImageService.h"

#include "WebImageCache.h"
#include "WebImage.h"

#include "OnlineSubsystemMcp.h"
#include "OnlineImageServiceMcp.h"

class FWebImageServiceImpl :
	public FWebImageService
{
public:

	virtual void RequestImage(FString ImageUrl, FString BrushName) override
	{
		if (!BrushNameToImageMap.Contains(BrushName))
		{ 
			TSharedPtr<const FWebImage> WebImage = WebImageCache.Download(ImageUrl, DefaultImageUrl);
			BrushNameToImageMap.Add(BrushName, WebImage);
		}
	}

	virtual void RequestTitleIcon(FString ClientID) override
	{
		if (ClientID.IsEmpty())
		{
			ClientID = TEXT("Default");
		}

		FOnlineSubsystemMcp* OnlineSubMcp = (FOnlineSubsystemMcp*)IOnlineSubsystem::Get(TEXT("MCP"));
		if (OnlineSubMcp != NULL)
		{
			FOnlineImageServiceMcpPtr ImageService = OnlineSubMcp->GetMcpImageService();
			if (ImageService.IsValid())
			{
				
// 				FString ImageUrl = ImageService->GetImageUrl(TEXT("title_icons"), ClientID, TEXT("40x40"));
// 				if (DefaultImageUrl.IsEmpty())
// 				{ 
// 					DefaultImageUrl = ImageService->GetImageUrl(TEXT("title_icons"), TEXT("launcher"), TEXT("40x40"));
// 				}

				FString BaseUrl = ImageService->GetBaseUrl();
				BaseUrl.RemoveFromEnd(TEXT("imageservice"));
				FString LookUpUrl = TEXT("launcher-resources/0.1_b76b28ed708e4efcbb6d0e843fcc6456/`lookupId/icon.png");
				FString DefaultID = TEXT("launcher");
				FString ImageUrl = BaseUrl / LookUpUrl.Replace(TEXT("`lookupId"), *ClientID);
				DefaultImageUrl = BaseUrl / LookUpUrl.Replace(TEXT("`lookupId"), *DefaultID);

				ImageUrl = ImageUrl.ToLower();
				DefaultImageUrl = DefaultImageUrl.ToLower();

				RequestImage(ImageUrl, ClientID);
			}
		}
	}

	virtual void SetDefaultTitleIcon(const FSlateBrush* Brush)  override
	{
		StandInBrush = Brush;
		WebImageCache.SetDefaultStandInBrush(Brush);
	}

	virtual const FSlateBrush* GetBrush(FString BrushName) const override
	{
		if (BrushName.IsEmpty())
		{
			BrushName = TEXT("Default");
		}

		const TWeakPtr<const FWebImage>* WebImage = BrushNameToImageMap.Find(BrushName);
		if (WebImage)
		{
			return WebImage->Pin()->GetBrush();
		}
		return StandInBrush;
	}

private:

	FWebImageServiceImpl()
		: StandInBrush(nullptr)
	{

	}

	FWebImageCache WebImageCache;
	TMap<FString, TWeakPtr<const FWebImage> > BrushNameToImageMap;
	const FSlateBrush* StandInBrush;
	FString DefaultImageUrl;

	friend FWebImageServiceFactory;
};

TSharedRef< FWebImageService > FWebImageServiceFactory::Create()
{
	TSharedRef< FWebImageServiceImpl > WebImageService(new FWebImageServiceImpl());
	return WebImageService;
}

#undef LOCTEXT_NAMESPACE