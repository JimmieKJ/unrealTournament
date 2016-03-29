// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class UBlendProfile;

DECLARE_DELEGATE_OneParam(FOnBlendProfileSelected, UBlendProfile*);

// Picker for UBlendProfile instances inside a USkeleton
class SBlendProfilePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendProfilePicker)
		: _TargetSkeleton(nullptr)
		, _InitialProfile(nullptr)
		, _OnBlendProfileSelected()
		, _AllowNew(true)
		, _AllowClear(true)
	{}
		// Skeleton to operate on
		SLATE_ARGUMENT(USkeleton*, TargetSkeleton)
		// Initial blend profile selected
		SLATE_ARGUMENT(UBlendProfile*, InitialProfile)
		// Delegate to call when the picker selection is changed
		SLATE_EVENT(FOnBlendProfileSelected, OnBlendProfileSelected)
		// Allow the option to create new profiles in the picker
		SLATE_ARGUMENT(bool, AllowNew)
		// Allow the option to clear the profile selection
		SLATE_ARGUMENT(bool, AllowClear)
	SLATE_END_ARGS()

	PERSONA_API void Construct(const FArguments& InArgs);

	/** Set the selected profile externally 
	 *  @param InProfile New Profile to set
	 *  @param bBroadcast Whether or not to broadcast this selection
	 */
	PERSONA_API void SetSelectedProfile(UBlendProfile* InProfile, bool bBroadcast = true);

private:

	void OnClearSelection();
	void OnCreateNewProfile();
	void OnCreateNewProfileComitted(const FText& NewName, ETextCommit::Type CommitType);

	void OnProfileSelected(UBlendProfile* Profile);

	void RefreshBlendProfileList();

	FText GetSelectedProfileName() const;

	TSharedRef<SWidget> GetMenuContent();

	bool bShowNewOption;
	bool bShowClearOption;

	UBlendProfile* SelectedProfile;

	USkeleton* TargetSkeleton;

	FOnBlendProfileSelected BlendProfileSelectedDelegate;

};
