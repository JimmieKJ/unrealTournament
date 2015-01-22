// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


typedef TSharedPtr< class FPluginCategoryTreeItem > FPluginCategoryTreeItemPtr;


/**
 * Represents a category in the plugin category tree
 */
class FPluginCategoryTreeItem
{

public:

	/** @return Returns the parent category or NULL if this is a root category */
	const FPluginCategoryTreeItemPtr GetParentCategory() const
	{
		return ParentCategoryWeak.Pin();
	}

	/** @return Returns the path up to this category */
	const FString& GetCategoryPath() const
	{
		return CategoryPath;
	}

	/** @return	Returns the name of this category */
	const FString& GetCategoryName() const
	{
		return CategoryName;
	}

	const FText& GetCategoryDisplayName() const
	{
		return CategoryDisplayName;
	}

	/** @return Returns all of this category's sub-categories */
	const TArray< TSharedPtr< FPluginCategoryTreeItem > >& GetSubCategories() const
	{
		return SubCategories;
	}

	/** @return Returns all of this category's sub-categories in a mutable way */
	TArray< TSharedPtr< FPluginCategoryTreeItem > >& AccessSubCategories()
	{
		return SubCategories;
	}

	/** @return Returns the plugins in this category */
	const TArray< TSharedPtr< FPluginStatus > >& GetPlugins() const
	{
		return Plugins;
	}

	/** Adds a plugin to this category */
	void AddPlugin( const TSharedRef< FPluginStatus >& PluginStatus )
	{
		Plugins.Add( PluginStatus );
	}


public:

	/** Constructor for FPluginCategoryTreeItem */
	FPluginCategoryTreeItem(const FPluginCategoryTreeItemPtr ParentCategory, const FString& InitCategoryPath, const FString& InitCategoryName, const FText& InitCategoryDisplayName)
		: ParentCategoryWeak( ParentCategory ),
		  CategoryPath( InitCategoryPath ),
		  CategoryName(InitCategoryName),
		  CategoryDisplayName(InitCategoryDisplayName)
	{
	}


private:

	/** Parent category item or NULL if this is a root category */
	TWeakPtr< FPluginCategoryTreeItem > ParentCategoryWeak;

	/** Category path string.  The full path up to this category */
	FString CategoryPath;

	/** Name of the category */
	FString CategoryName;

	/** Display name of the category */
	FText CategoryDisplayName;

	/** Child categories */
	TArray< TSharedPtr< FPluginCategoryTreeItem > > SubCategories;

	/** Plugins in this category */
	TArray< TSharedPtr< FPluginStatus > > Plugins;

};



