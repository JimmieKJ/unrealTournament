// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SSessionBrowserSessionListRow"


/**
 * Implements a row widget for the session browser list.
 */
class SSessionBrowserSessionListRow
	: public SMultiColumnTableRow<ISessionInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SSessionBrowserSessionListRow) { }
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_ARGUMENT(TSharedPtr<STableViewBase>, OwnerTableView)
		SLATE_ARGUMENT(ISessionInfoPtr, SessionInfo)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		HighlightText = InArgs._HighlightText;
		SessionInfo = InArgs._SessionInfo;

		SMultiColumnTableRow<ISessionInfoPtr>::Construct(FSuperRowType::FArguments().Style(FEditorStyle::Get(), "TableView.Row"), InOwnerTableView);
	}

public:

	/**
	 * Generates the widget for the specified column.
	 *
	 * @param ColumnName The name of the column to generate the widget for.
	 * @return The widget.
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == "Name")
		{
			return SNew(SVerticalBox)

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14))
						.HighlightText(HighlightText)
						.Text(this, &SSessionBrowserSessionListRow::HandleGetSessionName)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
								.Text(FText::FromString(SessionInfo->GetSessionOwner()))
						]

					+ SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("adsf", "1 Instance(s)"))
						]
				];
		}
		else if (ColumnName == "Selection")
		{
			return SNew(SColorBlock)
				.Color(FLinearColor::Gray);
		}
		
		return SNullWidget::NullWidget;
	}

protected:

	/** Callback for getting the name of the session. */
	FText HandleGetSessionName() const
	{
		const FString& SessionName = SessionInfo->GetSessionName();

		// return name of a launched session
		if (!SessionInfo->IsStandalone() || !SessionName.IsEmpty())
		{
			return FText::FromString(SessionName);
		}

		// generate name for a standalone session
		TArray<ISessionInstanceInfoPtr> Instances;
		SessionInfo->GetInstances(Instances);

		if (Instances.Num() > 0)
		{
			const ISessionInstanceInfoPtr& FirstInstance = Instances[0];

			if ((Instances.Num() == 1) && (FirstInstance->GetInstanceId() == FApp::GetInstanceId()))
			{
				return LOCTEXT("ThisApplicationSessionText", "This Application");
			}

			if (FirstInstance->GetDeviceName() == FPlatformProcess::ComputerName())
			{
				return LOCTEXT("UnnamedLocalSessionText", "Unnamed Session (Local)");
			}

			return LOCTEXT("UnnamedRemoteSessionText", "Unnamed Session (Remote)");
		}

		return LOCTEXT("UnnamedSessionText", "Unnamed Session");
	}

private:

	/** Holds the highlight string for the session name. */
	TAttribute<FText> HighlightText;

	/** Holds a reference to the session info that is displayed in this row. */
	ISessionInfoPtr SessionInfo;
};


#undef LOCTEXT_NAMESPACE