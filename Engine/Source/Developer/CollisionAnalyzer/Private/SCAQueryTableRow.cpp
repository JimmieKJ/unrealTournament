// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CollisionAnalyzerPCH.h"

#define LOCTEXT_NAMESPACE "CollisionAnalyzer"

void SCAQueryTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	OwnerAnalyzerWidgetPtr = InArgs._OwnerAnalyzerWidget;
	SMultiColumnTableRow< TSharedPtr<FQueryTreeItem> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);

	if(Item->bIsGroup)
	{
		BorderImage = FEditorStyle::GetBrush("CollisionAnalyzer.GroupBackground");
	}
}

TSharedRef<SWidget> SCAQueryTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	TSharedPtr<SCollisionAnalyzer> OwnerAnalyzerWidget = OwnerAnalyzerWidgetPtr.Pin();

	// GROUP
	if(Item->bIsGroup)
	{
		if (ColumnName == TEXT("ID"))
		{
			return	SNew(SExpanderArrow, SharedThis(this));
		}
		else if (ColumnName == TEXT("Frame") && OwnerAnalyzerWidget->GroupBy == EQueryGroupMode::ByFrameNum)
		{
			return	SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(FText::AsNumber(Item->FrameNum));
		}
		else if (ColumnName == TEXT("Tag") && OwnerAnalyzerWidget->GroupBy == EQueryGroupMode::ByTag)
		{
			return	SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(FText::FromName(Item->GroupName));
		}
		else if (ColumnName == TEXT("Owner") && OwnerAnalyzerWidget->GroupBy == EQueryGroupMode::ByOwnerTag)
		{
			return	SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(FText::FromName(Item->GroupName));
		}
		else if (ColumnName == TEXT("Time"))
		{
			return	SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(this, &SCAQueryTableRow::GetTotalTimeText);
		}
	}
	// ITEM
	else
	{
		const int32 QueryId = Item->QueryIndex;
		if(QueryId >= OwnerAnalyzerWidget->Analyzer->Queries.Num())
		{
			return	SNew(STextBlock)
					.Text( LOCTEXT("ErrorMessage", "ERROR") );
		}

		FCAQuery& Query = OwnerAnalyzerWidget->Analyzer->Queries[QueryId];

		if (ColumnName == TEXT("ID"))
		{	
			return	SNew(STextBlock)
					.Text( FText::AsNumber(QueryId) );
		}
		else if (ColumnName == TEXT("Frame"))
		{
			return	SNew(STextBlock)
					.Text( FText::AsNumber(Query.FrameNum) );
		}
		else if (ColumnName == TEXT("Type"))
		{
			return	SNew(STextBlock)
					.Text(FText::FromString(SCollisionAnalyzer::QueryTypeToString(Query.Type)));
		}
		else if (ColumnName == TEXT("Shape"))
		{
			return	SNew(STextBlock)
					.Text(FText::FromString(SCollisionAnalyzer::QueryShapeToString(Query.Shape)));
		}
		else if (ColumnName == TEXT("Tag"))
		{
			return	SNew(STextBlock)
					.Text(FText::FromName(Query.Params.TraceTag));
		}
		else if (ColumnName == TEXT("Owner"))
		{
			return	SNew(STextBlock)
					.Text(FText::FromName(Query.Params.OwnerTag));
		}
		else if (ColumnName == TEXT("NumBlock"))
		{
			FHitResult* FirstHit = FHitResult::GetFirstBlockingHit(Query.Results);
			bool bStartPenetrating = (FirstHit != NULL) && FirstHit->bStartPenetrating;
			// Draw number in red if we start penetrating
			return	SNew(STextBlock)
					.Text(FText::AsNumber(FHitResult::GetNumBlockingHits(Query.Results)))
					.ColorAndOpacity(bStartPenetrating ? FLinearColor(1.f,0.25f,0.25f) : FSlateColor::UseForeground() );
		}
		else if (ColumnName == TEXT("NumTouch"))
		{
			return	SNew(STextBlock)
					.Text(FText::AsNumber(FHitResult::GetNumOverlapHits(Query.Results)));
		}
		else if (ColumnName == TEXT("Time"))
		{
			static const FNumberFormattingOptions TimeFormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(3)
				.SetMaximumFractionalDigits(3);

			return	SNew(STextBlock)
					.Text(FText::AsNumber(Query.CPUTime, &TimeFormatOptions));
		}
	}

	return SNullWidget::NullWidget;
}

FText SCAQueryTableRow::GetTotalTimeText() const
{
	static const FNumberFormattingOptions TimeFormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(3)
		.SetMaximumFractionalDigits(3);

	check(Item->bIsGroup)
	return FText::AsNumber(Item->TotalCPUTime, &TimeFormatOptions);
}

#undef LOCTEXT_NAMESPACE