// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalResources.h"
#include "MetalShaderResources.h"

/**
 * EMetalDebugCommandType: Types of command recorded in our debug command-buffer wrapper.
 */
enum EMetalDebugCommandType
{
	EMetalDebugCommandTypeRenderEncoder,
	EMetalDebugCommandTypeComputeEncoder,
	EMetalDebugCommandTypeBlitEncoder,
	EMetalDebugCommandTypeEndEncoder,
	EMetalDebugCommandTypeDraw,
	EMetalDebugCommandTypeDispatch,
	EMetalDebugCommandTypeBlit,
	EMetalDebugCommandTypeSignpost,
	EMetalDebugCommandTypePushGroup,
	EMetalDebugCommandTypePopGroup,
	EMetalDebugCommandTypeInvalid
};

/**
 * EMetalDebugLevel: Level of Metal debug features to be enabled.
 */
enum EMetalDebugLevel
{
	EMetalDebugLevelOff,
	EMetalDebugLevelValidation,
	EMetalDebugLevelLogDebugGroups,
	EMetalDebugLevelLogOperations,
	EMetalDebugLevelConditionalSubmit,
	EMetalDebugLevelWaitForComplete
};

NS_ASSUME_NONNULL_BEGIN
/**
 * FMetalDebugCommand: The data recorded for each command in the debug command-buffer wrapper.
 */
struct FMetalDebugCommand
{
	NSString* Label;
	EMetalDebugCommandType Type;
	MTLRenderPassDescriptor* PassDesc;
	TRefCountPtr<FMetalBoundShaderState> RenderPipeline;
	TRefCountPtr<FMetalComputeShader> ComputeShader;
};

/**
 * FMetalDebugCommandBuffer: Wrapper around id<MTLCommandBuffer> that records information about commands.
 * This allows reporting of substantially more information in debug modes which can be especially helpful 
 * when debugging GPU command-buffer failures.
 */
@interface FMetalDebugCommandBuffer : NSObject<MTLCommandBuffer>
{
	TArray<FMetalDebugCommand*> DebugCommands;
	NSMutableArray<NSString*>* DebugGroup;
	NSString* ActiveEncoder;
	TSet<id<MTLResource>> Resources;
	TSet<id> States;
};

/** The wrapped native command-buffer for which we collect debug information. */
@property (readonly) id<MTLCommandBuffer> InnerBuffer;

/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithCommandBuffer:(id<MTLCommandBuffer>)Buffer;

/** Add the resource to be tracked in this command-buffer so we can validate lifetime on failure. */
-(void) trackResource:(id<MTLResource>)Resource;
/** Add the state to be tracked in this command-buffer so we can validate lifetime on failure. */
-(void) trackState:(id)State;

/** Record a bgein render encoder command. */
-(void) beginRenderCommandEncoder:(NSString*)Label withDescriptor:(MTLRenderPassDescriptor*)Desc;
/** Record a begin compute encoder command. */
-(void) beginComputeCommandEncoder:(NSString*)Label;
/** Record a begin blit encoder command. */
-(void) beginBlitCommandEncoder:(NSString*)Label;
/** Record an end encoder command. */
-(void) endCommandEncoder;

/** Record a draw command. */
-(void) draw:(NSString*)Desc withPipeline:(FMetalBoundShaderState*)BSS;
/** Record a dispatch command. */
-(void) dispatch:(NSString*)Desc withShader:(FMetalComputeShader*)Shader;
/** Record a blit command. */
-(void) blit:(NSString*)Desc;

/** Record an signpost command. */
-(void) insertDebugSignpost:(NSString*)Label;
/** Record a push debug group command. */
-(void) pushDebugGroup:(NSString*)Group;
/** Record a pop debug group command. */
-(void) popDebugGroup;

@end
NS_ASSUME_NONNULL_END

// Debug command-buffer logging macros that simplify the calling code
#if METAL_DEBUG_OPTIONS
#define METAL_DEBUG_COMMAND_BUFFER_DRAW_LOG(Context, LabelFormat, ...)	\
			if (Context->GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)	\
			{	\
				FMetalDebugCommandBuffer* CmdBuf = (FMetalDebugCommandBuffer*)Context->GetCurrentCommandBuffer();	\
				[CmdBuf draw:[NSString stringWithFormat: LabelFormat, __VA_ARGS__] withPipeline:Context->GetCurrentState().GetBoundShaderState()];	\
			}

#define METAL_DEBUG_COMMAND_BUFFER_DISPATCH_LOG(Context, LabelFormat, ...)	\
			if (Context->GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)	\
			{	\
				FMetalDebugCommandBuffer* CmdBuf = (FMetalDebugCommandBuffer*)Context->GetCurrentCommandBuffer();	\
				[CmdBuf dispatch:[NSString stringWithFormat: LabelFormat, __VA_ARGS__] withShader:Context->GetCurrentState().GetComputeShader()];	\
			}
#define METAL_DEBUG_COMMAND_BUFFER_BLIT_LOG(Context, LabelFormat, ...)	\
			if (Context->GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)	\
			{	\
				FMetalDebugCommandBuffer* CmdBuf = (FMetalDebugCommandBuffer*)Context->GetCurrentCommandBuffer();	\
				[CmdBuf blit:[NSString stringWithFormat: LabelFormat, __VA_ARGS__]];	\
			}
#define METAL_DEBUG_COMMAND_BUFFER_BLIT_ASYNC_LOG(Context, MtlCmdBuf, LabelFormat, ...)	\
			if (Context->GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)	\
			{	\
				FMetalDebugCommandBuffer* CmdBuf = (FMetalDebugCommandBuffer*)MtlCmdBuf;	\
				[CmdBuf blit:[NSString stringWithFormat: LabelFormat, __VA_ARGS__]];	\
			}
#define METAL_DEBUG_COMMAND_BUFFER_TRACK_RES(MtlCmdBuf, Resource)	\
			if (MtlCmdBuf && Resource && GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)	\
			{	\
				FMetalDebugCommandBuffer* CmdBuf = (FMetalDebugCommandBuffer*)MtlCmdBuf;	\
				[CmdBuf trackResource: Resource];	\
			}
#define METAL_DEBUG_COMMAND_BUFFER_TRACK_STATE(MtlCmdBuf, State)	\
			if (MtlCmdBuf && State && GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)	\
			{	\
				FMetalDebugCommandBuffer* CmdBuf = (FMetalDebugCommandBuffer*)MtlCmdBuf;	\
				[CmdBuf trackState: State];	\
			}
#else
#define METAL_DEBUG_COMMAND_BUFFER_DRAW_LOG(Context, LabelFormat, ...)
#define METAL_DEBUG_COMMAND_BUFFER_DISPATCH_LOG(Context, LabelFormat, ...)
#define METAL_DEBUG_COMMAND_BUFFER_BLIT_LOG(Context, LabelFormat, ...)
#define METAL_DEBUG_COMMAND_BUFFER_BLIT_ASYNC_LOG(Context, MtlCmdBuf, LabelFormat, ...)
#define METAL_DEBUG_COMMAND_BUFFER_TRACK_RES(MtlCmdBuf, Resource)
#define METAL_DEBUG_COMMAND_BUFFER_TRACK_STATE(MtlCmdBuf, State)
#endif
