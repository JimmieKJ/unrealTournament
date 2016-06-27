// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Runtime/NetworkReplayStreaming/NetworkReplayStreaming/Public/NetworkReplayStreaming.h"
#include "../SUWindowsStyle.h"
#include "../Base/SUTPanelBase.h"

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

class UNREALTOURNAMENT_API SUTReplayBrowserPanel : public SUTPanelBase
{
public:
	bool bLiveOnly;
	bool bShowReplaysFromAllUsers;
	FString MetaString;

	FString LastUserId;
	void BuildReplayList(const FString& UserId);

	~SUTReplayBrowserPanel();
private:

	virtual void ConstructPanel(FVector2D ViewportSize);

	bool CanWatchReplays();

protected:

	TSharedPtr<INetworkReplayStreamer> ReplayStreamer;
	TArray< TSharedPtr<FReplayData> > ReplayList;
	TSharedPtr< SListView< TSharedPtr<FReplayData> > > ReplayListView;
	TSharedPtr<class SButton>  WatchReplayButton;
	TSharedPtr<class SEditableTextBox> MetaTagText;

	virtual void OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow);

	bool bShouldShowAllReplays;

	void OnEnumerateStreamsComplete(const TArray<FNetworkReplayStreamInfo>& Streams);

	TSharedRef<ITableRow> OnGenerateWidgetForList(TSharedPtr<FReplayData> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnReplayListSelectionChanged(TSharedPtr<FReplayData> SelectedItem, ESelectInfo::Type SelectInfo);
	virtual void OnListMouseButtonDoubleClick(TSharedPtr<FReplayData> SelectedServer);
	virtual void OnMetaTagTextCommited(const FText& NewText, ETextCommit::Type CommitType);

	virtual void PlayerIDResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	TSharedPtr< SComboBox< TSharedPtr<FString> > > FriendListComboBox;
	TArray<TSharedPtr<FString>> FriendList;
	TArray<FString> FriendStatIDList;
	TSharedPtr<STextBlock> SelectedFriend;
	void OnFriendSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateStringListWidget(TSharedPtr<FString> InItem);

	FDelegateHandle FriendsListUpdatedDelegateHandle;
	void FriendsListUpdated();

	TSharedPtr< SCheckBox> LiveOnlyCheckbox;

	virtual FReply OnWatchClick();
	virtual FReply OnRefreshClick();
	virtual FReply OnPlayerIDClick();
};

#endif