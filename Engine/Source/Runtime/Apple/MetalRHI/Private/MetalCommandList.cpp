// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandList.cpp: Metal command buffer list wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandList.h"
#include "MetalCommandQueue.h"

#pragma mark - Public C++ Boilerplate -

FMetalCommandList::FMetalCommandList(FMetalCommandQueue& InCommandQueue, bool const bInImmediate)
: CommandQueue(InCommandQueue)
, SubmittedBuffers(!bInImmediate ? [NSMutableArray<id<MTLCommandBuffer>> new] : nil)
, bImmediate(bInImmediate)
{
}

FMetalCommandList::~FMetalCommandList(void)
{
	[SubmittedBuffers release];
	SubmittedBuffers = nil;
}
	
#pragma mark - Public Command List Mutators -

void FMetalCommandList::Commit(id<MTLCommandBuffer> Buffer, bool const bWait)
{
	check(Buffer);
	
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	[Buffer addCompletedHandler : ^ (id <MTLCommandBuffer> CompletedBuffer)
	{
		if (CompletedBuffer.status == MTLCommandBufferStatusError)
		{
			NSString* Label = CompletedBuffer.label;
			int32 Code = CompletedBuffer.error.code;
			NSString* Domain = CompletedBuffer.error.domain;
			NSString* ErrorDesc = CompletedBuffer.error.localizedDescription;
			NSString* FailureDesc = CompletedBuffer.error.localizedFailureReason;
			NSString* RecoveryDesc = CompletedBuffer.error.localizedRecoverySuggestion;
			
			FString LabelString = Label ? FString(Label) : FString(TEXT("Unknown"));
			FString DomainString = Domain ? FString(Domain) : FString(TEXT("Unknown"));
			FString ErrorString = ErrorDesc ? FString(ErrorDesc) : FString(TEXT("Unknown"));
			FString FailureString = FailureDesc ? FString(FailureDesc) : FString(TEXT("Unknown"));
			FString RecoveryString = RecoveryDesc ? FString(RecoveryDesc) : FString(TEXT("Unknown"));
			
			UE_LOG(LogMetal, Fatal, TEXT("Command Buffer %s Failure! Error Domain: %s Code: %d Description %s %s %s"), *LabelString, *DomainString, Code, *ErrorString, *FailureString, *RecoveryString);
		}
	}];
#endif
	
	if (bImmediate)
	{
		CommandQueue.CommitCommandBuffer(Buffer);
		if (bWait)
		{
			[Buffer waitUntilCompleted];
		}
	}
	else
	{
		check(SubmittedBuffers);
		check(!bWait);
		[SubmittedBuffers addObject: Buffer];
	}
}

void FMetalCommandList::Submit(uint32 Index, uint32 Count)
{
	// Only deferred contexts should call Submit, the immediate context commits directly to the command-queue.
	check(!bImmediate);

	// Command queue takes ownership of the array
	CommandQueue.SubmitCommandBuffers(this, Index, Count);
}

void FMetalCommandList::OnScheduled(void)
{
	[SubmittedBuffers removeAllObjects];
}
	
#pragma mark - Public Command List Accessors -

NSArray<id<MTLCommandBuffer>>* FMetalCommandList::GetCommandBuffers() const
{
	return SubmittedBuffers;
}
