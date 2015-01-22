// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// SLiveEditorConnectionWindow

class SLiveEditorConnectionWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveEditorConnectionWindow){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	TSharedRef<ITableRow> OnGenerateWidgetForConnection(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
	FReply OnConnect();
	void DisconnectFrom( const FString &IPAddress );

	FText GetNewHostText() const;
	void OnNewHostTextCommited(const FText& InText, ETextCommit::Type InCommitType);

private:
	void RefreshConnections();

private:
	TArray< TSharedPtr<FString> > SharedConnectionIPs;
	TSharedPtr< SListView< TSharedPtr<FString> > > ConnectionListView;

	FString NewConnectionString;
};
