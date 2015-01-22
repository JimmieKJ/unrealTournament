// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "SceneOutlinerFwd.h"

/**
 * Implements the Scene Outliner module.
 */
class FSceneOutlinerModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a scene outliner widget
	 *
	 * @param	InitOptions						Programmer-driven configuration for this widget instance
	 * @param	MakeContentMenuWidgetDelegate	Optional delegate to execute to build a context menu when right clicking on actors
	 * @param	OnActorPickedDelegate			Optional callback when an actor is selected in 'actor picking' mode
	 *
	 * @return	New scene outliner widget
	 */
	virtual TSharedRef< ISceneOutliner > CreateSceneOutliner(
		const SceneOutliner::FInitializationOptions& InitOptions,
		const FOnActorPicked& OnActorPickedDelegate ) const;

	/**
	 * Creates a scene outliner widget
	 *
	 * @param	InitOptions						Programmer-driven configuration for this widget instance
	 * @param	MakeContentMenuWidgetDelegate	Optional delegate to execute to build a context menu when right clicking on actors
	 * @param	OnItemPickedDelegate			Optional callback when an item is selected in 'picking' mode
	 *
	 * @return	New scene outliner widget
	 */
	virtual TSharedRef< ISceneOutliner > CreateSceneOutliner(
		const SceneOutliner::FInitializationOptions& InitOptions,
		const FOnSceneOutlinerItemPicked& OnItemPickedDelegate ) const;

public:
	/** Register a new type of column available to all scene outliners */
	template< typename T >
	void RegisterColumnType()
	{
		auto ID = T::GetID();
		if ( !ColumnMap.Contains( ID ) )
		{
			auto CreateColumn = []( ISceneOutliner& Outliner ){
				return TSharedRef< ISceneOutlinerColumn >( MakeShareable( new T(Outliner) ) );
			};

			ColumnMap.Add( ID, FCreateSceneOutlinerColumn::CreateStatic( CreateColumn ) );
		}
	}

	/** Unregister a previously registered column type */
	template< typename T >
	void UnRegisterColumnType()
	{
		ColumnMap.Remove( T::GetID() );
	}

	/** Factory a new column from the specified name. Returns null if no type has been registered under that name. */
	TSharedPtr< ISceneOutlinerColumn > FactoryColumn( FName ID, ISceneOutliner& Outliner ) const
	{
		if ( auto* Factory = ColumnMap.Find( ID ) )
		{
			return Factory->Execute(Outliner);
		}
		
		return nullptr;
	}

private:

	/** Map of column type name -> factory delegate */
	TMap< FName, FCreateSceneOutlinerColumn > ColumnMap;

public:

	// IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
