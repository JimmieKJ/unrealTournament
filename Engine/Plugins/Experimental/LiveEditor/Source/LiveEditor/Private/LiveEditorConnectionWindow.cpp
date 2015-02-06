// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorConnectionWindow.h"

#define LOCTEXT_NAMESPACE "LiveEditorConnection"

class SConnectionDetailWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConnectionDetailWindow){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedPtr<FString> &_ConnectionInfo, SLiveEditorConnectionWindow *_Owner)
	{
		check( _Owner != NULL );
		ConnectionInfo = _ConnectionInfo;
		Owner = _Owner;

		ChildSlot
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew( SButton )
				.Text( LOCTEXT("Disconnect", "Disconnect") )
				.ToolTipText( LOCTEXT("DisconnectTooltip", "Disconnect from this IP") )
				.OnClicked( this, &SConnectionDetailWindow::OnDisconnect )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text( this, &SConnectionDetailWindow::GetConnectionInfo )
			]
		];
	}

	FReply OnDisconnect()
	{
		check( ConnectionInfo.IsValid() );
		Owner->DisconnectFrom( *ConnectionInfo.Get() );
		return FReply::Handled();
	}

	FText GetConnectionInfo() const
	{
		check( ConnectionInfo.IsValid() );
		return FText::FromString(*ConnectionInfo.Get());
	}

private:
	SLiveEditorConnectionWindow *Owner;
	TSharedPtr<FString> ConnectionInfo;
};

void SLiveEditorConnectionWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew( SButton )
				.Text( LOCTEXT("Connect", "Connect") )
				.ToolTipText( LOCTEXT("ConnectTooltip", "Connect to this IP for Remote LiveEditing") )
				.OnClicked( this, &SLiveEditorConnectionWindow::OnConnect )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(SEditableTextBox)
					.SelectAllTextWhenFocused(true)
					.Text(this, &SLiveEditorConnectionWindow::GetNewHostText)
					.ToolTipText( LOCTEXT("HostnameTooltip", "The host IP you would like to connect to for LiveEditing. Usage xxx.xxx.xxx.xxx (no port)") )
					.OnTextCommitted(this, &SLiveEditorConnectionWindow::OnNewHostTextCommited)
					.OnTextChanged(this, &SLiveEditorConnectionWindow::OnNewHostTextCommited, ETextCommit::Default)
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SAssignNew( ConnectionListView, SListView< TSharedPtr<FString> > )
			.ListItemsSource(&SharedConnectionIPs)
			.OnGenerateRow(this, &SLiveEditorConnectionWindow::OnGenerateWidgetForConnection)
		]
	];
}

TSharedRef<ITableRow> SLiveEditorConnectionWindow::OnGenerateWidgetForConnection(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		.Padding(5.0f)
		[
			SNew(SConnectionDetailWindow, Item, this)
		];
}

FReply SLiveEditorConnectionWindow::OnConnect()
{
	if ( FLiveEditorManager::Get().ConnectToRemoteHost( NewConnectionString ) )
	{
		SharedConnectionIPs.Add( MakeShareable(new FString(NewConnectionString)) );
		RefreshConnections();

		NewConnectionString = FString(TEXT(""));
	}
	return FReply::Handled();
}

void SLiveEditorConnectionWindow::DisconnectFrom( const FString &IPAddress )
{
	FLiveEditorManager::Get().DisconnectFromRemoteHost( IPAddress );
	for ( int32 i = SharedConnectionIPs.Num()-1; i >= 0; --i )
	{
		if ( !SharedConnectionIPs[i].IsValid() || *SharedConnectionIPs[i].Get() == IPAddress )
		{
			SharedConnectionIPs.RemoveAtSwap(i);
		}
	}
	RefreshConnections();
}

FText SLiveEditorConnectionWindow::GetNewHostText() const
{
	return ( !NewConnectionString.IsEmpty() )? FText::FromString(NewConnectionString) : LOCTEXT("EnterIP", "Enter IP");
}

void SLiveEditorConnectionWindow::OnNewHostTextCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	NewConnectionString = InText.ToString();
}

void SLiveEditorConnectionWindow::RefreshConnections()
{
	if ( ConnectionListView.IsValid() )
	{
		ConnectionListView->RequestListRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
