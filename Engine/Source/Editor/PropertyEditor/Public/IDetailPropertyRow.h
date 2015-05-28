// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forward declarations
class FDetailWidgetRow;

/**
 * A single row for a property in a details panel                                                              
 */
class IDetailPropertyRow
{
public:
	virtual ~IDetailPropertyRow(){}

	/**
	 * Sets the localized display name of the property
	 *
	 * @param InDisplayName	The name of the property
	 */
	virtual IDetailPropertyRow& DisplayName( const FText& InDisplayName ) = 0;

	/**
	 * Sets the localized tooltip of the property
	 *
	 * @param InToolTip	The tooltip of the property
	 */
	virtual IDetailPropertyRow& ToolTip( const FText& InToolTip ) = 0;

	/**
	 * Sets whether or not we show the default property editing buttons for this property
	 *
	 * @param bShowPropertyButtons	if true property buttons for this property will be shown.  
	 */
	virtual IDetailPropertyRow& ShowPropertyButtons( bool bShowPropertyButtons ) = 0;

	/**
	 * Sets an edit condition for this property.  If the edit condition fails, the property is not editable
	 * This will add a checkbox before the name of the property that users can click to toggle the edit condition
	 * Properties with built in edit conditions will override this automatically.  If the 
	 *
	 * @param EditConditionValue			Attribute for the value of the edit condition (true indicates that the edit condition passes)
	 * @param OnEditConditionValueChanged	Delegate called when the edit condition value changes
	 */
	virtual IDetailPropertyRow& EditCondition( TAttribute<bool> EditConditionValue, FOnBooleanValueChanged OnEditConditionValueChanged ) = 0;

	/**
	 * Sets whether or not this property is enabled
	 *
	 * @param InIsEnabled	Attribute for the enabled state of the property (true to enable the property)
	 */
	virtual IDetailPropertyRow& IsEnabled( TAttribute<bool> InIsEnabled ) = 0;
	

	/**
	 * Sets whether or not this property should auto-expand
	 *
	 * @param bForceExpansion	true to force the property to be expanded
	 */
	virtual IDetailPropertyRow& ShouldAutoExpand(bool bForceExpansion = true) = 0;

	/**
	 * Sets the visibility of this property
	 *
	 * @param Visibility	Attribute for the visibility of this property
	 */
	virtual IDetailPropertyRow& Visibility( TAttribute<EVisibility> Visibility ) = 0;

	/**
	 * Overrides the behavior of reset to default
	 *
	 * @param IsResetToDefaultVisible	Attribute to indicate whether or not reset to default is visible
	 * @param OnResetToDefaultClicked	Delegate called when reset to default is clicked
	 */
	virtual IDetailPropertyRow& OverrideResetToDefault( TAttribute<bool> IsResetToDefaultVisible, FSimpleDelegate OnResetToDefaultClicked ) = 0;

	/**
	 * Returns the name and value widget of this property row.  You can use this widget to apply further customization to existing widgets (by using this  with CustomWidget)
	 *
	 * @param OutNameWidget		The default name widget
	 * @param OutValueWidget	The default value widget
	 */
	virtual void GetDefaultWidgets( TSharedPtr<SWidget>& OutNameWidget, TSharedPtr<SWidget>& OutValueWidget ) = 0;

	/**
	 * Returns the name and value widget of this property row.  You can use this widget to apply further customization to existing widgets (by using this  with CustomWidget)
	 *
	 * @param OutNameWidget		The default name widget
	 * @param OutValueWidget	The default value widget
	 * @param OutCustomRow		The default widget row
	 */
	virtual void GetDefaultWidgets( TSharedPtr<SWidget>& OutNameWidget, TSharedPtr<SWidget>& OutValueWidget, FDetailWidgetRow& Row ) = 0;

	/**
	 * Overrides the property widget
	 *
	 * @param bShowChildren	Whether or not to still show any children of this property
	 * @return a row for the property that custom widgets can be added to
	 */
	virtual FDetailWidgetRow& CustomWidget( bool bShowChildren = false ) = 0;

};