// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "IMediaModule.h"
#include "IMediaPlayer.h"
#include "IMediaPlayerFactory.h"
#include "MediaPlayer.h"
#include "MediaPlayerCustomization.h"
#include "SFilePathPicker.h"


#define LOCTEXT_NAMESPACE "FMediaPlayerCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FMediaPlayerCustomization::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	DetailBuilder.GetObjectsBeingCustomized(CustomizedMediaPlayers);

	// customize 'Source' category
	IDetailCategoryBuilder& SourceCategory = DetailBuilder.EditCategory("Source");
	{
		// URL
		UrlProperty = DetailBuilder.GetProperty("URL");
		{
			IDetailPropertyRow& UrlRow = SourceCategory.AddProperty(UrlProperty);

			UrlRow.DisplayName(LOCTEXT("URLLabel", "URL"));
			UrlRow.CustomWidget()
				.NameContent()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text(LOCTEXT("FileOrUrlPropertyName", "File or URL"))
								.ToolTipText(UrlProperty->GetToolTipText())
						]

					+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SImage)
								.Image(FCoreStyle::Get().GetBrush("Icons.Warning"))
								.ToolTipText(LOCTEXT("InvalidUrlPathWarning", "The current URL points to a file that does not exist or is not located inside the /Content/Movies/ directory."))
								.Visibility(this, &FMediaPlayerCustomization::HandleUrlWarningIconVisibility)
						]
				]
				.ValueContent()
				.MaxDesiredWidth(0.0f)
				.MinDesiredWidth(125.0f)
				[
					SNew(SFilePathPicker)
						.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
						.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.BrowseButtonToolTip(LOCTEXT("UrlBrowseButtonToolTip", "Choose a file from this computer"))
						.BrowseDirectory(FPaths::GameContentDir() / TEXT("Movies"))
						.FilePath(this, &FMediaPlayerCustomization::HandleUrlPickerFilePath)
						.FileTypeFilter(this, &FMediaPlayerCustomization::HandleUrlPickerFileTypeFilter)
						.OnPathPicked(this, &FMediaPlayerCustomization::HandleUrlPickerPathPicked)
						.ToolTipText(LOCTEXT("UrlToolTip", "The path to a media file on this computer, or a URL to a media source on the internet"))
				];
		}
	}

	// customize 'Information' category
	IDetailCategoryBuilder& InformationCategory = DetailBuilder.EditCategory("Information", FText::GetEmpty(), ECategoryPriority::Uncommon);
	{
		// duration
		InformationCategory.AddCustomRow(LOCTEXT("Duration", "Duration"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("Duration", "Duration"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(STextBlock)
					.Text(this, &FMediaPlayerCustomization::HandleDurationTextBlockText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("DurationToolTip", "The total duration of this media, i.e. how long it plays"))
			];

		// forward rates (thinned)
		InformationCategory.AddCustomRow(LOCTEXT("ForwardRatesThinned", "Forward Rates (Thinned)"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("ForwardRatesThinned", "Forward Rates (Thinned)"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(STextBlock)
					.Text(this, &FMediaPlayerCustomization::HandleSupportedRatesTextBlockText, EMediaPlaybackDirections::Forward, false)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("ThinnedForwardRateToolTip", "The rate at which this media can be played thinned (with skipping frames) in the forward direction."))
			];

		// forward rates (unthinned)
		InformationCategory.AddCustomRow(LOCTEXT("ForwardRatesUnthinned", "Forward Rates (Unthinned)"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("ForwardRatesUnthinned", "Forward Rates (Unthinned)"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(STextBlock)
					.Text(this, &FMediaPlayerCustomization::HandleSupportedRatesTextBlockText, EMediaPlaybackDirections::Forward, true)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("UnthinnedForwardRateToolTip", "The rate at which this media can be played unthinned (without skipping frames) in the forward direction."))
			];

		// reverse rates (thinned)
		InformationCategory.AddCustomRow(LOCTEXT("ReverseRatesThinned", "Reverse Rates (Thinned)"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("ReverseRatesThinned", "Reverse Rates (Thinned)"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(STextBlock)
					.Text(this, &FMediaPlayerCustomization::HandleSupportedRatesTextBlockText, EMediaPlaybackDirections::Reverse, false)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("ThinnedReverseRateToolTip", "The rate at which this media can be played thinned (with skipping frames) in the reverse direction."))
			];

		// reverse rates (unthinned)
		InformationCategory.AddCustomRow(LOCTEXT("ReverseRatesUnthinned", "Reverse Rates (Unthinned)"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("ReverseRatesUnthinned", "Reverse Rates (Unthinned)"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(STextBlock)
					.Text(this, &FMediaPlayerCustomization::HandleSupportedRatesTextBlockText, EMediaPlaybackDirections::Reverse, true)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("UnthinnedReverseRateToolTip", "The rate at which this media can be played unthinned (without skipping frames) in the reverse direction."))
			];

		// supports scrubbing
		InformationCategory.AddCustomRow(LOCTEXT("SupportsScrubbing", "Supports Scrubbing"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("SupportsScrubbing", "Supports Scrubbing"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(STextBlock)
					.Text(this, &FMediaPlayerCustomization::HandleSupportsScrubbingTextBlockText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("ScrubbingToolTip", "Whether this media supports scrubbing, i.e. manually controlling the playback speed and position."))
			];

		// supports seeking
		InformationCategory.AddCustomRow(LOCTEXT("SupportsSeeking", "Supports Seeking"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("SupportsSeeking", "Supports Seeking"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(STextBlock)
					.Text(this, &FMediaPlayerCustomization::HandleSupportsSeekingTextBlockText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.ToolTipText(LOCTEXT("SeekingToolTip", "Whether this media supports seeking, i.e. jumping to a specific position."))
			];
	}
}


/* FMediaPlayerCustomization callbacks
 *****************************************************************************/

FText FMediaPlayerCustomization::HandleDurationTextBlockText() const
{
	TOptional<FTimespan> Duration;

	// ensure that all assets have the same value
	for (auto MediaPlayerObject : CustomizedMediaPlayers)
	{
		if (MediaPlayerObject.IsValid())
		{
			FTimespan OtherDuration = Cast<UMediaPlayer>(MediaPlayerObject.Get())->GetDuration();

			if (!Duration.IsSet())
			{
				Duration = OtherDuration;
			}
			else if (OtherDuration != Duration.GetValue())
			{
				return FText::GetEmpty();
			}
		}
	}

	if (!Duration.IsSet())
	{
		return FText::GetEmpty();
	}

	return FText::AsTimespan(Duration.GetValue());
}


FText FMediaPlayerCustomization::HandleSupportedRatesTextBlockText( EMediaPlaybackDirections Direction, bool Unthinned ) const
{
	TOptional<TRange<float>> Rates;

	// ensure that all assets have the same value
	for (auto MediaPlayerObject : CustomizedMediaPlayers)
	{
		if (MediaPlayerObject.IsValid())
		{
			IMediaPlayerPtr Player = Cast<UMediaPlayer>(MediaPlayerObject.Get())->GetPlayer();

			if (!Player.IsValid())
			{
				return FText::GetEmpty();
			}

			TRange<float> OtherRates = Player->GetMediaInfo().GetSupportedRates(Direction, Unthinned);

			if (!Rates.IsSet())
			{
				Rates = OtherRates;
			}
			else if (!Player.IsValid() || (OtherRates != Rates.GetValue()))
			{
				return FText::GetEmpty();
			}
		}
	}

	if (!Rates.IsSet())
	{
		return FText::GetEmpty();
	}

	if ((Rates.GetValue().GetLowerBoundValue() == 0.0f) && (Rates.GetValue().GetUpperBoundValue() == 0.0f))
	{
		return LOCTEXT("RateNotSupported", "Not supported");
	}

	return FText::Format(LOCTEXT("RatesFormat", "{0} to {1}"),
		FText::AsNumber(Rates.GetValue().GetLowerBoundValue()),
		FText::AsNumber(Rates.GetValue().GetUpperBoundValue()));
}


FText FMediaPlayerCustomization::HandleSupportsScrubbingTextBlockText() const
{
	TOptional<bool> SupportsScrubbing;

	// ensure that all assets have the same value
	for (auto MediaPlayerObject : CustomizedMediaPlayers)
	{
		if (MediaPlayerObject.IsValid())
		{
			bool OtherSupportsScrubbing = Cast<UMediaPlayer>(MediaPlayerObject.Get())->SupportsScrubbing();

			if (!SupportsScrubbing.IsSet())
			{
				SupportsScrubbing = OtherSupportsScrubbing;
			}
			else if (OtherSupportsScrubbing != SupportsScrubbing.GetValue())
			{
				return FText::GetEmpty();
			}
		}
	}

	if (!SupportsScrubbing.IsSet())
	{
		return FText::GetEmpty();
	}

	return SupportsScrubbing.GetValue() ? GTrue : GFalse;
}


FText FMediaPlayerCustomization::HandleSupportsSeekingTextBlockText() const
{
	TOptional<bool> SupportsSeeking;

	// ensure that all assets have the same value
	for (auto MediaPlayerObject : CustomizedMediaPlayers)
	{
		if (MediaPlayerObject.IsValid())
		{
			bool OtherSupportsSeeking = Cast<UMediaPlayer>(MediaPlayerObject.Get())->SupportsSeeking();

			if (!SupportsSeeking.IsSet())
			{
				SupportsSeeking = OtherSupportsSeeking;
			}
			else if (OtherSupportsSeeking != SupportsSeeking.GetValue())
			{
				return FText::GetEmpty();
			}
		}
	}

	if (!SupportsSeeking.IsSet())
	{
		return FText::GetEmpty();
	}

	return SupportsSeeking.GetValue() ? GTrue : GFalse;
}


FString FMediaPlayerCustomization::HandleUrlPickerFilePath() const
{
	FString Url;
	UrlProperty->GetValue(Url);

	return Url;
}


FString FMediaPlayerCustomization::HandleUrlPickerFileTypeFilter() const
{
	FString Filter = TEXT("All files (*.*)|*.*");

	auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		return Filter;
	}

	FMediaFileTypes FileTypes;
	MediaModule->GetSupportedFileTypes(FileTypes);

	if (FileTypes.Num() == 0)
	{
		return Filter;
	}

	FString AllExtensions;
	FString AllFilters;
			
	for (auto& Format : FileTypes)
	{
		if (!AllExtensions.IsEmpty())
		{
			AllExtensions += TEXT(";");
		}

		AllExtensions += TEXT("*.") + Format.Key;
		AllFilters += TEXT("|") + Format.Value.ToString() + TEXT(" (*.") + Format.Key + TEXT(")|*.") + Format.Key;
	}

	Filter = TEXT("All movie files (") + AllExtensions + TEXT(")|") + AllExtensions + TEXT("|") + Filter + AllFilters;

	return Filter;
}


void FMediaPlayerCustomization::HandleUrlPickerPathPicked( const FString& PickedPath )
{
	if (PickedPath.IsEmpty() || PickedPath.StartsWith(TEXT("./")) || PickedPath.Contains(TEXT("://")))
	{
		UrlProperty->SetValue(PickedPath);
	}
	else
	{	
		FString FullUrl = FPaths::ConvertRelativePathToFull(PickedPath);
		const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());

		if (FullUrl.StartsWith(FullGameContentDir))
		{
			FPaths::MakePathRelativeTo(FullUrl, *FullGameContentDir);
			FullUrl = FString(TEXT("./")) + FullUrl;
		}

		UrlProperty->SetValue(FullUrl);
	}
}


EVisibility FMediaPlayerCustomization::HandleUrlWarningIconVisibility() const
{
	FString Url;

	if ((UrlProperty->GetValue(Url) != FPropertyAccess::Success) || Url.IsEmpty() || Url.Contains(TEXT("://")))
	{
		return EVisibility::Hidden;
	}

	const FString FullMoviesPath = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir() / TEXT("Movies"));
	const FString FullUrl = FPaths::ConvertRelativePathToFull(FPaths::IsRelative(Url) ? FPaths::GameContentDir() / Url : Url);

	if (FullUrl.StartsWith(FullMoviesPath))
	{
		if (FPaths::FileExists(FullUrl))
		{
			return EVisibility::Hidden;
		}

		// file doesn't exist
		return EVisibility::Visible;
	}

	// file not inside Movies folder
	return EVisibility::Visible;
}


#undef LOCTEXT_NAMESPACE
