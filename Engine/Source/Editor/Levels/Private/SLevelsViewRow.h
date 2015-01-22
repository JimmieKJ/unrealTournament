// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Editor/UnrealEd/Public/DragAndDrop/ActorDragDropGraphEdOp.h"
#include "AssetToolsModule.h"
#include "ISourceControlModule.h"
#include "SVectorInputBox.h"
#include "LevelUtils.h"
#include "Engine/LevelStreaming.h"

#define LOCTEXT_NAMESPACE "LevelsView"

namespace LevelsView
{
	/** IDs for list columns */
	static const FName ColumnID_LevelLabel( "Level" );
	static const FName ColumnID_Visibility( "Visibility" );
	static const FName ColumnID_Lock( "Lock" );
	static const FName ColumnID_SCCStatus( "SCC_Status" );
	static const FName ColumnID_Save( "Save" );
	static const FName ColumnID_Kismet( "Kismet" );
	static const FName ColumnID_Color( "Color" );
	static const FName ColumnID_ActorCount( "ActorCount" );
	static const FName ColumnID_LightmassSize( "LightmassSize" );
	static const FName ColumnID_FileSize( "FileSize" );
	static const FName ColumnID_EditorOffset( "EditorOffset" );
}

/**
 * The widget that represents a row in the LevelsView's list view control.  Generates widgets for each column on demand.
 */
class SLevelsViewRow : public SMultiColumnTableRow< TSharedPtr< FLevelViewModel > >
{

public:

	SLATE_BEGIN_ARGS( SLevelsViewRow ){}
		SLATE_ATTRIBUTE( FText, HighlightText )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The Level the row widget is supposed to represent
	 * @param	InOwnerTableView	The owner of the row widget
	 */
	void Construct( const FArguments& InArgs,  TSharedRef< FLevelViewModel > InViewModel, TSharedRef< STableViewBase > InOwnerTableView )
	{
		ViewModel = InViewModel;

		// Check we have the streaming level
		ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
		if(LevelStreaming)
		{
			// Cache Transform, so we can update spin box without rotating level till commit
			LevelTransform = LevelStreaming->LevelTransform;
		}			
		bSliderMovement = false;

		HighlightText = InArgs._HighlightText;

		SMultiColumnTableRow< TSharedPtr< FLevelViewModel > >::Construct( FSuperRowType::FArguments(), InOwnerTableView );

		ForegroundColor = TAttribute<FSlateColor>( this, &SLevelsViewRow::GetForegroundBasedOnSelection );
	}

	FSlateColor GetForegroundBasedOnSelection() const
	{
		static const FName DefaultForegroundName("DefaultForeground");
		return FEditorStyle::GetSlateColor(DefaultForegroundName);
	}

protected:

	/** @return the visibility for the Actor Count column */
	EVisibility IsActorColumnVisible() const
	{
		return (GetDefault<ULevelBrowserSettings>()->bDisplayActorCount)?
			EVisibility::Visible : EVisibility::Collapsed;
	}

	/** @return the visibility for the Lightmass Size column */
	EVisibility IsLightmassSizeColumnVisible() const
	{
		return (GetDefault<ULevelBrowserSettings>()->bDisplayLightmassSize)?
			EVisibility::Visible : EVisibility::Collapsed;
	}

	/** @return the visibility for the File Size column */
	EVisibility IsFileSizeColumnVisible() const
	{
		return (GetDefault<ULevelBrowserSettings>()->bDisplayFileSize)?
			EVisibility::Visible : EVisibility::Collapsed;
	}

	/** @return the visibility for the Editor Offset column */
	EVisibility IsEditorOffsetColumnVisible() const
	{
		return (ViewModel->IsPersistent() || !ViewModel->IsLevel()) ? EVisibility::Hidden : EVisibility::Visible;
	}

	/**
	 * Constructs the widget that represents the specified ColumnID for this Row
	 * 
	 * @param ColumnName    A unique ID for a column in this TableView; see SHeaderRow::FColumn for more info.
	 *
	 * @return a widget to represent the contents of a cell in this row of a TableView. 
	 */
	virtual TSharedRef< SWidget > GenerateWidgetForColumn( const FName& ColumnID ) override;

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
		TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
		if (!DragActorOp.IsValid())
		{
			return FReply::Unhandled();
		}

		bool bCanAssign = false;
		FText Message;
		if( DragActorOp->Actors.Num() > 1 )
		{
			bCanAssign = ViewModel->CanAssignActors( DragActorOp->Actors, OUT Message );
		}
		else
		{
			bCanAssign = ViewModel->CanAssignActor( DragActorOp->Actors[ 0 ], OUT Message );
		}

		if ( bCanAssign )
		{
			DragActorOp->SetToolTip( FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, Message );
		}
		else
		{
			DragActorOp->SetToolTip( FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, Message );
		}

		return FReply::Handled();
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
		TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
		if (!DragActorOp.IsValid())
		{
			return FReply::Unhandled();
		}

		ViewModel->AddActors( DragActorOp->Actors );

		return FReply::Handled();
	}


private:
	
	/** @return the FSlateFontInfo for the Level label; bold if current or level invalid */
	FSlateFontInfo GetFont() const
	{
		if ( ViewModel->IsCurrent() || 
			(!ViewModel->IsLevel() && ViewModel->IsLevelStreaming()) )
		{
			return FEditorStyle::GetFontStyle("LevelBrowser.LabelFontBold");
		}
		else
		{
			return FEditorStyle::GetFontStyle("LevelBrowser.LabelFont");
		}
	}

	/**
	 *	Gets the appropriate SlateColor for each button depending on the button's current state
	 *
	 *	@return			The ForegroundColor
	 */
	FSlateColor GetForegroundColorForVisibilityButton() const
	{
		return FSlateColor::UseForeground();
	}
	FSlateColor GetForegroundColorForLockButton() const
	{
		return FSlateColor::UseForeground();
	}
	FSlateColor GetForegroundColorForKismetButton() const
	{
		return FSlateColor::UseForeground();
	}

	/**
	 *	Returns the Color and Opacity for displaying the bound Level's Name.
	 *	The Color and Opacity changes depending on whether a drag/drop operation is occurring
	 *
	 *	@return	The SlateColor to render the Level's name in
	 */
	FSlateColor GetColorAndOpacity() const
	{
		if ( ViewModel->IsCurrent() )
		{
			return FLinearColor( 0.12f, 0.56f, 1.0f );
		}

		// Force the text to display red if level is missing
		if (!ViewModel->IsLevel() && ViewModel->IsLevelStreaming())
		{
			return FLinearColor( 1.0f, 0.0f, 0.0f);
		}

		if ( !FSlateApplication::Get().IsDragDropping() )
		{
			return FSlateColor::UseForeground();
		}

		bool bCanAcceptDrop = false;
		TSharedPtr<FDragDropOperation> DragDropOp = FSlateApplication::Get().GetDragDroppingContent();

		if (DragDropOp.IsValid() && DragDropOp->IsOfType<FActorDragDropGraphEdOp>())
		{
			TSharedPtr<FActorDragDropGraphEdOp> DragDropActorOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>(DragDropOp);

			FText Message;
			bCanAcceptDrop = ViewModel->CanAssignActors( DragDropActorOp->Actors, OUT Message );
		}

		return ( bCanAcceptDrop ) ? FSlateColor::UseForeground() : FLinearColor( 0.30f, 0.30f, 0.30f );
	}

	/**
	 *	Returns the Color and Opacity for displaying the bound Level's Name.
	 *	The Color and Opacity changes depending on whether a drag/drop operation is occurring
	 *
	 *	@return	The SlateColor to render the Level's name in
	 */
	FSlateColor GetLockButtonColorAndOpacity() const
	{
		if ( !ViewModel->GetLevel().IsValid() )
		{
			return FSlateColor::UseForeground();
		}

		return ( ViewModel->IsPersistent() ) ? FSlateColor::UseForeground() : FLinearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	}

	/**
	 *	Returns the Color and Opacity for displaying the Level's Save Button.
	 *
	 *	@return	The SlateColor to render the Level's Saves icon
	 */
	FSlateColor GetSaveButtonColorAndOpacity() const
	{
		if ( ViewModel->IsDirty() )
		{
			return FLinearColor::White;
		}
		else
		{
			return FSlateColor::UseForeground();
		}
	}

	/**
	 *	Returns the Color and Opacity for displaying the Level's color.
	 *
	 *	@return	The SlateColor to render the Level's color icon
	 */
	FSlateColor GetLevelColorAndOpacity() const
	{
		return ViewModel->GetColor();
	}

	/**
	 *	Called when the user clicks on the visibility icon for a Level's row widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnToggleVisibility()
	{
		ViewModel->ToggleVisibility();
		return FReply::Handled();
	}

	/**
	 *	Called when the user clicks on the lock icon for a Level's row widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnToggleLock()
	{
		// If were locking a level, lets make sure to close the level transform mode if its the same level currently selected for edit
		FEdModeLevel* LevelMode = static_cast<FEdModeLevel*>(GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Level ));		
		if( LevelMode )
		{
			ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
			if( LevelMode->IsEditing( LevelStreaming ) )
			{
				GLevelEditorModeTools().DeactivateMode( FBuiltinEditorModes::EM_Level );
			}
		}

		const FScopedTransaction Transaction( LOCTEXT("ToggleLevelLock", "Toggle Level Lock") );

		ViewModel->ToggleLock();
		return FReply::Handled();
	}

	/**
	 *	Called when the user clicks on the save icon for a Level's row widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnSave()
	{
		ViewModel->Save();
		return FReply::Handled();
	}

	/**
	 *	Called when the user clicks on the kismet icon for a Level's row widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnOpenKismet()
	{
		ViewModel->OpenKismet();
		return FReply::Handled();
	}

	/**
	 *	Called when the user clicks on the color icon for a Level's row widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnChangeColor()
	{
		ViewModel->ChangeColor(AsShared());
		return FReply::Handled();
	}

	/**
	 *	Called to get the Slate Image Brush representing the visibility state of
	 *	the Level this row widget represents
	 *
	 *	@return	The SlateBrush representing the Level's visibility state
	 */
	const FSlateBrush* GetVisibilityBrushForLevel() const
	{
		if ( ViewModel->IsLevel() )
		{
			if ( ViewModel->IsVisible() )
			{
				return VisibilityButton->IsHovered() ? FEditorStyle::GetBrush( "Level.VisibleHighlightIcon16x" ) :
													   FEditorStyle::GetBrush( "Level.VisibleIcon16x" );
			}
			else
			{
				return VisibilityButton->IsHovered() ? FEditorStyle::GetBrush( "Level.NotVisibleHighlightIcon16x" ) :
													   FEditorStyle::GetBrush( "Level.NotVisibleIcon16x" );
			}
		}
		else
		{
			return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
		}
	}

	/**
	 *	Called to get the Slate Image Brush representing the lock state of
	 *	the Level this row widget represents
	 *
	 *	@return	The SlateBrush representing the Level's lock state
	 */
	const FSlateBrush* GetLockBrushForLevel() const
	{
		if ( !ViewModel->IsLevel() || ViewModel->IsPersistent() )
		{
			//Locking the persistent level is not allowed; stub in a different brush
			return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
		}
		else 
		{
			//Non-Persistent
			if ( GEngine && GEngine->bLockReadOnlyLevels )
			{
				if(ViewModel->IsReadOnly())
				{
					return LockButton->IsHovered() ? FEditorStyle::GetBrush( "Level.ReadOnlyLockedHighlightIcon16x" ) :
													 FEditorStyle::GetBrush( "Level.ReadOnlyLockedIcon16x" );
				}
			}
			
			if ( ViewModel->IsLocked() )
			{
				return LockButton->IsHovered() ? FEditorStyle::GetBrush( "Level.LockedHighlightIcon16x" ) :
												 FEditorStyle::GetBrush( "Level.LockedIcon16x" );
			}
			else
			{
				return LockButton->IsHovered() ? FEditorStyle::GetBrush( "Level.UnlockedHighlightIcon16x" ) :
												 FEditorStyle::GetBrush( "Level.UnlockedIcon16x" );
			}
		}
	}

	/**
	 *	Called to get the tooltip string representing the lock state of
	 *	the Level this row widget represents
	 *
	 *	@return	The tooltip representing the Level's lock state
	 */
	FString GetLockToolTipForLevel() const
	{
		//Non-Persistent

		if ( GEngine && GEngine->bLockReadOnlyLevels )
		{
			if(ViewModel->IsReadOnly())
			{
				return LOCTEXT("ReadOnly_LockButtonToolTip", "Read-Only levels are locked!").ToString();
			}
		}

		return LOCTEXT("LockButtonToolTip", "Toggle Level Lock").ToString();
	}

	FString GetSCCStateTooltip() const
	{
		ULevel* Level = ViewModel->GetLevel().Get();
		if (Level != NULL)
		{
			FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(Level->GetOutermost(), EStateCacheUsage::Use);
			if(SourceControlState.IsValid())
			{
				return SourceControlState->GetDisplayTooltip().ToString();
			}
		}

		return FString();
	}

	const FSlateBrush* GetSCCStateImage() const
	{
		ULevel* Level = ViewModel->GetLevel().Get();
		if (Level != NULL)
		{
			FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(Level->GetOutermost(), EStateCacheUsage::Use);
			if(SourceControlState.IsValid())
			{
				return FEditorStyle::GetBrush(SourceControlState->GetSmallIconName());
			}
		}
		return NULL;
	}

	/**
	 *	Called to get the Slate Image Brush representing the save state of
	 *	the Level this row widget represents
	 *
	 *	@return	The SlateBrush representing the Level's save state
	 */
	const FSlateBrush* GetSaveBrushForLevel() const
	{
		if ( ViewModel->IsLevel() )
		{
			if ( ViewModel->IsLocked() )
			{
				return FEditorStyle::GetBrush( "Level.SaveDisabledIcon16x" );
			}
			else
			{
				if ( ViewModel->IsDirty() )
				{
					return SaveButton->IsHovered() ? FEditorStyle::GetBrush( "Level.SaveModifiedHighlightIcon16x" ) :
													 FEditorStyle::GetBrush( "Level.SaveModifiedIcon16x" );
				}
				else
				{
					return SaveButton->IsHovered() ? FEditorStyle::GetBrush( "Level.SaveHighlightIcon16x" ) :
													 FEditorStyle::GetBrush( "Level.SaveIcon16x" );
				}
			}								
		}
		else
		{
			return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
		}	
	}

	/**
	 *	Called to get the Slate Image Brush representing the kismet state of
	 *	the Level this row widget represents
	 *
	 *	@return	The SlateBrush representing the Level's kismet state
	 */
	const FSlateBrush* GetKismetBrushForLevel() const
	{
		if ( ViewModel->IsLevel() )
		{
			if ( ViewModel->HasKismet() )
			{
				return KismetButton->IsHovered() ? FEditorStyle::GetBrush( "Level.ScriptHighlightIcon16x" ) :
												   FEditorStyle::GetBrush( "Level.ScriptIcon16x" );
			}
			else
			{
				return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
			}
		}
		else
		{
			return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
		}	
	}

	/**
	 *	Called to get the Slate Image Brush representing the color of
	 *	the Level this row widget represents
	 *
	 *	@return	The SlateBrush representing the Level's color
	 */
	const FSlateBrush* GetColorBrushForLevel() const
	{
		if ( !ViewModel->IsLevel() || ViewModel->IsPersistent() )
		{
			//stub in a different brush for the persistent level, since the color cannot be changed
			return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
		}
		else
		{
			return FEditorStyle::GetBrush( "Level.ColorIcon40x" );
		}
	}

	/**
	 *	Called by the Editor Level Transform column, to set the new values.
	 *
	 *	@param NewValue		The new value entered.
	 *	@param CommitInfo	The way in which the value was committed, currently ignored, we always use the new value.
	 *	@param Axis			The axis being edited.
	 */
	void OnSetLevelOffset( float NewValue, ETextCommit::Type CommitInfo, int32 Axis )
	{
		// Check we have the streaming level
		ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
		if(!LevelStreaming)
		{
			return;
		}

		// Setup a new transform		
		FVector Translation = LevelTransform.GetTranslation();
		Translation[Axis] = NewValue;
		LevelTransform.SetTranslation(Translation);
		FLevelUtils::SetEditorTransform(LevelStreaming, LevelTransform);
	}

	/**
	 *	Called by the Editor Level Transform column, to get the current values.
	 *
	 *	@param Axis		The axis to return the value for.
	 *
	 *	@return			The current value of the translation for given axis.
	 */
	TOptional<float> GetLevelOffset( int32 Axis ) const
	{
		ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
		if(!LevelStreaming)
		{
			FVector Translation = LevelTransform.GetTranslation();
			return Translation[Axis];
		}	
		FVector Translation = LevelStreaming->LevelTransform.GetTranslation();
		return Translation[Axis];
	}	

	/**
	 *	Called by the Editor Level Rotation column, to commit the new values.
	 *
	 *	@param NewValue		The new value entered.
	 *	@param CommitInfo	The way in which the value was committed, currently ignored, we always use the new value.
	 */
	void OnCommitLevelRotation( float NewValue, ETextCommit::Type CommitInfo )
	{
		// Check we have the streaming level
		ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
		if(!LevelStreaming)
		{
			return;
		}		
		// Use LevelTransform set by OnSetLeveRotation as OnCommitLevelRotation NewValue seems to ignore min delta of a spin box
		FLevelUtils::SetEditorTransform(LevelStreaming, LevelTransform);
	}

	/**
	 *	Called by the Editor Level Rotation column, to set the new values.
	 *
	 *	@param NewValue		The new value entered.
	 */
	void OnSetLevelRotation( float NewValue )
	{		
		FRotator Rotation = LevelTransform.GetRotation().Rotator();
		Rotation.Yaw = NewValue;
		LevelTransform.SetRotation(Rotation.Quaternion());
	}

	/**
	 *	Called by the Editor Level Rotation column, to set weve started spinning the value.
	 */
	void OnBeginLevelRotatonSlider()
	{		
		bSliderMovement = true;
	}

	/**
	 *	Called by the Editor Level Rotation column, to set weve stopped spinning the value.
	 *
	 *	@param NewValue		The new value entered.
	 */
	void OnEndLevelRotatonSlider( float NewValue )
	{
		bSliderMovement = false;
	}

	/**
	 *	Called by the Editor Level Transform column, to get the current values.
	 *
	 *	@return			The current value of the Level Yaw.
	 */
	TOptional<float> GetLevelRotation() const
	{
		// If were not using the spin box use the actual transform instead of cached as it may have changed with the view port widget
		if( !bSliderMovement )
		{
			ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
			if(LevelStreaming)
			{				
				return LevelStreaming->LevelTransform.GetRotation().Rotator().Yaw;
			}			
		}
		return LevelTransform.GetRotation().Rotator().Yaw;	
	}

	/**
	 *	Called by the Editor Level Edit Viewport button.
	 */
	FReply OnEditLevelClicked() 
	{
		ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
		if(!LevelStreaming)
		{
			return FReply::Handled();
		}		

		if( !GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Level))
		{
			// Activate Level Mode if it was not active
			GLevelEditorModeTools().ActivateMode( FBuiltinEditorModes::EM_Level );
		}
			
		FEdModeLevel* ActiveMode = static_cast<FEdModeLevel*>(GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Level ));		
		check(ActiveMode != NULL);

		if( ActiveMode->IsEditing(LevelStreaming) == true )
		{
			// Toggle this mode off if already editing this level
			GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_Level);

			// Cache Transform, as it might of changed by doing a view port edit
			LevelTransform = LevelStreaming->LevelTransform;
		}
		else
		{
			// Set the level we now want to edit
			ActiveMode->SetLevel(LevelStreaming);
		}		

		return FReply::Handled();
	}

	bool LevelTransformAllowed() const
	{
		ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
		if(LevelStreaming)
		{
			if( LevelStreaming->GetLoadedLevel()->bIsVisible && !LevelStreaming->bLocked )
			{
				return true;
			}
		}
		return false;
	}

	bool LevelEditTextTransformAllowed() const
	{
		ULevelStreaming* LevelStreaming = ViewModel->GetLevelStreaming().Get();
		FEdModeLevel* ActiveMode = static_cast<FEdModeLevel*>(GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Level ));		
		if( ActiveMode && ActiveMode->IsEditing(LevelStreaming) == true )
		{
			return false;
		}

		return LevelTransformAllowed();
	}

private:

	/** The Level associated with this row of data */
	TSharedPtr< FLevelViewModel > ViewModel;

	/**	The visibility button for the Level */
	TSharedPtr< SButton > VisibilityButton;

	/**	The lock button for the Level */
	TSharedPtr< SButton > LockButton;

	/**	The save button for the Level */
	TSharedPtr< SButton > SaveButton;

	/**	The kismet button for the Level */
	TSharedPtr< SButton > KismetButton;

	/**	The color button for the Level */
	TSharedPtr< SButton > ColorButton;

	/** The string to highlight on any text contained in the row widget */
	TAttribute< FText > HighlightText;

	/** Cached Level Transform, so we don't have to edit the original before we commit change*/
	FTransform LevelTransform;

	/** Set When the user is using the rotation spin box so we can update the value with the cached Transform */
	bool bSliderMovement;
};


#undef LOCTEXT_NAMESPACE
