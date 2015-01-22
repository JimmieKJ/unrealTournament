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
					.Text(FString::FromInt(Item->FrameNum));
		}
		else if (ColumnName == TEXT("Tag") && OwnerAnalyzerWidget->GroupBy == EQueryGroupMode::ByTag)
		{
			return	SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(Item->GroupName.ToString());
		}
		else if (ColumnName == TEXT("Owner") && OwnerAnalyzerWidget->GroupBy == EQueryGroupMode::ByOwnerTag)
		{
			return	SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(Item->GroupName.ToString());
		}
		else if (ColumnName == TEXT("Time"))
		{
			return	SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
					.Text(this, &SCAQueryTableRow::GetTotalTimeString);
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
					.Text(SCollisionAnalyzer::QueryTypeToString(Query.Type));
		}
		else if (ColumnName == TEXT("Shape"))
		{
			return	SNew(STextBlock)
					.Text(SCollisionAnalyzer::QueryShapeToString(Query.Shape));
		}
		else if (ColumnName == TEXT("Tag"))
		{
			return	SNew(STextBlock)
					.Text(Query.Params.TraceTag.ToString());
		}
		else if (ColumnName == TEXT("Owner"))
		{
			return	SNew(STextBlock)
					.Text(Query.Params.OwnerTag.ToString());
		}
		else if (ColumnName == TEXT("NumBlock"))
		{
			FHitResult* FirstHit = FHitResult::GetFirstBlockingHit(Query.Results);
			bool bStartPenetrating = (FirstHit != NULL) && FirstHit->bStartPenetrating;
			// Draw number in red if we start penetrating
			return	SNew(STextBlock)
					.Text(FString::FromInt(FHitResult::GetNumBlockingHits(Query.Results)))
					.ColorAndOpacity(bStartPenetrating ? FLinearColor(1.f,0.25f,0.25f) : FSlateColor::UseForeground() );
		}
		else if (ColumnName == TEXT("NumTouch"))
		{
			return	SNew(STextBlock)
					.Text(FString::FromInt(FHitResult::GetNumOverlapHits(Query.Results)));
		}
		else if (ColumnName == TEXT("Time"))
		{
			return	SNew(STextBlock)
					.Text(FString::Printf(TEXT("%.3f"), Query.CPUTime));
		}
	}

	return SNullWidget::NullWidget;
}

FString SCAQueryTableRow::GetTotalTimeString() const
{
	check(Item->bIsGroup)
	return FString::Printf(TEXT("%.3f"), Item->TotalCPUTime);
}

#undef LOCTEXT_NAMESPACE