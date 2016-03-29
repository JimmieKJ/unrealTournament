// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "SequenceRecorderUtils.h"

namespace SequenceRecorderUtils
{

AActor* GetAttachment(AActor* InActor, FName& SocketName, FName& ComponentName)
{
	AActor* AttachedActor = nullptr;
	if(InActor)
	{
		USceneComponent* RootComponent = InActor->GetRootComponent();
		if(RootComponent && RootComponent->AttachParent != nullptr)
		{
			AttachedActor = RootComponent->AttachParent->GetOwner();
			SocketName = RootComponent->AttachSocketName;
			ComponentName = RootComponent->AttachParent->GetFName();
		}
	}

	return AttachedActor;
}

}