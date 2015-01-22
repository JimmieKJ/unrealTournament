// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintGraphDefinitions.h"		// @todo sequencer uobject: workaround for header generator dependencies
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_PlayMovieScene.generated.h"

#if WITH_EDITOR
DECLARE_EVENT( UK2Node_PlayMovieScene, FOnMovieSceneBindingsChanged );
#endif

/** Action to add a 'Play MovieScene' node to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddPlayMovieScene : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	FEdGraphSchemaAction_K2AddPlayMovieScene()
		: FEdGraphSchemaAction_K2NewNode()
	{
	}
	
	FEdGraphSchemaAction_K2AddPlayMovieScene(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{
	}
	
	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface

};


UCLASS( MinimalAPI )
class UK2Node_PlayMovieScene : public UK2Node
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	// UEdGraphNode interface.
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	// UK2Node interface
	virtual bool NodeCausesStructuralBlueprintChange() const override
	{ 
		// @todo sequencer: Not sure if we really need to return true for this
		return true; 
	}
	virtual void PinConnectionListChanged( UEdGraphPin* Pin ) override;


	/**
	 * Associates a MovieScene asset with this node
	 *
	 * @param	NewMovieScene	The MovieScene asset to use (can be NULL to clear it)
	 */
	virtual void SetMovieScene( class UMovieScene* NewMovieScene );

	/** @return	Returns the MovieScene associated with this node (or NULL, if no MovieScene is associated yet) */
	virtual class UMovieScene* GetMovieScene();

	/** @return	Returns MovieSceneBindings for this node */
	virtual class UMovieSceneBindings* GetMovieSceneBindings();

	/**
	 * Binds a possessable in the MovieScene asset (based on the guid of that possessable) to the specified objects
	 *
	 * @param	PossessableGuid		The guid of the posessable in the MovieScene to bind to
	 * @param	Objects				The object(s) to bind to
	 */
	virtual void BindPossessableToObjects( const FGuid& PossessableGuid, const TArray< UObject* >& Objects );

	/**
	 * Given a guid for a possessable, attempts to find the object bound to that guid
	 *
	 * @param	Guid	The possessable guid to search for
	 *
	 * @return	The bound objects, if found, otherwise NULL
	 */
	virtual TArray< UObject* > FindBoundObjects( const FGuid& Guid );

	/**
	 * Finds a guid for a bound posessable object if it exists 
	 *
	 * @param Object	The object to search for
	 * @return A valid guid if the object was found.  Otherwise the guid is invalid
	 */
	virtual FGuid FindGuidForObject( UObject* Object ) const;

	/**
	 * Returns access to an event you can bind to to find out when the object bindings change
	 *
	 * @return	OnBindingsChanged event object
	 */
	virtual FOnMovieSceneBindingsChanged& OnBindingsChanged()
	{
		return OnBindingsChangedEvent;
	}

#endif // #if WITH_EDITOR

	/** @return	Returns the graph pin for the "Play" input */
	MOVIESCENETOOLS_API UEdGraphPin* GetPlayPin() const;

	/** @return	Returns the graph pin for the "Pause" input */
	MOVIESCENETOOLS_API UEdGraphPin* GetPausePin() const;

protected:

#if WITH_EDITOR

	/**
	 * Creates a pin on this node for the specified bound object
	 *
	 * @param	BoundObject		The object to create a pin for
	 */
	virtual void CreatePinForBoundObject( struct FMovieSceneBoundObject& BoundObject );

	/**
	 * Creates a MovieSceneBindings object if we don't have one yet
	 */
	void CreateBindingsIfNeeded();
#endif

protected:

	/** Binding information that connects the MovieScene to objects that can be possessed.  At runtime, the bindings
	   object is actually owned by the LevelScriptActor in the level this node is associated with. */
	UPROPERTY()
	class UMovieSceneBindings* MovieSceneBindings;

#if WITH_EDITOR
	/** Event that is broadcast when the bindings are changed */
	FOnMovieSceneBindingsChanged OnBindingsChangedEvent;

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
#endif
};

