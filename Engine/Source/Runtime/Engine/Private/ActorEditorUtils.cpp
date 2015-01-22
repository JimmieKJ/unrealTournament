// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActorEditorUtils.h"

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
		for( TFieldIterator<UObjectProperty> PropIt(InActor->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt )
		{
			UObjectProperty* ObjectProp = *PropIt;
			if( ObjectProp->HasAnyPropertyFlags(CPF_Edit) )
			{
				UObject* ObjPtr = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(InActor));

				if( ObjPtr && ObjPtr->IsA<UActorComponent>() )
				{
					OutEditableComponents.Add( CastChecked<UActorComponent>( ObjPtr ) );
				}
			}
		}
	}
}


