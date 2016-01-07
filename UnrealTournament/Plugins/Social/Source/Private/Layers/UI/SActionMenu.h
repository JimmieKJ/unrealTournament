// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE_RetVal(bool, FIsMenuOpen);

class FFriendViewModel;
class FFriendContainerViewModel;

class SActionMenu : public SUserWidget
{
public:
	/** Helper class used to define content of one item in SActionMenu. */
	class FActionData
	{

	public:

		/** Ctor */
		FActionData(EFriendActionType::Type InActionType, const FText& InEntryText, const FSlateBrush* InEntryIcon, const FName& InButtonTag, bool InIsEnabled, bool InIsVisibleOffline)
			: ActionType(InActionType)
			, EntryText(InEntryText)
			, EntryIcon(InEntryIcon)
			, bIsEnabled(InIsEnabled)
			, ButtonTag(InButtonTag)
			, bIsVisibleOffline(InIsVisibleOffline)
		{}

		/** Action */
		EFriendActionType::Type ActionType;

		/** Text content */
		FText EntryText;

		/** Optional icon brush */
		const FSlateBrush* EntryIcon;

		/** Is this item actually enabled/selectable */
		bool bIsEnabled;

		/** Tag that will be returned by OnDropdownItemClicked delegate when button corresponding to this item is clicked */
		FName ButtonTag;

		/** Should this item be visible when offline*/
		bool bIsVisibleOffline;
	};

	/** Helper class allowing to fill array of FActionData with syntax similar to Slate */
	class FActionsArray : public TArray < FActionData >
	{

	public:

		/** Adds item to array and returns itself */
		FActionsArray & operator + (const FActionData& TabData)
		{
			Add(TabData);
			return *this;
		}

		/** Adds item to array and returns itself */
		FActionsArray & AddItem(EFriendActionType::Type InActionType, const FText& InEntryText, const FSlateBrush* InEntryIcon, const FName& InButtonTag, bool InIsEnabled = true, bool InIsVisibleOffline = false)
		{
			return operator+(FActionData(InActionType, InEntryText, InEntryIcon, InButtonTag, InIsEnabled, InIsVisibleOffline));
		}


	};

	SLATE_USER_ARGS(SActionMenu)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> InFontService, const TSharedRef<FFriendContainerViewModel>& ContainerViewModel){}

	virtual void SetFriendViewModel(const TSharedRef<FFriendViewModel>& FriendViewModel) = 0;
	virtual void PlayIntroAnim() = 0;
	virtual float GetMenuWidth() const = 0;
};