// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	KismetCompilerVMBackend.cpp
=============================================================================*/

#include "KismetCompilerPrivatePCH.h"

#include "KismetCompilerBackend.h"

#include "DefaultValueHelper.h"

#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h"
#include "Engine/UserDefinedStruct.h"

//////////////////////////////////////////////////////////////////////////
// FScriptBytecodeWriter

//
// Little class for writing to scripts.
//
class FScriptBytecodeWriter : public FArchiveUObject
{
public:
	TArray<uint8>& ScriptBuffer;
public:
	FScriptBytecodeWriter( TArray<uint8>& InScriptBuffer )
		: ScriptBuffer( InScriptBuffer )
	{
	}
	
	void Serialize( void* V, int64 Length ) override
	{
		int32 iStart = ScriptBuffer.AddUninitialized( Length );
		FMemory::Memcpy( &(ScriptBuffer[iStart]), V, Length );
	}

	FArchive& operator<<(FName& Name) override
	{
		FArchive& Ar = *this;

		// We can't call Serialize directly as we need to store the data endian clean.
		FScriptName ScriptName = NameToScriptName(Name);
		Ar << ScriptName.ComparisonIndex;
		Ar << ScriptName.DisplayIndex;
		Ar << ScriptName.Number;

		return Ar;
	}

	FArchive& operator<<(UObject*& Res) override
	{
		ScriptPointerType D = (ScriptPointerType)Res; 
		FArchive& Ar = *this;

		Ar << D;
		return Ar;
	}

	FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override
	{
		return FArchive::operator<<(LazyObjectPtr);
	}

	FArchive& operator<<(FAssetPtr& AssetPtr) override
	{
		return FArchive::operator<<(AssetPtr);
	}

	FArchive& operator<<(FStringAssetReference& Value) override
	{
		return FArchiveUObject::operator<<(Value);
	}

	FArchive& operator<<(TCHAR* S)
	{
		Serialize(S, FCString::Strlen(S) + 1); 
		return *this;
	}

	FArchive& operator<<(EExprToken E)
	{
		checkSlow(E < 0xFF);

		uint8 B = E; 
		Serialize(&B, 1); 
		return *this;
	}

	FArchive& operator<<(ECastToken E)
	{
		uint8 B = E; 
		Serialize(&B, 1); 
		return *this;
	}

	FArchive& operator<<(EPropertyType E)
	{
		uint8 B = E; 
		Serialize(&B, 1); 
		return *this;
	}

	CodeSkipSizeType EmitPlaceholderSkip()
	{
		CodeSkipSizeType Result = ScriptBuffer.Num();

		CodeSkipSizeType Placeholder = -1;
		(*this) << Placeholder;

		return Result;
	}

	void CommitSkip(CodeSkipSizeType WriteOffset, CodeSkipSizeType NewValue)
	{
		//@TODO: Any endian issues?
#if SCRIPT_LIMIT_BYTECODE_TO_64KB
		static_assert(sizeof(CodeSkipSizeType) == 2, "Update this code as size changed.");
		ScriptBuffer[WriteOffset] = NewValue & 0xFF;
		ScriptBuffer[WriteOffset+1] = (NewValue >> 8) & 0xFF;
#else
		static_assert(sizeof(CodeSkipSizeType) == 4, "Update this code as size changed.");
		ScriptBuffer[WriteOffset] = NewValue & 0xFF;
		ScriptBuffer[WriteOffset+1] = (NewValue >> 8) & 0xFF;
		ScriptBuffer[WriteOffset+2] = (NewValue >> 16) & 0xFF;
		ScriptBuffer[WriteOffset+3] = (NewValue >> 24) & 0xFF;
#endif
	}
};

//////////////////////////////////////////////////////////////////////////
// FSkipOffsetEmitter

struct FSkipOffsetEmitter
{
	CodeSkipSizeType SkipWriteIndex;
	CodeSkipSizeType StartIndex;
	TArray<uint8>& Script;

	FSkipOffsetEmitter(TArray<uint8>& InScript)
		: SkipWriteIndex(-1)
		, StartIndex(-1)
		, Script(InScript)
	{
	}

	void Emit()
	{
		SkipWriteIndex = (CodeSkipSizeType)Script.Num();
		StartIndex = SkipWriteIndex;

		// Reserve space
		for (int32 i = 0; i < sizeof(CodeSkipSizeType); ++i)
		{
			Script.Add(0);
		}
	}

	void BeginCounting()
	{
		StartIndex = Script.Num();
	}

	void Commit()
	{
		check(SkipWriteIndex != -1);
		CodeSkipSizeType BytesToSkip = Script.Num() - StartIndex;

		//@TODO: Any endian issues?
#if SCRIPT_LIMIT_BYTECODE_TO_64KB
		static_assert(sizeof(CodeSkipSizeType) == 2, "Update this code as size changed.");
		Script[SkipWriteIndex] = BytesToSkip & 0xFF;
		Script[SkipWriteIndex+1] = (BytesToSkip >> 8) & 0xFF;
#else
		static_assert(sizeof(CodeSkipSizeType) == 4, "Update this code as size changed.");
		Script[SkipWriteIndex] = BytesToSkip & 0xFF;
		Script[SkipWriteIndex+1] = (BytesToSkip >> 8) & 0xFF;
		Script[SkipWriteIndex+2] = (BytesToSkip >> 16) & 0xFF;
		Script[SkipWriteIndex+3] = (BytesToSkip >> 24) & 0xFF;
#endif
	}
};

//////////////////////////////////////////////////////////////////////////
// FScriptBuilderBase

class FScriptBuilderBase
{
private:
	FScriptBytecodeWriter Writer;
	UBlueprintGeneratedClass* ClassBeingBuilt;
	UEdGraphSchema_K2* Schema;
	friend class FContextEmitter;

	// Pointers to commonly used structures (found in constructor)
	UScriptStruct* VectorStruct;
	UScriptStruct* RotatorStruct;
	UScriptStruct* TransformStruct;
	UScriptStruct* LatentInfoStruct;
	UScriptStruct* ProfileStruct;

	FKismetCompilerVMBackend::TStatementToSkipSizeMap StatementLabelMap;
	FKismetCompilerVMBackend::TStatementToSkipSizeMap& UbergraphStatementLabelMap;

	// Fixup list for jump targets (location to overwrite; jump target)
	TMap<CodeSkipSizeType, FBlueprintCompiledStatement*> JumpTargetFixupMap;
	
	// Is this compiling the ubergraph?
	bool bIsUbergraph;

	FBlueprintCompiledStatement& ReturnStatement;
protected:
	/**
	 * This class is designed to be used like so to emit a bytecode context expression:
	 * 
	 *   {
	 *       FContextEmitter ContextHandler;
	 *       if (Needs Context)
	 *       {
	 *           ContextHandler.StartContext(context);
	 *       }
	 *       Do stuff predicated on context
	 *       // Emitter closes when it falls out of scope
	 *   }
	 */
	struct FContextEmitter
	{
	private:
		FScriptBuilderBase& ScriptBuilder;
		FScriptBytecodeWriter& Writer;
		TArray<FSkipOffsetEmitter> SkipperStack;
		bool bInContext;
	public:
		FContextEmitter(FScriptBuilderBase& InScriptBuilder)
			: ScriptBuilder(InScriptBuilder)
			, Writer(ScriptBuilder.Writer)
			, bInContext(false)
		{
		}

		/** Starts a context if the Term isn't NULL */
		void TryStartContext(FBPTerminal* Term, bool bUnsafeToSkip = false, bool bIsInterfaceContext = false, FBPTerminal* RValueTerm = NULL)
		{
			if (Term != NULL)
			{
				StartContext(Term, bUnsafeToSkip, bIsInterfaceContext, RValueTerm);
			}
		}

		void StartContext(FBPTerminal* Term, bool bUnsafeToSkip = false, bool bIsInterfaceContext = false, FBPTerminal* RValueTerm = NULL)
		{
			bInContext = true;

			if (bUnsafeToSkip)
			{
				Writer << EX_Context;
			}
			else
			{
				Writer << EX_Context_FailSilent;
			}

			if (bIsInterfaceContext)
			{
				Writer << EX_InterfaceContext;
			}

			// Function contexts must always be objects, so if we have a literal, give it the default object property so the compiler knows how to handle it
			UProperty* CoerceProperty = Term->bIsLiteral ? ((UProperty*)(GetDefault<UObjectProperty>())) : NULL;
			ScriptBuilder.EmitTerm(Term, CoerceProperty);

			// Skip offset if the expression evaluates to null (counting from later on)
			FSkipOffsetEmitter Skipper(ScriptBuilder.Writer.ScriptBuffer);
			Skipper.Emit();

			// R-Value property, see ReadVariableSize in UObject::ProcessContextOpcode() for usage
			//@TODO: Not sure what to use for yet
			UProperty* RValueProperty = RValueTerm ? RValueTerm->AssociatedVarProperty : NULL;
			Writer << RValueProperty;

			// Property type if needed, it seems it's not used in ue4
			//@TODO: Not sure what to use for yet
			uint8 PropetyType = 0;
			Writer << PropetyType;

			// Context expression (this is the part that gets skipped if the object turns out NULL)
			Skipper.BeginCounting();

			SkipperStack.Push( Skipper );
		}

		void CloseContext()
		{
			// Point to skip to (end of sequence)
			for (int32 i = 0; i < SkipperStack.Num(); ++i)
			{
				SkipperStack[i].Commit();
			}

			bInContext = false;
		}

		~FContextEmitter()
		{
			if (bInContext)
			{
				CloseContext();
			}
		}
	};
public:
	FScriptBuilderBase(TArray<uint8>& InScript, UBlueprintGeneratedClass* InClass, UEdGraphSchema_K2* InSchema, FKismetCompilerVMBackend::TStatementToSkipSizeMap& InUbergraphStatementLabelMap, bool bInIsUbergraph, FBlueprintCompiledStatement& InReturnStatement)
		: Writer(InScript)
		, ClassBeingBuilt(InClass)
		, Schema(InSchema)
		, UbergraphStatementLabelMap(InUbergraphStatementLabelMap)
		, bIsUbergraph(bInIsUbergraph)
		, ReturnStatement(InReturnStatement)
	{
		VectorStruct = GetBaseStructure(TEXT("Vector"));
		RotatorStruct = GetBaseStructure(TEXT("Rotator"));
		TransformStruct = GetBaseStructure(TEXT("Transform"));
		LatentInfoStruct = FLatentActionInfo::StaticStruct();
	}

	void CopyStatementMapToUbergraphMap()
	{
		UbergraphStatementLabelMap = StatementLabelMap;
	}

	void EmitStringLiteral(const FString& String)
	{
		if (FCString::IsPureAnsi(*String))
		{
			Writer << EX_StringConst;
			uint8 OutCh;
			for (const TCHAR* Ch = *String; *Ch; ++Ch)
			{
				OutCh = CharCast<ANSICHAR>(*Ch);
				Writer << OutCh;
			}

			OutCh = 0;
			Writer << OutCh;
		}
		else
		{
			Writer << EX_UnicodeStringConst;
			uint16 OutCh;
			for (const TCHAR* Ch = *String; *Ch; ++Ch)
			{
				OutCh = CharCast<UCS2CHAR>(*Ch);
				Writer << OutCh;
			}

			OutCh = 0;
			Writer << OutCh;
		}
	}

	virtual void EmitTermExpr(FBPTerminal* Term, UProperty* CoerceProperty = NULL, bool bAllowStaticArray = false)
	{
		//@TODO: Must have a coercion type if it's a literal, because the symbol table isn't plumbed in here and the literals don't carry type information either, yay!
		check((!Term->bIsLiteral) || (CoerceProperty != NULL));

		if (Term->bIsLiteral)
		{
			if (CoerceProperty->IsA(UStrProperty::StaticClass()))
			{
				EmitStringLiteral(Term->Name);
			}
			else if (CoerceProperty->IsA(UTextProperty::StaticClass()))
			{
				Writer << EX_TextConst;
				
				EmitStringLiteral(FTextInspector::GetSourceString(Term->TextLiteral)? *FTextInspector::GetSourceString(Term->TextLiteral) : TEXT(""));
				EmitStringLiteral(FTextInspector::GetKey(Term->TextLiteral).Get(TEXT("")));
				EmitStringLiteral(FTextInspector::GetNamespace(Term->TextLiteral).Get(TEXT("")));
			}
			else if (CoerceProperty->IsA(UFloatProperty::StaticClass()))
			{
				float Value = FCString::Atof(*(Term->Name));
				Writer << EX_FloatConst;
				Writer << Value;
			}
			else if (CoerceProperty->IsA(UIntProperty::StaticClass()))
			{
				// In certain cases (like UKismetArrayLibrary functions), we have
				// polymorphic functions that provide their own "custom thunk" 
				// (custom execution code). The actual function acts as a 
				// template, where the parameter types can be changed out for 
				// other types (much like c++ template functions, the "custom 
				// thunk" is generic). Traditionally, we use integer refs as the 
				// place holder type (that's why this is nested in a UIntProperty 
				// check)... Complications arise here, when we try to emit 
				// literal values fed into the function when they don't match 
				// the template's (int) type. For most types, this here is 
				// circumvented with AutoCreateRefTerm, but when it is a self 
				// (literal) node we still end up here. So, we try to detect and 
				// handle that case here.
				if ((Term->Type.PinSubCategory == Schema->PN_Self) && CoerceProperty->HasAnyPropertyFlags(CPF_ReferenceParm))
				{
					Writer << EX_Self;
				}
				else
				{
					//@TODO: There are smaller encodings EX_IntZero, EX_IntOne, EX_IntConstByte available which could be used instead when the value fits
					int32 Value = FCString::Atoi(*(Term->Name));
					Writer << EX_IntConst;
					Writer << Value;
				}
			}
			else if (CoerceProperty->IsA(UByteProperty::StaticClass()))
			{
				UByteProperty* ByteProp = CastChecked< UByteProperty >( CoerceProperty );
				uint8 Value = 0;

				//Check for valid enum object reference
				if (ByteProp->Enum)
				{
					//Get index from enum string
					Value = ByteProp->Enum->FindEnumIndex( *(Term->Name) );
				}
				// Allow enum literals to communicate with byte properties as literals
				else if (UEnum* EnumPtr = Cast<UEnum>(Term->Type.PinSubCategoryObject.Get()))
				{
					Value = EnumPtr->FindEnumIndex( *(Term->Name) );
				}
				else
				{
					Value = FCString::Atoi(*(Term->Name));
				}

				Writer << EX_ByteConst;
				Writer << Value;
			}
			else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(CoerceProperty))
			{
				bool bValue = Term->Name.ToBool();
				Writer << (bValue ? EX_True : EX_False);
			}
			else if (UNameProperty* NameProperty = Cast<UNameProperty>(CoerceProperty))
			{
				FName LiteralName(*(Term->Name));
				Writer << EX_NameConst;
				Writer << LiteralName;
			}
			//else if (UClassProperty* ClassProperty = Cast<UClassProperty>(CoerceProperty))
			//{
			//	ensureMsg(false, TEXT("Class property literals are not supported yet!"));
			//}
			else if (UStructProperty* StructProperty = Cast<UStructProperty>(CoerceProperty))
			{
				if (StructProperty->Struct == VectorStruct)
				{
					FVector V = FVector::ZeroVector;
					FDefaultValueHelper::ParseVector(Term->Name, /*out*/ V);
					Writer << EX_VectorConst;
					Writer << V;

				}
				else if (StructProperty->Struct == RotatorStruct)
				{
					FRotator R = FRotator::ZeroRotator;
					FDefaultValueHelper::ParseRotator(Term->Name, /*out*/ R);
					Writer << EX_RotationConst;
					Writer << R;
				}
				else if (StructProperty->Struct == TransformStruct)
				{
					FTransform T = FTransform::Identity;
					T.InitFromString( Term->Name );
					Writer << EX_TransformConst;
					Writer << T;
				}
				else
				{
					UScriptStruct* Struct = StructProperty->Struct;
					int32 StructSize = Struct->GetStructureSize() * StructProperty->ArrayDim;
					uint8* StructData = (uint8*)FMemory_Alloca(StructSize);
					StructProperty->InitializeValue(StructData);
					if (!ensure(bAllowStaticArray || 1 == StructProperty->ArrayDim))
					{
						UE_LOG(LogK2Compiler, Error, TEXT("Unsupported static array. Property: %s, Struct: %s"), *StructProperty->GetName(), *Struct->GetName());
					}
					if(!FStructureEditorUtils::Fill_MakeStructureDefaultValue(Cast<UUserDefinedStruct>(Struct), StructData))
					{
						UE_LOG(LogK2Compiler, Warning, TEXT("MakeStructureDefaultValue parsing error. Property: %s, Struct: %s"), *StructProperty->GetName(), *Struct->GetName());
					}

					// Assume that any errors on the import of the name string have been caught in the function call generation
					StructProperty->ImportText(Term->Name.IsEmpty() ? TEXT("()") : *Term->Name, StructData, 0, NULL, GLog);

 					Writer << EX_StructConst;
					Writer << Struct;
					Writer << StructSize;

					for( UProperty* Prop = Struct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext )
					{
						for (int32 ArrayIter = 0; ArrayIter < Prop->ArrayDim; ++ArrayIter)
						{
							// Create a new term for each property, and serialize it out
							FBPTerminal NewTerm;
							NewTerm.bIsLiteral = true;
							Prop->ExportText_InContainer(ArrayIter, NewTerm.Name, StructData, StructData, NULL, PPF_None);
							if (Prop->IsA(UTextProperty::StaticClass()))
							{
								NewTerm.TextLiteral = FText::FromString(NewTerm.Name);
							}
							else if (Prop->IsA(UObjectProperty::StaticClass()))
							{
								NewTerm.ObjectLiteral = Cast<UObjectProperty>(Prop)->GetObjectPropertyValue(Prop->ContainerPtrToValuePtr<void>(StructData));
							}

							EmitTermExpr(&NewTerm, Prop, true);
						}
					}

					Writer << EX_EndStructConst;
				}
			}
			else if (auto ArrayPropr = Cast<UArrayProperty>(CoerceProperty))
			{
				UProperty* InnerProp = ArrayPropr->Inner;
				ensure(InnerProp);
				FScriptArray ScriptArray;
				ArrayPropr->ImportText(*Term->Name, &ScriptArray, 0, NULL, GLog);
				int32 ElementNum = ScriptArray.Num();

				FScriptArrayHelper ScriptArrayHelper(ArrayPropr, &ScriptArray);

				Writer << EX_ArrayConst;
				Writer << InnerProp;
				Writer << ElementNum;
				for (int32 ElemIdx = 0; ElemIdx < ElementNum; ++ElemIdx)
				{
					FBPTerminal NewTerm;
					NewTerm.bIsLiteral = true;
					uint8* RawElemData = ScriptArrayHelper.GetRawPtr(ElemIdx);
					InnerProp->ExportText_Direct(NewTerm.Name, RawElemData, RawElemData, NULL, PPF_None);
					if (InnerProp->IsA(UTextProperty::StaticClass()))
					{
						NewTerm.TextLiteral = FText::FromString(NewTerm.Name);
					}
					else if (InnerProp->IsA(UObjectProperty::StaticClass()))
					{
						NewTerm.ObjectLiteral = Cast<UObjectProperty>(InnerProp)->GetObjectPropertyValue(RawElemData);
					}
					EmitTermExpr(&NewTerm, InnerProp);
				}
				Writer << EX_EndArrayConst;
			}
			else if (CoerceProperty->IsA(UDelegateProperty::StaticClass()))
			{
				if (Term->Name == TEXT(""))
				{
					ensureMsg(false, TEXT("Cannot use an empty literal expression for a delegate property"));
				}
				else
				{
					FName FunctionName(*(Term->Name)); //@TODO: K2 Delegate Support: Need to verify this function actually exists and has the right signature?

					Writer << EX_InstanceDelegate;
					Writer << FunctionName;
				}
			}
			else if (CoerceProperty->IsA(UObjectPropertyBase::StaticClass()))
			{
				// Note: This case handles both UObjectProperty and UClassProperty
				if (Term->Type.PinSubCategory == Schema->PN_Self)
				{
					Writer << EX_Self;
				}
				else if (Term->ObjectLiteral == NULL)
				{
					Writer << EX_NoObject;
				}
				else
				{
					Writer << EX_ObjectConst;
					Writer << Term->ObjectLiteral;
				}
			}
			else if (CoerceProperty->IsA(UInterfaceProperty::StaticClass()))
			{
				if (Term->Type.PinSubCategory == Schema->PN_Self)
				{
					Writer << EX_Self;
				}
				else if (Term->ObjectLiteral == nullptr)
				{
					Writer << EX_NoInterface;
				}
				else
				{
					ensureMsgf(false, TEXT("It is not possible to express this interface property as a literal value! (%s)"), *CoerceProperty->GetFullName());
				}
			}
			// else if (CoerceProperty->IsA(UMulticastDelegateProperty::StaticClass()))
			// Cannot assign a literal to a multicast delegate; it should be added instead of assigned
			else
			{
				ensureMsgf(false, TEXT("It is not possible to express this type as a literal value! (%s)"), *CoerceProperty->GetFullName());
			}
		}
		else
		{
			check(Term->AssociatedVarProperty);
			if (Term->bIsLocal)
			{
				Writer << (Term->AssociatedVarProperty->HasAnyPropertyFlags(CPF_OutParm) ? EX_LocalOutVariable : EX_LocalVariable);
			}
			else
			{
				Writer << EX_InstanceVariable;
			}
			Writer << Term->AssociatedVarProperty;
		}
	}

	void EmitLatentInfoTerm(FBPTerminal* Term, UProperty* LatentInfoProperty, FBlueprintCompiledStatement* TargetLabel)
	{
		// Special case of the struct property emitter.  Needs to emit a linkage property for fixup
		UStructProperty* StructProperty = CastChecked<UStructProperty>(LatentInfoProperty);
		check(StructProperty->Struct == LatentInfoStruct);

		int32 StructSize = LatentInfoStruct->GetStructureSize();
		uint8* StructData = (uint8*)FMemory_Alloca(StructSize);
		StructProperty->InitializeValue(StructData);

		// Assume that any errors on the import of the name string have been caught in the function call generation
		StructProperty->ImportText(*Term->Name, StructData, 0, NULL, GLog);

		Writer << EX_StructConst;
		Writer << LatentInfoStruct;
		Writer << StructSize;

		for (UProperty* Prop = LatentInfoStruct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
		{
			if (TargetLabel && Prop->GetBoolMetaData(FBlueprintMetadata::MD_NeedsLatentFixup))
			{
				// Emit the literal and queue a fixup to correct it once the address is known
				Writer << EX_SkipOffsetConst;
				CodeSkipSizeType PatchUpNeededAtOffset = Writer.EmitPlaceholderSkip();
				JumpTargetFixupMap.Add(PatchUpNeededAtOffset, TargetLabel);
			}
			else if (Prop->GetBoolMetaData(FBlueprintMetadata::MD_LatentCallbackTarget))
			{
				FBPTerminal CallbackTargetTerm;
				CallbackTargetTerm.bIsLiteral = true;
				CallbackTargetTerm.Type.PinSubCategory = Schema->PN_Self;
				EmitTermExpr(&CallbackTargetTerm, Prop);
			}
			else
			{
				// Create a new term for each property, and serialize it out
				FBPTerminal NewTerm;
				NewTerm.bIsLiteral = true;
				Prop->ExportText_InContainer(0, NewTerm.Name, StructData, StructData, NULL, PPF_None);

				EmitTermExpr(&NewTerm, Prop);
			}
		}

		Writer << EX_EndStructConst;
	}

	void EmitFunctionCall(FKismetCompilerContext& CompilerContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
	{
		UFunction* FunctionToCall = Statement.FunctionToCall;
		check(FunctionToCall);

		if (FunctionToCall->HasAllFunctionFlags(FUNC_Native))
		{
			// Array output parameters are cleared, in case the native function doesn't clear them before filling.
			int32 NumParams = 0;
			for (TFieldIterator<UProperty> PropIt(FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				UProperty* Param = *PropIt;
				check(Param);
				const bool bShouldParameterBeCleared = Param->IsA<UArrayProperty>()
					&& Param->HasAllPropertyFlags(CPF_Parm | CPF_OutParm)
					&& !Param->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_ConstParm | CPF_ReturnParm);
				if (bShouldParameterBeCleared)
				{
					// SetArray instruction will be called with empty parameter list.
					Writer << EX_SetArray;
					FBPTerminal* ArrayTerm = Statement.RHS[NumParams];
					ensure(ArrayTerm && !ArrayTerm->bIsLiteral && !Statement.ArrayCoersionTermMap.Find(ArrayTerm));
					EmitTerm(ArrayTerm, Param);
					Writer << EX_EndArray;
				}
				NumParams += Param->HasAnyPropertyFlags(CPF_ReturnParm) ? 0 : 1;
			}
		}
		
		// The target label will only ever be set on a call function when calling into the Ubergraph, which requires a patchup
		// or when re-entering from a latent function which requires a different kind of patchup
		if ((Statement.TargetLabel != NULL) && !bIsUbergraph)
		{
			CodeSkipSizeType OffsetWithinUbergraph = UbergraphStatementLabelMap.FindChecked(Statement.TargetLabel);

			// Overwrite RHS(0) text with the state index to kick off
			check(Statement.RHS[Statement.UbergraphCallIndex]->bIsLiteral);
			Statement.RHS[Statement.UbergraphCallIndex]->Name = FString::FromInt(OffsetWithinUbergraph);

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
			// Store optimization data if this is a simple call into the ubergraph
			if (FunctionContext.bIsSimpleStubGraphWithNoParams && CompilerContext.NewClass->UberGraphFunction)
			{
				check(FunctionToCall == CompilerContext.NewClass->UberGraphFunction);
				check(FunctionToCall->ParmsSize == sizeof(int32));

				if ((FunctionToCall->FirstPropertyToInit == nullptr) && (FunctionToCall->PostConstructLink == nullptr))
				{
					FunctionContext.Function->EventGraphFunction = FunctionToCall;
					FunctionContext.Function->EventGraphCallOffset = OffsetWithinUbergraph;
				}
			}
#endif
		}

		// Handle the return value assignment if present
		bool bHasOutputValue = false;
		for (TFieldIterator<UProperty> PropIt(FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* FuncParamProperty = *PropIt;
			if (FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				EmitDestinationExpression(Statement.LHS);
				bHasOutputValue = true;
			}
			else if (FuncParamProperty->HasAnyPropertyFlags(CPF_OutParm) && !FuncParamProperty->HasAnyPropertyFlags(CPF_ConstParm))
			{
				// Non const values passed by ref are also an output
				bHasOutputValue = true;
			}
		}

		// Handle the function calling context if needed
		FContextEmitter CallContextWriter(*this);
		CallContextWriter.TryStartContext(Statement.FunctionContext, /*bUnsafeToSkip=*/ bHasOutputValue, Statement.bIsInterfaceContext);

		// Emit the call type
		if (FunctionToCall->HasAnyFunctionFlags(FUNC_Delegate))
		{
			// @todo: Default delegate functions are no longer callable (and also now have mangled names.)  FindField will fail.
			check(false);
		}
		else if (FunctionToCall->HasAnyFunctionFlags(FUNC_Final) || Statement.bIsParentContext)
		{
			// The function to call doesn't have a native index
			Writer << EX_FinalFunction;
			Writer << FunctionToCall;
		}
		else
		{
			FName FunctionName(FunctionToCall->GetFName());
			Writer << EX_VirtualFunction;
			Writer << FunctionName;
		}

		// Emit function parameters
		int32 NumParams = 0;
		for (TFieldIterator<UProperty> PropIt(FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* FuncParamProperty = *PropIt;

			if (!FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				FBPTerminal* Term = Statement.RHS[NumParams];
				check(Term != NULL);

				// See if this is a hidden array param term, which needs to be fixed up with the final generated UArrayProperty
				if( FBPTerminal** ArrayParmTerm = Statement.ArrayCoersionTermMap.Find(Term) )
				{
					Term->ObjectLiteral = (*ArrayParmTerm)->AssociatedVarProperty;
				}

				// Latent function handling:  Need to emit a fixup request into the FLatentInfo struct
				static const FName NAME_LatentInfo = TEXT("LatentInfo");
 				if (bIsUbergraph && FuncParamProperty->GetName() == FunctionToCall->GetMetaData("LatentInfo"))
 				{
					EmitLatentInfoTerm(Term, FuncParamProperty, Statement.TargetLabel);
 				}
				else
				{
					// Emit parameter term normally
					EmitTerm(Term, FuncParamProperty);
				}


				NumParams++;
			}
		}

		// End of parameter list
		Writer << EX_EndFunctionParms;
	}

	void EmitCallDelegate(FBlueprintCompiledStatement& Statement)
	{
		UFunction* FunctionToCall = Statement.FunctionToCall;
		check(NULL != FunctionToCall);
		check(NULL != Statement.FunctionContext);
		check(FunctionToCall->HasAnyFunctionFlags(FUNC_Delegate));

		// The function to call doesn't have a native index
		Writer << EX_CallMulticastDelegate;
		Writer << FunctionToCall;
		EmitTerm(Statement.FunctionContext);

		// Emit function parameters
		int32 NumParams = 0;
		for (TFieldIterator<UProperty> PropIt(FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* FuncParamProperty = *PropIt;

			FBPTerminal* Term = Statement.RHS[NumParams];
			check(Term != NULL);

			// See if this is a hidden array param term, which needs to be fixed up with the final generated UArrayProperty
			if( FBPTerminal** ArrayParmTerm = Statement.ArrayCoersionTermMap.Find(Term) )
			{
				Term->ObjectLiteral = (*ArrayParmTerm)->AssociatedVarProperty;
			}

			// Emit parameter term normally
			EmitTerm(Term, FuncParamProperty);

			NumParams++;
		}

		// End of parameter list
		Writer << EX_EndFunctionParms;
	}

	void EmitTerm(FBPTerminal* Term, UProperty* CoerceProperty = NULL, FBPTerminal* RValueTerm = NULL)
	{
		if (Term->Context == NULL)
		{
			EmitTermExpr(Term, CoerceProperty);
		}
		else
		{
			if (Term->Context->bIsStructContext)
			{
				check(Term->AssociatedVarProperty);

				Writer << EX_StructMemberContext;
				Writer << Term->AssociatedVarProperty;

				// Now run the context expression
				EmitTerm(Term->Context, NULL);
			}
			else
			{
				// If this is the top of the chain this context, then save it off the r-value and pass it down the chain so we can safely handle runtime null contexts
				if( RValueTerm == NULL )
				{
					RValueTerm = Term;
				}

 				FContextEmitter CallContextWriter(*this);
				CallContextWriter.TryStartContext(Term->Context, /*@TODO: bUnsafeToSkip*/ true, /*bIsInterfaceContext*/ false, RValueTerm);

				EmitTermExpr(Term, CoerceProperty);
			}
		}
	}

	void EmitDestinationExpression(FBPTerminal* DestinationExpression)
	{
		check(DestinationExpression->AssociatedVarProperty != NULL);

		const bool bIsDelegate = Cast<UDelegateProperty>(DestinationExpression->AssociatedVarProperty) != NULL;
		const bool bIsMulticastDelegate = Cast<UMulticastDelegateProperty>(DestinationExpression->AssociatedVarProperty) != NULL;
		const bool bIsBoolean = Cast<UBoolProperty>(DestinationExpression->AssociatedVarProperty) != NULL;
		const bool bIsObj = Cast<UObjectPropertyBase>(DestinationExpression->AssociatedVarProperty) != NULL;
		const bool bIsWeakObjPtr = Cast<UWeakObjectProperty>(DestinationExpression->AssociatedVarProperty) != NULL;
		if (bIsMulticastDelegate)
		{
			Writer << EX_LetMulticastDelegate;
		}
		else if (bIsDelegate)
		{
			Writer << EX_LetDelegate;
		}
		else if (bIsBoolean)
		{
			Writer << EX_LetBool;
		}
		else if (bIsObj)
		{
			if( !bIsWeakObjPtr )
			{
				Writer << EX_LetObj;
			}
			else
			{
				Writer << EX_LetWeakObjPtr;
			}
		}
		else
		{
			Writer << EX_Let;
		}
		EmitTerm(DestinationExpression);
	}

	void EmitAssignmentStatment(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* DestinationExpression = Statement.LHS;
		FBPTerminal* SourceExpression = Statement.RHS[0];

		EmitDestinationExpression(DestinationExpression);

		EmitTerm(SourceExpression, DestinationExpression->AssociatedVarProperty);
	}

	void EmitAssignmentOnPersistentFrameStatment(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* DestinationExpression = Statement.LHS;
		FBPTerminal* SourceExpression = Statement.RHS[0];

		Writer << Ex_LetValueOnPersistentFrame;
		check(ClassBeingBuilt && ClassBeingBuilt->UberGraphFunction);
		Writer << DestinationExpression->AssociatedVarProperty;

		EmitTerm(SourceExpression, DestinationExpression->AssociatedVarProperty);
	}

	void EmitCastObjToInterfaceStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* DestinationExpression = Statement.LHS;
		FBPTerminal* InterfaceExpression = Statement.RHS[0];
		FBPTerminal* TargetExpression = Statement.RHS[1];

		Writer << EX_Let;
		EmitTerm(DestinationExpression);

		Writer << EX_ObjToInterfaceCast;
		UClass* ClassPtr = CastChecked<UClass>(InterfaceExpression->ObjectLiteral);
		check(ClassPtr);
		Writer << ClassPtr;
		EmitTerm(TargetExpression, (UProperty*)(GetDefault<UObjectProperty>()));
	}

	void EmitCastBetweenInterfacesStatement(FBlueprintCompiledStatement& Statement) 
	{
		FBPTerminal* DestinationExpression = Statement.LHS;
		FBPTerminal* InterfaceExpression   = Statement.RHS[0];
		FBPTerminal* TargetExpression      = Statement.RHS[1];

		Writer << EX_Let;
		EmitTerm(DestinationExpression);

		Writer << EX_CrossInterfaceCast;
		UClass* ClassPtr = CastChecked<UClass>(InterfaceExpression->ObjectLiteral);
		check(ClassPtr);
		Writer << ClassPtr;
		EmitTerm(TargetExpression, (UProperty*)(GetDefault<UInterfaceProperty>()));
	}

	void EmitCastInterfaceToObjStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* DestinationExpression     = Statement.LHS;
		FBPTerminal* ResultObjClassExpression  = Statement.RHS[0];
		FBPTerminal* TargetInterfaceExpression = Statement.RHS[1];

		Writer << EX_Let;
		EmitTerm(DestinationExpression);

		Writer << EX_InterfaceToObjCast;
		UClass* ClassPtr = CastChecked<UClass>(ResultObjClassExpression->ObjectLiteral);
		check(ClassPtr != nullptr);
		Writer << ClassPtr;
		EmitTerm(TargetInterfaceExpression, (UProperty*)(GetDefault<UObjectProperty>()));
	}

	void EmitDynamicCastStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* DestinationExpression = Statement.LHS;
		FBPTerminal* ResultClassExpression = Statement.RHS[0];
		FBPTerminal* TargetExpression = Statement.RHS[1];

		Writer << EX_Let;
		EmitTerm(DestinationExpression);

		Writer << EX_DynamicCast;
		UClass* ClassPtr = CastChecked<UClass>(ResultClassExpression->ObjectLiteral);
		Writer << ClassPtr;
		EmitTerm(TargetExpression, (UProperty*)(GetDefault<UObjectProperty>()));
	}

	void EmitMetaCastStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* DestinationExpression = Statement.LHS;
		FBPTerminal* InterfaceExpression = Statement.RHS[0];
		FBPTerminal* TargetExpression = Statement.RHS[1];

		Writer << EX_Let;
		EmitTerm(DestinationExpression);

		Writer << EX_MetaCast;
		UClass* ClassPtr = CastChecked<UClass>(InterfaceExpression->ObjectLiteral);
		Writer << ClassPtr;
		EmitTerm(TargetExpression, (UProperty*)(GetDefault<UClassProperty>()));
	}

	void EmitObjectToBoolStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* DestinationExpression = Statement.LHS;
		FBPTerminal* TargetExpression = Statement.RHS[0];

		UClass* PSCObjClass = Cast<UClass>(TargetExpression->Type.PinSubCategoryObject.Get());
		const bool bIsInterfaceCast = (PSCObjClass && PSCObjClass->HasAnyClassFlags(CLASS_Interface));

		Writer << EX_Let;
		EmitTerm(DestinationExpression);

		Writer << EX_PrimitiveCast;
		uint8 CastType = !bIsInterfaceCast ? CST_ObjectToBool : CST_InterfaceToBool;
		Writer << CastType;
		
		UProperty* TargetProperty = !bIsInterfaceCast ? ((UProperty*)(GetDefault<UObjectProperty>())) : ((UProperty*)(GetDefault<UInterfaceProperty>()));
		EmitTerm(TargetExpression, TargetProperty);
	}

	void EmitAddMulticastDelegateStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* Delegate = Statement.LHS;
		FBPTerminal* DelegateToAdd = Statement.RHS[0];

		Writer << EX_AddMulticastDelegate;
		EmitTerm(Delegate);
		EmitTerm(DelegateToAdd);
	}

	void EmitRemoveMulticastDelegateStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* Delegate = Statement.LHS;
		FBPTerminal* DelegateToAdd = Statement.RHS[0];

		Writer << EX_RemoveMulticastDelegate;
		EmitTerm(Delegate);
		EmitTerm(DelegateToAdd);
	}

	void EmitBindDelegateStatement(FBlueprintCompiledStatement& Statement)
	{
		check(2 == Statement.RHS.Num());
		FBPTerminal* Delegate = Statement.LHS;
		FBPTerminal* NameTerm = Statement.RHS[0];
		FBPTerminal* ObjectTerm = Statement.RHS[1];
		check(Delegate && ObjectTerm);
		check(NameTerm && NameTerm->bIsLiteral);
		check(!NameTerm->Name.IsEmpty());

		FName FunctionName(*(NameTerm->Name));
		Writer << EX_BindDelegate;
		Writer << FunctionName;
		
		EmitTerm(Delegate);
		EmitTerm(ObjectTerm, (UProperty*)(GetDefault<UObjectProperty>()));
	}

	void EmitClearMulticastDelegateStatement(FBlueprintCompiledStatement& Statement)
	{
		FBPTerminal* Delegate = Statement.LHS;

		Writer << EX_ClearMulticastDelegate;
		EmitTerm(Delegate);
	}

	void EmitCreateArrayStatement(FBlueprintCompiledStatement& Statement)
	{
		Writer << EX_SetArray;

		FBPTerminal* ArrayTerm = Statement.LHS;
		EmitTerm(ArrayTerm);
		
		UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(ArrayTerm->AssociatedVarProperty);
		UProperty* InnerProperty = ArrayProperty->Inner;

		for(auto ArrayItemIt = Statement.RHS.CreateIterator(); ArrayItemIt; ++ArrayItemIt)
		{
			FBPTerminal* Item = *ArrayItemIt;
			EmitTerm(Item, (Item->bIsLiteral ? InnerProperty : NULL));
		}

		Writer << EX_EndArray;
	}

	void EmitGoto(FBlueprintCompiledStatement& Statement)
	{
		if (Statement.Type == KCST_ComputedGoto)
		{
			// Emit the computed jump operation
			Writer << EX_ComputedJump;

			// Now include the integer offset expression
			EmitTerm(Statement.LHS, (UProperty*)(GetDefault<UIntProperty>()));
		}
		else if (Statement.Type == KCST_GotoIfNot)
		{
			// Emit the jump with a dummy address
			Writer << EX_JumpIfNot;
			CodeSkipSizeType PatchUpNeededAtOffset = Writer.EmitPlaceholderSkip();

			// Queue up a fixup to be done once all label offsets are known
			JumpTargetFixupMap.Add(PatchUpNeededAtOffset, Statement.TargetLabel);

			// Now include the boolean expression
			EmitTerm(Statement.LHS, (UProperty*)(GetDefault<UBoolProperty>()));
		}
		else if (Statement.Type == KCST_EndOfThreadIfNot)
		{
			// Emit the pop if not opcode
			Writer << EX_PopExecutionFlowIfNot;

			// Now include the boolean expression
			EmitTerm(Statement.LHS, (UProperty*)(GetDefault<UBoolProperty>()));
		}
		else if (Statement.Type == KCST_UnconditionalGoto)
		{
			// Emit the jump with a dummy address
			Writer << EX_Jump;
			CodeSkipSizeType PatchUpNeededAtOffset = Writer.EmitPlaceholderSkip();

			// Queue up a fixup to be done once all label offsets are known
			JumpTargetFixupMap.Add(PatchUpNeededAtOffset, Statement.TargetLabel);
		}
		else if (Statement.Type == KCST_GotoReturn)
		{
			// Emit the jump with a dummy address
			Writer << EX_Jump;
			CodeSkipSizeType PatchUpNeededAtOffset = Writer.EmitPlaceholderSkip();

			// Queue up a fixup to be done once all label offsets are known
			JumpTargetFixupMap.Add(PatchUpNeededAtOffset, &ReturnStatement);
		}
		else if (Statement.Type == KCST_GotoReturnIfNot)
		{
			// Emit the jump with a dummy address
			Writer << EX_JumpIfNot;
			CodeSkipSizeType PatchUpNeededAtOffset = Writer.EmitPlaceholderSkip();

			// Queue up a fixup to be done once all label offsets are known
			JumpTargetFixupMap.Add(PatchUpNeededAtOffset, &ReturnStatement);

			// Now include the boolean expression
			EmitTerm(Statement.LHS, (UProperty*)(GetDefault<UBoolProperty>()));
		}
		else
		{
			ensureMsg(false, TEXT("FScriptBuilderBase::EmitGoto unknown type"));
		}
	}

	void EmitPushExecState(FBlueprintCompiledStatement& Statement)
	{
		// Push the address onto the flow stack
		Writer << EX_PushExecutionFlow;
		CodeSkipSizeType PatchUpNeededAtOffset = Writer.EmitPlaceholderSkip();

		// Mark the target for fixup once the addresses have been resolved
		JumpTargetFixupMap.Add(PatchUpNeededAtOffset, Statement.TargetLabel);
	}

	void EmitPopExecState(FBlueprintCompiledStatement& Statement)
	{
		// Pop the state off the flow stack
		Writer << EX_PopExecutionFlow;
	}

	void EmitReturn(FKismetFunctionContext& Context)
	{
		UObject* ReturnProperty = Context.Function->GetReturnProperty();

		Writer << EX_Return;
		
		if (ReturnProperty == NULL)
		{
			Writer << EX_Nothing;
		}
		else
		{
			Writer << EX_LocalOutVariable;
			Writer << ReturnProperty;
		}
	}

	void PushReturnAddress(FBlueprintCompiledStatement& ReturnTarget)
	{
		Writer << EX_PushExecutionFlow;
		CodeSkipSizeType PatchUpNeededAtOffset = Writer.EmitPlaceholderSkip();

		JumpTargetFixupMap.Add(PatchUpNeededAtOffset, &ReturnTarget);
	}

	void CloseScript()
	{
		Writer << EX_EndOfScript;
	}

	virtual ~FScriptBuilderBase()
	{
	}

	void GenerateCodeForStatement(FKismetCompilerContext& CompilerContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement, UEdGraphNode* SourceNode)
	{
		// Record the start of this statement in the bytecode if it's needed as a target label
		if (Statement.bIsJumpTarget)
		{
			StatementLabelMap.Add(&Statement, Writer.ScriptBuffer.Num());
		}

		// Generate bytecode for the statement
		switch (Statement.Type)
		{
		case KCST_Nop:
			Writer << EX_Nothing;
			break;
		case KCST_WireTraceSite:
			{
				UEdGraphPin const* TrueSourcePin = Cast<UEdGraphPin const>(FunctionContext.MessageLog.FindSourceObject(Statement.ExecContext));
				if (TrueSourcePin)
				{
					int32 Offset = Writer.ScriptBuffer.Num();
					ClassBeingBuilt->GetDebugData().RegisterPinToCodeAssociation(TrueSourcePin, FunctionContext.Function, Offset);
				}
			}
			// no break, continue down through KCST_DebugSite
		case KCST_DebugSite:
			if (SourceNode != NULL)
			{
				// Record where this NOP is
				UEdGraphNode* TrueSourceNode = Cast<UEdGraphNode>(FunctionContext.MessageLog.FindSourceObject(SourceNode));
				if (TrueSourceNode)
				{
					// If this is a debug site for an expanded macro instruction, there should also be a macro source node associated with it
					UEdGraphNode* MacroSourceNode = Cast<UEdGraphNode>(CompilerContext.MessageLog.FinalNodeBackToMacroSourceMap.FindSourceObject(SourceNode));
					if (MacroSourceNode == SourceNode)
					{
						// The function above will return the given node if not found in the map. In that case there is no associated source macro node, so we clear it.
						MacroSourceNode = NULL;
					}

					TArray<TWeakObjectPtr<UEdGraphNode>> MacroInstanceNodes;
					bool bBreakpointSite = Statement.Type == KCST_DebugSite;

					if (MacroSourceNode)
					{
						// Only associate macro instance node breakpoints with source nodes that are linked to the entry node in an impure macro graph
						if (bBreakpointSite)
						{
							const UK2Node_MacroInstance* MacroInstanceNode = Cast<const UK2Node_MacroInstance>(TrueSourceNode);
							if (MacroInstanceNode)
							{
								TArray<const UEdGraphNode*> ValidBreakpointLocations;
								FKismetDebugUtilities::GetValidBreakpointLocations(MacroInstanceNode, ValidBreakpointLocations);
								bBreakpointSite = ValidBreakpointLocations.Contains(MacroSourceNode);
							}
						}

						// Gather up all the macro instance nodes that lead to this macro source node
						CompilerContext.MessageLog.MacroSourceToMacroInstanceNodeMap.MultiFind(MacroSourceNode, MacroInstanceNodes);
					}

					int32 Offset = Writer.ScriptBuffer.Num();
					ClassBeingBuilt->GetDebugData().RegisterNodeToCodeAssociation(TrueSourceNode, MacroSourceNode, MacroInstanceNodes, FunctionContext.Function, Offset, bBreakpointSite);
				}
			}
			Writer << ((Statement.Type == KCST_DebugSite) ? EX_Tracepoint : EX_WireTracepoint);
			break;
		case KCST_CallFunction:
			EmitFunctionCall(CompilerContext, FunctionContext, Statement);
			break;
		case KCST_CallDelegate:
			EmitCallDelegate(Statement);
			break;
		case KCST_Assignment:
			EmitAssignmentStatment(Statement);
			break;
		case KCST_AssignmentOnPersistentFrame:
			EmitAssignmentOnPersistentFrameStatment(Statement);
			break;
		case KCST_CastObjToInterface:
			EmitCastObjToInterfaceStatement(Statement);
			break;
		case KCST_CrossInterfaceCast:
			EmitCastBetweenInterfacesStatement(Statement);
			break;
		case KCST_CastInterfaceToObj:
			EmitCastInterfaceToObjStatement(Statement);
			break;
		case KCST_DynamicCast:
			EmitDynamicCastStatement(Statement);
			break;
		case KCST_MetaCast:
			EmitMetaCastStatement(Statement);
			break;
		case KCST_ObjectToBool:
			EmitObjectToBoolStatement(Statement);
			break;
		case KCST_AddMulticastDelegate:
			EmitAddMulticastDelegateStatement(Statement);
			break;
		case KCST_RemoveMulticastDelegate:
			EmitRemoveMulticastDelegateStatement(Statement);
			break;
		case KCST_BindDelegate:
			EmitBindDelegateStatement(Statement);
			break;
		case KCST_ClearMulticastDelegate:
			EmitClearMulticastDelegateStatement(Statement);
			break;
		case KCST_CreateArray:
			EmitCreateArrayStatement(Statement);
			break;
		case KCST_ComputedGoto:
		case KCST_UnconditionalGoto:
		case KCST_GotoIfNot:
		case KCST_EndOfThreadIfNot:
		case KCST_GotoReturn:
		case KCST_GotoReturnIfNot:
			EmitGoto(Statement);
			break;
		case KCST_PushState:
			EmitPushExecState(Statement);
			break;
		case KCST_EndOfThread:
			EmitPopExecState(Statement);
			break;
		case KCST_Comment:
			// VM ignores comments
			break;
		case KCST_Return:
			EmitReturn(FunctionContext);
			break;
		default:
			UE_LOG(LogK2Compiler, Warning, TEXT("VM backend encountered unsupported statement type %d"), (int32)Statement.Type);
		}
	}

	// Fix up all jump targets
	void PerformFixups()
	{
		for (TMap<CodeSkipSizeType, FBlueprintCompiledStatement*>::TIterator It(JumpTargetFixupMap); It; ++It)
		{
			CodeSkipSizeType OffsetToFix = It.Key();
			FBlueprintCompiledStatement* TargetStatement = It.Value();

			CodeSkipSizeType TargetStatementOffset = StatementLabelMap.FindChecked(TargetStatement);

			Writer.CommitSkip(OffsetToFix, TargetStatementOffset);
		}

		JumpTargetFixupMap.Empty();
	}
};

//////////////////////////////////////////////////////////////////////////
// FKismetCompilerVMBackend

void FKismetCompilerVMBackend::GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly)
{
	// Generate script bytecode
	for (int32 i = 0; i < Functions.Num(); ++i)
	{
		FKismetFunctionContext& Function = Functions[i];
		if (Function.IsValid())
		{
			const bool bIsUbergraph = (i == 0);
			ConstructFunction(Function, bIsUbergraph, bGenerateStubsOnly);
		}
	}
}

void FKismetCompilerVMBackend::ConstructFunction(FKismetFunctionContext& FunctionContext, bool bIsUbergraph, bool bGenerateStubOnly)
{
	UFunction* Function = FunctionContext.Function;
	UBlueprintGeneratedClass* Class = FunctionContext.NewClass;

	FString FunctionName;
	Function->GetName(FunctionName);

	TArray<uint8>& ScriptArray = Function->Script;

	// Return statement, to push on FlowStack or to use with _GotoReturn
	FBlueprintCompiledStatement ReturnStatement;
	ReturnStatement.Type = KCST_Return;

	FScriptBuilderBase ScriptWriter(ScriptArray, Class, Schema, UbergraphStatementLabelMap, bIsUbergraph, ReturnStatement);

	if (!bGenerateStubOnly)
	{
		ReturnStatement.bIsJumpTarget = true;
		if (FunctionContext.bUseFlowStack)
		{
			ScriptWriter.PushReturnAddress(ReturnStatement);
		}
	
		// Emit code in the order specified by the linear execution list (the first node is always the entry point for the function)
		for (int32 NodeIndex = 0; NodeIndex < FunctionContext.LinearExecutionList.Num(); ++NodeIndex)
		{
			UEdGraphNode* StatementNode = FunctionContext.LinearExecutionList[NodeIndex];
			TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(StatementNode);

			if (StatementList != NULL)
			{
				for (int32 StatementIndex = 0; StatementIndex < StatementList->Num(); ++StatementIndex)
				{
					FBlueprintCompiledStatement* Statement = (*StatementList)[StatementIndex];

					ScriptWriter.GenerateCodeForStatement(CompilerContext, FunctionContext, *Statement, StatementNode);
				}
			}
		}
	}

	// Handle the function return value
	ScriptWriter.GenerateCodeForStatement(CompilerContext, FunctionContext, ReturnStatement, NULL);	
	
	// Fix up jump addresses
	ScriptWriter.PerformFixups();

	// Close out the script
	ScriptWriter.CloseScript();

	// Save off the offsets within the ubergraph, needed to patch up the stubs later on
	if (bIsUbergraph)
	{
		ScriptWriter.CopyStatementMapToUbergraphMap();
	}

	// Make sure we didn't overflow the maximum bytecode size
#if SCRIPT_LIMIT_BYTECODE_TO_64KB
	if (ScriptArray.Num() > 0xFFFF)
	{
		MessageLog.Error(TEXT("Script exceeded bytecode length limit of 64 KB"));
		ScriptArray.Empty();
		ScriptArray.Add(EX_EndOfScript);
	}
#else
	static_assert(sizeof(CodeSkipSizeType) == 4, "Update this code as size changed.");
#endif
}
