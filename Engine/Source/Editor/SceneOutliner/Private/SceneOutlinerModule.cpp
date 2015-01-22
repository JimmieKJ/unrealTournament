// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerPrivatePCH.h"
#include "ModuleManager.h"

#include "SceneOutlinerActorInfoColumn.h"
#include "SceneOutlinerGutter.h"
#include "SceneOutlinerItemLabelColumn.h"

/* FSceneOutlinerModule interface
 *****************************************************************************/

namespace SceneOutliner
{
	void OnSceneOutlinerItemClicked(TSharedRef<ITreeItem> Item, FOnActorPicked OnActorPicked)
	{
		Item->Visit(
			FFunctionalVisitor()
			.Actor([&](const FActorTreeItem& ActorItem){
				if (AActor* Actor = ActorItem.Actor.Get())
				{
					OnActorPicked.ExecuteIfBound(Actor);
				}
			})
		);
	}
}


void FSceneOutlinerModule::StartupModule()
{
	RegisterColumnType< SceneOutliner::FSceneOutlinerGutter >();
	RegisterColumnType< SceneOutliner::FItemLabelColumn >();
	RegisterColumnType< SceneOutliner::FActorInfoColumn >();
}


void FSceneOutlinerModule::ShutdownModule()
{
	UnRegisterColumnType< SceneOutliner::FSceneOutlinerGutter >();
	UnRegisterColumnType< SceneOutliner::FItemLabelColumn >();
	UnRegisterColumnType< SceneOutliner::FActorInfoColumn >();
}


TSharedRef< ISceneOutliner > FSceneOutlinerModule::CreateSceneOutliner( const SceneOutliner::FInitializationOptions& InitOptions, const FOnActorPicked& OnActorPickedDelegate ) const
{
	auto OnItemPicked = FOnSceneOutlinerItemPicked::CreateStatic( &SceneOutliner::OnSceneOutlinerItemClicked, OnActorPickedDelegate );
	return CreateSceneOutliner(InitOptions, OnItemPicked);
}


TSharedRef< ISceneOutliner > FSceneOutlinerModule::CreateSceneOutliner( const SceneOutliner::FInitializationOptions& InitOptions, const FOnSceneOutlinerItemPicked& OnItemPickedDelegate ) const
{
	return SNew( SceneOutliner::SSceneOutliner, InitOptions )
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		.OnItemPickedDelegate( OnItemPickedDelegate );
}


/* Class constructors
 *****************************************************************************/

USceneOutlinerSettings::USceneOutlinerSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


IMPLEMENT_MODULE(FSceneOutlinerModule, SceneOutliner);
