// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * Represents a message log attribute that can be filtered.
 * Handles all callbacks for any slate checkboxes which wish to alter such filters.
 */
class FMessageFilter : public TSharedFromThis<FMessageFilter>
{
public:
	FMessageFilter(const FText& InName, const FSlateBrush* InImage)
		: Name(InName)
		, Image(InImage)
		, Display(true)
	{}

	virtual ~FMessageFilter() {}

	/** Fires a callback when the mouse is released on an option (not linked to states) */
	FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Gets the display state to send to a display filter check box */
	virtual ECheckBoxState OnGetDisplayCheckState() const;
	
	/** Sets the display state from a display filter check box */
	virtual void OnDisplayCheckStateChanged(ECheckBoxState InNewState);

	/**
	 * Returns a delegate to be invoked when the filter state changes.
	 *
	 * @return The delegate.
	 */
	virtual FSimpleMulticastDelegate& OnFilterChanged( )
	{
		return RefreshCallback;
	}


	const FText& GetName() const {return Name;}
	const FSlateBrush* GetIcon() const {return Image;}
	const bool GetDisplay() const {return Display;}

private:
	FText Name;
	const FSlateBrush* Image;
	bool Display;

	FSimpleMulticastDelegate RefreshCallback;
};
