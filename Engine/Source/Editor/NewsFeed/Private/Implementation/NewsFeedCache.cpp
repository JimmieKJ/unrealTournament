// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NewsFeedPrivatePCH.h"


/* Constants
 *****************************************************************************/

const FString FeedFilename(TEXT("newsfeed.json"));


/* FNewsFeedCache structors
 *****************************************************************************/

FNewsFeedCache::FNewsFeedCache( )
	: LoaderState(ENewsFeedState::NotStarted)
	, CurrentCultureSpec(ENewsFeedCultureSpec::Full)
{
	TickDelegate = FTickerDelegate::CreateRaw(this, &FNewsFeedCache::HandleTicker);
}


FNewsFeedCache::~FNewsFeedCache( )
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	SaveSettings();
}


/* FNewsFeedCache interface
 *****************************************************************************/

const FSlateBrush* FNewsFeedCache::GetIcon( const FString& IconName ) const
{
	const TSharedPtr<FSlateDynamicImageBrush>* IconBrush = Icons.Find(IconName);

	if (IconBrush != nullptr)
	{
		return IconBrush->Get();
	}

	return FEditorStyle::GetBrush(TEXT("NewsFeed.PendingIcon"));
}


//bool FNewsFeedCache::LoadFeed( IOnlineTitleFilePtr InTitleFile )
bool FNewsFeedCache::LoadFeed(  )
{
	if (LoaderState == ENewsFeedState::Loading)
	{
		return false;
	}

//	if (!InTitleFile.IsValid())
//	{
//		return false;
//	}

	LoaderState = ENewsFeedState::Loading;
	Icons.Empty();

//	TitleFile = InTitleFile;
//	TitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(FOnEnumerateFilesCompleteDelegate::CreateRaw(this, &FNewsFeedCache::HandleEnumerateFilesComplete));
//	TitleFile->AddOnReadFileCompleteDelegate_Handle(FOnReadFileCompleteDelegate::CreateRaw(this, &FNewsFeedCache::HandleReadFileComplete));
//	TitleFile->ClearFiles();
//	TitleFile->EnumerateFiles();
	LoadTitleFile();

	// auto reload after 15 minutes
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 15 * 60.0f);

	return true;
}


/* FNewsFeedCache implementation
 *****************************************************************************/

void FNewsFeedCache::FailLoad( )
{
	LoaderState = ENewsFeedState::Failure;
	LoadFailedDelegate.Broadcast();
}


void FNewsFeedCache::LoadTitleFile( )
{
	FString CultureString;

	// get current culture
	FCultureRef Culture = FInternationalization::Get().GetCurrentCulture();

	if (CurrentCultureSpec == ENewsFeedCultureSpec::Full)
	{
		CultureString = Culture->GetName();
	}
	else if (CurrentCultureSpec == ENewsFeedCultureSpec::LanguageOnly)
	{
		CultureString = Culture->GetTwoLetterISOLanguageName();
	}

	// get title file source
	const UNewsFeedSettings* NewsFeedSettings = GetDefault<UNewsFeedSettings>();

	if (NewsFeedSettings->Source == NEWSFEED_Cdn)
	{
		if (!NewsFeedSettings->CdnSourceUrl.IsEmpty())
		{
			FString CdnSourceUrl;

			if (!CultureString.IsEmpty())
			{
				CdnSourceUrl = NewsFeedSettings->CdnSourceUrl.Replace(TEXT("[[Culture]]"), *CultureString);
			}
			else
			{
				CdnSourceUrl = NewsFeedSettings->CdnSourceUrl.Replace(TEXT("[[Culture]]/"), TEXT(""));
			}

			TitleFile = FCdnNewsFeedTitleFile::Create(CdnSourceUrl);
		}
	}
	else if (NewsFeedSettings->Source == NEWSFEED_Local)
	{
		if (!NewsFeedSettings->LocalSourcePath.IsEmpty())
		{
			FString RootDirectory = FPaths::Combine(FPlatformProcess::BaseDir(), *FPaths::EngineContentDir(), *NewsFeedSettings->LocalSourcePath);

			if (!CultureString.IsEmpty())
			{
				RootDirectory = RootDirectory.Replace(TEXT("[[Culture]]"), *CultureString);
			}
			else
			{
				RootDirectory = RootDirectory.Replace(TEXT("[[Culture]]/"), TEXT(""));
			}

			TitleFile = FLocalNewsFeedTitleFile::Create(RootDirectory);
		}
	}
	else if (NewsFeedSettings->Source == NEWSFEED_Mcp)
	{
		TitleFile = Online::GetTitleFileInterface(TEXT("MCP"));
	}

	// fetch news feed
	if (TitleFile.IsValid())
	{
		TitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(FOnEnumerateFilesCompleteDelegate::CreateRaw(this, &FNewsFeedCache::HandleEnumerateFilesComplete));
		TitleFile->AddOnReadFileCompleteDelegate_Handle(FOnReadFileCompleteDelegate::CreateRaw(this, &FNewsFeedCache::HandleReadFileComplete));
		TitleFile->ClearFiles();
		TitleFile->EnumerateFiles();
	}
	else
	{
		FailLoad();
	}
}


void FNewsFeedCache::ProcessIconFile( const FString& IconName )
{
	TArray<uint8> FileContents;
	TitleFile->GetFileContents(IconName, FileContents);

	TSharedPtr<FSlateDynamicImageBrush> IconBrush = RawDataToBrush(*(FString(TEXT("NewsFeed.")) + IconName), FileContents);
	Icons.Add(IconName, IconBrush);
}


void FNewsFeedCache::ProcessNewsFeedFile( )
{
	Items.Empty();

	// read news feed file
	TArray<uint8> FileContents;
	TitleFile->GetFileContents(FeedFileHeader.DLName, FileContents);

	FString NewsFeedJson;
	FFileHelper::BufferToString(NewsFeedJson, FileContents.GetData(), FileContents.Num());

	// deserialize the news feed Json
	TSharedRef<TJsonReader<> > Reader = TJsonReaderFactory<TCHAR>::Create(NewsFeedJson);
	TSharedPtr<FJsonObject> FeedObject;

	if (!FJsonSerializer::Deserialize(Reader, FeedObject))
	{
		FailLoad();

		return;
	}

	// get the file list again for any icons we may have to load
	TArray<FCloudFileHeader> FileHeaders;
	TitleFile->GetFileList(FileHeaders);

	// parse the news feed items
	const UNewsFeedSettings* NewsFeedSettings = GetDefault<UNewsFeedSettings>();
	TArray<TSharedPtr<FJsonValue> > ItemObjects = FeedObject->GetArrayField(TEXT("news"));

	// for each news item...
	for (int32 ItemObjectIndex = 0; ItemObjectIndex < ItemObjects.Num(); ++ItemObjectIndex)
	{
		const TSharedPtr<FJsonObject>& ItemObject = ItemObjects[ItemObjectIndex]->AsObject();

		// ... that has a valid identifier...
		if (ItemObject->HasTypedField<EJson::String>("id"))
		{
			FGuid ItemId;

			if (FGuid::Parse(ItemObject->GetStringField("id"), ItemId))
			{
				// .. parse its data...
				FNewsFeedItemRef NewsFeedItem = MakeShareable(new FNewsFeedItem(ItemId));

				NewsFeedItem->Excerpt = FText::FromString(ItemObject->GetStringField("excerpt"));
				NewsFeedItem->FullText = FText::FromString(ItemObject->GetStringField("fulltext"));
				NewsFeedItem->Read = NewsFeedSettings->ReadItems.Contains(ItemId);
				NewsFeedItem->Title = FText::FromString(ItemObject->GetStringField("title"));
				NewsFeedItem->Url = ItemObject->GetStringField("url");

				FDateTime Issued;

				if (FDateTime::Parse(ItemObject->GetStringField("issued"), Issued))
				{
					NewsFeedItem->Issued = Issued;
				}
				else
				{
					NewsFeedItem->Issued = FDateTime::MinValue();
				}

				// ... and load its icon image
				const FString IconName = ItemObject->GetStringField("icon");

				for (int32 FileHeaderIndex = 0; FileHeaderIndex < FileHeaders.Num(); ++FileHeaderIndex)
				{
					const FCloudFileHeader& FileHeader = FileHeaders[FileHeaderIndex];

					if (FileHeader.FileName == IconName)
					{
						NewsFeedItem->IconName = FileHeader.DLName;
						TitleFile->ReadFile(FileHeader.DLName);

						break;
					}
				}

				Items.Add(NewsFeedItem);
			}
		}
	}

	LoaderState = ENewsFeedState::Success;
	LoadCompletedDelegate.Broadcast();
}


TSharedPtr<FSlateDynamicImageBrush> FNewsFeedCache::RawDataToBrush( FName ResourceName, const TArray< uint8 >& InRawData ) const
{
	TSharedPtr<FSlateDynamicImageBrush> Brush;

	uint32 BytesPerPixel = 4;
	int32 Width = 0;
	int32 Height = 0;

	bool bSucceeded = false;
	TArray<uint8> DecodedImage;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );

	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(InRawData.GetData(), InRawData.Num()))
	{
		Width = ImageWrapper->GetWidth();
		Height = ImageWrapper->GetHeight();

		const TArray<uint8>* RawData = NULL;

		if (ImageWrapper->GetRaw( ERGBFormat::BGRA, 8, RawData))
		{
			DecodedImage.AddUninitialized( Width * Height * BytesPerPixel );
			DecodedImage = *RawData;
			bSucceeded = true;
		}
	}

	if (bSucceeded && FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(ResourceName, ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), DecodedImage))
	{
		Brush = MakeShareable(new FSlateDynamicImageBrush(ResourceName, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight())));
	}

	return Brush;
}


void FNewsFeedCache::SaveSettings( )
{
	UNewsFeedSettings* NewsFeedSettings = GetMutableDefault<UNewsFeedSettings>();

	// remember read items
	for (auto NewsFeedItem : Items)
	{
		if (NewsFeedItem->Read)
		{
			NewsFeedSettings->ReadItems.AddUnique(NewsFeedItem->ItemId);
		}
	}

	NewsFeedSettings->SaveConfig();
}


/* FNewsFeedCache callbacks
 *****************************************************************************/

void FNewsFeedCache::HandleEnumerateFilesComplete( bool bSuccess )
{
	if (bSuccess)
	{
		// load news feed files
		TArray<FCloudFileHeader> FileHeaders;
		TitleFile->GetFileList(FileHeaders);

		for (int32 FileHeaderIndex = 0; FileHeaderIndex < FileHeaders.Num(); ++FileHeaderIndex)
		{
			const FCloudFileHeader& FileHeader = FileHeaders[FileHeaderIndex];

			if (FileHeader.FileName == FeedFilename)
			{
				FeedFileHeader = FileHeader;
				TitleFile->ReadFile(FileHeader.DLName);

				return;
			}
		}

		FailLoad();
	}
	else
	{
		// move on to the next culture specification and try again
		if (CurrentCultureSpec != ENewsFeedCultureSpec::None)
		{
			CurrentCultureSpec = static_cast<ENewsFeedCultureSpec>(int(CurrentCultureSpec) + 1);
			LoadTitleFile();
		}
		else
		{
			FailLoad();
		}
	}
}


void FNewsFeedCache::HandleReadFileComplete( bool bSuccess, const FString& Filename )
{
	if (Filename == FeedFileHeader.DLName)
	{
		if (bSuccess)
		{
			ProcessNewsFeedFile();
		}
		else
		{
			FailLoad();
		}
	}
	else if (bSuccess)
	{
		if (!Icons.Contains(Filename))
		{
			ProcessIconFile(Filename);
		}		
	}
}


bool FNewsFeedCache::HandleTicker( float DeltaTime )
{
	LoadFeed();

	return false;
}
