// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __ActorEditorUtils_h__
#define __ActorEditorUtils_h__

#pragma once



namespace FActorEditorUtils
{
	/**
	* Return whether this actor is a builder brush or not.
	*
	* @return true if this actor is a builder brush, false otherwise
	*/
	ENGINE_API bool IsABuilderBrush( const AActor* InActor );

	/** 
	* Return whether this actor is a preview actor or not.
	*
	* @return true if this actor is a preview actor, false otherwise
	*/
	ENGINE_API bool IsAPreviewOrInactiveActor( const AActor* InActor );

	/**
	 * Gets a list of components, from an actor, which can be externally modified.  Editable components have properties visible when viewing their owner actor in a details view
	 *
	 * @param InActor		The actor to get components from
	 * @param OutEditableComponents The populated list of editable components on InActor
	 */
	ENGINE_API void GetEditableComponents( const AActor* InActor, TArray<UActorComponent*>& OutEditableComponents );
};

#endif// __ActorEditorUtils_h__
