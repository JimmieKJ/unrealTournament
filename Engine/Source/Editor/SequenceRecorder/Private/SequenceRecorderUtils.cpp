// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderUtils.h"
#include "GameFramework/Actor.h"

namespace SequenceRecorderUtils
{

AActor* GetAttachment(AActor* InActor, FName& SocketName, FName& ComponentName)
{
	AActor* AttachedActor = nullptr;
	if(InActor)
	{
		USceneComponent* RootComponent = InActor->GetRootComponent();
		if(RootComponent && RootComponent->GetAttachParent() != nullptr)
		{
			AttachedActor = RootComponent->GetAttachParent()->GetOwner();
			SocketName = RootComponent->GetAttachSocketName();
			ComponentName = RootComponent->GetAttachParent()->GetFName();
		}
	}

	return AttachedActor;
}

}
