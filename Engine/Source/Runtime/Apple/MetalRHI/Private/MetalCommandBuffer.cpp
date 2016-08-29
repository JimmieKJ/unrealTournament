// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandBuffere.cpp: Metal command buffer wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandBuffer.h"

NSString* GMetalDebugCommandTypeNames[EMetalDebugCommandTypeInvalid] = {
	@"RenderEncoder",
	@"ComputeEncoder",
	@"BlitEncoder",
	@"EndEncoder",
	@"Draw",
	@"Dispatch",
	@"Blit",
	@"Signpost",
	@"PushGroup",
	@"PopGroup"
};

@implementation FMetalDebugCommandBuffer

@synthesize InnerBuffer;

-(id)initWithCommandBuffer:(id<MTLCommandBuffer>)Buffer
{
	id Self = [super init];
	if (Self)
	{
		InnerBuffer = Buffer;
		DebugGroup = [NSMutableArray new];
		ActiveEncoder = nil;
	}
	return Self;
}

-(void)dealloc
{
	for (FMetalDebugCommand* Command : DebugCommands)
	{
		[Command->Label release];
		[Command->PassDesc release];
		delete Command;
	}
	
	[InnerBuffer release];
	[DebugGroup release];
	
	[super dealloc];
}

-(id <MTLDevice>) device
{
	return InnerBuffer.device;
}

-(id <MTLCommandQueue>) commandQueue
{
	return InnerBuffer.commandQueue;
}

-(BOOL) retainedReferences
{
	return InnerBuffer.retainedReferences;
}

-(NSString *)label
{
	return InnerBuffer.label;
}

-(void)setLabel:(NSString *)Text
{
	InnerBuffer.label = Text;
}

-(MTLCommandBufferStatus) status
{
	return InnerBuffer.status;
}

-(NSError *)error
{
	return InnerBuffer.error;
}

- (void)enqueue
{
	[InnerBuffer enqueue];
}

- (void)commit
{
	[InnerBuffer commit];
}

- (void)addScheduledHandler:(MTLCommandBufferHandler)block
{
	[InnerBuffer addScheduledHandler:^(id <MTLCommandBuffer>){
		block(self);
	 }];
}

- (void)presentDrawable:(id <MTLDrawable>)drawable
{
	[InnerBuffer presentDrawable:drawable];
}

- (void)presentDrawable:(id <MTLDrawable>)drawable atTime:(CFTimeInterval)presentationTime
{
	[InnerBuffer presentDrawable:drawable atTime:presentationTime];
}

- (void)waitUntilScheduled
{
	[InnerBuffer waitUntilScheduled];
}

- (void)addCompletedHandler:(MTLCommandBufferHandler)block
{
	[InnerBuffer addCompletedHandler:^(id <MTLCommandBuffer>){
		block(self);
	}];
}

- (void)waitUntilCompleted
{
	[InnerBuffer waitUntilCompleted];
}

- (id <MTLBlitCommandEncoder>)blitCommandEncoder
{
	return [InnerBuffer blitCommandEncoder];
}

- (id <MTLRenderCommandEncoder>)renderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)renderPassDescriptor
{
	return [InnerBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
}

- (id <MTLComputeCommandEncoder>)computeCommandEncoder
{
	return [InnerBuffer computeCommandEncoder];
}

- (id <MTLParallelRenderCommandEncoder>)parallelRenderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)renderPassDescriptor
{
	return [InnerBuffer parallelRenderCommandEncoderWithDescriptor:renderPassDescriptor];
}

-(NSString*) description
{
	NSMutableString* String = [NSMutableString new];
	NSString* Label = self.label ? self.label : @"Unknown";
	[String appendFormat:@"Command Buffer %p %@:", self, Label];
	return String;
}

-(NSString*) debugDescription
{
	NSMutableString* String = [NSMutableString new];
	NSString* Label = self.label ? self.label : @"Unknown";
	[String appendFormat:@"Command Buffer %p %@:", self, Label];

	for (FMetalDebugCommand* Command : DebugCommands)
	{
		[String appendFormat:@"\n\t%@: %@", GMetalDebugCommandTypeNames[Command->Type], Command->Label];
	}
	return String;
}

-(void) beginRenderCommandEncoder:(NSString*)Label withDescriptor:(MTLRenderPassDescriptor*)Desc
{
	check(!ActiveEncoder);
	ActiveEncoder = [Label retain];
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeRenderEncoder;
	Command->Label = [ActiveEncoder retain];
	Command->PassDesc = [Desc retain];
	DebugCommands.Add(Command);
}

-(void) beginComputeCommandEncoder:(NSString*)Label
{
	check(!ActiveEncoder);
	ActiveEncoder = [Label retain];
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeComputeEncoder;
	Command->Label = [ActiveEncoder retain];
	Command->PassDesc = nil;
	DebugCommands.Add(Command);
}

-(void) beginBlitCommandEncoder:(NSString*)Label
{
	check(!ActiveEncoder);
	ActiveEncoder = [Label retain];
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeBlitEncoder;
	Command->Label = [ActiveEncoder retain];
	Command->PassDesc = nil;
	DebugCommands.Add(Command);
}

-(void) endCommandEncoder
{
	check(ActiveEncoder);
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeEndEncoder;
	Command->Label = ActiveEncoder;
	Command->PassDesc = nil;
	DebugCommands.Add(Command);
	ActiveEncoder = nil;
}

-(void) draw:(NSString*)Desc withPipeline:(FMetalBoundShaderState*)BSS
{
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeDraw;
	Command->Label = [Desc retain];
	Command->PassDesc = nil;
	Command->RenderPipeline = BSS;
	DebugCommands.Add(Command);
}

-(void) dispatch:(NSString*)Desc withShader:(FMetalComputeShader*)Shader
{
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeDispatch;
	Command->Label = [Desc retain];
	Command->PassDesc = nil;
	Command->ComputeShader = Shader;
	DebugCommands.Add(Command);
}

-(void) blit:(NSString*)Desc
{
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeBlit;
	Command->Label = [Desc retain];
	Command->PassDesc = nil;
	DebugCommands.Add(Command);
}

-(void) insertDebugSignpost:(NSString*)Label
{
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypeSignpost;
	Command->Label = [Label retain];
	Command->PassDesc = nil;
	DebugCommands.Add(Command);
}

-(void) pushDebugGroup:(NSString*)Group
{
	[DebugGroup addObject:Group];
	FMetalDebugCommand* Command = new FMetalDebugCommand;
	Command->Type = EMetalDebugCommandTypePushGroup;
	Command->Label = [Group retain];
	Command->PassDesc = nil;
	DebugCommands.Add(Command);
}

-(void) popDebugGroup
{
	if (DebugGroup.lastObject)
	{
		FMetalDebugCommand* Command = new FMetalDebugCommand;
		Command->Type = EMetalDebugCommandTypePopGroup;
		Command->Label = [DebugGroup.lastObject retain];
		Command->PassDesc = nil;
		DebugCommands.Add(Command);
	}
}

@end

