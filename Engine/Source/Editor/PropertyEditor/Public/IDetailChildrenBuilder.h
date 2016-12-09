// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomNodeBuilder.h"
#include "Widgets/SWidget.h"
#include "UObject/StructOnScope.h"
#include "PropertyHandle.h"

/**
 * Builder for adding children to a detail customization
 */
class IDetailChildrenBuilder
{
public:
	virtual ~IDetailChildrenBuilder(){}

	/**
	 * Adds a custom builder as a child
	 *
	 * @param InCustomBuilder		The custom builder to add
	 */
	virtual IDetailChildrenBuilder& AddChildCustomBuilder( TSharedRef<IDetailCustomNodeBuilder> InCustomBuilder ) = 0;

	/**
	 * Adds a group to the category
	 *
	 * @param GroupName	The name of the group 
	 * @param LocalizedDisplayName	The display name of the group
	 * @param true if the group should appear in the advanced section of the category
	 */
	virtual class IDetailGroup& AddChildGroup( FName GroupName, const FText& LocalizedDisplayName ) = 0;

	/**
	 * Adds new custom content as a child to the struct
	 *
	 * @param SearchString	Search string that will be matched when users search in the details panel.  If the search string doesnt match what the user types, this row will be hidden
	 * @return A row that accepts widgets
	 */
	virtual class FDetailWidgetRow& AddChildContent( const FText& SearchString ) = 0;

	/**
	 * Adds a property to the struct
	 * 
	 * @param PropertyHandle	The handle to the property to add
	 * @return An interface to the property row that can be used to customize the appearance of the property
	 */
	virtual class IDetailPropertyRow& AddChildProperty( TSharedRef<IPropertyHandle> PropertyHandle ) = 0;

	/**
	 * Adds a child structure
	 * 
	 * @param ChildStructure	The structure to add
	 * @return An array of interfaces to the properties that were added
	 */
	virtual TArray<TSharedPtr<IPropertyHandle>> AddChildStructure( TSharedRef<FStructOnScope> ChildStructure ) = 0;

	/**
	 * Generates a value widget from a customized struct
	 * If the customized struct has no value widget an empty widget will be returned
	 *
	 * @param StructPropertyHandle	The handle to the struct property to generate the value widget from
	 */
	virtual TSharedRef<SWidget> GenerateStructValueWidget(TSharedRef<IPropertyHandle> StructPropertyHandle) = 0;

};
