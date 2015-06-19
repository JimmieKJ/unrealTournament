// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

#if !CRASH_REPORT_UNATTENDED_ONLY

#include "SCrashReportClient.h"
#include "CrashReportClientStyle.h"
#include "SlateStyle.h"
#include "SThrobber.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

static void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata, TSharedRef<SWidget> ParentWidget)
{
	const FString* UrlPtr = Metadata.Find(TEXT("href"));
	if(UrlPtr)
	{
		FPlatformProcess::LaunchURL(**UrlPtr, nullptr, nullptr);
	}
}

void SCrashReportClient::Construct(const FArguments& InArgs, TSharedRef<FCrashReportClient> Client)
{
	CrashReportClient = Client;

	auto CrashedAppName = CrashReportClient->GetCrashedAppName();

	// Set the text displaying the name of the crashed app, if available
	FText CrashedAppText = CrashedAppName.IsEmpty() ?
		LOCTEXT("CrashedAppNotFound", "An Unreal process has crashed") :
		LOCTEXT("CrashedApp", "The following process has crashed: ");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// Stuff anchored to the top
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FCrashReportClientStyle::Get(), "Title")
					.Text(CrashedAppText)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FCrashReportClientStyle::Get(), "Title")
					.Text(FText::FromString(CrashedAppName))
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin( 4, 10 ) )
			[
				SNew( SRichTextBlock )
				.Text( LOCTEXT("CrashDetailed", "We are very sorry that this crash occurred. Our goal is to prevent crashes like this from occurring in the future. Please help us track down and fix this crash by providing detailed information about what you were doing so that we may reproduce the crash and fix it quickly. You can also log a Bug Report with us at <a id=\"browser\" href=\"https://answers.unrealengine.com\" style=\"Hyperlink\">AnswerHub</> and work directly with support staff to report this issue."))
				.AutoWrapText(true)
				.DecoratorStyleSet( &FCoreStyle::Get() )
				+ SRichTextBlock::HyperlinkDecorator( TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateStatic( &OnBrowserLinkClicked, AsShared() ) )
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin( 4, 10 ) )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("CrashThanks", "Thanks for your help in improving the Unreal Engine."))
				.AutoWrapText(true)
			]

			+SVerticalBox::Slot()
			.Padding(FMargin(4, 10, 4, 4))
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+SSplitter::Slot()
				.Value(0.3f)
				[
					SNew( SOverlay )
					+ SOverlay::Slot()
					[
						SAssignNew( CrashDetailsInformation, SMultiLineEditableTextBox )
						.Style( &FCrashReportClientStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>( "NormalEditableTextBox" ) )
						.OnTextCommitted( CrashReportClient.ToSharedRef(), &FCrashReportClient::UserCommentChanged )
						.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT( "Slate/Fonts/Roboto-Regular.ttf" ), 9 ) )
						.AutoWrapText( true )
						.BackgroundColor( FSlateColor( FLinearColor::Black ) )
						.ForegroundColor( FSlateColor( FLinearColor::White * 0.8f ) )
					]

					// HintText is not implemented in SMultiLineEditableTextBox, so this is a workaround.
					+ SOverlay::Slot()
					[
						SNew(STextBlock)
						.Margin( FMargin(4,2,0,0) )
						.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT( "Slate/Testing/Fonts/Roboto-Italic.ttf" ), 9 ) )
						.ColorAndOpacity( FSlateColor( FLinearColor::White * 0.5f ) )
						.Text( LOCTEXT( "CrashProvide", "Please provide detailed information about what you were doing when the crash occurred." ) )
						.Visibility( this, &SCrashReportClient::IsHintTextVisible )
					]	
				]

				+SSplitter::Slot()
				.Value(0.7f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SOverlay)

						+ SOverlay::Slot()			
						[
							SNew(SColorBlock)
							.Color(FLinearColor::Black)
						]

						+ SOverlay::Slot()
						[
							SNew(STextBlock)
							.Margin(FMargin(4, 2, 0, 8))
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Testing/Fonts/Roboto-Italic.ttf"), 9))
							.Text(LOCTEXT("CrashReportData", "Crash report data:"))
							.ColorAndOpacity(FSlateColor(FLinearColor::White * 0.5f))
							.AutoWrapText(false)
						]
					]

					+ SVerticalBox::Slot()
					.FillHeight(0.7f)
					[
						SNew(SOverlay)
		
						+ SOverlay::Slot()
						[
							SNew( SMultiLineEditableTextBox )
							.Style( &FCrashReportClientStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>( "NormalEditableTextBox" ) )
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT( "Slate/Fonts/Roboto-Regular.ttf" ), 8 ) )
							.AutoWrapText( false )
							.IsReadOnly( true )
							.ReadOnlyForegroundColor( FSlateColor( FLinearColor::Black ) )
							.BackgroundColor( FSlateColor( FLinearColor::Black ) )
							.ForegroundColor( FSlateColor( FLinearColor::White * 0.8f ) )
							.Text( Client, &FCrashReportClient::GetDiagnosticText )
						]


						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SThrobber)
							.Visibility(CrashReportClient.ToSharedRef(), &FCrashReportClient::IsThrobberVisible)
							.NumPieces(5)
						]
					]
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin( 4, 12 ) )
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(CrashReportClient.ToSharedRef(), &FCrashReportClient::SCrashReportClient_OnCheckStateChanged)
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("IAgree", "I agree to be contacted by Epic Games via email if additional information about this crash would help fix it."))
				]
			]

			// Stuff anchored to the bottom
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin(4, 4+16, 4, 4) )
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0)
				[			
					SNew(SSpacer)
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding( FMargin(6) )
				[
					SNew(SButton)
					.ContentPadding( FMargin(8,2) )
					.Text(LOCTEXT("SendAndRestartEditor", "Send and Restart"))
					.OnClicked(Client, &FCrashReportClient::SubmitAndRestart)
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding( FMargin(0) )
				[
					SNew(SButton)
					.ContentPadding( FMargin(8,2) )
					.Text(LOCTEXT("Send", "Send"))
					.OnClicked(Client, &FCrashReportClient::Submit)
				]
			]
		]
	];

	FSlateApplication::Get().SetUnhandledKeyDownEventHandler(FOnKeyEvent::CreateSP(this, &SCrashReportClient::OnUnhandledKeyDown));
}

FReply SCrashReportClient::OnUnhandledKeyDown(const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter)
	{
		CrashReportClient->Submit();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

EVisibility SCrashReportClient::IsHintTextVisible() const
{
	return CrashDetailsInformation->GetText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE

#endif // !CRASH_REPORT_UNATTENDED_ONLY
