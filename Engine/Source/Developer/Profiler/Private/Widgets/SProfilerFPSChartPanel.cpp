// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"

#define LOCTEXT_NAMESPACE "SProfilerFPSChartPanel"

class SProfilerFPSStatisticsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SProfilerFPSStatisticsPanel )
	{}
		SLATE_ARGUMENT( TSharedPtr<FFPSAnalyzer>, FPSAnalyzer )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		FPSAnalyzer = InArgs._FPSAnalyzer;

		ChildSlot
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding( 2.0f )
			[
				SNew( SVerticalBox )

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("StatisticsLabel", "Statistics"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SProfilerFPSStatisticsPanel::HandleSampleCount)
				]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleMinFPS)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleMaxFPS)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleAverageFPS)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS30)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS25)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS20)
					]
			]	
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

protected:
	FText HandleSampleCount() const
	{
		return FText::Format(LOCTEXT("SamplesCountFmt", "Samples: {0}"), FText::AsNumber(FPSAnalyzer->Samples.Num()));
	}
	FText HandleMinFPS() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("MinFPSFmt", "Min FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(FPSAnalyzer->MinFPS, &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleMaxFPS() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("MaxFPSFmt", "Max FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(FPSAnalyzer->MaxFPS, &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleAverageFPS() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("AverageFPSFmt", "Ave FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(FPSAnalyzer->AveFPS, &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS30() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+30FPSFmt", "+30FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS30 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS25() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+25FPSFmt", "+25FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS25 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS20() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+20FPSFmt", "+20FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS20 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}

protected:
	TSharedPtr<FFPSAnalyzer> FPSAnalyzer;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProfilerFPSChartPanel::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		.Padding( 2.0f )
		[
			SNew( SHorizontalBox )

			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SHistogram)
				.Description(FHistogramDescription(InArgs._FPSAnalyzer.ToSharedRef(), 5, 0, 60, true))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SProfilerFPSStatisticsPanel )
				.FPSAnalyzer( InArgs._FPSAnalyzer )
			]
		]	
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
