// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


namespace ELevelEditorActiveToolkit
{
	enum Type
	{
		/** An traditional editor "mode" */
		LegacyEditorMode,

		/** A "world-centric" asset editor */
		Toolkit
	};
}


class SLevelEditorActiveToolkit : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SLevelEditorActiveToolkit ) { }
	SLATE_END_ARGS()


	/** Constructs this widget.  Called by Slate declarative syntax macros. */
	void Construct( const FArguments&, const TSharedPtr< class IToolkit >& InitToolkit, const FEdMode* InitEditorMode );

	/** Deactivate this toolkits current mode */
	void	DeactivateMode();
protected:

	/** Returns the text that should be displayed in the toolkit's title area */
	FText GetToolkitTextLabel() const;

	/** @return Returns a color to use for the background of toolkit label that's current active */
	FSlateColor GetToolkitBackgroundOverlayColor() const;

	/** Called when the "close toolkit" button is clicked */
	FReply OnToolkitCloseButtonClicked();

	/** @return Returns visibility state for the "unsaved change" icon next to the asset name */
	EVisibility GetVisibilityForUnsavedChangeIcon() const;

	/** Called when the user clicks on the name of the toolkit in the toolkit display */
	void OnNavigateToToolkit( );


protected:

	/** Reference to the actual toolkit object */
	TSharedPtr< class IToolkit > Toolkit;

	/** For legacy editor mode toolkit types, a pointer to the editor mode */
	const FEdMode* EditorMode;

	/** The type of active toolkit */
	ELevelEditorActiveToolkit::Type ActiveToolkitType;

	friend class SToolkitDisplay;
};



/**
 * Shows currently active toolkits in the level editor and allows toolkit-specific interactions
 */
class SToolkitDisplay : public SCompoundWidget
{

public:

	DECLARE_DELEGATE_OneParam(FOnWidgetChanged, TSharedPtr<SWidget>)

	SLATE_BEGIN_ARGS( SToolkitDisplay ) { }
		SLATE_EVENT(FOnWidgetChanged, OnInlineContentChanged)
	SLATE_END_ARGS()


	/** Constructs this widget.  Called by Slate declarative syntax macros. */
	void Construct( const FArguments&, const TSharedRef< class ILevelEditor >& OwningLevelEditor );

	/** Destructor, so we can unbind delegates */
	virtual ~SToolkitDisplay();

	/** Called by the SLevelEditorToolBox to let us know about a newly-hosted toolkit */
	void OnToolkitHostingStarted( const TSharedRef< IToolkit >& NewToolkit );

	/** Called by the SLevelEditorToolBox to let us know when an existing toolkit is no longer hosted */
	void OnToolkitHostingFinished( const TSharedRef< IToolkit >& DestroyingToolkit );


protected:

	/** Called by GEditorModeTools() when an editor mode is entered or exited */
	void OnEditorModeChanged( FEdMode* EditorMode, bool bEntered );

	/** Adds a legacy editor mode to our active modes list */
	void AddEditorMode( FEdMode* EditorMode );

	/** Removes a legacy editor mode from our active modes list */
	void RemoveEditorMode( FEdMode* EditorMode );

	/** Adds a new hosted toolkit */
	void AddToolkit( const TSharedRef< IToolkit >& NewToolkit );

	/** Removes a toolkit that we're already hosting */
	void RemoveToolkit( const TSharedRef< IToolkit >& DestroyingToolkit );


protected:

	/** The vertical box that we add our content to */
	TSharedPtr< SVerticalBox > VBox;

	/** What to do with inline content that is collected from the current editor mode */
	FOnWidgetChanged OnInlineContentChangedDelegate;

	/** List of currently active toolkits (non-authoritative) */
	TArray< TSharedPtr< SLevelEditorActiveToolkit > > ActiveToolkits;
};

