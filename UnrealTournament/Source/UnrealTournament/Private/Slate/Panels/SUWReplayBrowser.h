// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Runtime/NetworkReplayStreaming/NetworkReplayStreaming/Public/NetworkReplayStreaming.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class FReplayData : public TSharedFromThis<FReplayData>
{
public:
	FNetworkReplayStreamInfo StreamInfo;
	FString		Date;
	FString		Size;
	int32		ResultsIndex;
};

class UNREALTOURNAMENT_API SReplayBrowserRow : public SMultiColumnTableRow< TSharedPtr<FReplayData> >
{
public:

	SLATE_BEGIN_ARGS(SReplayBrowserRow)
		: _ReplayData()
	{}

	SLATE_ARGUMENT(TSharedPtr<FReplayData>, ReplayData)
	SLATE_STYLE_ARGUMENT(FTableRowStyle, Style)

	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		ReplayData = InArgs._ReplayData;

		FSuperRowType::Construct(FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView);
	}
	
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FReplayData> ReplayData;
};

class UNREALTOURNAMENT_API SUWReplayBrowser : public SUWPanel
{
public:

private:

	virtual void ConstructPanel(FVector2D ViewportSize);

protected:

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;

	TSharedPtr<INetworkReplayStreamer> ReplayStreamer;
	TArray< TSharedPtr<FReplayData> > ReplayList;
	TSharedPtr< SListView< TSharedPtr<FReplayData> > > ReplayListView;
	TSharedPtr<class SButton>  WatchReplayButton;

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);

	bool bShouldShowAllReplays;

	void BuildReplayList();
	void OnEnumerateStreamsComplete(const TArray<FNetworkReplayStreamInfo>& Streams);

	TSharedRef<ITableRow> OnGenerateWidgetForList(TSharedPtr<FReplayData> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnReplayListSelectionChanged(TSharedPtr<FReplayData> SelectedItem, ESelectInfo::Type SelectInfo);
	virtual void OnListMouseButtonDoubleClick(TSharedPtr<FReplayData> SelectedServer);

	virtual FReply OnWatchClick();
	virtual FReply OnRefreshClick();
};

#endif