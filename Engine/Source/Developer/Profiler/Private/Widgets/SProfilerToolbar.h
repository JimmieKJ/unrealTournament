// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

/** Ribbon based toolbar used as a main menu in the Profiler window. */
class SProfilerToolbar : public SCompoundWidget
{
public:
	/** Default constructor. */
	SProfilerToolbar();

	/** Virtual destructor. */
	virtual ~SProfilerToolbar();

	SLATE_BEGIN_ARGS( SProfilerToolbar )
		{}
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	bool IsImplemented() const { return false; }
	bool IsShowingStats() const { return true; }
	bool IsShowingMemory() const { return false; }

protected:
	/** Called when the list of sessions has changed. @see FProfilerManager.FOnSessionsUpdated */
	void ProfilerManager_SessionsUpdated();

	void AddSessionInstanceItem( TSharedRef<SHorizontalBox>& SessionsHBox, const FString& ProfilerSessionName, const FGuid& SessionInstanceID );
	EVisibility IsInstancesOwnerVisible() const;

	/**
	 * Builds the context menu for the specified profiler session.
	 *
	 * @param SessionInstanceID - the session instance ID for which the context menu will be built 
	 *
	 */
	TSharedRef<SWidget> BuildProfilerSessionContextMenu( const FGuid SessionInstanceID );

	/** Create the UI commands for the toolbar */
	void CreateCommands();

	/** Shows the stats profiler */
	void ShowStats();

	/** Shows the memory profiler */
	void ShowMemory();

	/** Shows the FPSChart view */
	void ShowFPSChart();

protected:
	TSharedPtr<SBorder> Border;
};