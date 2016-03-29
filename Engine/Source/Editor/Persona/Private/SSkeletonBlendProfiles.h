// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LOCTEXT_NAMESPACE "SkeletonBlendProfiles"

class UBlendProfile;
class FAssetEditorToolkit;
class FPersona;

//////////////////////////////////////////////////////////////////////////

DECLARE_DELEGATE_OneParam(FSuccessfulCommit, const FName&);

//////////////////////////////////////////////////////////////////////////

class FSkeletonBlendProfilesSummoner : public FWorkflowTabFactory
{
public:
	FSkeletonBlendProfilesSummoner(TSharedPtr<FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
};

//////////////////////////////////////////////////////////////////////////

class FDisplayedBlendProfileInfo
{
public:
	TWeakObjectPtr<UBlendProfile> ObservedBlendProfile;

	static TSharedRef<FDisplayedBlendProfileInfo> Make(TWeakObjectPtr<UBlendProfile>& InBlendProfile)
	{
		return MakeShareable(new FDisplayedBlendProfileInfo(InBlendProfile));
	}

private:
	// Hidden, use FDisplayedBlendProfileInfo::Make
	FDisplayedBlendProfileInfo(TWeakObjectPtr<UBlendProfile>& InBlendProfile)
		: ObservedBlendProfile(InBlendProfile)
	{

	}

	// Disabled
	FDisplayedBlendProfileInfo() {};
};

//////////////////////////////////////////////////////////////////////////

typedef SListView<TSharedPtr<FDisplayedBlendProfileInfo>> SBlendProfileListView;

// Blend profile manager widget for the blend profile tab in Persona
class SSkeletonBlendProfiles : public SCompoundWidget//, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SSkeletonBlendProfiles)
		: _WeakPersona()
	{}

	// Hosting Persona Instance
	SLATE_ARGUMENT(TWeakPtr<FPersona>, WeakPersona)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	TSharedRef<ITableRow> GenerateViewRow(TSharedPtr<FDisplayedBlendProfileInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedPtr<SWidget> GetContextMenu() const;

	void RebuildBlendProfileList();

	void OnRequestNewBlendProfile();
	void CreateNewBlendProfile(const FName& InNewName);

	bool MenuCanDelete() const;
	bool MenuCanRename() const;

	void DeleteSelectedProfiles();

	void OnRequestRenameSelectedProfile();
	void ValidateUserTextEntry(const FText& InText);
	void HandleTextEntryCommit(const FText& InText, ETextCommit::Type InCommitType, FSuccessfulCommit OnCommit);
	void OnRenameTextCommitted(const FText& InText, ETextCommit::Type InCommitType);
	void RenameSelectedProfile(const FName& NewName);

	void RefreshObservedData();

	void OnListSelectionChanged(TSharedPtr<FDisplayedBlendProfileInfo> Item, ESelectInfo::Type InSelectionType);

	// Hosting Persona instance
	TWeakPtr<FPersona> WeakPersona;

	// Skeleton active in Persona
	USkeleton* CurrentSkeleton;

	// Widgets
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SBlendProfileListView> ListView;

	// List displayed in the list view
	TArray<TSharedPtr<FDisplayedBlendProfileInfo>> BlendProfileList;

	// Currently active text entry widget
	TSharedPtr<STextEntryPopup> CurrentTextEntryPopup;
	// Whether or not there's an error in the current popup content
	bool bCurrentTextEntryHasError;

	FSuccessfulCommit OnSuccesfulRenameCommit;
	FSuccessfulCommit OnSuccesfulNewProfileCommit;
};

#undef LOCTEXT_NAMESPACE