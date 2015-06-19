// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Stack.h: Kismet VM execution stack definition.
=============================================================================*/

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogScriptFrame, Warning, All);

/**
 * Property data type enums.
 * @warning: if values in this enum are modified, you must update:
 * - FPropertyBase::GetSize() hardcodes the sizes for each property type
 */
enum EPropertyType
{
	CPT_None,
	CPT_Byte,
	CPT_UInt16,
	CPT_UInt32,
	CPT_UInt64,
	CPT_Int8,
	CPT_Int16,
	CPT_Int,
	CPT_Int64,
	CPT_Bool,
	CPT_Bool8,
	CPT_Bool16,
	CPT_Bool32,
	CPT_Bool64,
	CPT_Float,
	CPT_ObjectReference,
	CPT_Name,
	CPT_Delegate,
	CPT_Interface,
	CPT_Range,
	CPT_Struct,
	CPT_Vector,
	CPT_Rotation,
	CPT_String,
	CPT_Text,
	CPT_MulticastDelegate,
	CPT_WeakObjectReference,
	CPT_LazyObjectReference,
	CPT_AssetObjectReference,
	CPT_Double,
	CPT_Map,

	// when you add new property types, make sure you add the corresponding entry
	// in the PropertyTypeToNameMap array in ScriptCompiler.cpp!!
	CPT_MAX
};



/*-----------------------------------------------------------------------------
	Execution stack helpers.
-----------------------------------------------------------------------------*/

typedef TArray< CodeSkipSizeType, TInlineAllocator<8> > FlowStackType;

//
// Information remembered about an Out parameter.
//
struct FOutParmRec
{
	UProperty* Property;
	uint8*      PropAddr;
	FOutParmRec* NextOutParm;
};

//
// Information about script execution at one stack level.
//

struct FFrame : public FOutputDevice
{	
public:
	// Variables.
	UFunction* Node;
	UObject* Object;
	uint8* Code;
	uint8* Locals;

	UProperty* MostRecentProperty;
	uint8* MostRecentPropertyAddress;

	/** The execution flow stack for compiled Kismet code */
	FlowStackType FlowStack;

	/** Previous frame on the stack */
	FFrame* PreviousFrame;

	/** contains information on any out parameters */
	FOutParmRec* OutParms;

	/** If a class is compiled in then this is set to the property chain for compiled-in functions. In that case, we follow the links to setup the args instead of executing by code. */
	UField* PropertyChainForCompiledIn;

	/** Currently executed native function */
	UFunction* CurrentNativeFunction;
public:

	// Constructors.
	FFrame( UObject* InObject, UFunction* InNode, void* InLocals, FFrame* InPreviousFrame = NULL, UField* InPropertyChainForCompiledIn = NULL );

	virtual ~FFrame()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (GScriptStack.Num())
		{
			GScriptStack.Pop();
		}
#endif
	}

	// Functions.
	COREUOBJECT_API void Step( UObject* Context, RESULT_DECL );

	/** Replacement for Step that uses an explicitly specified property to unpack arguments **/
	COREUOBJECT_API void StepExplicitProperty(void*const Result, UProperty* Property);

	/** Replacement for Step that checks the for byte code, and if none exists, then PropertyChainForCompiledIn is used. Also, makes an effort to verify that the params are in the correct order and the types are compatible. **/
	template<class TProperty>
	FORCEINLINE_DEBUGGABLE void StepCompiledIn(void*const Result);

	/** Replacement for Step that checks the for byte code, and if none exists, then PropertyChainForCompiledIn is used. Also, makes an effort to verify that the params are in the correct order and the types are compatible. **/
	template<class TProperty, typename TNativeType>
	FORCEINLINE_DEBUGGABLE TNativeType& StepCompiledInRef(void*const TemporaryBuffer);

	COREUOBJECT_API virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
	
	COREUOBJECT_API static void KismetExecutionMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity);

	int32 ReadInt();
	float ReadFloat();
	FName ReadName();
	UObject* ReadObject();
	int32 ReadWord();
	UProperty* ReadProperty();

	/**
	 * Reads a value from the bytestream, which represents the number of bytes to advance
	 * the code pointer for certain expressions.
	 *
	 * @param	ExpressionField		receives a pointer to the field representing the expression; used by various execs
	 *								to drive VM logic
	 */
	CodeSkipSizeType ReadCodeSkipCount();

	/**
	 * Reads a value from the bytestream which represents the number of bytes that should be zero'd out if a NULL context
	 * is encountered
	 *
	 * @param	ExpressionField		receives a pointer to the field representing the expression; used by various execs
	 *								to drive VM logic
	 */
	VariableSizeType ReadVariableSize( UField** ExpressionField=NULL );

	/**
 	 * This will return the StackTrace of the current callstack from script land
	 **/
	COREUOBJECT_API FString GetStackTrace() const;
};


/*-----------------------------------------------------------------------------
	FFrame implementation.
-----------------------------------------------------------------------------*/

inline FFrame::FFrame( UObject* InObject, UFunction* InNode, void* InLocals, FFrame* InPreviousFrame, UField* InPropertyChainForCompiledIn )
	: Node(InNode)
	, Object(InObject)
	, Code(InNode->Script.GetData())
	, Locals((uint8*)InLocals)
	, MostRecentProperty(NULL)
	, MostRecentPropertyAddress(NULL)
	, PreviousFrame(InPreviousFrame)
	, OutParms(NULL)
	, PropertyChainForCompiledIn(InPropertyChainForCompiledIn)
	, CurrentNativeFunction(NULL)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FScriptTraceStackNode StackNode(InNode->GetOuter()->GetFName(), InNode->GetFName());
 	GScriptStack.Push(StackNode);
#endif
}

inline int32 FFrame::ReadInt()
{
	int32 Result;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(int32));
#else
	Result = *(int32*)Code;
#endif
	Code += sizeof(int32);
	return Result;
}

inline UObject* FFrame::ReadObject()
{
	// we always pull 64-bits of data out, which is really a UObject* in some representation (depending on platform)
	ScriptPointerType TempCode;

#ifdef REQUIRES_ALIGNED_INT_ACCESS
	FMemory::Memcpy(&TempCode, Code, sizeof(ScriptPointerType));
#else
	TempCode = *(ScriptPointerType*)Code;
#endif

	// turn that uint32 into a UObject pointer
	UObject* Result = (UObject*)(TempCode);
	Code += sizeof(ScriptPointerType);
	return Result;
}

inline UProperty* FFrame::ReadProperty()
{
	UProperty* Result = (UProperty*)ReadObject();
	MostRecentProperty = Result;

	// Callers don't check for NULL; this method is expected to succeed.
	check(Result);

	return Result;
}

inline float FFrame::ReadFloat()
{
	float Result;
#ifdef REQUIRES_ALIGNED_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(float));
#else
	Result = *(float*)Code;
#endif
	Code += sizeof(float);
	return Result;
}

inline int32 FFrame::ReadWord()
{
	int32 Result;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
	uint16 Temporary;
	FMemory::Memcpy(&Temporary, Code, sizeof(uint16));
	Result = Temporary;
#else
	Result = *(uint16*)Code;
#endif
	Code += sizeof(uint16);
	return Result;
}

/**
 * Reads a value from the bytestream, which represents the number of bytes to advance
 * the code pointer for certain expressions.
 */
inline CodeSkipSizeType FFrame::ReadCodeSkipCount()
{
	CodeSkipSizeType Result;

#ifdef REQUIRES_ALIGNED_INT_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(CodeSkipSizeType));
#else
	Result = *(CodeSkipSizeType*)Code;
#endif

	Code += sizeof(CodeSkipSizeType);
	return Result;
}

inline VariableSizeType FFrame::ReadVariableSize( UField** ExpressionField/*=NULL*/ )
{
	VariableSizeType Result=0;

	UObject* Field = ReadObject();
	uint8 NullPropertyType = *Code++;

	if ( Field != NULL )
	{
		// currently only Property (or null) seems to be a valid option.
		if (UProperty* Property = dynamic_cast<UProperty*>(Field))
		{
			Result = Property->GetSize();
		}
		else if (UEnum* ExplicitEnumValue = dynamic_cast<UEnum*>(Field))
		{
			Result = 1;
		}
		else if (UFunction* FunctionRef = dynamic_cast<UFunction*>(Field))
		{
			Result = sizeof(ScriptPointerType);
		}
	}
	else
	{
		switch ( NullPropertyType )
		{
		case CPT_None:
			// nothing...
			break;
		case CPT_Byte: Result = sizeof(uint8);
			break;
		case CPT_UInt16: Result = sizeof(uint16);
			break;
		case CPT_UInt32: Result = sizeof(uint32);
			break;
		case CPT_UInt64: Result = sizeof(uint64);
			break;
		case CPT_Int8: Result = sizeof(int8);
			break;
		case CPT_Int16: Result = sizeof(int16);
			break;
		case CPT_Int: Result = sizeof(int32);
			break;
		case CPT_Int64: Result = sizeof(int64);
			break;
		case CPT_Bool: Result = sizeof(bool);
			break;
		case CPT_Bool8: Result = sizeof(uint8);
			break;
		case CPT_Bool16: Result = sizeof(uint16);
			break;
		case CPT_Bool32: Result = sizeof(uint32);
			break;
		case CPT_Bool64: Result = sizeof(uint64);
			break;
		case CPT_Float: Result = sizeof(float);
			break;
		case CPT_Double: Result = sizeof(double);
			break;
		case CPT_Name: Result = sizeof(FScriptName);
			break;
		case CPT_Vector: Result = sizeof(FVector);
			break;
		case CPT_Rotation: Result = sizeof(FRotator);
			break;
		case CPT_Delegate: Result = sizeof(FScriptDelegate);
			break;
		case CPT_MulticastDelegate: Result = sizeof(FMulticastScriptDelegate);
			break;
		case CPT_WeakObjectReference: Result = sizeof(FWeakObjectPtr);
			break;
		case CPT_LazyObjectReference: Result = sizeof(FLazyObjectPtr);
			break;
		case CPT_AssetObjectReference: Result = sizeof(FAssetPtr);
			break;
		case CPT_Text: Result = sizeof(FText);
			break;
		case CPT_Map: Result = sizeof(FScriptMap);
			break;
		default:
			UE_LOG(LogScriptFrame, Fatal, TEXT("Unhandled property type in FFrame::ReadVariableSize(): %u"), NullPropertyType);
			break;
		}
	}

	if ( ExpressionField != NULL )
	{
		*ExpressionField = dynamic_cast<UField*>(Field);
	}

	return Result;
}

inline FName FFrame::ReadName()
{
	FScriptName Result;
#ifdef REQUIRES_ALIGNED_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(FScriptName));
#else
	Result = *(FScriptName*)Code;
#endif
	Code += sizeof(FScriptName);
	return ScriptNameToName(Result);
}

COREUOBJECT_API void GInitRunaway();

/**
 * Replacement for Step that checks the for byte code, and if none exists, then PropertyChainForCompiledIn is used.
 * Also makes an effort to verify that the params are in the correct order and the types are compatible.
 **/
template<class TProperty>
FORCEINLINE_DEBUGGABLE void FFrame::StepCompiledIn(void*const Result)
{
	if (Code)
	{
		Step(Object, Result);
	}
	else
	{
		checkSlow(dynamic_cast<TProperty*>(PropertyChainForCompiledIn) && dynamic_cast<UProperty*>(PropertyChainForCompiledIn));
		TProperty* Property = (TProperty*)PropertyChainForCompiledIn;
		PropertyChainForCompiledIn = Property->Next;
		StepExplicitProperty(Result, Property);
	}
}

template<class TProperty, typename TNativeType>
FORCEINLINE_DEBUGGABLE TNativeType& FFrame::StepCompiledInRef(void*const TemporaryBuffer)
{
	MostRecentPropertyAddress = NULL;

	if (Code)
	{
		Step(Object, TemporaryBuffer);
	}
	else
	{
		checkSlow(dynamic_cast<TProperty*>(PropertyChainForCompiledIn) && dynamic_cast<UProperty*>(PropertyChainForCompiledIn));
		TProperty* Property = (TProperty*)PropertyChainForCompiledIn;
		PropertyChainForCompiledIn = Property->Next;
		StepExplicitProperty(TemporaryBuffer, Property);
	}

	return (MostRecentPropertyAddress != NULL) ? *(TNativeType*)(MostRecentPropertyAddress) : *(TNativeType*)TemporaryBuffer;
}
