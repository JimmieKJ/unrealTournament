// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"

#include "SoundDefinitions.h"
#include "Dialogs/DlgSoundWaveOptions.h"
#include "SoundPreviewThread.h"
#include "SlateBasics.h"
#include "Sound/SoundWave.h"

/** 
 * Info used to setup the rows of the sound quality previewer
 */
FPreviewInfo SSoundWaveCompressionOptions::PreviewInfo[] =
{
	FPreviewInfo( 5 ),
	FPreviewInfo( 10 ),
	FPreviewInfo( 15 ),
	FPreviewInfo( 20 ),
	FPreviewInfo( 25 ),
	FPreviewInfo( 30 ),
	FPreviewInfo( 35 ),
	FPreviewInfo( 40 ),
	FPreviewInfo( 50 ),
	FPreviewInfo( 60 ),
	FPreviewInfo( 70 ) 
};

FPreviewInfo::FPreviewInfo( int32 Quality )
{
	QualitySetting = Quality;

	OriginalSize = 0;

	DecompressedOggVorbis = NULL;
	DecompressedXMA = NULL;
	DecompressedPS3 = NULL;

	OggVorbisSize = 0;
	XMASize = 0;
	PS3Size = 0;
}

void FPreviewInfo::Cleanup( void )
{
	if( DecompressedOggVorbis )
	{
		FMemory::Free( DecompressedOggVorbis );
		DecompressedOggVorbis = NULL;
	}

	if( DecompressedXMA )
	{
		FMemory::Free( DecompressedXMA );
		DecompressedXMA = NULL;
	}

	if( DecompressedPS3 )
	{
		FMemory::Free( DecompressedPS3 );
		DecompressedPS3 = NULL;
	}

	OriginalSize = 0;
	OggVorbisSize = 0;
	XMASize = 0;
	PS3Size = 0;
}

namespace SoundWaveCompressionOptions
{
	static const FName ColumnID_QualityLabel( "Quality" );
	static const FName ColumnID_OriginalDataSizeLabel( "Original DataSize(Kb)" );
	static const FName ColumnID_VorbisDataSizeLabel( "Vorbis DataSize(Kb)" );
	static const FName ColumnID_XMADataSizeLabel( "XMA DataSize(Kb)" );
	static const FName ColumnID_PS3DataSizeLabel( "PS3 DataSize(Kb)" );
}

struct FPreviewInfoContainer
{
	FPreviewInfo* PreviewInfo;

	FPreviewInfoContainer(FPreviewInfo* InPreviewInfo) : PreviewInfo(InPreviewInfo)
	{}
};

/** Widget that represents a row in the sound wave compression's List control.  Generates widgets for each column on demand. */
class SSoundWaveCompressionListRow
	: public SMultiColumnTableRow< FSoundWaveCompressionListItemPtr >
{

public:

	SLATE_BEGIN_ARGS( SSoundWaveCompressionListRow ){}
		SLATE_ARGUMENT(FSoundWaveCompressionListItemPtr, Item)
		SLATE_ARGUMENT(USoundWave*, SoundWave)
	SLATE_END_ARGS()

		/** Construct function for this widget */
		void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		Item = InArgs._Item;
		SoundWave = InArgs._SoundWave;

		SMultiColumnTableRow< FSoundWaveCompressionListItemPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
	}


	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the List row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		// Create the widget for this item
		TSharedPtr< SWidget > TableRowContent;

		if( ColumnName == SoundWaveCompressionOptions::ColumnID_QualityLabel )
		{
			TableRowContent =
				SNew( STextBlock )
					.Text(FText::AsNumber(Item->PreviewInfo->QualitySetting));
		}
		else if( ColumnName == SoundWaveCompressionOptions::ColumnID_OriginalDataSizeLabel )
		{
			TableRowContent =
				SNew( STextBlock )
					.Text(this, &SSoundWaveCompressionListRow::GetOriginalDataSize);
		}
		else if( ColumnName == SoundWaveCompressionOptions::ColumnID_VorbisDataSizeLabel )
		{
			TableRowContent =
				SNew( STextBlock )
					.Text(this, &SSoundWaveCompressionListRow::GetVorbisDataSize)
					.OnDoubleClicked(this, &SSoundWaveCompressionListRow::PlayVorbis);
		}
		else if( ColumnName == SoundWaveCompressionOptions::ColumnID_XMADataSizeLabel )
		{
			TableRowContent =
				SNew( STextBlock )
					.Text(this, &SSoundWaveCompressionListRow::GetXMADataSize)
					.OnDoubleClicked(this, &SSoundWaveCompressionListRow::PlayXMA);
		}
		else if( ColumnName == SoundWaveCompressionOptions::ColumnID_PS3DataSizeLabel )
		{
			TableRowContent =
				SNew( STextBlock )
					.Text(this, &SSoundWaveCompressionListRow::GetPS3DataSize)
					.OnDoubleClicked(this, &SSoundWaveCompressionListRow::PlayPS3);
		}

		return TableRowContent.ToSharedRef();
	}

private:
	/** Common code to format a data size percentage string */
	static FText GetFormattedDataSizeText(const int32 InSize, const int32 InOriginalSize)
	{
		static const FNumberFormattingOptions DataSizeFormat = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		static const FNumberFormattingOptions OriginalDataSizePercFormat = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(1)
			.SetMaximumFractionalDigits(1);

		const float SizePerc = (InOriginalSize == 0) ? 0.0f : static_cast<float>(InSize) / static_cast<float>(InOriginalSize);
		return FText::Format(
			NSLOCTEXT("SoundWaveCompressionListRow", "DataSizePercentageOriginalFmt", "{0} ({1})"), 
			FText::AsNumber(static_cast<float>(InSize) / 1024.0f, &DataSizeFormat), 
			FText::AsPercent(SizePerc, &OriginalDataSizePercFormat)
			);
	}

	/** Retrieves the original data size string. */
	FText GetOriginalDataSize() const
	{
		return GetFormattedDataSizeText(Item->PreviewInfo->OriginalSize, Item->PreviewInfo->OriginalSize);
	}

	/** Retrieves the Vorbis data size string. */
	FText GetVorbisDataSize() const
	{
		return GetFormattedDataSizeText(Item->PreviewInfo->OggVorbisSize, Item->PreviewInfo->OriginalSize);
	}

	/** Plays the Vorbis sound wave. */
	FReply PlayVorbis()
	{
		SoundWave->RawPCMData = Item->PreviewInfo->DecompressedOggVorbis;

		PlayPreviewSound();

		return FReply::Handled();
	}

	/** Retrieves the XMA data size string. */
	FText GetXMADataSize() const
	{
		return GetFormattedDataSizeText(Item->PreviewInfo->XMASize, Item->PreviewInfo->OriginalSize);
	}

	/** Plays the XMA sound wave. */
	FReply PlayXMA()
	{
		SoundWave->RawPCMData = Item->PreviewInfo->DecompressedXMA;

		PlayPreviewSound();

		return FReply::Handled();
	}

	/** Retrieves the PS3 data size string. */
	FText GetPS3DataSize() const
	{
		return GetFormattedDataSizeText(Item->PreviewInfo->PS3Size, Item->PreviewInfo->OriginalSize);
	}

	/** Plays the PS3 sound wave. */
	FReply PlayPS3()
	{
		SoundWave->RawPCMData = Item->PreviewInfo->DecompressedPS3;

		PlayPreviewSound();

		return FReply::Handled();
	}

	/** Starts playing the sound preview. */
	void PlayPreviewSound()
	{
		// As this sound now has RawPCMData, this will set it to DTYPE_Preview
		SoundWave->DecompressionType = DTYPE_Setup;

		GEditor->PlayPreviewSound( SoundWave );
	}

	/** Stops the sound preview if it is playing. */
	void StopPreviewSound()
	{
		GEditor->ResetPreviewAudioComponent();
	}

private:
	/** The data this row represents. */
	FSoundWaveCompressionListItemPtr Item;
	
	/** Node to perform all of our modifications on. */
	USoundWave* SoundWave;
};

bool SSoundWaveCompressionOptions::bQualityPreviewerActive(false);

void SSoundWaveCompressionOptions::Construct( const FArguments& InArgs )
{
	bQualityPreviewerActive = true;

	SoundWave = InArgs._SoundWave;

	for( int32 i = 0; i < sizeof( PreviewInfo ) / sizeof( FPreviewInfo ); i++ )
	{
		ListData.Add(MakeShareable(new FPreviewInfoContainer(&PreviewInfo[i])));
	}

	TSharedRef< SHeaderRow > HeaderRowWidget =
		SNew( SHeaderRow )

			.Visibility( EVisibility::Visible )

			// Quality label column
			+ SHeaderRow::Column( SoundWaveCompressionOptions::ColumnID_QualityLabel )
				.DefaultLabel( NSLOCTEXT("SoundWaveOptions", "QualityColumnLabel", "Quality") )
		
			// Original Data Size label column
			+ SHeaderRow::Column( SoundWaveCompressionOptions::ColumnID_OriginalDataSizeLabel )
				.DefaultLabel( NSLOCTEXT("SoundWaveOptions", "OriginalDataSizeColumnLabel", "Original DataSize(Kb)") )
				.FillWidth( 3.0f )

			// Vorbis Data Size label column
			+ SHeaderRow::Column( SoundWaveCompressionOptions::ColumnID_VorbisDataSizeLabel )
				.DefaultLabel( NSLOCTEXT("SoundWaveOptions", "VorbisDataSizeColumnLabel", "Vorbis DataSize(Kb)") )
				.FillWidth( 3.0f )
			
			// XMA Data Size label column
			+ SHeaderRow::Column( SoundWaveCompressionOptions::ColumnID_XMADataSizeLabel )
				.DefaultLabel( NSLOCTEXT("SoundWaveOptions", "XMADataSizeColumnLabel", "XMA DataSize(Kb)") )
				.FillWidth( 3.0f )
			
			// PS3 Data Size label column
			+ SHeaderRow::Column( SoundWaveCompressionOptions::ColumnID_PS3DataSizeLabel )
				.DefaultLabel( NSLOCTEXT("SoundWaveOptions", "PS3DataSizeColumnLabel", "PS3 DataSize(Kb)") )
				.FillWidth( 3.0f );

	this->ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
				.AutoHeight()				
			[
				SAssignNew(SoundWaveCompressionList, SSoundWaveCompressionListView)
					// Point the List to our array of root-level items.  Whenever this changes, we'll call RequestListRefresh()
					.ListItemsSource( &ListData )

					// Generates the actual widget for a list item
					.OnGenerateRow( this, &SSoundWaveCompressionOptions::OnGenerateRowForSoundWavecompressionList ) 

					// Header for the List
					.HeaderRow(HeaderRowWidget)
			]

			+SVerticalBox::Slot()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SOverlay)
						.Visibility(this, &SSoundWaveCompressionOptions::IsProgressBarVisible)

					+SOverlay::Slot()
					[
						SNew(SProgressBar)
							.Percent(this, &SSoundWaveCompressionOptions::GetProgress)
					]

					+SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(this, &SSoundWaveCompressionOptions::GetProgressString)
					]
				]

				+SHorizontalBox::Slot()
					.AutoWidth()
				[
					SNew(SButton)
						.OnClicked(this, &SSoundWaveCompressionOptions::OnOK_Clicked)
						[
							SNew(STextBlock)
								.Text(NSLOCTEXT("SoundWaveOptions", "OK", "OK"))
						]
				]

				+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked(this, &SSoundWaveCompressionOptions::OnCancel_Clicked)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SoundWaveOptions", "Cancel", "Cancel"))
						]
					]
			]
		];

	SoundWaveCompressionList->RequestListRefresh();

	CreateCompressedWaves();
}

SSoundWaveCompressionOptions::~SSoundWaveCompressionOptions()
{
	SoundPreviewThreadRunnable->Stop();
	SoundPreviewThread->WaitForCompletion();
	delete SoundPreviewThread;
	SoundPreviewThread = NULL;
	delete SoundPreviewThreadRunnable;
	SoundPreviewThreadRunnable = NULL;

	GEditor->ResetPreviewAudioComponent();
	SoundWave->RawPCMData = NULL;
	SoundWave->DecompressionType = DTYPE_Setup;

	for( int32 i = 0; i < sizeof( PreviewInfo ) / sizeof( FPreviewInfo ); i++ )
	{
		PreviewInfo[i].Cleanup();
	}

	bQualityPreviewerActive = false;
}

void SSoundWaveCompressionOptions::CreateCompressedWaves( void )
{
	OriginalQuality = SoundWave->CompressionQuality;
	int32 Count = sizeof( PreviewInfo ) / sizeof( FPreviewInfo );

	SoundPreviewThreadRunnable = new FSoundPreviewThread( Count, SoundWave, PreviewInfo );

	SoundPreviewThread = FRunnableThread::Create(SoundPreviewThreadRunnable, TEXT("SoundPreviewThread"));
}

TOptional<float> SSoundWaveCompressionOptions::GetProgress() const
{
	if(SoundPreviewThreadRunnable->GetCount())
	{
		return (float)SoundPreviewThreadRunnable->GetIndex() / (float)SoundPreviewThreadRunnable->GetCount();
	}

	return 0.0f;
}

FText SSoundWaveCompressionOptions::GetProgressString() const
{
	return FText::Format(
		NSLOCTEXT("SoundWaveOptions", "CompressionProgressXOfYFmt", "{0}/{1}"), 
		FText::AsNumber(SoundPreviewThreadRunnable->GetIndex()), 
		FText::AsNumber(SoundPreviewThreadRunnable->GetCount())
		);
}

EVisibility SSoundWaveCompressionOptions::IsProgressBarVisible() const
{
	return SoundPreviewThreadRunnable->GetIndex() == SoundPreviewThreadRunnable->GetCount() ? EVisibility::Hidden : EVisibility::Visible;
}

FReply SSoundWaveCompressionOptions::OnOK_Clicked()
{
	if( SoundWaveCompressionList->GetSelectedItems().Num() > 0 )
	{
		SoundWave->CompressionQuality = SoundWaveCompressionList->GetSelectedItems()[0]->PreviewInfo->QualitySetting;
		SoundWave->PostEditChange();
		SoundWave->MarkPackageDirty();
	}
	else
	{
		SoundWave->CompressionQuality = OriginalQuality;
	}

	MyWindow.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}

FReply SSoundWaveCompressionOptions::OnCancel_Clicked()
{
	MyWindow.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

TSharedRef< ITableRow > SSoundWaveCompressionOptions::OnGenerateRowForSoundWavecompressionList( FSoundWaveCompressionListItemPtr Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return
		SNew( SSoundWaveCompressionListRow, OwnerTable )
			.Item( Item )
			.SoundWave(SoundWave);
}
