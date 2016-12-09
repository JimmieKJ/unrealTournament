// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/Attribute.h"
#include "Templates/SubclassOf.h"
#include "Framework/Commands/UICommandList.h"
#include "ISequencerSection.h"
#include "MovieSceneTrack.h"

class FMenuBuilder;
class FPaintArgs;
class FSlateWindowElementList;
class SHorizontalBox;
class UMovieScene;

/** Data structure containing information required to build an edit widget */
struct FBuildEditWidgetParams
{
	/** Attribute that specifies when the node relating to this edit widget is hovered */
	TAttribute<bool> NodeIsHovered;
};

/**
 * Interface for sequencer track editors.
 */
class ISequencerTrackEditor
{
public:

	/**
	 * Manually adds a key.
	 *
	 * @param ObjectGuid The Guid of the object that we are adding a key to.
	 */
	virtual void AddKey(const FGuid& ObjectGuid) = 0;

	/**
	 * Add a new track to the sequence.
	 */
	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) = 0;

	/**
	 * Allows the track editors to bind commands.
	 *
	 * @param SequencerCommandBindings The command bindings to map to.
	*/
	virtual void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) = 0;

	/**
	 * Builds up the sequencer's "Add Track" menu.
	 *
	 * @param MenuBuilder The menu builder to change.
	 */
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) = 0;

	/**
	 * Builds up the object binding edit buttons for the outliner.
	 *
	 * @param EditBox The edit box to add buttons to.
	 * @param ObjectBinding The object binding this is for.
	 * @param ObjectClass The class of the object this is for.
	 */
	virtual void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) = 0;

	/**
	 * Builds up the object binding track menu for the outliner.
	 *
	 * @param MenuBuilder The menu builder to change.
	 * @param ObjectBinding The object binding this is for.
	 * @param ObjectClass The class of the object this is for.
	 */
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) = 0;

	/**
	 * Builds an edit widget for the outliner nodes which represent tracks which are edited by this editor.
	 * @param ObjectBinding The object binding associated with the track being edited by this editor.
	 * @param Track The track being edited by this editor.
	 * @param Params Parameter struct containing data relevant to the edit widget
	 * @returns The the widget to display in the outliner, or an empty shared ptr if not widget is to be displayed.
	 */
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) = 0;

	/**
	 * Builds the context menu for the track.
	 * @param MenuBuilder The menu builder to use to build the track menu. 
	 */
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) = 0;

	/**
	 * Called when an asset is dropped into Sequencer. Can potentially consume the asset
	 * so it doesn't get added as a spawnable.
	 *
	 * @param Asset The asset that is dropped in.
	 * @param TargetObjectGuid The object guid this asset is dropped onto, if applicable.
	 * @return true if we want to consume this asset, false otherwise.
	 */
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) = 0;

	/** Gets whether the tool can key all*/
	virtual bool IsAllowedKeyAll() const = 0;

	/** Gets whether the tool can legally autokey */
	virtual bool IsAllowedToAutoKey() const = 0;

	/**
	 * Called to generate a section layout for a particular section.
	 *
	 * @param SectionObject The section to make UI for.
	 * @param Track The track that owns the section.
	 * @param ObjectBinding the object binding for the track that owns the section, if there is one.
	 */
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) = 0;

	/** Gets an icon brush for this track editor */
	virtual const FSlateBrush* GetIconBrush() const { return nullptr; }

	/** Called when the instance of this track editor is initialized */
	virtual void OnInitialize() = 0;

	/** Called when the instance of this track editor is released */
	virtual void OnRelease() = 0;

	/** Allows the track editor to paint on a track area. */
	virtual int32 PaintTrackArea(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle) = 0;

	/**
	 * Returns whether a track class is supported by this tool.
	 *
	 * @param TrackClass The track class that could be supported.
	 * @return true if the type is supported.
	 */
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const = 0;

	/**
	 * Ticks this tool.
	 *
	 * @param DeltaTime The time since the last tick.
	 */
	virtual void Tick(float DeltaTime) = 0;

public:

	/** Virtual destructor. */
	virtual ~ISequencerTrackEditor() { }
};
