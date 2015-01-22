// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

#if !CRASH_REPORT_UNATTENDED_ONLY

#include "SCrashReportClient.h"
#include "CrashReportClientStyle.h"
#include "SlateStyle.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

void SCrashReportClient::Construct(const FArguments& InArgs, TSharedRef<FCrashReportClient> Client)
{
	CrashReportClient = Client;

	TSharedPtr<SVerticalBox> CrashedAppBox;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(3, 0))
		[
			SNew(SVerticalBox)

			// Stuff anchored to the top
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SAssignNew(CrashedAppBox, SVerticalBox)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("Blurb", "Please describe what you were doing immediately before the crash:"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SAssignNew(UserCommentBox, SEditableTextBox)
					.OnTextCommitted(CrashReportClient.ToSharedRef(), &FCrashReportClient::UserCommentChanged)
					.HintText(LOCTEXT("CircumstancesOfCrash", "Circumstances of crash"))
				]
			]

			+SVerticalBox::Slot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.MaxHeight(550)
				[
					SNew(SBorder)
					.BorderImage(new FSlateBoxBrush(TEXT("Common/TextBlockHighlightShape"), FMargin(.5f)))
					.BorderBackgroundColor(FSlateColor(FLinearColor::Black))
					[
						SNew(SScrollBox)
						+SScrollBox::Slot()
						[
							SNew(STextBlock)
							.TextStyle(FCrashReportClientStyle::Get(), "Code")
							.Text(Client, &FCrashReportClient::GetDiagnosticText)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(2)
				[
					SNew(SButton)
					.Text(LOCTEXT("CopyCallstack", "Copy Callstack to Clipboard"))
					.OnClicked(CrashReportClient.ToSharedRef(), &FCrashReportClient::CopyCallstack)
				]
			]

			// Stuff anchored to the bottom
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
				.TextStyle(FCrashReportClientStyle::Get(), "Status")
				.Text(Client, &FCrashReportClient::GetStatusText)
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.Padding(3)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KeyboardShortcuts", "Shift+Enter: submit\nEscape: cancel"))
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("Submit", "Submit Crash Report"))
						.OnClicked(Client, &FCrashReportClient::Submit)
						.Visibility(Client, &FCrashReportClient::SubmitButtonVisibility)
					]

					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(Client, &FCrashReportClient::GetCancelButtonText)
						.OnClicked(Client, &FCrashReportClient::Cancel)
					]
				]
			]
		]
	];

	auto CrashedAppName = CrashReportClient->GetCrashedAppName();

	// Set the text displaying the name of the crashed app, if available
	FText CrashedAppText = CrashedAppName.IsEmpty() ?
		LOCTEXT("CrashedAppNotFound", "An Unreal process has crashed") :
		LOCTEXT("CrashedApp", "The following process has crashed:");

	CrashedAppBox->AddSlot()
	[
		SNew(STextBlock)
		.TextStyle(FCrashReportClientStyle::Get(), "Title")
		.Text(CrashedAppText)
	];

	if (!CrashedAppName.IsEmpty())
	{
		CrashedAppBox->AddSlot()
		.Padding(5)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(CrashedAppName))
		];
	}

	FSlateApplication::Get().SetUnhandledKeyDownEventHandler(FOnKeyEvent::CreateSP(this, &SCrashReportClient::OnUnhandledKeyDown));
}

void SCrashReportClient::SetDefaultFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(UserCommentBox.ToSharedRef());
}

FReply SCrashReportClient::OnUnhandledKeyDown(const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		CrashReportClient->Cancel();
		return FReply::Handled();
	}
	else if (Key == EKeys::Enter)
	{
		if (InKeyEvent.IsShiftDown())
		{
			CrashReportClient->Submit();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE

#endif // !CRASH_REPORT_UNATTENDED_ONLY
