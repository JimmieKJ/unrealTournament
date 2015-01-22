// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SNewsFeedListRow"


/**
 * Implements a row widget for the news list view.
 */
class SNewsFeedListRow
	: public SMultiColumnTableRow<FNewsFeedItemPtr>
{
public:

	SLATE_BEGIN_ARGS(SNewsFeedListRow) { }
		SLATE_ARGUMENT(FNewsFeedItemPtr, NewsFeedItem)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const FNewsFeedCacheRef& InNewsFeedCache )
	{
		check(InArgs._NewsFeedItem.IsValid());

		NewsFeedCache = InNewsFeedCache;
		NewsFeedItem = InArgs._NewsFeedItem;
		
		SMultiColumnTableRow<FNewsFeedItemPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	/**
	 * Generates the widget for the specified column.
	 *
	 * @param ColumnName The name of the column to generate the widget for.
	 * @return The widget.
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == TEXT("Icon"))
		{
			return SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(4.0f)
				[
					SNew(SBox)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						.HeightOverride(40.0f)
						.WidthOverride(40.0f)
						[
							SNew(SImage)
								.Image(this, &SNewsFeedListRow::HandleIconImage)
						]
				];
		}
		else if (ColumnName == TEXT("Content"))
		{
			return SNew(SVerticalBox)
				.ToolTipText(NewsFeedItem->FullText)

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
								.ColorAndOpacity(this, &SNewsFeedListRow::HandleTitleColorAndOpacity)
								.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12))
								.Text(NewsFeedItem->Title)							
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(24.0f, 0.0f, 0.0f, 0.0f)
						.VAlign(VAlign_Top)
						[
							SNew(STextBlock)
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
								.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8))
								.Text(FText::AsDate(NewsFeedItem->Issued))
						]
				]
				
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 0.0f, 4.0f, 8.0f)
				[
					SNew(STextBlock)
						.AutoWrapText(true)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text(NewsFeedItem->Excerpt)
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	// Callback for getting the image brush of the icon.
	const FSlateBrush* HandleIconImage( ) const
	{
		return NewsFeedCache->GetIcon(NewsFeedItem->IconName);
	}

	// Callback for getting the color of the title text.
	FSlateColor HandleTitleColorAndOpacity() const
	{
		if (NewsFeedItem->Read)
		{
			return FSlateColor::UseSubduedForeground();
		}

		return FSlateColor::UseForeground();
	}

private:

	// Holds a pointer to the news feed cache.
	FNewsFeedCachePtr NewsFeedCache;

	// Holds the news item shown in this row.
	FNewsFeedItemPtr NewsFeedItem;
};


#undef LOCTEXT_NAMESPACE
