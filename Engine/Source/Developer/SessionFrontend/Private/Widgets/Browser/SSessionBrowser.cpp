// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SessionFrontendPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SSessionBrowser"


/* SSessionBrowser structors
 *****************************************************************************/

SSessionBrowser::~SSessionBrowser()
{
	if (SessionManager.IsValid())
	{
		SessionManager->OnSelectedSessionChanged().RemoveAll(this);
		SessionManager->OnSessionsUpdated().RemoveAll(this);
	}
}


/* SSessionBrowser interface
 *****************************************************************************/

void SSessionBrowser::Construct( const FArguments& InArgs, ISessionManagerRef InSessionManager )
{
	SessionManager = InSessionManager;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						// session combo box
						SAssignNew(SessionComboBox, SComboBox<ISessionInfoPtr>)
							.ContentPadding(FMargin(6.0f, 2.0f))
							.OptionsSource(&SessionList)
							.ToolTipText(LOCTEXT("SessionComboBoxToolTip", "Select the game session to interact with."))
							.OnGenerateWidget(this, &SSessionBrowser::HandleSessionComboBoxGenerateWidget)
							.OnSelectionChanged(this, &SSessionBrowser::HandleSessionComboBoxSelectionChanged)
							[
								SNew(STextBlock)
									.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 13))
									.Text(this, &SSessionBrowser::HandleSessionComboBoxText)
							]
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(this, &SSessionBrowser::HandleSessionDetailsText)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f)
					[
						SNew(SSeparator)
							.Orientation(Orient_Vertical)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						// terminate button
						SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
							.ContentPadding(FMargin(6.0f, 2.0f))
							.IsEnabled(this, &SSessionBrowser::HandleTerminateSessionButtonIsEnabled)
							.OnClicked(this, &SSessionBrowser::HandleTerminateSessionButtonClicked)
							.ToolTipText(LOCTEXT("TerminateButtonTooltip", "Shuts down all game instances that are part of this session."))
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SImage)
											.Image(FEditorStyle::GetBrush("SessionBrowser.Terminate"))
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(4.0f, 1.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock)
											.TextStyle(FEditorStyle::Get(), "SessionBrowser.Terminate.Font")
											.Text(LOCTEXT("TerminateSessionButtonLabel", "Terminate Session"))
									]
							]
					]
			]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						// instance list
						SAssignNew(InstanceListView, SSessionInstanceList, SessionManager.ToSharedRef())
							.SelectionMode(ESelectionMode::Multi)
					]
			]
	];

	SessionManager->OnSelectedSessionChanged().AddSP(this, &SSessionBrowser::HandleSessionManagerSelectedSessionChanged);
	SessionManager->OnSessionsUpdated().AddSP(this, &SSessionBrowser::HandleSessionManagerSessionsUpdated);

	ReloadSessions();
}


/* SSessionBrowser implementation
 *****************************************************************************/

void SSessionBrowser::FilterSessions()
{
	SessionList.Reset();

	for (int32 Index = 0; Index < AvailableSessions.Num(); ++Index)
	{
		const ISessionInfoPtr& Session = AvailableSessions[Index];

		if (Session->GetSessionOwner() == FPlatformProcess::UserName(true))
		{
			SessionList.Add(Session);
		}
	}

	SessionComboBox->RefreshOptions();
	SessionComboBox->SetSelectedItem(SessionManager->GetSelectedSession());
}


FText SSessionBrowser::GetSessionName( const ISessionInfoPtr& SessionInfo ) const
{
	FText SessionName;

	if (!SessionInfo->IsStandalone() || !SessionInfo->GetSessionName().IsEmpty())
	{
		SessionName = FText::FromString(SessionInfo->GetSessionName());
	}
	else
	{
		TArray<ISessionInstanceInfoPtr> Instances;
		SessionInfo->GetInstances(Instances);

		if (Instances.Num() > 0)
		{
			const ISessionInstanceInfoPtr& FirstInstance = Instances[0];

			if ((Instances.Num() == 1) && (FirstInstance->GetInstanceId() == FApp::GetInstanceId()))
			{
				SessionName = LOCTEXT("ThisApplicationSessionText", "This Application");
			}
			else if (FirstInstance->GetDeviceName() == FPlatformProcess::ComputerName())
			{
				SessionName = LOCTEXT("UnnamedLocalSessionText", "Unnamed Session (Local)");
			}
			else
			{
				SessionName = LOCTEXT("UnnamedRemoteSessionText", "Unnamed Session (Remote)");
			}
		}
		else
		{
			SessionName = LOCTEXT("UnnamedSessionText", "Unnamed Session");
		}
	}

	return SessionName;
}


void SSessionBrowser::ReloadSessions()
{
	SessionManager->GetSessions(AvailableSessions);
	FilterSessions();

	if ((SessionList.Num() > 0) && !SessionManager->GetSelectedSession().IsValid())
	{
		SessionManager->SelectSession(SessionList[0]);
	}
}


/* SSessionBrowser event handlers
 *****************************************************************************/

TSharedRef<SWidget> SSessionBrowser::HandleSessionComboBoxGenerateWidget( ISessionInfoPtr SessionInfo ) const
{
	return SNew(SVerticalBox)

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f)
		[
			SNew(STextBlock)
				.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14))
				.Text(GetSessionName(SessionInfo))
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
						.Text(FText::FromString(SessionInfo->GetSessionOwner()))
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(16.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("InstanceCountFormat", "{0} Instance(s)"), FText::AsNumber(SessionInfo->GetNumInstances())))
				]
		];
}


void SSessionBrowser::HandleSessionComboBoxSelectionChanged( ISessionInfoPtr SelectedItem, ESelectInfo::Type SelectInfo )
{
	if (!SessionManager->SelectSession(SelectedItem))
	{
		SessionComboBox->SetSelectedItem(SessionManager->GetSelectedSession());
	}
}


FText SSessionBrowser::HandleSessionComboBoxText() const
{
	const ISessionInfoPtr& SelectedSession = SessionManager->GetSelectedSession();

	if (SelectedSession.IsValid())
	{
		return GetSessionName(SelectedSession);
	}

	return LOCTEXT("SelectSessionHint", "Select a session...");
}


FText SSessionBrowser::HandleSessionDetailsText() const
{
	const ISessionInfoPtr& SelectedSession = SessionManager->GetSelectedSession();

	if (!SelectedSession.IsValid())
	{
		return FText::GetEmpty();
	}

	FFormatNamedArguments FormatArguments;
	FormatArguments.Add(TEXT("NumInstances"), SelectedSession->GetNumInstances());
	FormatArguments.Add(TEXT("OwnerName"), SelectedSession->GetSessionOwner().IsEmpty() ? LOCTEXT("UnknownOwner", "<unknown>") : FText::FromString(SelectedSession->GetSessionOwner()));

	return FText::Format(LOCTEXT("SessionDetails", "Owner: {OwnerName}, {NumInstances} Instance(s)"), FormatArguments);
}


TSharedRef<ITableRow> SSessionBrowser::HandleSessionListViewGenerateRow( ISessionInfoPtr Item, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SSessionBrowserSessionListRow, OwnerTable)
		.SessionInfo(Item)
		.ToolTipText(FText::Format(LOCTEXT("SessionToolTipSessionIdFmt", "Session ID: {0}"), FText::FromString(Item->GetSessionId().ToString(EGuidFormats::DigitsWithHyphensInBraces))));
}


void SSessionBrowser::HandleSessionListViewSelectionChanged( ISessionInfoPtr Item, ESelectInfo::Type SelectInfo )
{
	if (!SessionManager->SelectSession(Item))
	{
		SessionListView->SetSelection(SessionManager->GetSelectedSession());
	}
}


void SSessionBrowser::HandleSessionManagerSelectedSessionChanged( const ISessionInfoPtr& SelectedSession )
{
	if (SessionComboBox->GetSelectedItem() != SelectedSession)
	{
		SessionComboBox->SetSelectedItem(SelectedSession);
	}
}


void SSessionBrowser::HandleSessionManagerSessionsUpdated()
{
	ReloadSessions();
}


FReply SSessionBrowser::HandleTerminateSessionButtonClicked()
{
	int32 DialogResult = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("TerminateSessionDialogPrompt", "Are you sure you want to terminate this session and its instances?"));

	if (DialogResult == EAppReturnType::Yes)
	{
		const ISessionInfoPtr& SelectedSession = SessionManager->GetSelectedSession();

		if (SelectedSession.IsValid())
		{
			if (SelectedSession->GetSessionOwner() == FPlatformProcess::UserName(true))
			{
				SelectedSession->Terminate();
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("TerminateDeniedPrompt", "You are not authorized to terminate the currently selected session, because it is owned by {0}"), FText::FromString( SelectedSession->GetSessionOwner() ) ) );
			}
		}
	}

	return FReply::Handled();
}


bool SSessionBrowser::HandleTerminateSessionButtonIsEnabled() const
{
	return SessionManager->GetSelectedSession().IsValid();
}


#undef LOCTEXT_NAMESPACE
