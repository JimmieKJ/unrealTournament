// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UMovieSceneSection;
class FSequencerDisplayNode;
class FObjectBindingNode;
class UMovieSceneTrack;

/**
 * Represents a tree of sequencer display nodes, used to populate the Sequencer UI with MovieScene data
 */
class FSequencerNodeTree : public TSharedFromThis<FSequencerNodeTree>
{
public:
	FSequencerNodeTree( class FSequencer& InSequencer )
		: Sequencer( InSequencer )
	{}
	
	/**
	 * Empties the entire tree
	 */
	void Empty();

	/**
	 * Updates the tree with sections from a MovieScene
	 */
	void Update();

	/**
	 * @return The root nodes of the tree
	 */
	const TArray< TSharedRef<FSequencerDisplayNode> >& GetRootNodes() const;

	/** @return Whether or not there is an active filter */
	bool HasActiveFilter() const { return !FilterString.IsEmpty(); }

	/**
	 * Returns whether or not a node is filtered
	 *
	 * @param Node	The node to check if it is filtered
	 */
	bool IsNodeFiltered( const TSharedRef<const FSequencerDisplayNode> Node ) const;
	
	/**
	 * Filters the nodes based on the passed in filter terms
	 *
	 * @param InFilter	The filter terms
	 */
	void FilterNodes( const FString& InFilter );

	/** Gets the parent sequencer of this tree */
	FSequencer& GetSequencer() {return Sequencer;}

	/**
	 * Saves the expansion state of a display node
	 *
	 * @param Node		The node whose expansion state should be saved
	 * @param bExpanded	The new expansion state of the node
	 */
	void SaveExpansionState( const FSequencerDisplayNode& Node, bool bExpanded );

	/**
	 * Gets the saved expansion state of a display node
	 *
	 * @param Node	The node whose expansion state may have been saved
	 * @return true if the node should be expanded, false otherwise	
	 */
	bool GetSavedExpansionState( const FSequencerDisplayNode& Node ) const;
	
	/** Updates nodes visibility for use when shot filters change at all */
	void UpdateCachedVisibilityBasedOnShotFiltersChanged();

private:
	/**
	 * Finds or adds a type editor for the track
	 *
	 * @param Track	The type to find an editor for
	 * @rerturn The editor for the type
	 */
	TSharedRef<FMovieSceneTrackEditor> FindOrAddTypeEditor( UMovieSceneTrack& Track );

	/**
	 * Makes section interfaces for all sections in a track
	 *
	 * @param Track	The type to get sections from
	 * @param SectionAreaNode	The section area which section interfaces belong to
	 */
	void MakeSectionInterfaces( UMovieSceneTrack& Track, TSharedRef<class FTrackNode>& SectionAreaNode );

	/**
	 * Creates a new root object binding node
	 * 
	 * @param ObjectBinding	The object binding guid
	 */
	TSharedRef<FObjectBindingNode> AddObjectBinding( const FString& ObjectName, const FGuid& ObjectBinding );
private:
	/** Tools for building movie scene section layouts.  One tool for each track */
	TMap< class UMovieSceneTrack*, TSharedPtr<FMovieSceneTrackEditor> > EditorMap;
	/** Root nodes */
	TArray< TSharedRef<FSequencerDisplayNode> > RootNodes;
	/** Mapping of object binding guids to their node (for fast lookup) */
	TMap< FGuid, TSharedPtr<FObjectBindingNode> > ObjectBindingMap;
	/** Set of all filtered nodes */
	TSet< TSharedRef<const FSequencerDisplayNode> > FilteredNodes;
	/** Active filter string if any */
	FString FilterString;
	/** Sequencer interface */
	FSequencer& Sequencer;
};
