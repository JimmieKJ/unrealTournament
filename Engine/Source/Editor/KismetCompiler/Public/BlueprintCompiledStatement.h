// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FBlueprintCompiledStatement

enum EKismetCompiledStatementType
{
	KCST_Nop = 0,
	KCST_CallFunction = 1,		// [wiring =] TargetObject->FunctionToCall(wiring)
	KCST_Assignment = 2,		// TargetObject->TargetProperty = [wiring]
	KCST_CompileError = 3,		// One of the other types with a compilation error during statement generation
	KCST_UnconditionalGoto = 4,	// goto TargetLabel
	KCST_PushState = 5,         // FlowStack.Push(TargetLabel)
	KCST_GotoIfNot = 6,			// [if (!TargetObject->TargetProperty)] goto TargetLabel
	KCST_Return = 7,			// return TargetObject->TargetProperty
	KCST_EndOfThread = 8,		// if (FlowStack.Num()) { NextState = FlowStack.Pop; } else { return; }
	KCST_Comment = 9,			// /* Comment */
	KCST_ComputedGoto = 10,		// NextState = LHS;
	KCST_EndOfThreadIfNot = 11, // [if (!TargetObject->TargetProperty)] { same as KCST_EndOfThread; }
	KCST_DebugSite = 12,		// NOP with recorded address
	KCST_CastObjToInterface = 13,	// TargetInterface(TargetObject)
	KCST_DynamicCast = 14,	    // Cast<TargetClass>(TargetObject)
	KCST_ObjectToBool = 15,		// (TargetObject != None)
	KCST_AddMulticastDelegate = 16,		// TargetDelegate->Add(EventDelegate)
	KCST_ClearMulticastDelegate = 17,	// TargetDelegate->Clear()
	KCST_WireTraceSite = 18,    // NOP with recorded address (never a step target)
	KCST_BindDelegate = 19,		// Creates simple delegate
	KCST_RemoveMulticastDelegate = 20,	//  TargetDelegate->Remove(EventDelegate)
	KCST_CallDelegate = 21,		// TargetDelegate->Broadcast(...)
	KCST_CreateArray = 22,		// Creates and sets an array literal term
	KCST_CrossInterfaceCast = 23,	// TargetInterface(Interface)
	KCST_MetaCast = 24,	    // Cast<TargetClass>(TargetObject)
	KCST_AssignmentOnPersistentFrame = 25, //
	KCST_CastInterfaceToObj = 26, // Cast<TargetClass>(TargetInterface)
	KCST_GotoReturn = 27,	// goto ReturnLabel
	KCST_GotoReturnIfNot = 28, // [if (!TargetObject->TargetProperty)] goto TargetLabel
	KCST_SwitchValue = 29,
};

//@TODO: Too rigid / icky design
struct FBlueprintCompiledStatement
{
	FBlueprintCompiledStatement()
		: Type(KCST_Nop)
		, FunctionContext(NULL)
		, FunctionToCall(NULL)
		, TargetLabel(NULL)
		, UbergraphCallIndex(-1)
		, LHS(NULL)
		, bIsJumpTarget(false)
		, bIsInterfaceContext(false)
		, bIsParentContext(false)
		, ExecContext(NULL)
	{
	}

	EKismetCompiledStatementType Type;

	// Object that the function should be called on, or NULL to indicate self (KCST_CallFunction)
	struct FBPTerminal* FunctionContext;

	// Function that executes the statement (KCST_CallFunction)
	UFunction* FunctionToCall;

	// Target label (KCST_Goto, or KCST_CallFunction that requires an ubergraph reference)
	FBlueprintCompiledStatement* TargetLabel;

	// The index of the argument to replace (only used when KCST_CallFunction has a non-NULL TargetLabel)
	int32 UbergraphCallIndex;

	// Destination of assignment statement or result from function call
	struct FBPTerminal* LHS;

	// Argument list of function call or source of assignment statement
	TArray<struct FBPTerminal*> RHS;

	// Is this node a jump target?
	bool bIsJumpTarget;

	// Is this node an interface context? (KCST_CallFunction)
	bool bIsInterfaceContext;

	// Is this function called on a parent class (super, etc)?  (KCST_CallFunction)
	bool bIsParentContext;

	// Exec pin about to execute (KCST_WireTraceSite)
	class UEdGraphPin* ExecContext;

	// Comment text
	FString Comment;
};
