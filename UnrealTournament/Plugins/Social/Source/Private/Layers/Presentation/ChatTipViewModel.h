// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class containing the friend information - used to build the list view.
 */
class FChatTipViewModel
	:	public TSharedFromThis<FChatTipViewModel>
{
public:

	virtual ~FChatTipViewModel() {}

	/**
	 * Get a list of chat tips e.g. Global. Displayed when the user enters / into the chat box.
	 * @return the array of chat tips.
	 */
	virtual TArray<TSharedRef<class IChatTip> >& GetChatTips() = 0;

	/**
	 * return the selected active tip.
	 */
	virtual TSharedPtr<class IChatTip> GetActiveTip() = 0;

	/**
	 * Event broadcast when a chat tip becomes available.
	 */
	DECLARE_EVENT(FChatTipViewModel, FChatTipAvailable)
	virtual FChatTipAvailable& OnChatTipAvailable() = 0;

	/**
	 * Event broadcast when a chat tip is selected.
	 */
	DECLARE_EVENT_OneParam(FChatTipViewModel, FChatTipSelected, TSharedRef<class IChatTip> /* Selected Tip */)
	virtual FChatTipSelected& OnChatTipSelected() = 0;

};

/**
* Creates the implementation for a ChatTipViewModel.
*
* @return the newly created ChatTipViewModel implementation.
*/
FACTORY(TSharedRef< FChatTipViewModel >, FChatTipViewModel, const TSharedRef<class FFriendsChatMarkupService>& MarkupService);