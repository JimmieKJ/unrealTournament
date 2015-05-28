// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE_RetVal(bool, FIsMenuOpen);
DECLARE_DELEGATE_OneParam(FOnDropdownItemClicked, FName);

struct FFriendsAndChatComboButtonStyle
{
	const FComboButtonStyle* ComboButtonStyle;
	const FTextBlockStyle* ButtonTextStyle;
};

class SFriendsAndChatCombo : public SUserWidget
{
public:
	/** Helper class used to define content of one item in SFriendsAndChatCombo. */
	class FItemData
	{

	public:

		/** Ctor */
		FItemData(const FText& InEntryText, const FSlateBrush* InEntryIcon, const FName& InButtonTag, bool InIsEnabled, bool InIsVisibleOffline)
			: EntryText(InEntryText)
			, EntryIcon(InEntryIcon)
			, bIsEnabled(InIsEnabled)
			, ButtonTag(InButtonTag)
			, bIsVisibleOffline(InIsVisibleOffline)
		{}

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

	/** Helper class allowing to fill array of SItemData with syntax similar to Slate */
	class FItemsArray : public TArray < FItemData >
	{

	public:

		/** Adds item to array and returns itself */
		FItemsArray & operator + (const FItemData& TabData)
		{
			Add(TabData);
			return *this;
		}

		/** Adds item to array and returns itself */
		FItemsArray & AddItem(const FText& InEntryText, const FSlateBrush* InEntryIcon, const FName& InButtonTag, bool InIsEnabled = true, bool InIsVisibleOffline = false)
		{
			return operator+(FItemData(InEntryText, InEntryIcon, InButtonTag, InIsEnabled, InIsVisibleOffline));
		}
	};

	SLATE_USER_ARGS(SFriendsAndChatCombo)
		: _bShowIcon(false)
		, _bOnline(true)
		, _IconBrush(nullptr)
		, _ButtonStyleOverride(nullptr)
		, _bSetButtonTextToSelectedItem(false)
		, _bAutoCloseWhenClicked(true)
		, _ButtonSize(150, 36)
		, _Placement(MenuPlacement_ComboBox)
	{}

		/** Slot for this designers content (optional) */
		SLATE_NAMED_SLOT(FArguments, Content)

		/** Text to display on main button. */
		SLATE_TEXT_ATTRIBUTE(ButtonText)

		/** Whether the optional icon is shown */
		SLATE_ATTRIBUTE(bool, bShowIcon)

		/** Whether the optional icon is shown */
		SLATE_ATTRIBUTE(bool, bOnline)

		/** Optional icon brush */
		SLATE_ATTRIBUTE(const FSlateBrush*, IconBrush)

		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)

		SLATE_ARGUMENT(const FFriendsAndChatComboButtonStyle*, ButtonStyleOverride)

		/** If true, text displayed on the main button will be set automatically after user selects a dropdown item */
		SLATE_ARGUMENT(bool, bSetButtonTextToSelectedItem)

		/** List of items to display in dropdown list. */
		SLATE_ATTRIBUTE(FItemsArray, DropdownItems)

		/** Should the dropdown list be closed automatically when user clicks an item. */
		SLATE_ARGUMENT(bool, bAutoCloseWhenClicked)

		/** Size of the button content. Needs to be supplied manually, because dropdown must also be scaled manually. */
		SLATE_ARGUMENT(FVector2D, ButtonSize)

		/** Popup menu placement. */
		SLATE_ATTRIBUTE(EMenuPlacement, Placement)

		/** Called when user clicks an item from the dropdown. */
		SLATE_EVENT(FOnDropdownItemClicked, OnDropdownItemClicked)

		/** Called when dropdown is opened (main button is clicked). */
		SLATE_EVENT(FOnComboBoxOpened, OnDropdownOpened)

	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs){}

	virtual bool IsOpen() const = 0;
};