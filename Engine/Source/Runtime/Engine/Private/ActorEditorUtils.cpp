// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActorEditorUtils.h"

#if WITH_EDITOR
#include "ComponentEditorUtils.h"
#endif

namespace FActorEditorUtils
{
	bool IsABuilderBrush( const AActor* InActor )
	{
		bool bIsBuilder = false;
#if WITH_EDITOR		
		if ( InActor && InActor->GetWorld() && !InActor->HasAnyFlags(RF_ClassDefaultObject) )
		{
			ULevel* ActorLevel = InActor->GetLevel();
			if ((ActorLevel != nullptr) && (ActorLevel->Actors.Num() >= 2))
			{
				// If the builder brush exists then it will be the 2nd actor in the actors array.
				ABrush* BuilderBrush = Cast<ABrush>(ActorLevel->Actors[1]);
				// If the second actor is not a brush then it certainly cannot be the builder brush.
				if ((BuilderBrush != nullptr) && (BuilderBrush->GetBrushComponent() != nullptr) && (BuilderBrush->Brush != nullptr))
				{
					bIsBuilder = (BuilderBrush == InActor);
				}
			}			
		}
#endif
		return bIsBuilder;
	}

	bool IsAPreviewOrInactiveActor( const AActor* InActor )
	{
		UWorld* World = InActor ? InActor->GetWorld() : NULL;
		return World && (World->WorldType == EWorldType::Preview || World->WorldType == EWorldType::Inactive);
	}

	void GetEditableComponents( const AActor* InActor, TArray<UActorComponent*>& OutEditableComponents )
	{
		TInlineComponentArray<UActorComponent*> InstanceComponents;
		InActor->GetComponents(InstanceComponents);
		for (auto Component : InstanceComponents)
		{
#if WITH_EDITOR
			if (Component->CreationMethod == EComponentCreationMethod::Native)
			{
				// Make sure it's an exposed native component
				if (FComponentEditorUtils::CanEditNativeComponent(Component))
				{
					OutEditableComponents.Add(Component);
				}
			}
			else if (Component->CreationMethod == EComponentCreationMethod::Instance)
#else
			if (!Component->IsCreatedByConstructionScript())
#endif
			{
				OutEditableComponents.Add(Component);
			}
		}
	}
}


