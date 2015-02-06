// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SDeviceProcessesProcessListRow"


/**
 * Implements a row widget for the process list view.
 */
class SDeviceProcessesProcessListRow
	: public SMultiColumnTableRow<FDeviceProcessesProcessTreeNodePtr>
{
public:

	SLATE_BEGIN_ARGS(SDeviceProcessesProcessListRow) { }
		SLATE_ARGUMENT(FDeviceProcessesProcessTreeNodePtr, Node)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		Node = InArgs._Node;

		SMultiColumnTableRow<FDeviceProcessesProcessTreeNodePtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	/**
	 * Generates the widget for the specified column.
	 *
	 * @param ColumnName The name of the column to generate the widget for.
	 *
	 * @return The widget.
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == TEXT("Name"))
		{
			TSharedRef<SWidget> Content = SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(Node->GetProcessInfo().Name))
				];

			if (OwnerTablePtr.Pin()->GetTableViewMode() == ETableViewMode::Tree)
			{
				return 
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Top)
						[
							SNew(SExpanderArrow, SharedThis(this))
						]
					
					+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							Content
						];
			}
			else
			{
				return Content;
			}
		}
		else if (ColumnName == TEXT("Parent"))
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(Node->GetProcessInfo().ParentId))
				];
		}
		else if (ColumnName == TEXT("PID"))
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(Node->GetProcessInfo().Id))
				];
		}
		else if (ColumnName == TEXT("Threads"))
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(Node->GetProcessInfo().Threads.Num()))
				];
		}
		else if (ColumnName == TEXT("User"))
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(Node->GetProcessInfo().UserName))
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	// Holds the name of the process.
	FDeviceProcessesProcessTreeNodePtr Node;
};


#undef LOCTEXT_NAMESPACE
