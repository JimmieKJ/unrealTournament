// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IDetailPropertyRow;
class FDetailWidgetRow;
class IPropertyHandle;

/**
 * A group in the details panel that can have children                                                              
 */
class IDetailGroup
{
public:
	virtual ~IDetailGroup(){}

	/**
	 * Makes a custom row for the groups header
	 *
	 * @return a new row for customizing the header
	 */
	virtual FDetailWidgetRow& HeaderRow() = 0;

	/**
	 * Adds a property as the groups header      
	 */
	virtual IDetailPropertyRow& HeaderProperty( TSharedRef<IPropertyHandle> PropertyHandle ) = 0;

	/** 
	 * Adds a new row for custom widgets *
	 * 
	 * @return a new row for adding widgets
	 */
	virtual class FDetailWidgetRow& AddWidgetRow() = 0;

	/**
	 * Adds a new row for a property
	 *
	 * @return an interface for customizing the appearance of the property row
	 */
	virtual class IDetailPropertyRow& AddPropertyRow( TSharedRef<IPropertyHandle> PropertyHandle ) = 0;

	/**
	 * Toggles expansion on the group
	 *
	 * @param bExpand	true to expand the group, false to collapse
	 */
	virtual void ToggleExpansion( bool bExpand ) = 0;

	/**
	 * Gets the current state of expansion for the group
	 */
	virtual bool GetExpansionState() const = 0;
};
