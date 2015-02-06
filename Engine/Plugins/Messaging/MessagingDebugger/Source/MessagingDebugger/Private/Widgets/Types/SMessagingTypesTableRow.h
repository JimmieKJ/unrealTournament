// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMessagingTypesTableRow"


/**
 * Implements a row widget for the message type list.
 */
class SMessagingTypesTableRow
	: public SMultiColumnTableRow<FMessageTracerTypeInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SMessagingTypesTableRow) { }
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_ARGUMENT(FMessageTracerTypeInfoPtr, TypeInfo)
		SLATE_ARGUMENT(TSharedPtr<ISlateStyle>, Style)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InOwnerTableView The table view that owns this row.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const FMessagingDebuggerModelRef& InModel )
	{
		check(InArgs._Style.IsValid());
		check(InArgs._TypeInfo.IsValid());

		HighlightText = InArgs._HighlightText;
		Model = InModel;
		Style = InArgs._Style;
		TypeInfo = InArgs._TypeInfo;

		SMultiColumnTableRow<FMessageTracerTypeInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	// SMultiColumnTableRow interface

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == "Break")
		{
			return SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.33f))
				.BorderImage(Style->GetBrush("BreakpointBorder"));

/*			return SNew(SImage)
				.Image(Style->GetBrush("BreakDisabled"));*/
		}
		else if (ColumnName == "Messages")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.HighlightText(HighlightText)
						.Text(this, &SMessagingTypesTableRow::HandleMessagesText)
				];
		}
		else if (ColumnName == "Name")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.HighlightText(HighlightText)
						.Text(FText::FromName(TypeInfo->TypeName))
				];
		}
		else if (ColumnName == "Visibility")
		{
			return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
						.Style(&Style->GetWidgetStyle<FCheckBoxStyle>("VisibilityCheckbox"))
						.IsChecked(this, &SMessagingTypesTableRow::HandleVisibilityCheckBoxIsChecked)
						.OnCheckStateChanged(this, &SMessagingTypesTableRow::HandleVisibilityCheckBoxCheckStateChanged)
						.ToolTipText(LOCTEXT("VisibilityCheckboxTooltipText", "Toggle visibility of messages of this type"))
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	/** Gets the text for the Messages column. */
	FText HandleMessagesText() const
	{
		return FText::AsNumber(TypeInfo->Messages.Num());
	}

	/** Handles changing the checked state of the visibility check box. */
	void HandleVisibilityCheckBoxCheckStateChanged( ECheckBoxState CheckState )
	{
		Model->SetTypeVisibility(TypeInfo.ToSharedRef(), (CheckState == ECheckBoxState::Checked));
	}

	/** Gets the image for the visibility check box. */
	ECheckBoxState HandleVisibilityCheckBoxIsChecked() const
	{
		if (Model->IsTypeVisible(TypeInfo.ToSharedRef()))
		{
			return ECheckBoxState::Checked;
		}

		return ECheckBoxState::Unchecked;
	}

private:

	/** Holds the highlight string for the message. */
	TAttribute<FText> HighlightText;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;

	/** Holds message type's debug information. */
	FMessageTracerTypeInfoPtr TypeInfo;
};


#undef LOCTEXT_NAMESPACE
