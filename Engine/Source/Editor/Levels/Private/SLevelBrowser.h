// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ModuleManager.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"

typedef TTextFilter< const TSharedPtr< FLevelViewModel >& > LevelTextFilter;

namespace ELevelBrowserMode
{
	enum Type
	{
		Levels,
		Count
	};
}


//////////////////////////////////////////////////////////////////////////
// SLevelBrowser
class SLevelBrowser : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SLevelBrowser ) {}
	SLATE_END_ARGS()

	~SLevelBrowser()
	{
		LevelCollectionViewModel->OnLevelsChanged().RemoveAll( this );
		LevelCollectionViewModel->OnSelectionChanged().RemoveAll( this );
		LevelCollectionViewModel->RemoveFilter( SearchBoxLevelFilter.ToSharedRef() );
	}

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The UI logic not specific to slate
	 */
	void Construct( const FArguments& InArgs );


protected:

	/** True if the user can shift the selected streaming level(s) */
	bool CanShiftSelection() const
	{
		return LevelCollectionViewModel->CanShiftSelection();
	}

	/** Called when the user clicks on the Shift Up button */
	FReply OnClickUp()
	{
		LevelCollectionViewModel->ShiftSelection(true);
		return FReply::Handled();
	}

	/** Called when the user clicks on the Shift Down button */
	FReply OnClickDown()
	{
		LevelCollectionViewModel->ShiftSelection(false);
		return FReply::Handled();
	}

	/** Overridden from SWidget: Called when a key is pressed down */
	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called during drag and drop when the drag leaves a widget.
	 *
	 * @param DragDropEvent   The drag and drop event.
	 */
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override
	{
		TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
		if (DragActorOp.IsValid())
		{
			DragActorOp->ResetToDefaultToolTip();
		}
	}

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
	{
		//@todo: add drag + drop support
		return FReply::Unhandled();
	}

	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
	{
		//@todo: add drag + drop support
		return FReply::Unhandled();
	}

private:
	
	/** Removes the specified actors from the selected level */
	void RemoveActorsFromSelectedLevel( const TArray< TWeakObjectPtr< AActor > >& Actors )
	{
		SelectedLevelViewModel->RemoveActors( Actors );
	}

	/** populates the ContentAreaBox with content for LevelsMode */
	void SetupLevelsMode() 
	{
		ContentAreaBox->ClearChildren();
		ContentAreaBox->AddSlot()
		.FillHeight( 1.0f )
		[
			LevelsSection.ToSharedRef()
		];

		//@todo: Add support for level contents, similar to layer browser?
		//Add a button here for transitioning to LevelContentsMode

		Mode = ELevelBrowserMode::Levels;
	}

	/** Populates the ContentAreaBox with content for LevelContentsMode */
	void SetupLevelContentsMode() 
	{
		//@todo: Add support for level contents, similar to layer browser?
	}

	/** Appends the Level's name to the OutSearchStrings array if the Level is valid */
	void TransformLevelToString( const TSharedPtr< FLevelViewModel >& Level, OUT TArray< FString >& OutSearchStrings ) const
	{
		if( !Level.IsValid() )
		{
			return;
		}

		OutSearchStrings.Add( Level->GetName() );
	}

	/** Applies the currently selected levels to the SelectedLevelsFilter */
	void UpdateLevelContentsFilter()
	{
		TArray< FName > LevelNames;
		LevelCollectionViewModel->GetSelectedLevelNames( LevelNames );
		SelectedLevelsFilter->SetLevels( LevelNames );
	}

	/** Sets the SelectedLevelViewModel to the currently selected Level if exactly one is selected, otherwise NULL */
	void UpdateSelectedLevel()
	{
		UpdateLevelContentsFilter();

		if( LevelCollectionViewModel->GetSelectedLevels().Num() == 1 )
		{
			const auto Levels = LevelCollectionViewModel->GetSelectedLevels();
			check( Levels.Num() == 1 );
			SelectedLevelViewModel->SetDataSource( Levels[ 0 ]->GetLevel() );
		}
		else
		{
			SelectedLevelViewModel->SetDataSource( NULL );
		}
	}

	/** Called whenever the list of Levels has changed; Updates the LevelContentsFilter or SelectedLevel when necessary */
	void OnLevelsChanged( const ELevelsAction::Type Action, const TWeakObjectPtr< ULevel >& ChangedLevel, const FName& ChangedProperty )
	{
		if( Action != ELevelsAction::Reset && Action != ELevelsAction::Delete )
		{
			if( !ChangedLevel.IsValid() || SelectedLevelViewModel->GetLevel() == ChangedLevel )
			{
				UpdateLevelContentsFilter();
			}

			return;
		}

		UpdateSelectedLevel();

		//@todo: if in LevelContentsMode, transition to LevelsMode. (Once LevelContentsMode is implemented)
	}

	/** @return the SWidget containing the context menu */
	TSharedPtr< SWidget > ConstructLevelContextMenu()
	{
		LevelCollectionViewModel->CacheCanExecuteSourceControlVars();
		return SNew( SLevelsCommandsMenu, LevelCollectionViewModel.ToSharedRef() );
	}


private:

	/**	The base vertical box containing the LevelViewRows or LevelContents*/
	TSharedPtr< SVerticalBox > ContentAreaBox;

	/**	The border for the list of LevelViewRows */
	TSharedPtr< SBorder > LevelsSection;

	/**	The LevelTextFilter that constrains which Levels appear in the list */
	TSharedPtr< LevelTextFilter > SearchBoxLevelFilter;

	/**	The FActorsAssignedToSpecificLevelsFilter that constrains which Actors appear in the list of Actors for the specified level  */
	TSharedPtr< FActorsAssignedToSpecificLevelsFilter > SelectedLevelsFilter;

	/**	The ViewModel containing all LevelViewRows */
	TSharedPtr< FLevelCollectionViewModel > LevelCollectionViewModel;

	/**	The current mode of the LevelBrowser */
	ELevelBrowserMode::Type Mode;

	/**	The currently selected LevelViewModel if exactly one is selected, otherwise NULL */
	TSharedPtr< FLevelViewModel > SelectedLevelViewModel;
};
