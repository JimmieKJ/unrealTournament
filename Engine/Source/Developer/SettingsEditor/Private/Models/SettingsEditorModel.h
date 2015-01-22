// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a view model for the settings editor widget.
 */
class FSettingsEditorModel
	: public ISettingsEditorModel
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InSettingsContainer The settings container to use.
	 */
	FSettingsEditorModel( const ISettingsContainerRef& InSettingsContainer )
		: SettingsContainer(InSettingsContainer)
	{
		SettingsContainer->OnSectionRemoved().AddRaw(this, &FSettingsEditorModel::HandleSettingsContainerSectionRemoved);
	}

	/** Virtual destructor. */
	virtual ~FSettingsEditorModel()
	{
		SettingsContainer->OnSectionRemoved().RemoveAll(this);
	}

public:

	// ISettingsEditorModel interface

	virtual const ISettingsSectionPtr& GetSelectedSection() const override
	{
		return SelectedSection;
	}

	virtual const ISettingsContainerRef& GetSettingsContainer() const override
	{
		return SettingsContainer;
	}

	virtual FSimpleMulticastDelegate& OnSelectionChanged() override
	{
		return OnSelectionChangedDelegate;
	}

	virtual void SelectSection( const ISettingsSectionPtr& Section ) override
	{
		if (Section == SelectedSection)
		{
			return;
		}

		SelectedSection = Section;
		OnSelectionChangedDelegate.Broadcast();
	}

private:

	/** Handles the removal of sections from the settings container. */
	void HandleSettingsContainerSectionRemoved( const ISettingsSectionRef& Section )
	{
		if (SelectedSection == Section)
		{
			SelectSection(nullptr);
		}
	}

private:

	/** Holds the currently selected settings section. */
	ISettingsSectionPtr SelectedSection;

	/** Holds a reference to the settings container. */
	ISettingsContainerRef SettingsContainer;

private:

	/** Holds a delegate that is executed when the selected settings section has changed. */
	FSimpleMulticastDelegate OnSelectionChangedDelegate;
};
