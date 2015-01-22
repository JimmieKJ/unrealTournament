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
	FString HandleSampleCount() const
	{
		FString Text = FString::Printf(TEXT("Samples: %6d"), FPSAnalyzer->Samples.Num());
		return Text;
	}
	FString HandleMinFPS() const
	{
		FString Text = FPSAnalyzer->Samples.Num() > 0 ? FString::Printf(TEXT("Min FPS: %3.2f"), FPSAnalyzer->MinFPS) : TEXT("Min FPS: ");
		return Text;
	}
	FString HandleMaxFPS() const
	{
		FString Text = FPSAnalyzer->Samples.Num() > 0 ? FString::Printf(TEXT("Max FPS: %3.2f"), FPSAnalyzer->MaxFPS) : TEXT("Max FPS: ");
		return Text;
	}
	FString HandleAverageFPS() const
	{
		FString Text = FPSAnalyzer->Samples.Num() > 0 ? FString::Printf(TEXT("Ave FPS: %3.2f"), FPSAnalyzer->AveFPS) : TEXT("Ave FPS: ");
		return Text;
	}
	FString HandleFPS30() const
	{
		FString Text = FPSAnalyzer->Samples.Num() > 0 ? FString::Printf(TEXT("+30FPS: %3.2f"), 100.0f*(float)FPSAnalyzer->FPS30 / (float)FPSAnalyzer->Samples.Num()) : TEXT("+30FPS: ");
		return Text;
	}
	FString HandleFPS25() const
	{
		FString Text = FPSAnalyzer->Samples.Num() > 0 ? FString::Printf(TEXT("+25FPS: %3.2f"), 100.0f*(float)FPSAnalyzer->FPS25 / (float)FPSAnalyzer->Samples.Num()) : TEXT("+25FPS: ");
		return Text;
	}
	FString HandleFPS20() const
	{
		FString Text = FPSAnalyzer->Samples.Num() > 0 ? FString::Printf(TEXT("+20FPS: %3.2f"), 100.0f*(float)FPSAnalyzer->FPS20 / (float)FPSAnalyzer->Samples.Num()) : TEXT("+20FPS: ");
		return Text;
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
