// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PrivatePCH.h"
#include "Parent.h"

FParent::FParent(int InNumTotalChildren, int InMaxChildrenAtOnce)
	:	NumTotalChildren(InNumTotalChildren)
	,	MaxChildrenAtOnce(InMaxChildrenAtOnce)
{

}

FProcHandle FParent::Launch()
{
	// Launch the worker process
	int32 PriorityModifier = -1; // below normal

	uint32 WorkerId = 0;
	FString WorkerName = FPlatformProcess::ExecutableName(false);
	FProcHandle WorkerHandle = FPlatformProcess::CreateProc(*WorkerName, TEXT("proc-child"), true, false, false, &WorkerId, PriorityModifier, NULL, NULL);
	if (!WorkerHandle.IsValid())
	{
		// If this doesn't error, the app will hang waiting for jobs that can never be completed
		UE_LOG(LogTestPAL, Fatal, TEXT("Couldn't launch %s! Make sure the file is in your binaries folder."), *WorkerName);
	}

	return WorkerHandle;
}

void FParent::Run()
{
	while (NumTotalChildren > 0)
	{
		// see if there are any new children to spawn
		while (Children.Num() < MaxChildrenAtOnce)
		{
			UE_LOG(LogTestPAL, Log, TEXT("Launching a child (%d more to go)."), NumTotalChildren - 1);
			FProcHandle Child = Launch();
			UE_LOG(LogTestPAL, Log, TEXT("Launch successful"));

			Children.Add(Child);
			--NumTotalChildren;
		}

		// sleep for a while
		FPlatformProcess::Sleep(0.5f);

		// see if any children have finished
		for (int ChildIdx = 0; ChildIdx < Children.Num();)
		{
			int32 ReturnCode = -1;
			if (FPlatformProcess::GetProcReturnCode(Children[ChildIdx], &ReturnCode))
			{
				// print its return code
				UE_LOG(LogTestPAL, Log, TEXT("Child finished, return code %d"), ReturnCode);

				Children.RemoveAt(ChildIdx);
			}
			else
			{
				++ChildIdx;
			}
		}
	}
}
