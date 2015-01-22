// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealHeaderTool.h"
#include "HeaderParser.h"
#include "NativeClassExporter.h"
#include "ClassMaps.h"
#include "Classes.h"
#include "StringUtils.h"
#include "UObjectAnnotation.h"
#include "DefaultValueHelper.h"
#include "IScriptGeneratorPluginInterface.h"
#include "Manifest.h"

/*-----------------------------------------------------------------------------
	Constants & declarations.
-----------------------------------------------------------------------------*/

// Annotation of classes that have had an error during parsing
static FUObjectAnnotationSparseBool FailedClassesAnnotation;

enum {MAX_ARRAY_SIZE=2048};

static const FName NAME_ToolTip(TEXT("ToolTip"));
TMap<FClass*, ClassDefinitionRange> ClassDefinitionRanges;
/**
 * Dirty hack global variable to allow different result codes passed through
 * exceptions. Needs to be fixed in future versions of UHT.
 */
extern ECompilationResult::Type GCompilationResult;

/*-----------------------------------------------------------------------------
	Utility functions.
-----------------------------------------------------------------------------*/

namespace
{
	bool IsActorClass(UClass *Class)
	{
		static FName ActorName = FName(TEXT("Actor"));
		while (Class)
		{
			if (Class->GetFName() == ActorName)
			{
				return true;
			}
			Class = Class->GetSuperClass();
		}
		return false;
	}

	bool ProbablyAMacro(const TCHAR* Identifier)
	{
		// Test for known delegate and event macros.
		TCHAR DelegateStart[] = TEXT("DECLARE_DELEGATE_");
		if (!FCString::Strncmp(Identifier, DelegateStart, ARRAY_COUNT(DelegateStart) - 1))
			return true;

		TCHAR DelegateEvent[] = TEXT("DECLARE_EVENT");
		if (!FCString::Strncmp(Identifier, DelegateEvent, ARRAY_COUNT(DelegateEvent) - 1))
			return true;

		// Failing that, we'll guess about it being a macro based on it being a fully-capitalized identifier.
		while (TCHAR Ch = *Identifier++)
		{
			if (Ch != TEXT('_') && (Ch < TEXT('A') || Ch > TEXT('Z')))
				return false;
		}

		return true;
	}

	/**
	 * Parse and validate an array of identifiers (inside FUNC_NetRequest, FUNC_NetResponse) 
	 * @param FuncInfo function info for the current function
	 * @param Identifiers identifiers inside the net service declaration
	 */
	void ParseNetServiceIdentifiers(FFuncInfo& FuncInfo, const TArray<FString>& Identifiers)
	{
		FString IdTag         (TEXT("Id="));
		FString ResponseIdTag (TEXT("ResponseId="));
		FString MCPTag        (TEXT("MCP"));
		FString ProtobufferTag(TEXT("Protobuffer"));
		for (auto& Identifier : Identifiers)
		{
			if (Identifier == ProtobufferTag)
			{
				FuncInfo.FunctionExportFlags |= FUNCEXPORT_NeedsProto;
			}
			else if (Identifier == MCPTag)
			{
				FuncInfo.FunctionExportFlags |= FUNCEXPORT_NeedsMCP;
			}
			else if (Identifier.StartsWith(IdTag))
			{
				int32 TempInt = FCString::Atoi(*Identifier.Mid(IdTag.Len()));
				if (TempInt <= 0 || TempInt > MAX_uint16)
				{
					FError::Throwf(TEXT("Invalid network identifier %s for function"), *Identifier);
				}
				FuncInfo.RPCId = TempInt;
			}
			else if (Identifier.StartsWith(ResponseIdTag))
			{
				int32 TempInt = FCString::Atoi(*Identifier.Mid(ResponseIdTag.Len()));
				if (TempInt <= 0 || TempInt > MAX_uint16)
				{
					FError::Throwf(TEXT("Invalid network identifier %s for function"), *Identifier);
				}
				FuncInfo.RPCResponseId = TempInt;
			}
			else
			{
				FError::Throwf(TEXT("Invalid network identifier %s for function"), *Identifier);
			}
		}
		if (FuncInfo.FunctionExportFlags & FUNCEXPORT_NeedsProto)
		{
			if (FuncInfo.RPCId == 0)
			{
				FError::Throwf(TEXT("net service function does not have an RPCId."));
			}
			if (FuncInfo.RPCId == FuncInfo.RPCResponseId)
			{
				FError::Throwf(TEXT("Net service RPCId and ResponseRPCId cannot be the same."));
			}
			if ((FuncInfo.FunctionFlags & FUNC_NetResponse) && FuncInfo.RPCResponseId > 0)
			{
				FError::Throwf(TEXT("Net service response functions cannot have a ResponseId."));
			}
		}
	
		if (!(FuncInfo.FunctionExportFlags & FUNCEXPORT_NeedsProto) && !(FuncInfo.FunctionExportFlags & FUNCEXPORT_NeedsMCP))
		{
			FError::Throwf(TEXT("net service function needs to specify at least one provider type."));
		}
	}

	/**
	 * Processes a set of UFUNCTION or UDELEGATE specifiers into an FFuncInfo struct.
	 *
	 * @param FuncInfo   - The FFuncInfo object to populate.
	 * @param Specifiers - The specifiers to process.
	 */
	void ProcessFunctionSpecifiers(FFuncInfo& FuncInfo, const TArray<FPropertySpecifier>& Specifiers)
	{
		bool bSpecifiedUnreliable = false;

		for (const auto& Specifier : Specifiers)
		{
			if (Specifier.Key == TEXT("BlueprintNativeEvent"))
			{
				if (FuncInfo.FunctionFlags & FUNC_Net)
				{
					FError::Throwf(TEXT("BlueprintNativeEvent functions cannot be replicated!") );
				}
				else if ( (FuncInfo.FunctionFlags & FUNC_BlueprintEvent) && !(FuncInfo.FunctionFlags & FUNC_Native) )
				{
					// already a BlueprintImplementableEvent
					FError::Throwf(TEXT("A function cannot be both BlueprintNativeEvent and BlueprintImplementableEvent!") );
				}
				else if ( (FuncInfo.FunctionFlags & FUNC_Private) )
				{
					FError::Throwf(TEXT("A Private function cannot be a BlueprintNativeEvent!") );
				}

				FuncInfo.FunctionFlags |= FUNC_Event;
				FuncInfo.FunctionFlags |= FUNC_BlueprintEvent;
			}
			else if (Specifier.Key == TEXT("BlueprintImplementableEvent"))
			{
				if (FuncInfo.FunctionFlags & FUNC_Net)
				{
					FError::Throwf(TEXT("BlueprintImplementableEvent functions cannot be replicated!") );
				}
				else if ( (FuncInfo.FunctionFlags & FUNC_BlueprintEvent) && (FuncInfo.FunctionFlags & FUNC_Native) )
				{
					// already a BlueprintNativeEvent
					FError::Throwf(TEXT("A function cannot be both BlueprintNativeEvent and BlueprintImplementableEvent!") );
				}
				else if ( (FuncInfo.FunctionFlags & FUNC_Private) )
				{
					FError::Throwf(TEXT("A Private function cannot be a BlueprintImplementableEvent!") );
				}

				FuncInfo.FunctionFlags |= FUNC_Event;
				FuncInfo.FunctionFlags |= FUNC_BlueprintEvent;
				FuncInfo.FunctionFlags &= ~FUNC_Native;
			}
			else if (Specifier.Key == TEXT("Exec"))
			{
				FuncInfo.FunctionFlags |= FUNC_Exec;
				if( FuncInfo.FunctionFlags & FUNC_Net )
				{
					FError::Throwf(TEXT("Exec functions cannot be replicated!") );
				}
			}
			else if (Specifier.Key == TEXT("SealedEvent"))
			{
				FuncInfo.bSealedEvent = true;
			}
			else if (Specifier.Key == TEXT("Server"))
			{
				if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
				{
					FError::Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as Client or Server"));
				}

				FuncInfo.FunctionFlags |= FUNC_Net;
				FuncInfo.FunctionFlags |= FUNC_NetServer;
				if( FuncInfo.FunctionFlags & FUNC_Exec )
				{
					FError::Throwf(TEXT("Exec functions cannot be replicated!") );
				}
			}
			else if (Specifier.Key == TEXT("Client"))
			{
				if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
				{
					FError::Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as Client or Server"));
				}

				FuncInfo.FunctionFlags |= FUNC_Net;
				FuncInfo.FunctionFlags |= FUNC_NetClient;
			}
			else if (Specifier.Key == TEXT("NetMulticast"))
			{
				if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
				{
					FError::Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as Multicast"));
				}

				FuncInfo.FunctionFlags |= FUNC_Net;
				FuncInfo.FunctionFlags |= FUNC_NetMulticast;
			}
			else if (Specifier.Key == TEXT("ServiceRequest"))
			{
				if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
				{
					FError::Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as a ServiceRequest"));
				}

				FuncInfo.FunctionFlags |= FUNC_Net;
				FuncInfo.FunctionFlags |= FUNC_NetReliable;
				FuncInfo.FunctionFlags |= FUNC_NetRequest;
				FuncInfo.FunctionExportFlags |= FUNCEXPORT_CustomThunk;

				ParseNetServiceIdentifiers(FuncInfo, Specifier.Values);
			}
			else if (Specifier.Key == TEXT("ServiceResponse"))
			{
				if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
				{
					FError::Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as a ServiceResponse"));
				}

				FuncInfo.FunctionFlags |= FUNC_Net;
				FuncInfo.FunctionFlags |= FUNC_NetReliable;
				FuncInfo.FunctionFlags |= FUNC_NetResponse;

				ParseNetServiceIdentifiers(FuncInfo, Specifier.Values);
			}
			else if (Specifier.Key == TEXT("Reliable"))
			{
				FuncInfo.FunctionFlags |= FUNC_NetReliable;
			}
			else if (Specifier.Key == TEXT("Unreliable"))
			{
				bSpecifiedUnreliable = true;
			}
			else if (Specifier.Key == TEXT("CustomThunk"))
			{
				FuncInfo.FunctionExportFlags |= FUNCEXPORT_CustomThunk;
			}
			else if (Specifier.Key == TEXT("BlueprintCallable"))
			{
				FuncInfo.FunctionFlags |= FUNC_BlueprintCallable;
			}
			else if (Specifier.Key == TEXT("BlueprintPure"))
			{
				// This function can be called, and is also pure.
				FuncInfo.FunctionFlags |= FUNC_BlueprintCallable;
				FuncInfo.FunctionFlags |= FUNC_BlueprintPure;
			}
			else if (Specifier.Key == TEXT("BlueprintAuthorityOnly"))
			{
				FuncInfo.FunctionFlags |= FUNC_BlueprintAuthorityOnly;
			}
			else if (Specifier.Key == TEXT("BlueprintCosmetic"))
			{
				FuncInfo.FunctionFlags |= FUNC_BlueprintCosmetic;
			}
			else if (Specifier.Key == TEXT("WithValidation"))
			{
				FuncInfo.FunctionFlags |= FUNC_NetValidate;
			}
			else
			{
				FError::Throwf(TEXT("Unknown function specifier '%s'"), *Specifier.Key);
			}
		}

		if ( ( FuncInfo.FunctionFlags & FUNC_NetServer ) && !( FuncInfo.FunctionFlags & FUNC_NetValidate ) )
		{
			FError::Throwf( TEXT( "Server RPC missing 'WithValidation' keyword in the UPROPERTY() declaration statement.  Required for security purposes." ) );
		}

		if (FuncInfo.FunctionFlags & FUNC_Net)
		{
			// Network replicated functions are always events
			FuncInfo.FunctionFlags |= FUNC_Event;

			check(!(FuncInfo.FunctionFlags & (FUNC_BlueprintEvent | FUNC_Exec)));

			bool bIsNetService  = !!(FuncInfo.FunctionFlags & (FUNC_NetRequest | FUNC_NetResponse));
			bool bIsNetReliable = !!(FuncInfo.FunctionFlags & FUNC_NetReliable);

			if ( FuncInfo.FunctionFlags & FUNC_Static )
				FError::Throwf(TEXT("Static functions can't be replicated") );

			if (!bIsNetReliable && !bSpecifiedUnreliable && !bIsNetService)
				FError::Throwf(TEXT("Replicated function: 'reliable' or 'unreliable' is required"));

			if (bIsNetReliable && bSpecifiedUnreliable && !bIsNetService)
				FError::Throwf(TEXT("'reliable' and 'unreliable' are mutually exclusive"));
		}
		else if (FuncInfo.FunctionFlags & FUNC_NetReliable)
		{
			FError::Throwf(TEXT("'reliable' specified without 'client' or 'server'"));
		}
		else if (bSpecifiedUnreliable)
		{
			FError::Throwf(TEXT("'unreliable' specified without 'client' or 'server'"));
		}

		if (FuncInfo.bSealedEvent && !(FuncInfo.FunctionFlags & FUNC_Event))
		{
			FError::Throwf(TEXT("SealedEvent may only be used on events"));
		}

		if (FuncInfo.bSealedEvent && FuncInfo.FunctionFlags & FUNC_BlueprintEvent)
		{
			FError::Throwf(TEXT("SealedEvent cannot be used on Blueprint events"));
		}
	}

	void AddEditInlineMetaData(TMap<FName, FString>& MetaData)
	{
		MetaData.Add(TEXT("EditInline"), TEXT("true"));
	}
}
	
/////////////////////////////////////////////////////
// FScriptLocation

FHeaderParser* FScriptLocation::Compiler = NULL;

FScriptLocation::FScriptLocation()
{
	if ( Compiler != NULL )
	{
		Compiler->InitScriptLocation(*this);
	}
}

/////////////////////////////////////////////////////
// FHeaderParser

FString FHeaderParser::GetContext()
{
	// Return something useful if we didn't even get as far as class parsing
	if (!Class)
	{
		return Filename;
	}

	FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*GClassSourceFileMap[Class]);

	return FString::Printf(TEXT("%s(%i)"), *Filename, InputLine);
}

/*-----------------------------------------------------------------------------
	Code emitting.
-----------------------------------------------------------------------------*/


//
// Get a qualified class.
//
FClass* FHeaderParser::GetQualifiedClass(const FClasses& AllClasses, const TCHAR* Thing)
{
	TCHAR ClassName[256]=TEXT("");

	FToken Token;
	if (GetIdentifier(Token))
	{
		FCString::Strncat( ClassName, Token.Identifier, ARRAY_COUNT(ClassName) );
	}

	if (!ClassName[0])
	{
		FError::Throwf(TEXT("%s: Missing class name"), Thing );
	}

	return AllClasses.FindScriptClassOrThrow(ClassName);
}

/*-----------------------------------------------------------------------------
	Fields.
-----------------------------------------------------------------------------*/

/**
 * Find a field in the specified context.  Starts with the specified scope, then iterates
 * through the Outer chain until the field is found.
 * 
 * @param	InScope				scope to start searching for the field in 
 * @param	InIdentifier		name of the field we're searching for
 * @param	bIncludeParents		whether to allow searching in the scope of a parent struct
 * @param	FieldClass			class of the field to search for.  used to e.g. search for functions only
 * @param	Thing				hint text that will be used in the error message if an error is encountered
 *
 * @return	a pointer to a UField with a name matching InIdentifier, or NULL if it wasn't found
 */
UField* FHeaderParser::FindField
(
	UStruct*		Scope,
	const TCHAR*	InIdentifier,
	bool			bIncludeParents,
	UClass*			FieldClass,
	const TCHAR*	Thing
)
{
	check(InIdentifier);
	FName InName( InIdentifier, FNAME_Find, true );
	if (InName != NAME_None)
	{
		for( ; Scope; Scope = Cast<UStruct>(Scope->GetOuter()) )
		{
			for( TFieldIterator<UField> It(Scope); It; ++It )
			{
				if (It->GetFName() == InName)
				{
					if (!It->IsA(FieldClass))
					{
						if (Thing)
						{
							FError::Throwf(TEXT("%s: expecting %s, got %s"), Thing, *FieldClass->GetName(), *It->GetClass()->GetName() );
						}
						return NULL;
					}
					return *It;
				}
			}

			if (!bIncludeParents)
			{
				break;
			}
		}
	}

	return NULL;
}

// Check if a field obscures a field in an outer scope.
void FHeaderParser::CheckObscures(UStruct* Scope, const FString& ScriptName, const FString& FieldName)
{
	UEnum* FoundEnum = NULL;
	if (UEnum::LookupEnumName(*ScriptName, &FoundEnum) != INDEX_NONE)
	{
		FError::Throwf(TEXT("'%s' obscures a value in enumeration '%s'"), *FoundEnum->GetPathName());
	}

	UStruct* BaseScope = Scope->GetInheritanceSuper();
	while (BaseScope != NULL)
	{
		int32 OuterContextCount = 0;
		UField* Existing = FindField(BaseScope, *FieldName, false, UField::StaticClass(), NULL);
		if (Existing != NULL)
		{
			FError::Throwf(TEXT("'%s' obscures '%s' defined in %s class '%s'."), *ScriptName, *FieldName, OuterContextCount ? TEXT("outer") : TEXT("base"), *Existing->GetOuter()->GetName());
		}
		BaseScope = BaseScope->GetInheritanceSuper();
	}
}


/**
 * @return	true if Scope has UProperty objects in its list of fields
 */
bool FHeaderParser::HasMemberProperties( const UStruct* Scope )
{
	// it's safe to pass a NULL Scope to TFieldIterator, but this function shouldn't be called with a NULL Scope
	checkSlow(Scope);
	TFieldIterator<UProperty> It(Scope,EFieldIteratorFlags::ExcludeSuper);
	return It ? true : false;
}

/**
 * Get the parent struct specified.
 *
 * @param	CurrentScope	scope to start in
 * @param	SearchName		parent scope to search for
 *
 * @return	a pointer to the parent struct with the specified name, or NULL if the parent couldn't be found
 */
UStruct* FHeaderParser::GetSuperScope( UStruct* CurrentScope, const FName& SearchName )
{
	UStruct* SuperScope = CurrentScope;
	while (SuperScope && !SuperScope->GetInheritanceSuper())
	{
		SuperScope = CastChecked<UStruct>(SuperScope->GetOuter());
	}
	if (SuperScope != NULL)
	{
		// iterate up the inheritance chain looking for one that has the desired name
		do
		{
			UStruct* NextScope = SuperScope->GetInheritanceSuper();
			if (NextScope)
			{
				SuperScope = NextScope;
			}
			// if we reached the top and the scope is not a class, try the current class hierarchy next
			else if (Cast<UClass>(SuperScope) == NULL)
			{
				SuperScope = Class;
			}
			else
			{
				// otherwise we've failed
				SuperScope = NULL;
			}
		} while (SuperScope != NULL && SuperScope->GetFName() != SearchName);
	}

	return SuperScope;
}

/*-----------------------------------------------------------------------------
	Variables.
-----------------------------------------------------------------------------*/

//
// Compile an enumeration definition.
//
UEnum* FHeaderParser::CompileEnum(UClass* Scope)
{
	check(Scope);

	CheckAllow( TEXT("'Enum'"), ALLOW_TypeDecl );

	// Get the enum specifier list
	FToken                     EnumToken;
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Enum"), EnumToken.MetaData);

	FScriptLocation DeclarationPosition;

	// Check enum type. This can be global 'enum', 'namespace' or 'enum class' enums.
	bool            bReadEnumName = false;
	UEnum::ECppForm CppForm       = UEnum::ECppForm::Regular;
	if (!GetIdentifier(EnumToken))
	{
		FError::Throwf(TEXT("Missing identifier after UENUM()") );
	}

	if (EnumToken.Matches(TEXT("namespace"), ESearchCase::CaseSensitive))
	{
		CppForm      = UEnum::ECppForm::Namespaced;
		bReadEnumName = GetIdentifier(EnumToken);
	}
	else if (EnumToken.Matches(TEXT("enum"), ESearchCase::CaseSensitive))
	{
		if (!GetIdentifier(EnumToken))
		{
			FError::Throwf(TEXT("Missing identifier after enum") );
		}

		if (EnumToken.Matches(TEXT("class"), ESearchCase::CaseSensitive) || EnumToken.Matches(TEXT("struct"), ESearchCase::CaseSensitive))
		{
			CppForm       = UEnum::ECppForm::EnumClass;
			bReadEnumName = GetIdentifier(EnumToken);
		}
		else
		{
			CppForm       = UEnum::ECppForm::Regular;
			bReadEnumName = true;
		}
	}
	else
	{
		FError::Throwf(TEXT("UENUM() should be followed by \'enum\' or \'namespace\' keywords.") );
	}

	// Get enumeration name.
	if (!bReadEnumName)
	{
		FError::Throwf(TEXT("Missing enumeration name") );
	}

	// Verify that the enumeration definition is unique within this scope.
	UField* Existing = FindField(Scope, EnumToken.Identifier);
	if (Existing && Existing->GetOuter() == Scope)
	{
		FError::Throwf(TEXT("enum: '%s' already defined here"), *EnumToken.TokenName.ToString());
	}

	CheckObscures(Scope, EnumToken.Identifier, EnumToken.Identifier);
	ParseFieldMetaData(EnumToken.MetaData, EnumToken.Identifier);

	// Create enum definition.
	UEnum* Enum = new(FClassUtils::IsTemporaryClass(Scope) ? Scope->GetOuter() : Scope, EnumToken.Identifier, RF_Public) UEnum(FObjectInitializer());
	Enum->Next = Scope->Children;
	Scope->Children = Enum;

	// Validate the metadata for the enum
	ValidateMetaDataFormat(Enum, EnumToken.MetaData);

	// Read optional base for enum class
	if (CppForm == UEnum::ECppForm::EnumClass && MatchSymbol(TEXT(":")))
	{
		FToken BaseToken;
		if (!GetIdentifier(BaseToken))
		{
			FError::Throwf(TEXT("Missing enum base") );
		}

		// We only support uint8 at the moment, until the properties get updated
		if (FCString::Strcmp(BaseToken.Identifier, TEXT("uint8")))
		{
			FError::Throwf(TEXT("Only enum bases of type uint8 are currently supported"));
		}

		GEnumUnderlyingTypes.Add(Enum, CPT_Byte);
	}

	// Get opening brace.
	RequireSymbol( TEXT("{"), TEXT("'Enum'") );

	switch (CppForm)
	{
		case UEnum::ECppForm::Namespaced:
		{
			// Now handle the inner true enum portion
			RequireIdentifier(TEXT("enum"), TEXT("'Enum'"));

			FToken InnerEnumToken;
			if (!GetIdentifier(InnerEnumToken))
			{
				FError::Throwf(TEXT("Missing enumeration name") );
			}

			Enum->CppType = FString::Printf(TEXT("%s::%s"), EnumToken.Identifier, InnerEnumToken.Identifier);

			RequireSymbol( TEXT("{"), TEXT("'Enum'") );
		}
		break;

		case UEnum::ECppForm::Regular:
		case UEnum::ECppForm::EnumClass:
		{
			Enum->CppType = EnumToken.Identifier;
		}
		break;
	}

	// List of all metadata generated for this enum
	TMap<FName,FString> EnumValueMetaData = EnumToken.MetaData;

	// Add metadata for the module relative path
	const FString *ModuleRelativePath = GClassModuleRelativePathMap.Find(Scope);
	if(ModuleRelativePath != NULL)
	{
		EnumValueMetaData.Add(TEXT("ModuleRelativePath"), *ModuleRelativePath);
	}

	AddFormattedPrevCommentAsTooltipMetaData(EnumValueMetaData);

	// Parse all enums tags.
	FToken TagToken;

	TArray<FScriptLocation> EnumTagLocations;
	TArray<FName> EnumNames;

	int32 CurrentEnumValue = 0;

	while (GetIdentifier(TagToken))
	{
		AddFormattedPrevCommentAsTooltipMetaData(TagToken.MetaData);

		FScriptLocation* ValueDeclarationPos = new(EnumTagLocations) FScriptLocation();

		// Try to read an optional explicit enum value specification
		if (MatchSymbol(TEXT("=")))
		{
			int32 NewEnumValue = 0;
			GetConstInt(/*out*/ NewEnumValue, TEXT("Enumerator value"));

			if ((NewEnumValue < CurrentEnumValue) || (NewEnumValue > 255))
			{
				FError::Throwf(TEXT("Explicitly specified enum values must be greater than any previous value and less than 256"));
			}

			CurrentEnumValue = NewEnumValue;
		}

		int32 iFound;
		FName NewTag;
		switch (CppForm)
		{
			case UEnum::ECppForm::Namespaced:
			case UEnum::ECppForm::EnumClass:
			{
				NewTag = FName(*FString::Printf(TEXT("%s::%s"), EnumToken.Identifier, TagToken.Identifier), FNAME_Add, true);
			}
			break;

			case UEnum::ECppForm::Regular:
			{
				NewTag = FName(TagToken.Identifier, FNAME_Add, true);
			}
			break;
		}

		if (EnumNames.Find(NewTag, iFound))
		{
			FError::Throwf(TEXT("Duplicate enumeration tag %s"), TagToken.Identifier );
		}

		if (CurrentEnumValue > 255)
		{
			FError::Throwf(TEXT("Exceeded maximum of 255 enumerators") );
		}

		UEnum* FoundEnum = NULL;
		if (UEnum::LookupEnumName(NewTag, &FoundEnum) != INDEX_NONE)
		{
			FError::Throwf(TEXT("Enumeration tag '%s' already in use by enum '%s'"), TagToken.Identifier, *FoundEnum->GetPathName());
		}

		// Make sure the enum names array is tightly packed by inserting dummies
		//@TODO: UCREMOVAL: Improve the UEnum system so we can have loosely packed values for e.g., bitfields
		for (int32 DummyIndex = EnumNames.Num(); DummyIndex < CurrentEnumValue; ++DummyIndex)
		{
			FString DummyName              = FString::Printf(TEXT("UnusedSpacer_%d"), DummyIndex);
			FString DummyNameWithQualifier = FString::Printf(TEXT("%s::%s"), EnumToken.Identifier, *DummyName);
			EnumNames.Add(FName(*DummyNameWithQualifier));

			// These ternary operators are the correct way around, believe it or not.
			// Spacers are qualified with the ETheEnum:: when they're regular enums in order to prevent spacer name clashes.
			// They're not qualified when they're actually in a namespace or are enum classes.
			InsertMetaDataPair(EnumValueMetaData, ((CppForm != UEnum::ECppForm::Regular) ? DummyName : DummyNameWithQualifier) + TEXT(".Hidden"), TEXT(""));
			InsertMetaDataPair(EnumValueMetaData, ((CppForm != UEnum::ECppForm::Regular) ? DummyName : DummyNameWithQualifier) + TEXT(".Spacer"), TEXT(""));
		}

		// Save the new tag
		EnumNames.Add( NewTag );

		// Autoincrement the current enumerant value
		CurrentEnumValue++;

		// check for metadata on this enum value
		ParseFieldMetaData(TagToken.MetaData, TagToken.Identifier);
		if (TagToken.MetaData.Num() > 0)
		{
			// special case for enum value metadata - we need to prepend the key name with the enum value name
			const FString TokenString = TagToken.Identifier;
			for (const auto& MetaData : TagToken.MetaData)
			{
				FString KeyString = TokenString + TEXT(".") + MetaData.Key.ToString();
				EnumValueMetaData.Add(FName(*KeyString), MetaData.Value);
			}

			// now clear the metadata because we're going to reuse this token for parsing the next enum value
			TagToken.MetaData.Empty();
		}

		if (!MatchSymbol(TEXT(",")))
		{
			break;
		}
	}

	// Add the metadata gathered for the enum to the package
	if (EnumValueMetaData.Num() > 0)
	{
		UMetaData* PackageMetaData = Enum->GetOutermost()->GetMetaData();
		checkSlow(PackageMetaData);

		PackageMetaData->SetObjectValues(Enum, EnumValueMetaData);
	}

	if (!EnumNames.Num())
	{
		FError::Throwf(TEXT("Enumeration must contain at least one enumerator") );
	}

	// Trailing brace and semicolon for the enum
	RequireSymbol( TEXT("}"), TEXT("'Enum'") );
	MatchSemi();

	if (CppForm == UEnum::ECppForm::Namespaced)
	{
		// Trailing brace for the namespace.
		RequireSymbol( TEXT("}"), TEXT("'Enum'") );
	}

	// Register the list of enum names.
	if (!Enum->SetEnums(EnumNames, CppForm))
	{
		const FName MaxEnumItem      = *(Enum->GenerateEnumPrefix() + TEXT("_MAX"));
		const int32 MaxEnumItemIndex = Enum->FindEnumIndex(MaxEnumItem);
		if (MaxEnumItemIndex != INDEX_NONE)
		{
			ReturnToLocation(EnumTagLocations[MaxEnumItemIndex], false, true);
			FError::Throwf(TEXT("Illegal enumeration tag specified.  Conflicts with auto-generated tag '%s'"), *MaxEnumItem.ToString());
		}

		FError::Throwf(TEXT("Unable to generate enum MAX entry '%s' due to name collision"), *MaxEnumItem.ToString());
	}

	return Enum;
}

/**
 * Checks if a string is made up of all the same character.
 *
 * @param  Str The string to check for all
 * @param  Ch  The character to check for
 *
 * @return True if the string is made up only of Ch characters.
 */
bool IsAllSameChar(const TCHAR* Str, TCHAR Ch)
{
	check(Str);

	while (TCHAR StrCh = *Str++)
	{
		if (StrCh != Ch)
			return false;
	}

	return true;
}

/**
 * Checks if a string is made up of all the same character.
 *
 * @param  Str The string to check for all
 * @param  Ch  The character to check for
 *
 * @return True if the string is made up only of Ch characters.
 */
bool IsLineSeparator(const TCHAR* Str)
{
	check(Str);

	return IsAllSameChar(Str, TEXT('-')) || IsAllSameChar(Str, TEXT('=')) || IsAllSameChar(Str, TEXT('*'));
}

/**
 * @param		Input		An input string, expected to be a script comment.
 * @return					The input string, reformatted in such a way as to be appropriate for use as a tooltip.
 */
FString FHeaderParser::FormatCommentForToolTip(const FString& Input)
{
	// Return an empty string if there are no alpha-numeric characters or a Unicode characters above 0xFF
	// (which would be the case for pure CJK comments) in the input string.
	bool bFoundAlphaNumericChar = false;
	for ( int32 i = 0 ; i < Input.Len() ; ++i )
	{
		if ( FChar::IsAlnum(Input[i]) || (Input[i] > 0xFF) )
		{
			bFoundAlphaNumericChar = true;
			break;
		}
	}

	if ( !bFoundAlphaNumericChar )
	{
		return FString( TEXT("") );
	}

	// Check for known commenting styles.
	FString Result( Input );
	const bool bJavaDocStyle = Input.Contains(TEXT("/**"));
	const bool bCStyle = Input.Contains(TEXT("/*"));
	const bool bCPPStyle = Input.StartsWith(TEXT("//"));

	if ( bJavaDocStyle || bCStyle)
	{
		// Remove beginning and end markers.
		Result = Result.Replace( TEXT("/**"), TEXT("") );
		Result = Result.Replace( TEXT("/*"), TEXT("") );
		Result = Result.Replace( TEXT("*/"), TEXT("") );
	}

	if ( bCPPStyle )
	{
		// Remove c++-style comment markers.  Also handle javadoc-style comments by replacing
		// all triple slashes with double-slashes
		Result = Result.Replace(TEXT("///"), TEXT("//")).Replace( TEXT("//"), TEXT("") );

		// Parser strips cpptext and replaces it with "// (cpptext)" -- prevent
		// this from being treated as a comment on variables declared below the
		// cpptext section
		Result = Result.Replace( TEXT("(cpptext)"), TEXT("") );
	}

	// Get rid of carriage return or tab characters, which mess up tooltips.
	Result = Result.Replace( TEXT( "\r" ), TEXT( "" ) );

	//wx widgets has a hard coded tab size of 8
	{
		const int32 SpacesPerTab = 8;
		Result = Result.ConvertTabsToSpaces (SpacesPerTab);
	}

	// get rid of uniform leading whitespace and all trailing whitespace, on each line
	TArray<FString> Lines;
	Result.ParseIntoArray(&Lines, TEXT("\n"), false);

	for (auto& Line : Lines)
	{
		// Remove trailing whitespace
		Line.TrimTrailing();

		// Remove leading "*" and "* " in javadoc comments.
		if (bJavaDocStyle)
		{
			// Find first non-whitespace character
			int32 Pos = 0;
			while (Pos < Line.Len() && FChar::IsWhitespace(Line[Pos]))
			{
				++Pos;
			}

			// Is it a *?
			if (Pos < Line.Len() && Line[Pos] == '*')
			{
				// Eat next space as well
				if (Pos+1 < Line.Len() && FChar::IsWhitespace(Line[Pos+1]))
				{
					++Pos;
				}

				Line = Line.RightChop(Pos + 1);
			}
		}
	}

	// Find first meaningful line
	int32 FirstIndex = 0;
	for (FString Line : Lines)
	{
		Line.Trim();

		if (Line.Len() && !IsLineSeparator(*Line))
			break;

		++FirstIndex;
	}

	int32 LastIndex = Lines.Num();
	while (LastIndex != FirstIndex)
	{
		FString Line = Lines[LastIndex - 1];
		Line.Trim();

		if (Line.Len() && !IsLineSeparator(*Line))
			break;

		--LastIndex;
	}

	Result.Empty();

	if (FirstIndex != LastIndex)
	{
		auto& FirstLine = Lines[FirstIndex];

		// Figure out how much whitespace is on the first line
		int32 MaxNumWhitespaceToRemove;
		for (MaxNumWhitespaceToRemove = 0; MaxNumWhitespaceToRemove < FirstLine.Len(); MaxNumWhitespaceToRemove++)
		{
			if (!FChar::IsLinebreak(FirstLine[MaxNumWhitespaceToRemove]) && !FChar::IsWhitespace(FirstLine[MaxNumWhitespaceToRemove]))
			{
				break;
			}
		}

		for (int32 Index = FirstIndex; Index != LastIndex; ++Index)
		{
			FString Line = Lines[Index];

			int32 TemporaryMaxWhitespace = MaxNumWhitespaceToRemove;

			// Allow eating an extra tab on subsequent lines if it's present
			if ((Index > 0) && (Line.Len() > 0) && (Line[0] == '\t'))
			{
				TemporaryMaxWhitespace++;
			}

			// Advance past whitespace
			int32 Pos = 0;
			while (Pos < TemporaryMaxWhitespace && Pos < Line.Len() && FChar::IsWhitespace(Line[Pos]))
			{
				++Pos;
			}

			if (Pos > 0)
			{
				Line = Line.Mid(Pos);
			}

			if (Index > 0)
			{
				Result += TEXT("\n");
			}

			if (Line.Len() && !IsAllSameChar(*Line, TEXT('=')))
			{
				Result += Line;
			}
		}
	}

	//@TODO: UCREMOVAL: Really want to trim an arbitrary number of newlines above and below, but keep multiple newlines internally
	// Make sure it doesn't start with a newline
	if (!Result.IsEmpty() && FChar::IsLinebreak(Result[0]))
	{
		Result = Result.Mid(1);
	}

	// Make sure it doesn't end with a dead newline
	if (!Result.IsEmpty() && FChar::IsLinebreak(Result[Result.Len() - 1]))
	{
		Result = Result.Left(Result.Len() - 1);
	}

	// Done.
	return Result;
}

void FHeaderParser::AddFormattedPrevCommentAsTooltipMetaData(TMap<FName, FString>& MetaData)
{
	// Don't add a tooltip if one already exists.
	if (MetaData.Find(NAME_ToolTip))
	{
		return;
	}

	// Don't add a tooltip if the comment is empty after formatting.
	FString FormattedComment = FormatCommentForToolTip(PrevComment);
	if (!FormattedComment.Len())
	{
		return;
	}

	MetaData.Add(NAME_ToolTip, *FormattedComment);

	// We've already used this comment as a tooltip, so clear it so that it doesn't get used again
	PrevComment.Empty();
}

static const TCHAR* GetAccessSpecifierName(EAccessSpecifier AccessSpecifier)
{
	switch (AccessSpecifier)
	{
		case ACCESS_Public:
			return TEXT("public");
		case ACCESS_Protected:
			return TEXT("protected");
		case ACCESS_Private:
			return TEXT("private");
		default:
			check(0);
	}
	return TEXT("");
}

// Tries to parse the token as an access protection specifier (public:, protected:, or private:)
EAccessSpecifier FHeaderParser::ParseAccessProtectionSpecifier(FToken& Token)
{
	EAccessSpecifier ResultAccessSpecifier = ACCESS_NotAnAccessSpecifier;

	for (EAccessSpecifier Test = EAccessSpecifier(ACCESS_NotAnAccessSpecifier + 1); Test != ACCESS_Num; Test = EAccessSpecifier(Test + 1))
	{
		if (Token.Matches(GetAccessSpecifierName(Test)) || (Token.Matches(TEXT("private_subobject")) && Test == ACCESS_Public))
		{
			// Consume the colon after the specifier
			RequireSymbol(TEXT(":"), *FString::Printf(TEXT("after %s"), Token.Identifier));
			return Test;
		}
	}
	return ACCESS_NotAnAccessSpecifier;
}


/**
 * Compile a struct definition.
 */
UScriptStruct* FHeaderParser::CompileStructDeclaration(FClasses& AllClasses, FClass* Scope)
{
	check(Scope);

	// Make sure structs can be declared here.
	CheckAllow( TEXT("'struct'"), ALLOW_TypeDecl );//@TODO: UCREMOVAL: After the switch: Make this require global scope

	FScriptLocation StructDeclaration;

	bool IsNative = false;
	bool IsExport = false;
	bool IsTransient = false;
	uint32 StructFlags = STRUCT_Native;
	TMap<FName, FString> MetaData;

	// Add metadata for the module relative path
	const FString *ModuleRelativePath = GClassModuleRelativePathMap.Find(Scope);
	if(ModuleRelativePath != NULL)
	{
		MetaData.Add(TEXT("ModuleRelativePath"), *ModuleRelativePath);
	}

	// Get the struct specifier list
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Struct"), MetaData);

	// Consume the struct keyword
	RequireIdentifier(TEXT("struct"), TEXT("Struct declaration specifier"));

	// The struct name as parsed in script and stripped of it's prefix
	FString StructNameInScript;

	// The struct name stripped of it's prefix
	FString StructNameStripped;

	// The required API module for this struct, if any
	FString RequiredAPIMacroIfPresent;

	SkipDeprecatedMacroIfNecessary();

	// Read the struct name
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ StructNameInScript, /*out*/ RequiredAPIMacroIfPresent, TEXT("struct"));

	// Record that this struct is RequiredAPI if the CORE_API style macro was present
	if (!RequiredAPIMacroIfPresent.IsEmpty())
	{
		StructFlags |= STRUCT_RequiredAPI;
	}

	// Handle the forced naming inconsistency (field names for structs, etc... do *not* have their prefix on them)
	bool bOverrideStructName = false;
	FString CustomExportText;
	if (StructsWithNoPrefix.Contains(StructNameInScript))
	{
		CustomExportText = StructNameInScript;
	}
	else
	{
		bOverrideStructName = true;
		StructNameStripped = GetClassNameWithPrefixRemoved(StructNameInScript);
	}

	// Effective struct name
	const FString EffectiveStructName = bOverrideStructName ? *StructNameStripped : *StructNameInScript;

	// Process the list of specifiers
	for (TArray<FPropertySpecifier>::TIterator SpecifierIt(SpecifiersFound); SpecifierIt; ++SpecifierIt)
	{
		const FString& Specifier = SpecifierIt->Key;

		if (Specifier == TEXT("NoExport"))
		{
			//UE_LOG(LogCompile, Warning, TEXT("Struct named %s in %s is still marked noexport"), *EffectiveStructName, *(Class->GetName()));//@TODO: UCREMOVAL: Debug printing
			StructFlags &= ~STRUCT_Native;
		}
		else if (Specifier == TEXT("Atomic"))
		{
			StructFlags |= STRUCT_Atomic;
		}
		else if (Specifier == TEXT("Immutable"))
		{
			StructFlags |= STRUCT_Immutable | STRUCT_Atomic;

			if (Class != UObject::StaticClass())
			{
				FError::Throwf(TEXT("Immutable is being phased out in favor of SerializeNative, and is only legal on the mirror structs declared in UObject"));
			}
		}
		else
		{
			FError::Throwf(TEXT("Unknown struct specifier '%s'"), *Specifier);
		}
	}

	// Verify uniqueness (if declared within UClass).
	if (!FClassUtils::IsTemporaryClass(Scope))
	{
		UField* Existing = FindField(Scope, *EffectiveStructName);
		if (Existing && Existing->GetOuter() == Scope)
		{
			FError::Throwf(TEXT("struct: '%s' already defined here"), *EffectiveStructName);
		}

		if (FindObject<UClass>(ANY_PACKAGE, *EffectiveStructName) != NULL)
		{
			FError::Throwf(TEXT("struct: '%s' conflicts with class name"), *EffectiveStructName);
		}

		CheckObscures(Scope, StructNameInScript, EffectiveStructName);
	}

	// Get optional superstruct.
	bool bExtendsBaseStruct = false;
	
	if (MatchSymbol(TEXT(":")))
	{
		RequireIdentifier(TEXT("public"), TEXT("struct inheritance"));
		bExtendsBaseStruct = true;
	}

	UScriptStruct* BaseStruct = NULL;
	if (bExtendsBaseStruct)
	{
		FToken ParentScope, ParentName;
		if (GetIdentifier( ParentScope ))
		{
			UStruct* StructClass = Scope;
			FString ParentStructNameInScript = FString(ParentScope.Identifier);
			if (MatchSymbol(TEXT(".")))
			{
				if (GetIdentifier(ParentName))
				{
					ParentStructNameInScript = FString(ParentName.Identifier);
					FString ParentNameStripped = GetClassNameWithPrefixRemoved(ParentScope.Identifier);
					StructClass = AllClasses.FindClass(*ParentNameStripped);
					if( !StructClass )
					{
						// If we find the literal class name, the user didn't use a prefix
						StructClass = AllClasses.FindClass(ParentScope.Identifier);
						if( StructClass )
						{
							FError::Throwf(TEXT("'struct': Parent struct class '%s' is missing a prefix, expecting '%s'"), ParentScope.Identifier, *FString::Printf(TEXT("%s%s"),StructClass->GetPrefixCPP(),ParentScope.Identifier) );
						}
						else
						{
							FError::Throwf(TEXT("'struct': Can't find parent struct class '%s'"), ParentScope.Identifier );
						}
					}
				}
				else
				{
					FError::Throwf( TEXT("'struct': Missing parent struct type after '%s.'"), ParentScope.Identifier );
				}
			}
			
			FString ParentStructNameStripped;
			UField* Field = NULL;
			bool bOverrideParentStructName = false;

			if( !StructsWithNoPrefix.Contains(ParentStructNameInScript) )
			{
				bOverrideParentStructName = true;
				ParentStructNameStripped = GetClassNameWithPrefixRemoved(ParentStructNameInScript);
			}

			// If we're expecting a prefix, first try finding the correct field with the stripped struct name
			if (bOverrideParentStructName)
			{
				Field = FindField( StructClass, *ParentStructNameStripped, true, UScriptStruct::StaticClass(), TEXT("'extends'") );
			}

			// If it wasn't found, try to find the literal name given
			if (Field == NULL)
			{
				Field = FindField( StructClass, *ParentStructNameInScript, true, UScriptStruct::StaticClass(), TEXT("'extends'") );
			}

			// Resolve structs declared in another class  //@TODO: UCREMOVAL: This seems extreme
			if (Field == NULL)
			{
				if (bOverrideParentStructName)
				{
					Field = FindObject<UScriptStruct>(ANY_PACKAGE, *ParentStructNameStripped);
				}

				if (Field == NULL)
				{
					Field = FindObject<UScriptStruct>(ANY_PACKAGE, *ParentStructNameInScript);
				}
			}

			// If the struct still wasn't found, throw an error
			if ((Field == NULL) || !Field->IsA( UScriptStruct::StaticClass() ))
			{
				FError::Throwf(TEXT("'struct': Can't find struct '%s'"), *ParentStructNameInScript );
			}
			else
			{
				// If the struct was found, confirm it adheres to the correct syntax. This should always fail if we were expecting an override that was not found.
				BaseStruct = ((UScriptStruct*)Field);
				if( bOverrideParentStructName )
				{
					const TCHAR* PrefixCPP = StructsWithTPrefix.Contains(ParentStructNameStripped) ? TEXT("T") : BaseStruct->GetPrefixCPP();
					if( ParentStructNameInScript != FString::Printf(TEXT("%s%s"), PrefixCPP, *ParentStructNameStripped) )
					{
						BaseStruct = NULL;
						FError::Throwf(TEXT("Parent Struct '%s' is missing a valid Unreal prefix, expecting '%s'"), *ParentStructNameInScript, *FString::Printf(TEXT("%s%s"), PrefixCPP, *Field->GetName()));
					}
				}
				else
				{
				
				}
			}
		}
		else
		{
			FError::Throwf(TEXT("'struct': Missing parent struct after ': public'") );
		}
	}

	// if we have a base struct, propagate inherited struct flags now
	if (BaseStruct != NULL)
	{
		StructFlags |= (BaseStruct->StructFlags&STRUCT_Inherit);
	}
	
	// If declared in a header without UClass, the outer is the class package.
	UObject* StructOuter = FClassUtils::IsTemporaryClass(Scope) ? Scope->GetOuter() : Scope;
	// Create.
	UScriptStruct* Struct = new(StructOuter, *EffectiveStructName, RF_Public) UScriptStruct(FObjectInitializer(), BaseStruct);
	Struct->Next = Scope->Children;
	Scope->Children = Struct;

	// Check to make sure the syntactic native prefix was set-up correctly.
	// If this check results in a false positive, it will be flagged as an identifier failure.
	if (bOverrideStructName)
	{
		FString DeclaredPrefix = GetClassPrefix( StructNameInScript );
		if( DeclaredPrefix == Struct->GetPrefixCPP() || DeclaredPrefix == TEXT("T") )
		{
			// Found a prefix, do a basic check to see if it's valid
			const TCHAR* ExpectedPrefixCPP = StructsWithTPrefix.Contains(StructNameStripped) ? TEXT("T") : Struct->GetPrefixCPP();
			FString ExpectedStructName = FString::Printf(TEXT("%s%s"), ExpectedPrefixCPP, *StructNameStripped);
			if (StructNameInScript != ExpectedStructName)
			{
				FError::Throwf(TEXT("Struct '%s' has an invalid Unreal prefix, expecting '%s'"), *StructNameInScript, *ExpectedStructName);
			}
		}
		else
		{
			const TCHAR* ExpectedPrefixCPP = StructsWithTPrefix.Contains(StructNameInScript) ? TEXT("T") : Struct->GetPrefixCPP();
			FString ExpectedStructName = FString::Printf(TEXT("%s%s"), ExpectedPrefixCPP, *StructNameInScript);
			FError::Throwf(TEXT("Struct '%s' is missing a valid Unreal prefix, expecting '%s'"), *StructNameInScript, *ExpectedStructName);
		}			
	}

	Struct->StructFlags = EStructFlags(Struct->StructFlags | StructFlags);

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	// Register the metadata
	AddMetaDataToClassData(Struct, MetaData);

	// Get opening brace.
	RequireSymbol( TEXT("{"), TEXT("'struct'") );

	// Members of structs have a default public access level in c++
	// Assume that, but restore the parser state once we finish parsing this struct
	TGuardValue<EAccessSpecifier> HoldFromClass(CurrentAccessSpecifier, ACCESS_Public);

	{
		FToken StructToken;
		StructToken.Struct = Struct;
		StructToken.ExportInfo = CustomExportText;

		// add this struct to the compiler's persistent tracking system
		ClassData->AddStruct(StructToken);
	}

	int32 SavedLineNumber = InputLine;

	// Clear comment before parsing body of the struct.
	

	// Parse all struct variables.
	FToken Token;
	while (1)
	{
		ClearComment();
		GetToken( Token );

		if (EAccessSpecifier AccessSpecifier = ParseAccessProtectionSpecifier(Token))
		{
			CurrentAccessSpecifier = AccessSpecifier;
		}
		else if (Token.Matches(TEXT("UPROPERTY"), ESearchCase::CaseSensitive))
		{
			CompileVariableDeclaration(AllClasses, Struct, EPropertyDeclarationStyle::UPROPERTY);
		}
		else if (Token.Matches(TEXT("UFUNCTION"), ESearchCase::CaseSensitive))
		{
			FError::Throwf(TEXT("USTRUCTs cannot contain UFUNCTIONs."));
		}
		else if (Token.Matches(TEXT("GENERATED_USTRUCT_BODY")))
		{
			// Match 'GENERATED_USTRUCT_BODY' '(' [StructName] ')'
			if (CurrentAccessSpecifier != ACCESS_Public)
			{
				FError::Throwf(TEXT("GENERATED_USTRUCT_BODY must be in the public scope of '%s', not private or protected."), *StructNameInScript);
			}

			if (Struct->StructMacroDeclaredLineNumber != INDEX_NONE)
			{
				FError::Throwf(TEXT("Multiple GENERATED_USTRUCT_BODY declarations found in '%s'"), *StructNameInScript);
			}

			Struct->StructMacroDeclaredLineNumber = InputLine;
			RequireSymbol(TEXT("("), TEXT("'struct'"));

			FToken DuplicateStructName;
			if (GetIdentifier(DuplicateStructName))
			{
				if (!DuplicateStructName.Matches(*StructNameInScript))
				{
					FError::Throwf(TEXT("The argument to GENERATED_USTRUCT_BODY must match the struct name '%s' if present.  However, the argument can be omitted entirely."), *StructNameInScript);
				}
			}

			RequireSymbol(TEXT(")"), TEXT("'struct'"));

			// Eat a semicolon if present (not required)
			SafeMatchSymbol(TEXT(";"));
		}
		else if ( Token.Matches(TEXT("#")) && MatchIdentifier(TEXT("ifdef")) )
		{
			PushCompilerDirective(ECompilerDirective::Insignificant);
		}
		else if ( Token.Matches(TEXT("#")) && MatchIdentifier(TEXT("ifndef")) )
		{
			PushCompilerDirective(ECompilerDirective::Insignificant);
		}
		else if (Token.Matches(TEXT("#")) && MatchIdentifier(TEXT("endif")))
		{
			if (CompilerDirectiveStack.Num() < 1)
			{
				FError::Throwf(TEXT("Unmatched '#endif' in class or global scope"));
			}
			CompilerDirectiveStack.Pop();
			// Do nothing and hope that the if code below worked out OK earlier
		}
		else if ( Token.Matches(TEXT("#")) && MatchIdentifier(TEXT("if")) )
		{
			//@TODO: This parsing should be combined with CompileDirective and probably happen much much higher up!
			bool bInvertConditional = MatchSymbol(TEXT("!"));
			bool bConsumeAsCppText = false;

			if (MatchIdentifier(TEXT("WITH_EDITORONLY_DATA")) )
			{
				if (bInvertConditional)
				{
					FError::Throwf(TEXT("Cannot use !WITH_EDITORONLY_DATA"));
				}

				PushCompilerDirective(ECompilerDirective::WithEditorOnlyData);
			}
			else if (MatchIdentifier(TEXT("WITH_EDITOR")) )
			{
				if (bInvertConditional)
				{
					FError::Throwf(TEXT("Cannot use !WITH_EDITOR"));
				}
				PushCompilerDirective(ECompilerDirective::WithEditor);
			}
			else if (MatchIdentifier(TEXT("CPP")))
			{
				bConsumeAsCppText = !bInvertConditional;
				PushCompilerDirective(ECompilerDirective::Insignificant);
				//@todo: UCREMOVAL, !CPP should be interpreted as noexport and you should not need the no export.
				// this applies to structs, enums, and everything else
			}
			else
			{
				FError::Throwf(TEXT("'struct': Unsupported preprocessor directive inside a struct.") );
			}

			if (bConsumeAsCppText)
			{
				// Skip over the text, it is not recorded or processed
				int32 nest = 1;
				while (nest > 0)
				{
					TCHAR ch = GetChar(1);

					if ( ch==0 )
					{
						FError::Throwf(TEXT("Unexpected end of struct definition %s"), *Struct->GetName());
					}
					else if ( ch=='{' || (ch=='#' && (PeekIdentifier(TEXT("if")) || PeekIdentifier(TEXT("ifdef")))) )
					{
						nest++;
					}
					else if ( ch=='}' || (ch=='#' && PeekIdentifier(TEXT("endif"))) )
					{
						nest--;
					}

					if (nest==0)
					{
						RequireIdentifier(TEXT("endif"),TEXT("'if'"));
					}
				}
			}
		}
		else
		{
			if ( !Token.Matches( TEXT("}") ) )
			{
				FToken DeclarationFirstToken = Token;
				if (!SkipDeclaration(Token))
				{
					FError::Throwf(TEXT("'struct': Unexpected '%s'"), DeclarationFirstToken.Identifier );
				}	
			}
			else
			{
				MatchSemi();
				break;
			}			
		}		
	}

	// Validation
	bool bStructBodyFound = Struct->StructMacroDeclaredLineNumber != INDEX_NONE;
	bool bExported        = !!(StructFlags & STRUCT_Native);
	if (!bStructBodyFound && bExported)
	{
		// Roll the line number back to the start of the struct body and error out
		InputLine = SavedLineNumber;
		FError::Throwf(TEXT("Expected a GENERATED_USTRUCT_BODY() at the start of struct"));
	}

	// Link the properties within the struct
	Struct->StaticLink(true);

	return Struct;
}

/*-----------------------------------------------------------------------------
	Retry management.
-----------------------------------------------------------------------------*/

/**
 * Remember the current compilation points, both in the source being
 * compiled and the object code being emitted.
 *
 * @param	Retry	[out] filled in with current compiler position information
 */
void FHeaderParser::InitScriptLocation( FScriptLocation& Retry )
{
	Retry.Input = Input;
	Retry.InputPos = InputPos;
	Retry.InputLine	= InputLine;
}

/**
 * Return to a previously-saved retry point.
 *
 * @param	Retry	the point to return to
 * @param	Binary	whether to modify the compiled bytecode
 * @param	bText	whether to modify the compiler's current location in the text
 */
void FHeaderParser::ReturnToLocation(const FScriptLocation& Retry, bool Binary, bool bText)
{
	if (bText)
	{
		Input = Retry.Input;
		InputPos = Retry.InputPos;
		InputLine = Retry.InputLine;
	}
}

/*-----------------------------------------------------------------------------
	Nest information.
-----------------------------------------------------------------------------*/

//
// Return the name for a nest type.
//
const TCHAR *FHeaderParser::NestTypeName( ENestType NestType )
{
	switch( NestType )
	{
		case NEST_GlobalScope:
			return TEXT("Global Scope");
		case NEST_Class:
			return TEXT("Class");
		case NEST_Interface:
			return TEXT("Interface");
		case NEST_FunctionDeclaration:
			return TEXT("Function");
		default:
			check(false);
			return TEXT("Unknown");
	}
}

// Checks to see if a particular kind of command is allowed on this nesting level.
bool FHeaderParser::IsAllowedInThisNesting(uint32 AllowFlags)
{
	return ((TopNest->Allow & AllowFlags) != 0);
}

//
// Make sure that a particular kind of command is allowed on this nesting level.
// If it's not, issues a compiler error referring to the token and the current
// nesting level.
//
void FHeaderParser::CheckAllow( const TCHAR* Thing, uint32 AllowFlags )
{
	if (!IsAllowedInThisNesting(AllowFlags))
	{
		if (TopNest->NestType == NEST_GlobalScope)
		{
			FError::Throwf(TEXT("%s is not allowed before the Class definition"), Thing );
		}
		else
		{
			FError::Throwf(TEXT("%s is not allowed here"), Thing );
		}
	}
}

//
// Check that a specified object is accessible from
// this object's scope.
//
void FHeaderParser::CheckInScope( UObject* Obj )
{
	if ( Obj != NULL )
	{
		UClass* CheckClass = (Cast<UClass>(Obj) != NULL) ? Cast<UClass>(Obj) : Obj->GetClass();

		if ( !AllowReferenceToClass(CheckClass) )
		{
			FError::Throwf(TEXT("Invalid reference to unparsed class: '%s'"), *CheckClass->GetPathName());
		}
	}
}

/**
 * Returns whether the specified class can be referenced from the class currently being compiled.
 *
 * @param	CheckClass	the class we want to reference
 *
 * @return	true if the specified class is an intrinsic type or if the class has successfully been parsed
 */
bool FHeaderParser::AllowReferenceToClass( UClass* CheckClass ) const
{
	check(CheckClass);

	return	(Class->GetOuter() == CheckClass->GetOuter())
		||	((CheckClass->ClassFlags&CLASS_Parsed) != 0)
		||	((CheckClass->ClassFlags&CLASS_Intrinsic) != 0);
}

/*-----------------------------------------------------------------------------
	Nest management.
-----------------------------------------------------------------------------*/

/**
 * Increase the nesting level, setting the new top nesting level to
 * the one specified.  If pushing a function or state and it overrides a similar
 * thing declared on a lower nesting level, verifies that the override is legal.
 *
 * @param	NestType	the new nesting type
 * @param	ThisName	name of the new nest
 * @param	InNode		@todo
 */
void FHeaderParser::PushNest( ENestType NestType, FName ThisName, UStruct* InNode )
{
	// Defaults.
	UStruct* PrevTopNode = TopNode;
	UStruct* PrevNode = NULL;
	uint32 PrevAllow = 0;

	UField* Existing = NULL;
	if (NestType == NEST_FunctionDeclaration)
	{
		for (TFieldIterator<UField> It(TopNode,EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->GetFName() == ThisName)
			{
				FError::Throwf(TEXT("'%s' conflicts with '%s'"), *ThisName.ToString(), *It->GetFullName() );
			}
		}
	}

	// Update pointer to top nesting level.
	TopNest					= &Nest[NestLevel++];
	TopNode					= NULL;
	TopNest->Node			= InNode;
	TopNest->NestType		= NestType;

	// Prevent overnesting.
	if( NestLevel >= MAX_NEST_LEVELS )
	{
		FError::Throwf(TEXT("Maximum nesting limit exceeded") );
	}

	// Inherit info from stack node above us.
	const bool bIsNewNode = (NestType == NEST_Class) || (NestType == NEST_Interface) || (NestType == NEST_FunctionDeclaration);
	if (NestLevel > 1)
	{
		if (bIsNewNode)
		{
			// Create a new stack node.
			if ((NestType == NEST_Class) || (NestType == NEST_Interface))
			{
				TopNest->Node = TopNode = Class;
			}
			else if (NestType == NEST_FunctionDeclaration)
			{
				UFunction* Function = new(PrevTopNode ? (UObject*)PrevTopNode : (UObject*)Class, ThisName, RF_Public) UFunction( FObjectInitializer(), NULL );
				TopNest->Node = TopNode = Function;
				Function->RepOffset = MAX_uint16;
				Function->ReturnValueOffset = MAX_uint16;
				Function->FirstPropertyToInit = NULL;
			}
		}
		else
		{
			// Use the existing stack node.
			TopNest->Node = TopNest[-1].Node;
			TopNode = TopNest->Node;
		}

		check(TopNode != NULL);
		PrevNode  = TopNest[-1].Node;
		PrevAllow = TopNest[-1].Allow;
	}

	// NestType specific logic.
	switch( NestType )
	{
		case NEST_GlobalScope:
			check(PrevNode==NULL);
			TopNest->Allow = ALLOW_Class | ALLOW_TypeDecl;
			check(PrevNode == NULL);
			TopNode = InNode;
			break;

		case NEST_Class:
			check(ThisName != NAME_None);
			TopNest->Allow = ALLOW_VarDecl | ALLOW_Function | ALLOW_TypeDecl;
			break;

		// only function declarations are allowed inside interface nesting level
		case NEST_Interface:
			check(ThisName != NAME_None);
			TopNest->Allow = ALLOW_Function | ALLOW_TypeDecl;
			break;

		case NEST_FunctionDeclaration:
			check(ThisName != NAME_None);
			check(PrevNode != NULL);
			TopNest->Allow = ALLOW_VarDecl;
			
			TopNode->Next = PrevNode->Children;
			PrevNode->Children = TopNode;
			
			break;

		default:
			FError::Throwf(TEXT("Internal error in PushNest, type %i"), (uint8)NestType );
			break;
	}
}

/**
 * Decrease the nesting level and handle any errors that result.
 *
 * @param	NestType	nesting type of the current node
 * @param	Descr		text to use in error message if any errors are encountered
 */
void FHeaderParser::PopNest( FClasses& AllClasses, ENestType NestType, const TCHAR* Descr )
{
	// Validate the nesting state.
	if (NestLevel <= 0)
	{
		FError::Throwf(TEXT("Unexpected '%s' at global scope"), Descr, NestTypeName(NestType) );
	}
	else if (NestType == NEST_GlobalScope)
	{
		NestType = TopNest->NestType;
	}
	else if (TopNest->NestType != NestType)
	{
		FError::Throwf(TEXT("Unexpected end of %s in '%s' block"), Descr, NestTypeName(TopNest->NestType) );
	}

	// Remember code position.
	if (NestType == NEST_FunctionDeclaration)
	{
		//@TODO: UCREMOVAL: Move this code to occur at delegate var declaration, and force delegates to be declared before variables that use them
		if (ClassData->ContainsDelegates())
		{
			UFunction* TopFunction = CastChecked<UFunction>(TopNode);

			// now validate all delegate variables declared in the class
			TMap<FName, UFunction*> DelegateCache;
			FixupDelegateProperties(AllClasses, TopFunction, TopFunction->GetOwnerClass(), DelegateCache);
		}
	}
	else if (NestType == NEST_Class)
	{
		// Todo - One drawback to doing this here is we don't get correct line numbers
		// for conflicting implementations.
		UClass* TopClass = CastChecked<UClass>(TopNode);

		// first, fixup all delegate properties with the delegate types they're supposed to be bound to
		if (ClassData->ContainsDelegates())
		{
			// now validate all delegate variables declared in the class
			TMap<FName,UFunction*> DelegateCache;
			FixupDelegateProperties(AllClasses, TopClass, TopClass, DelegateCache);
		}

		// Validate all the rep notify events here, to make sure they're implemented
		VerifyRepNotifyCallbacks(TopClass);

		// Iterate over all the interfaces we claim to implement
		for (auto& Impl : TopClass->Interfaces)
		{
			// And their super-classes
			for( UClass* Interface = Impl.Class; Interface; Interface = Interface->GetSuperClass() )
			{
				// If this interface is a common ancestor, skip it
				if (TopNode->IsChildOf(Interface))
					continue;

				// So iterate over all functions this interface declares
				for (auto InterfaceFunction : TFieldRange<UFunction>(Interface, EFieldIteratorFlags::ExcludeSuper))
				{
					bool Implemented = false;

					// And try to find one that matches
					for (UFunction* ClassFunction : TFieldRange<UFunction>(TopNode))
					{
						if (ClassFunction->GetFName() != InterfaceFunction->GetFName())
							continue;

						if ((InterfaceFunction->FunctionFlags & FUNC_Event) && !(ClassFunction->FunctionFlags & FUNC_Event))
							FError::Throwf(TEXT("Implementation of function '%s' must be declared as 'event' to match declaration in interface '%s'"), *ClassFunction->GetName(), *Interface->GetName());

						if ((InterfaceFunction->FunctionFlags & FUNC_Delegate) && !(ClassFunction->FunctionFlags & FUNC_Delegate))
							FError::Throwf(TEXT("Implementation of function '%s' must be declared as 'delegate' to match declaration in interface '%s'"), *ClassFunction->GetName(), *Interface->GetName());

						// Making sure all the parameters match up correctly
						Implemented = true;

						if (ClassFunction->NumParms != InterfaceFunction->NumParms)
							FError::Throwf(TEXT("Implementation of function '%s' conflicts with interface '%s' - different number of parameters (%i/%i)"), *InterfaceFunction->GetName(), *Interface->GetName(), ClassFunction->NumParms, InterfaceFunction->NumParms);

						int32 Count = 0;
						for( TFieldIterator<UProperty> It1(InterfaceFunction),It2(ClassFunction); Count<ClassFunction->NumParms; ++It1,++It2,Count++ )
						{
							if( !FPropertyBase(*It1).MatchesType(FPropertyBase(*It2), 1) )
							{
								if( It1->PropertyFlags & CPF_ReturnParm )
								{
									FError::Throwf(TEXT("Implementation of function '%s' conflicts only by return type with interface '%s'"), *InterfaceFunction->GetName(), *Interface->GetName() );
								}
								else
								{
									FError::Throwf(TEXT("Implementation of function '%s' conflicts with interface '%s' - parameter %i '%s'"), *InterfaceFunction->GetName(), *Interface->GetName(), Count, *It1->GetName() );
								}
							}
						}
					}

					// Delegate signature functions are simple stubs and aren't required to be implemented (they are not callable)
					if (InterfaceFunction->FunctionFlags & FUNC_Delegate)
					{
						Implemented = true;
					}

					// Verify that if this has blueprint-callable functions that are not implementable events, we've implemented them as a UFunction in the target class
					if( !Implemented 
						&& !Interface->HasMetaData(TEXT("CannotImplementInterfaceInBlueprint"))  // FBlueprintMetadata::MD_CannotImplementInterfaceInBlueprint
						&& InterfaceFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
						&& !InterfaceFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent) )
					{
						FError::Throwf(TEXT("Missing UFunction implementation of function '%s' from interface '%s'.  This function needs a UFUNCTION() declaration."), *InterfaceFunction->GetName(), *Interface->GetName());
					}
				}
			}
		}
	}
	else if ( NestType == NEST_Interface )
	{
		if (ClassData->ContainsDelegates())
		{
			TMap<FName,UFunction*> DelegateCache;
			FixupDelegateProperties(AllClasses, TopNode, ExactCast<UClass>(TopNode), DelegateCache);
		}
	}
	else
	{
		FError::Throwf(TEXT("Bad first pass NestType %i"), (uint8)NestType );
	}

	bool bLinkProps = true;
	if (NestType == NEST_Class)
	{
		UClass* TopClass = CastChecked<UClass>(TopNode);
		bLinkProps = !TopClass->HasAnyClassFlags(CLASS_Intrinsic);
	}
	TopNode->StaticLink(bLinkProps);

	// Pop the nesting level.
	NestType = TopNest->NestType;
	NestLevel--;
	TopNest--;
	TopNode	= TopNest->Node;
}

/**
 * Binds all delegate properties declared in ValidationScope the delegate functions specified in the variable declaration, verifying that the function is a valid delegate
 * within the current scope.  This must be done once the entire class has been parsed because instance delegate properties must be declared before the delegate declaration itself.
 *
 * @todo: this function will no longer be required once the post-parse fixup phase is added (TTPRO #13256)
 *
 * @param	ValidationScope		the scope to validate delegate properties for
 * @param	OwnerClass			the class currently being compiled.
 * @param	DelegateCache		cached map of delegates that have already been found; used for faster lookup.
 */
void FHeaderParser::FixupDelegateProperties( FClasses& AllClasses, UStruct* ValidationScope, UClass* OwnerClass, TMap<FName, UFunction*>& DelegateCache )
{
	check(ValidationScope);
	check(OwnerClass);

	for ( UField* Field = ValidationScope->Children; Field; Field = Field->Next )
	{
		UProperty* Property = Cast<UProperty>(Field);
		if ( Property != NULL )
		{
			UDelegateProperty* DelegateProperty = Cast<UDelegateProperty>(Property);
			UMulticastDelegateProperty* MulticastDelegateProperty = Cast<UMulticastDelegateProperty>(Property);
			if ( DelegateProperty == NULL && MulticastDelegateProperty == NULL )
			{
				// if this is an array property, see if the array's type is a delegate
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
				if ( ArrayProp != NULL )
				{
					DelegateProperty = Cast<UDelegateProperty>(ArrayProp->Inner);
					MulticastDelegateProperty = Cast<UMulticastDelegateProperty>(ArrayProp->Inner);
				}
			}
			if ( DelegateProperty != NULL || MulticastDelegateProperty != NULL )
			{
				// this UDelegateProperty corresponds to an actual delegate variable (i.e. delegate<SomeDelegate> Foo); we need to lookup the token data for
				// this property and verify that the delegate property's "type" is an actual delegate function
				FTokenData* DelegatePropertyToken = ClassData->FindTokenData(Property);
				check(DelegatePropertyToken);

				// attempt to find the delegate function in the map of functions we've already found
				UFunction* SourceDelegateFunction = DelegateCache.FindRef(DelegatePropertyToken->Token.DelegateName);
				if ( SourceDelegateFunction == NULL )
				{
					FString NameOfDelegateFunction = DelegatePropertyToken->Token.DelegateName.ToString() + FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX );
					if ( !NameOfDelegateFunction.Contains(TEXT(".")) )
					{
						// an unqualified delegate function name - search for a delegate function by this name within the current scope
						SourceDelegateFunction = Cast<UFunction>(FindField(OwnerClass, *NameOfDelegateFunction, true, UFunction::StaticClass(), NULL));
						if ( SourceDelegateFunction == NULL )
						{
							// convert this into a fully qualified path name for the error message.
							NameOfDelegateFunction = OwnerClass->GetName() + TEXT(".") + NameOfDelegateFunction;
						}
						else
						{
							// convert this into a fully qualified path name for the error message.
							NameOfDelegateFunction = SourceDelegateFunction->GetOwnerClass()->GetName() + TEXT(".") + NameOfDelegateFunction;
						}
					}
					else
					{
						FString DelegateClassName, DelegateName;
						NameOfDelegateFunction.Split(TEXT("."), &DelegateClassName, &DelegateName);

						// verify that we got a valid string for the class name
						if ( DelegateClassName.Len() == 0 )
						{
							UngetToken(DelegatePropertyToken->Token);
							FError::Throwf(TEXT("Invalid scope specified in delegate property function reference: '%s'"), *NameOfDelegateFunction);
						}

						// verify that we got a valid string for the name of the function
						if ( DelegateName.Len() == 0 )
						{
							UngetToken(DelegatePropertyToken->Token);
							FError::Throwf(TEXT("Invalid delegate name specified in delegate property function reference '%s'"), *NameOfDelegateFunction);
						}

						// make sure that the class that contains the delegate can be referenced here
						UClass* DelegateOwnerClass = AllClasses.FindScriptClassOrThrow(DelegateClassName);
						CheckInScope(DelegateOwnerClass);
						SourceDelegateFunction = Cast<UFunction>(FindField(DelegateOwnerClass, *DelegateName, false, UFunction::StaticClass(), NULL));	
					}

					if ( SourceDelegateFunction == NULL )
					{
						UngetToken(DelegatePropertyToken->Token);
						FError::Throwf(TEXT("Failed to find delegate function '%s'"), *NameOfDelegateFunction);
					}
					else if ( (SourceDelegateFunction->FunctionFlags&FUNC_Delegate) == 0 )
					{
						UngetToken(DelegatePropertyToken->Token);
						FError::Throwf(TEXT("Only delegate functions can be used as the type for a delegate property; '%s' is not a delegate."), *NameOfDelegateFunction);
					}
				}

				// successfully found the delegate function that this delegate property corresponds to

				// save this into the delegate cache for faster lookup later
				DelegateCache.Add(DelegatePropertyToken->Token.DelegateName, SourceDelegateFunction);

				// bind it to the delegate property
				if( DelegateProperty != NULL )
				{
					if( !SourceDelegateFunction->HasAnyFunctionFlags( FUNC_MulticastDelegate ) )
					{
						DelegateProperty->SignatureFunction = DelegatePropertyToken->Token.Function = SourceDelegateFunction;
					}
					else
					{
						FError::Throwf(TEXT("Unable to declare a single-cast delegate property for a multi-cast delegate type '%s'.  Either add a 'multicast' qualifier to the property or change the delegate type to be single-cast as well."), *SourceDelegateFunction->GetName());
					}
				}
				else if( MulticastDelegateProperty != NULL )
				{
					if( SourceDelegateFunction->HasAnyFunctionFlags( FUNC_MulticastDelegate ) )
					{
						MulticastDelegateProperty->SignatureFunction = DelegatePropertyToken->Token.Function = SourceDelegateFunction;

						if(MulticastDelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable))
						{
							for (TFieldIterator<UProperty> PropIt(SourceDelegateFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
							{
								UProperty* FuncParam = *PropIt;
								if(FuncParam->HasAllPropertyFlags(CPF_OutParm) && !FuncParam->HasAllPropertyFlags(CPF_ConstParm))
								{
									FError::Throwf(TEXT("BlueprintAssignable delegates do not support non-const references at the moment. Function: %s Parameter: '%s'"), *SourceDelegateFunction->GetName(), *FuncParam->GetName());
								}
							}
						}

					}
					else
					{
						FError::Throwf(TEXT("Unable to declare a multi-cast delegate property for a single-cast delegate type '%s'.  Either remove the 'multicast' qualifier from the property or change the delegate type to be 'multicast' as well."), *SourceDelegateFunction->GetName());
					}
				}
			}
		}
		else
		{
			// if this is a state, function, or script struct, it might have its own delegate properties which need to be validated
			UStruct* InternalStruct = Cast<UStruct>(Field);
			if ( InternalStruct != NULL )
			{
				FixupDelegateProperties(AllClasses, InternalStruct, OwnerClass, DelegateCache);
			}
		}
	}
}

/**
 * Verifies that all specified class's UProperties with CFG_RepNotify have valid callback targets with no parameters nor return values
 *
 * @param	TargetClass			class to verify rep notify properties for
 */
void FHeaderParser::VerifyRepNotifyCallbacks( UClass* TargetClass )
{
	// Iterate over all properties, looking for those flagged as CPF_RepNotify
	for ( UField* Field = TargetClass->Children; Field; Field = Field->Next )
	{
		UProperty* Prop = Cast<UProperty>(Field);
		if( Prop && (Prop->GetPropertyFlags() & CPF_RepNotify) )
		{
			FTokenData* PropertyToken = ClassData->FindTokenData(Prop);
			check(PropertyToken);

			// Search through this class and its superclasses looking for the specified callback
			UFunction* TargetFunc = NULL;
			UClass* SearchClass = TargetClass;
			while( SearchClass && !TargetFunc )
			{
				// Since the function map is not valid yet, we have to iterate over the fields to look for the function
				for( UField* TestField = SearchClass->Children; TestField; TestField = TestField->Next )
				{
					UFunction* TestFunc = Cast<UFunction>(TestField);
					if( TestFunc && TestFunc->GetFName() == Prop->RepNotifyFunc )
					{
						TargetFunc = TestFunc;
						break;
					}
				}
				SearchClass = SearchClass->GetSuperClass();
			}

			if( TargetFunc )
			{
				if (TargetFunc->GetReturnProperty())
				{
					UngetToken(PropertyToken->Token);
					FError::Throwf(TEXT("Replication notification function %s must not have return values"), *Prop->RepNotifyFunc.ToString());
					break;
				}
				
				bool IsArrayProperty = ( Prop->ArrayDim > 1 || Cast<UArrayProperty>(Prop) );
				int32 MaxParms = IsArrayProperty ? 2 : 1;

				if ( TargetFunc->NumParms > MaxParms)
				{
					UngetToken(PropertyToken->Token);
					FError::Throwf(TEXT("Replication notification function %s has too many parameters"), *Prop->RepNotifyFunc.ToString());
					break;
				}
				
				TFieldIterator<UProperty> Parm(TargetFunc);
				if ( TargetFunc->NumParms >= 1 && Parm)
				{
					// First parameter is always the old value:
					if ( Parm->GetClass() != Prop->GetClass() )
					{
						UngetToken(PropertyToken->Token);
						FError::Throwf(TEXT("Replication notification function %s has invalid parameter for property $%s. First (optional) parameter must be a const reference of the same property type."), *Prop->RepNotifyFunc.ToString(), *Prop->GetName());
						break;
					}

					++Parm;
				}

				if ( TargetFunc->NumParms >= 2 && Parm)
				{
					// A 2nd parmaeter for arrays can be specified as a const TArray<uint8>&. This is a list of element indices that have changed
					UArrayProperty *ArrayProp = Cast<UArrayProperty>(*Parm);
					if (!(ArrayProp && Cast<UByteProperty>(ArrayProp->Inner)) || !(Parm->GetPropertyFlags() & CPF_ConstParm) || !(Parm->GetPropertyFlags() & CPF_ReferenceParm))
					{
						UngetToken(PropertyToken->Token);
						FError::Throwf(TEXT("Replication notification function %s (optional) parameter must be of type 'const TArray<uint8>&'"), *Prop->RepNotifyFunc.ToString());
						break;
					}
				}
			}
			else
			{
				// Couldn't find a valid function...
				UngetToken(PropertyToken->Token);
				FError::Throwf(TEXT("Replication notification function %s not found"), *Prop->RepNotifyFunc.ToString() );
			}
		}
	}
}


/*-----------------------------------------------------------------------------
	Compiler directives.
-----------------------------------------------------------------------------*/

//
// Process a compiler directive.
//
void FHeaderParser::CompileDirective(UClass* Class)
{
	FToken Directive;

	int32 LineAtStartOfDirective = InputLine;
	// Define directive are skipped but they can be multiline.
	bool bDefineDirective = false;

	if (!GetIdentifier(Directive))
	{
		FError::Throwf(TEXT("Missing compiler directive after '#'") );
	}
	else if (Directive.Matches(TEXT("Error")))
	{
		FError::Throwf(TEXT("#Error directive encountered") );
	}
	else if (Directive.Matches(TEXT("pragma")))
	{
		// Ignore all pragmas
	}
	else if (Directive.Matches(TEXT("linenumber")))
	{
		FToken Number;
		if (!GetToken(Number) || (Number.TokenType != TOKEN_Const) || (Number.Type != CPT_Int))
		{
			FError::Throwf(TEXT("Missing line number in line number directive"));
		}

		int32 newInputLine;
		if ( Number.GetConstInt(newInputLine) )
		{
			InputLine = newInputLine;
		}
	}
	else if (Directive.Matches(TEXT("include")))
	{
		FString ExpectedHeaderName = FString::Printf(TEXT("%s.generated.h"), *GClassHeaderNameWithNoPathMap[Class]);
		FToken IncludeName;
		if (GetToken(IncludeName) && (IncludeName.TokenType == TOKEN_Const) && (IncludeName.Type == CPT_String))
		{
			if (FCString::Stricmp(IncludeName.String, *ExpectedHeaderName) == 0)
			{
				bSpottedAutogeneratedHeaderInclude = true;
			}
			else
			{
				// Handle #include directive just like dependson. The included header name is stored with
				// the extension so that we later know where this dependson came from.
				GClassDependentOnMap.FindOrAdd(Class)->Add(*FPaths::GetCleanFilename(IncludeName.String));
			}
		}
	}
	else if (Directive.Matches(TEXT("if")))
	{
		// Eat the ! if present
		bool bNotDefined = MatchSymbol(TEXT("!"));

		FToken Define;
		if (!GetIdentifier(Define))
		{
			FError::Throwf(TEXT("Missing define name '#if'") );
		}

		if ( Define.Matches(TEXT("WITH_EDITORONLY_DATA")) )
		{
			PushCompilerDirective(ECompilerDirective::WithEditorOnlyData);
		}
		else if ( Define.Matches(TEXT("WITH_EDITOR")) )
		{
			PushCompilerDirective(ECompilerDirective::WithEditor);
		}
		else if (Define.Matches(TEXT("CPP")) && bNotDefined)
		{
			PushCompilerDirective(ECompilerDirective::Insignificant);
		}
		else
		{
			FError::Throwf(TEXT("Unknown define '#if %s' in class or global scope"), Define.Identifier);
		}
	}
	else if (Directive.Matches(TEXT("endif")))
	{
		if (CompilerDirectiveStack.Num() < 1)
		{
			FError::Throwf(TEXT("Unmatched '#endif' in class or global scope"));
		}
		CompilerDirectiveStack.Pop();
	}
	else if (Directive.Matches(TEXT("define")))
	{
		// Ignore the define directive (can be multiline).
		bDefineDirective = true;
	}
	else if (Directive.Matches(TEXT("ifdef")) || Directive.Matches(TEXT("ifndef")))
	{
		PushCompilerDirective(ECompilerDirective::Insignificant);
	}
	else if (Directive.Matches(TEXT("undef")) || Directive.Matches(TEXT("else")))
	{
		// Ignore. UHT can only handle #if directive
	}
	else
	{
		FError::Throwf(TEXT("Unrecognized compiler directive %s"), Directive.Identifier );
	}

	// Skip to end of line (or end of multiline #define).
	if (LineAtStartOfDirective == InputLine)
	{
		TCHAR LastCharacter = '\0';
		TCHAR c;
		do
		{			
			while ( !IsEOL( c=GetChar() ) )
			{
				LastCharacter = c;
			}
		} 
		// Continue until the entire multiline directive has been skipped.
		while (LastCharacter == '\\' && bDefineDirective);

		if (c == 0)
		{
			UngetChar();
		}
	}
}

/*-----------------------------------------------------------------------------
	Variable declaration parser.
-----------------------------------------------------------------------------*/

bool FHeaderParser::GetVarType
(
	FClasses&                       AllClasses,
	UStruct*                        Scope,
	FPropertyBase&                  VarProperty,
	EObjectFlags&                   ObjectFlags,
	uint64                          Disallow,
	const TCHAR*                    Thing,
	FToken*                         OuterPropertyType,
	EPropertyDeclarationStyle::Type PropertyDeclarationStyle,
	EVariableCategory::Type         VariableCategory,
	FIndexRange*                    ParsedVarIndexRange
)
{
	check(Scope);

	FName RepCallbackName      = FName(NAME_None);
	bool  bIsMulticastDelegate = false;
	bool  bIsWeak              = false;
	bool  bIsLazy              = false;
	bool  bIsAsset             = false;
	bool  bWeakIsAuto          = false;

	// Get flags.
	ObjectFlags = RF_Public;
	uint64 Flags = 0;
	// force members to be 'blueprint read only' if in a const class
	if (VariableCategory == EVariableCategory::Member && (Cast<UClass>(Scope) != NULL) && (((UClass*)Scope)->ClassFlags & CLASS_Const))
	{
		Flags |= CPF_BlueprintReadOnly;
	}
	uint32 ExportFlags = PROPEXPORT_Public;

	// Build up a list of specifiers
	TArray<FPropertySpecifier> SpecifiersFound;

	TMap<FName, FString> MetaDataFromNewStyle;

	bool bIsParamList = VariableCategory != EVariableCategory::Member && MatchIdentifier(TEXT("UPARAM"));

	// No specifiers are allowed inside a TArray
	if( (OuterPropertyType == NULL) || !OuterPropertyType->Matches(TEXT("TArray")) )
	{
		// New-style UPROPERTY() syntax 
		if (PropertyDeclarationStyle == EPropertyDeclarationStyle::UPROPERTY || bIsParamList)
		{
			ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Variable"), MetaDataFromNewStyle);
		}
		else
		{
			// Legacy variable support
			for ( ; ; )
			{
				FToken Specifier;
				GetToken(Specifier);

				if (IsValidVariableSpecifier(Specifier))
				{
					// Record the specifier
					FPropertySpecifier* NewPair = new (SpecifiersFound) FPropertySpecifier();
					NewPair->Key = Specifier.Identifier;

					// Look for a value for this specifier
					ReadOptionalCommaSeparatedListInParens(NewPair->Values, TEXT("'Variable declaration specifier'"));
				}
				else
				{
					UngetToken(Specifier);
					break;
				}
			}
		}
	}

	if (CompilerDirectiveStack.Num() > 0 && (CompilerDirectiveStack.Last()&ECompilerDirective::WithEditorOnlyData)!=0)
	{
		Flags |= CPF_EditorOnly;
	}

	// Store the start and end positions of the parsed type
	if (ParsedVarIndexRange)
	{
		ParsedVarIndexRange->StartIndex = InputPos;
	}

	// Process the list of specifiers
	bool bSeenEditSpecifier = false;
	bool bSeenBlueprintEditSpecifier = false;
	for (TArray<FPropertySpecifier>::TIterator SpecifierIt(SpecifiersFound); SpecifierIt; ++SpecifierIt)
	{
		const FString& Specifier = SpecifierIt->Key;

		if (VariableCategory == EVariableCategory::Member)
		{
			if (Specifier == TEXT("EditAnywhere"))
			{
				if (bSeenEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier);
				}
				Flags |= CPF_Edit;
				bSeenEditSpecifier = true;
			}
			else if (Specifier == TEXT("EditInstanceOnly"))
			{
				if (bSeenEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier);
				}
				Flags |= CPF_Edit|CPF_DisableEditOnTemplate;
				bSeenEditSpecifier = true;
			}
			else if (Specifier == TEXT("EditDefaultsOnly")) 
			{
				if (bSeenEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier);
				}
				Flags |= CPF_Edit|CPF_DisableEditOnInstance;
				bSeenEditSpecifier = true;
			}
			else if (Specifier == TEXT("VisibleAnywhere")) 
			{
				if (bSeenEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier);
				}
				Flags |= CPF_Edit|CPF_EditConst;
				bSeenEditSpecifier = true;
			}
			else if (Specifier == TEXT("VisibleInstanceOnly"))
			{
				if (bSeenEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier);
				}
				Flags |= CPF_Edit|CPF_EditConst|CPF_DisableEditOnTemplate;
				bSeenEditSpecifier = true;
			}
			else if (Specifier == TEXT("VisibleDefaultsOnly"))
			{
				if (bSeenEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier);
				}
				Flags |= CPF_Edit|CPF_EditConst|CPF_DisableEditOnInstance;
				bSeenEditSpecifier = true;
			}
			else if (Specifier == TEXT("BlueprintReadWrite"))
			{
				if (bSeenBlueprintEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one Blueprint read/write specifier (%s), only one is allowed"), *Specifier);
				}

				const FString* PrivateAccessMD = MetaDataFromNewStyle.Find(TEXT("AllowPrivateAccess"));  // FBlueprintMetadata::MD_AllowPrivateAccess
				const bool bAllowPrivateAccess = PrivateAccessMD ? (*PrivateAccessMD == TEXT("true")) : false;
				if ((CurrentAccessSpecifier == ACCESS_Private) && !bAllowPrivateAccess)
				{
					FError::Throwf(TEXT("BlueprintReadWrite should not be used on private members"));
				}
				Flags |= CPF_BlueprintVisible;
				bSeenBlueprintEditSpecifier = true;
			}
			else if (Specifier == TEXT("BlueprintReadOnly"))
			{
				if (bSeenBlueprintEditSpecifier)
				{
					FError::Throwf(TEXT("Found more than one Blueprint read/write specifier (%s), only one is allowed"), *Specifier);
				}

				const FString* PrivateAccessMD = MetaDataFromNewStyle.Find(TEXT("AllowPrivateAccess"));  // FBlueprintMetadata::MD_AllowPrivateAccess
				const bool bAllowPrivateAccess = PrivateAccessMD ? (*PrivateAccessMD == TEXT("true")) : false;
				if ((CurrentAccessSpecifier == ACCESS_Private) && !bAllowPrivateAccess)
				{
					FError::Throwf(TEXT("BlueprintReadOnly should not be used on private members"));
				}
				Flags |= CPF_BlueprintVisible|CPF_BlueprintReadOnly;
				bSeenBlueprintEditSpecifier = true;
			}
			else if (Specifier == TEXT("Config"))
			{
				Flags |= CPF_Config;
			}
			else if (Specifier == TEXT("GlobalConfig"))
			{
				Flags |= CPF_GlobalConfig | CPF_Config;
			}
			else if (Specifier == TEXT("Localized"))
			{
				Flags |= CPF_Localized | CPF_BlueprintReadOnly;
			}
			else if (Specifier == TEXT("Transient"))
			{
				Flags |= CPF_Transient;
			}
			else if (Specifier == TEXT("DuplicateTransient"))
			{
				Flags |= CPF_DuplicateTransient;
			}
			else if (Specifier == TEXT("TextExportTransient"))
			{
				Flags |= CPF_TextExportTransient;
			}
			else if (Specifier == TEXT("NonPIETransient"))
			{
				UE_LOG(LogCompile, Warning, TEXT("NonPIETransient is deprecated - NonPIEDuplicateTransient should be used instead"));
				Flags |= CPF_NonPIEDuplicateTransient;
			}
			else if (Specifier == TEXT("NonPIEDuplicateTransient"))
			{
				Flags |= CPF_NonPIEDuplicateTransient;
			}
			else if (Specifier == TEXT("Export"))
			{
				Flags |= CPF_ExportObject;
			}
			else if (Specifier == TEXT("EditInline"))
			{
				FError::Throwf(TEXT("EditInline is deprecated. Remove it, or use Instanced instead."));
			}
			else if (Specifier == TEXT("NoClear"))
			{
				Flags |= CPF_NoClear;
			}
			else if (Specifier == TEXT("EditFixedSize"))
			{
				Flags |= CPF_EditFixedSize;
			}
			else if (Specifier == TEXT("Replicated") || Specifier == TEXT("ReplicatedUsing"))
			{
				if (Scope->GetClass() != UScriptStruct::StaticClass())
				{
					Flags |= CPF_Net;

					// See if we've specified a rep notification function
					if (Specifier == TEXT("ReplicatedUsing"))
					{
						RepCallbackName = FName(*RequireExactlyOneSpecifierValue(*SpecifierIt));
						Flags |= CPF_RepNotify;
					}
				}
				else
				{
					FError::Throwf(TEXT("Struct members cannot be replicated"));
				}
			}
			else if (Specifier == TEXT("NotReplicated"))
			{
				if (Scope->GetClass() == UScriptStruct::StaticClass())
				{
					Flags |= CPF_RepSkip;
				}
				else
				{
					FError::Throwf(TEXT("Only Struct members can be marked NotReplicated"));
				}
			}
			else if (Specifier == TEXT("RepRetry"))
			{
				Flags |= CPF_RepRetry;
			}
			else if (Specifier == TEXT("Interp"))
			{
				Flags |= CPF_Edit;
				Flags |= CPF_BlueprintVisible;
				Flags |= CPF_Interp;
			}
			else if (Specifier == TEXT("NonTransactional"))
			{
				Flags |= CPF_NonTransactional;
			}
			else if (Specifier == TEXT("Instanced"))
			{
				Flags |= CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference;
				AddEditInlineMetaData(MetaDataFromNewStyle);
			}
			else if (Specifier == TEXT("BlueprintAssignable"))
			{
				Flags |= CPF_BlueprintAssignable;
			}
			else if (Specifier == TEXT("BlueprintCallable"))
			{
				Flags |= CPF_BlueprintCallable;
			}
			else if (Specifier == TEXT("BlueprintAuthorityOnly"))
			{
				Flags |= CPF_BlueprintAuthorityOnly;
			}
			else if (Specifier == TEXT("AssetRegistrySearchable"))
			{
				Flags |= CPF_AssetRegistrySearchable;
			}
			else if (Specifier == TEXT("SimpleDisplay"))
			{
				Flags |= CPF_SimpleDisplay;
			}
			else if (Specifier == TEXT("AdvancedDisplay"))
			{
				Flags |= CPF_AdvancedDisplay;
			}
			else if (Specifier == TEXT("SaveGame"))
			{
				Flags |= CPF_SaveGame;
			}
			else
			{
				FError::Throwf(TEXT("Unknown variable specifier '%s'"), *Specifier);
			}
		}
		else
		{
			if (Specifier == TEXT("Const"))
			{
				Flags |= CPF_ConstParm;
			}
			else if (Specifier == TEXT("Ref"))
			{
				Flags |= CPF_OutParm | CPF_ReferenceParm;
			}
			else if (Specifier == TEXT("NotReplicated"))
			{
				if (VariableCategory == EVariableCategory::ReplicatedParameter)
				{
					VariableCategory = EVariableCategory::RegularParameter;
					Flags |= CPF_RepSkip;
				}
				else
				{
					FError::Throwf(TEXT("Only parameters in service request functions can be marked NotReplicated"));
				}
			}
			else
			{
				FError::Throwf(TEXT("Unknown variable specifier '%s'"), *Specifier);
			}
		}
	}

	{
		const FString* ExposeOnSpawnStr = MetaDataFromNewStyle.Find(TEXT("ExposeOnSpawn"));
		const bool bExposeOnSpawn = (NULL != ExposeOnSpawnStr);
		if (bExposeOnSpawn)
		{
			if (0 != (CPF_DisableEditOnInstance & Flags))
			{
				UE_LOG(LogCompile, Warning, TEXT("Property cannot have 'DisableEditOnInstance' or 'BlueprintReadOnly' and 'ExposeOnSpawn' flags"));
			}
			if (0 == (CPF_BlueprintVisible & Flags))
			{
				UE_LOG(LogCompile, Warning, TEXT("Property cannot have 'ExposeOnSpawn' with 'BlueprintVisible' flag."));
			}
			Flags |= CPF_ExposeOnSpawn;
		}
	}

	if (CurrentAccessSpecifier == ACCESS_Public || VariableCategory != EVariableCategory::Member)
	{
		ObjectFlags |= RF_Public;
		Flags       &= ~CPF_Protected;
		ExportFlags |= PROPEXPORT_Public;
		ExportFlags &= ~(PROPEXPORT_Private|PROPEXPORT_Protected);
	}
	else if (CurrentAccessSpecifier == ACCESS_Protected)
	{
		ObjectFlags |= RF_Public;
		Flags       |= CPF_Protected;
		ExportFlags |= PROPEXPORT_Protected;
		ExportFlags &= ~(PROPEXPORT_Public|PROPEXPORT_Private);
	}
	else if (CurrentAccessSpecifier == ACCESS_Private)
	{
		ObjectFlags &= ~RF_Public;
		Flags       &= ~CPF_Protected;
		ExportFlags |= PROPEXPORT_Private;
		ExportFlags &= ~(PROPEXPORT_Public|PROPEXPORT_Protected);
	}
	else
	{
		FError::Throwf(TEXT("Unknown access level"));
	}

	// Get variable type.
	bool bUnconsumedStructKeyword = false;
	bool bUnconsumedClassKeyword  = false;
	bool bUnconsumedEnumKeyword   = false;
	bool bUnconsumedConstKeyword  = false;

	if (MatchIdentifier(TEXT("const")))
	{
		//@TODO: UCREMOVAL: Should use this to set the new (currently non-existent) CPF_Const flag appropriately!
		bUnconsumedConstKeyword = true;
	}

	if (MatchIdentifier(TEXT("mutable")))
	{
		//@TODO: Should flag as settable from a const context, but this is at least good enough to allow use for C++ land
	}

	if (MatchIdentifier(TEXT("struct")))
	{
		bUnconsumedStructKeyword = true;
	}
	else if (MatchIdentifier(TEXT("class")))
	{
		bUnconsumedClassKeyword = true;
	}
	else if (MatchIdentifier(TEXT("enum")))
	{
		if (VariableCategory == EVariableCategory::Member)
		{
			FError::Throwf(TEXT("%s: Cannot declare enum at variable declaration"), Thing );
		}

		bUnconsumedEnumKeyword = true;
	}

	//
	FToken VarType;
	if ( !GetIdentifier(VarType,1) )
	{
		if ( !Thing )
		{
			check(!MetaDataFromNewStyle.Num());
			return 0;
		}
		FError::Throwf(TEXT("%s: Missing variable type"), Thing );
	}

	if ( VarType.Matches(TEXT("int8")) )
	{
		VarProperty = FPropertyBase(CPT_Int8);
	}
	else if ( VarType.Matches(TEXT("int16")) )
	{
		VarProperty = FPropertyBase(CPT_Int16);
	}
	else if ( VarType.Matches(TEXT("int32")) )
	{
		VarProperty = FPropertyBase(CPT_Int);
	}
	else if ( VarType.Matches(TEXT("int64")) )
	{
		VarProperty = FPropertyBase(CPT_Int64);
	}
	else if ( VarType.Matches(TEXT("uint32")) && IsBitfieldProperty() )
	{
		// 32-bit bitfield (bool) type, treat it like 8 bit type
		VarProperty = FPropertyBase(CPT_Bool8);
	}
	else if ( VarType.Matches(TEXT("uint16")) && IsBitfieldProperty() )
	{
		// 16-bit bitfield (bool) type, treat it like 8 bit type.
		VarProperty = FPropertyBase(CPT_Bool8);
	}
	else if ( VarType.Matches(TEXT("uint8")) && IsBitfieldProperty() )
	{
		// 8-bit bitfield (bool) type
		VarProperty = FPropertyBase(CPT_Bool8);
	}
	else if ( VarType.Matches(TEXT("bool")) )
	{
		if (IsBitfieldProperty())
		{
			FError::Throwf(TEXT("bool bitfields are not supported."));
		}
		// C++ bool type
		VarProperty = FPropertyBase(CPT_Bool);
	}
	else if ( VarType.Matches(TEXT("uint8")) )
	{
		// Intrinsic Byte type.
		VarProperty = FPropertyBase(CPT_Byte);
	}
	else if ( VarType.Matches(TEXT("uint16")) )
	{
		VarProperty = FPropertyBase(CPT_UInt16);
	}
	else if ( VarType.Matches(TEXT("uint32")) )
	{
		VarProperty = FPropertyBase(CPT_UInt32);
	}
	else if ( VarType.Matches(TEXT("uint64")) )
	{
		VarProperty = FPropertyBase(CPT_UInt64);
	}
	else if ( VarType.Matches(TEXT("float")) )
	{
		// Intrinsic single precision floating point type.
		VarProperty = FPropertyBase(CPT_Float);
	}
	else if ( VarType.Matches(TEXT("double")) )
	{
		// Intrinsic double precision floating point type type.
		VarProperty = FPropertyBase(CPT_Double);
	}
	else if ( VarType.Matches(TEXT("FName")) )
	{
		// Intrinsic Name type.
		VarProperty = FPropertyBase(CPT_Name);
	}
	else if ( VarType.Matches(TEXT("TArray")) )
	{
		RequireSymbol( TEXT("<"), TEXT("'tarray'") );

		// GetVarType() clears the property flags of the array var, so use dummy 
		// flags when getting the inner property
		EObjectFlags InnerFlags;
		uint64 OriginalVarTypeFlags = VarType.PropertyFlags;
		VarType.PropertyFlags |= Flags;

		GetVarType( AllClasses, Scope, VarProperty, InnerFlags, Disallow, TEXT("'tarray'"), &VarType, EPropertyDeclarationStyle::None, VariableCategory );
		if (VarProperty.ArrayType != EArrayType::None)
		{
			FError::Throwf(TEXT("Arrays within arrays not supported.") );
		}

		if (bIsLazy || bIsWeak || bIsMulticastDelegate)
		{
			FError::Throwf(TEXT("Object reference and delegate flags must be specified inside array <>."));
		}

		OriginalVarTypeFlags |= VarProperty.PropertyFlags & (CPF_ContainsInstancedReference | CPF_InstancedReference); // propagate these to the array, we will fix them later
		VarType.PropertyFlags = OriginalVarTypeFlags;
		VarProperty.ArrayType = EArrayType::Dynamic;
		RequireSymbol( TEXT(">"), TEXT("'tarray'"), ESymbolParseOption::CloseTemplateBracket );
	}
	else if ( VarType.Matches(TEXT("FString")) )
	{
		VarProperty = FPropertyBase(CPT_String);

		if (VariableCategory != EVariableCategory::Member)
		{
			if (MatchSymbol(TEXT("&")))
			{
				if (Flags & CPF_ConstParm)
				{
					// 'const FString& Foo' came from 'FString' in .uc, no flags
					Flags &= ~CPF_ConstParm;

					// We record here that we encountered a const reference, because we need to remove that information from flags for code generation purposes.
					VarProperty.RefQualifier = ERefQualifier::ConstRef;
				}
				else
				{
					// 'FString& Foo' came from 'out FString' in .uc
					Flags |= CPF_OutParm;

					// And we record here that we encountered a non-const reference here too.
					VarProperty.RefQualifier = ERefQualifier::NonConstRef;
				}
			}
		}
	}
	else if ( VarType.Matches(TEXT("Text") ) )
	{
		FError::Throwf(TEXT("%s' is missing a prefix, expecting 'FText'"), VarType.Identifier);
	}
	else if ( VarType.Matches(TEXT("FText") ) )
	{
		VarProperty = FPropertyBase(CPT_Text);
	}
	else if (VarType.Matches(TEXT("TEnumAsByte")))
	{
		RequireSymbol(TEXT("<"), VarType.Identifier);

		// Eat the forward declaration enum text if present
		MatchIdentifier(TEXT("enum"));

		bool bFoundEnum = false;

		FToken InnerEnumType;
		if (GetIdentifier(InnerEnumType, true))
		{
			if (UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, InnerEnumType.Identifier))
			{
				// In-scope enumeration.
				VarProperty = FPropertyBase(Enum, CPT_Byte);
				bFoundEnum  = true;
			}
		}

		// Try to handle namespaced enums
		// Note: We do not verify the scoped part is correct, and trust in the C++ compiler to catch that sort of mistake
		if (MatchSymbol(TEXT("::")))
		{
			FToken ScopedTrueEnumName;
			if (!GetIdentifier(ScopedTrueEnumName, true))
			{
				FError::Throwf(TEXT("Expected a namespace scoped enum name.") );
			}
		}

		if (!bFoundEnum)
		{
			FError::Throwf(TEXT("Expected the name of a previously defined enum"));
		}

		RequireSymbol(TEXT(">"), VarType.Identifier, ESymbolParseOption::CloseTemplateBracket);
	}
	else if (UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, VarType.Identifier))
	{
		EPropertyType UnderlyingType = CPT_Byte;

		if (VariableCategory == EVariableCategory::Member)
		{
			auto* UnderlyingType = GEnumUnderlyingTypes.Find(Enum);
			if (!UnderlyingType || *UnderlyingType != CPT_Byte)
			{
				FError::Throwf(TEXT("You cannot use the raw enum name as a type for member variables, instead use TEnumAsByte or a C++11 enum class with an explicit underlying type (currently only uint8 supported)."), *Enum->CppType);
			}
		}

		// Try to handle namespaced enums
		// Note: We do not verify the scoped part is correct, and trust in the C++ compiler to catch that sort of mistake
		if (MatchSymbol(TEXT("::")))
		{
			FToken ScopedTrueEnumName;
			if (!GetIdentifier(ScopedTrueEnumName, true))
			{
				FError::Throwf(TEXT("Expected a namespace scoped enum name.") );
			}
		}

		// In-scope enumeration.
		VarProperty            = FPropertyBase(Enum, UnderlyingType);
		bUnconsumedEnumKeyword = false;
	}
	else
	{
		// Check for structs/classes
		bool bHandledType = false;
		FString IdentifierStripped = GetClassNameWithPrefixRemoved(VarType.Identifier);
		bool bStripped = false;
		UScriptStruct* Struct = FindObject<UScriptStruct>( ANY_PACKAGE, VarType.Identifier );
		if (!Struct)
		{
			Struct = FindObject<UScriptStruct>( ANY_PACKAGE, *IdentifierStripped );
			bStripped = true;
		}

		if (Struct)
		{
			if (bStripped)
			{
				const TCHAR* PrefixCPP = StructsWithTPrefix.Contains(IdentifierStripped) ? TEXT("T") : Struct->GetPrefixCPP();
				FString ExpectedStructName = FString::Printf(TEXT("%s%s"), PrefixCPP, *Struct->GetName() );
				if( FString(VarType.Identifier) != ExpectedStructName )
				{
					FError::Throwf( TEXT("Struct '%s' is missing or has an incorrect prefix, expecting '%s'"), VarType.Identifier, *ExpectedStructName );
				}
			}
			else if( !StructsWithNoPrefix.Contains(VarType.Identifier) )
			{
				const TCHAR* PrefixCPP = StructsWithTPrefix.Contains(VarType.Identifier) ? TEXT("T") : Struct->GetPrefixCPP();
				FError::Throwf(TEXT("Struct '%s' is missing a prefix, expecting '%s'"), VarType.Identifier, *FString::Printf(TEXT("%s%s"), PrefixCPP, *Struct->GetName()) );
			}

			bHandledType = true;

			VarProperty = FPropertyBase( Struct );
			if((Struct->StructFlags & STRUCT_HasInstancedReference) && !(Disallow & CPF_ContainsInstancedReference))
			{
				Flags |= CPF_ContainsInstancedReference;
			}
			// Struct keyword in front of a struct is legal, we 'consume' it
			bUnconsumedStructKeyword = false;
		}
		else if (UScriptStruct* Struct = FindObject<UScriptStruct>( ANY_PACKAGE, *IdentifierStripped ))
		{
			bHandledType = true;


			// Struct keyword in front of a struct is legal, we 'consume' it
			bUnconsumedStructKeyword = false;
		}
		else if ( UFunction* DelegateFunc = Cast<UFunction>(FindField(Scope, *(IdentifierStripped + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX))) )
		{
			bHandledType = true;

			VarProperty = FPropertyBase( DelegateFunc->HasAnyFunctionFlags(FUNC_MulticastDelegate) ? CPT_MulticastDelegate : CPT_Delegate);
			VarProperty.DelegateName = *IdentifierStripped;

			if (!(Disallow & CPF_InstancedReference))
			{
				Flags |= CPF_InstancedReference;
			}
		}
		else
		{
			// An object reference of some type (maybe a restricted class?)
			UClass* TempClass = NULL;
			bool bExpectStar = true;

			const bool bIsLazyPtrTemplate = VarType.Matches(TEXT("TLazyObjectPtr"));
			const bool bIsAssetPtrTemplate = VarType.Matches(TEXT("TAssetPtr"));
			const bool bIsAssetClassTemplate = VarType.Matches(TEXT("TAssetSubclassOf"));
			const bool bIsWeakPtrTemplate = VarType.Matches(TEXT("TWeakObjectPtr"));
			const bool bIsAutoweakPtrTemplate = VarType.Matches(TEXT("TAutoWeakObjectPtr"));
			const bool bIsScriptInterfaceWrapper = VarType.Matches(TEXT("TScriptInterface"));
			const bool bIsSubobjectPtrTemplate = VarType.Matches(TEXT("TSubobjectPtr"));

			if (VarType.Matches(TEXT("TSubclassOf")))
			{
				TempClass = UClass::StaticClass();
			}
			else if (VarType.Matches(TEXT("FScriptInterface")))
			{
				TempClass = UInterface::StaticClass();
				bExpectStar = false;
			}
			else if (bIsAssetClassTemplate)
			{
				TempClass = UClass::StaticClass();
				bIsAsset = true;
			}
			else if (bIsLazyPtrTemplate || bIsWeakPtrTemplate || bIsAutoweakPtrTemplate || bIsScriptInterfaceWrapper || bIsAssetPtrTemplate || bIsSubobjectPtrTemplate)
			{
				RequireSymbol(TEXT("<"), VarType.Identifier);

				// Consume a forward class declaration 'class' if present
				MatchIdentifier(TEXT("class"));

				// Also consume const
				MatchIdentifier(TEXT("const"));
				
				// Find the lazy/weak class
				FToken InnerClass;
				if (GetIdentifier(InnerClass))
				{
					TempClass = AllClasses.FindScriptClass(InnerClass.Identifier);
					if (TempClass == nullptr)
					{
						FError::Throwf(TEXT("Unrecognized type '%s' (in expression %s<%s>)"), InnerClass.Identifier, VarType.Identifier, InnerClass.Identifier);
					}

					if (bIsAutoweakPtrTemplate)
					{
						bIsWeak = true;
						bWeakIsAuto = true;
					}
					else if (bIsLazyPtrTemplate)
					{
						bIsLazy = true;
					}
					else if (bIsWeakPtrTemplate)
					{
						bIsWeak = true;
					}
					else if (bIsAssetPtrTemplate)
					{
						bIsAsset = true;
					}
					else if (bIsSubobjectPtrTemplate)
					{
						Flags |= CPF_SubobjectReference | CPF_InstancedReference;
					}

					bExpectStar = false;
				}
				else
				{
					FError::Throwf(TEXT("%s: Missing template type"), VarType.Identifier);
				}

				RequireSymbol(TEXT(">"), VarType.Identifier, ESymbolParseOption::CloseTemplateBracket);
			}
			else
			{
				TempClass = AllClasses.FindScriptClass(VarType.Identifier);
			}

			if (TempClass != NULL)
			{
				bHandledType = true;

				// An object reference.
				CheckInScope( TempClass );

				bool bAllowWeak = !(Disallow & CPF_AutoWeak); // if it is not allowing anything, force it strong. this is probably a function arg
				VarProperty = FPropertyBase( TempClass, NULL, bAllowWeak, bIsWeak, bWeakIsAuto, bIsLazy, bIsAsset );
				if (TempClass->IsChildOf(UClass::StaticClass()))
				{
					if ( MatchSymbol(TEXT("<")) )
					{
						bExpectStar = false;

						// Consume a forward class declaration 'class' if present
						MatchIdentifier(TEXT("class"));

						// Get the actual class type to restrict this to
						FToken Limitor;
						if( !GetIdentifier(Limitor) )
						{
							FError::Throwf(TEXT("'class': Missing class limitor"));
						}

						VarProperty.MetaClass = AllClasses.FindScriptClassOrThrow(Limitor.Identifier);

						RequireSymbol( TEXT(">"), TEXT("'class limitor'"), ESymbolParseOption::CloseTemplateBracket );
					}
					else
					{
						VarProperty.MetaClass = UObject::StaticClass();
					}

					if (bIsWeak)
					{
						FError::Throwf(TEXT("Class variables cannot be weak, they are always strong."));
					}

					if (bIsLazy)
					{
						FError::Throwf(TEXT("Class variables cannot be lazy, they are always strong."));
					}

					if (bIsAssetPtrTemplate)
					{
						FError::Throwf(TEXT("Class variables cannot be stored in TAssetPtr, use TAssetSubclassOf instead."));
					}
				}

				// Inherit instancing flags
				if (DoesAnythingInHierarchyHaveDefaultToInstanced(TempClass))
				{
					Flags |= ((CPF_InstancedReference|CPF_ExportObject) & (~Disallow)); 
				}

				// Eat the star that indicates this is a pointer to the UObject
				if (bExpectStar)
				{
					// Const after variable type but before pointer symbol
					MatchIdentifier(TEXT("const"));

					RequireSymbol(TEXT("*"), TEXT("Expected a pointer type"));

					VarProperty.PointerType = EPointerType::Native;
				}

				// Imply const if it's a parameter that is a pointer to a const class
				if (VariableCategory != EVariableCategory::Member && (TempClass != NULL) && (TempClass->HasAnyClassFlags(CLASS_Const)))
				{
					Flags |= CPF_ConstParm;
				}

				// Class keyword in front of a class is legal, we 'consume' it
				bUnconsumedClassKeyword = false;
				bUnconsumedConstKeyword = false;
			}
		}

		// Resolve delegates declared in another class  //@TODO: UCREMOVAL: This seems extreme
		if (!bHandledType)
		{
			for (FClass* OtherClass : AllClasses)
			{
				if (UFunction* DelegateFunc = Cast<UFunction>(FindField(OtherClass, *(IdentifierStripped + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX))))
				{
					bHandledType = true;

					VarProperty = FPropertyBase( DelegateFunc->HasAnyFunctionFlags(FUNC_MulticastDelegate) ? CPT_MulticastDelegate : CPT_Delegate);
					VarProperty.DelegateName = *FString::Printf(TEXT("%s.%s"), *OtherClass->GetNameWithPrefix(), *IdentifierStripped);
					
					if (!(Disallow & CPF_InstancedReference))
					{
						Flags |= CPF_InstancedReference;
					}

					break;
				}
			}

			if (!bHandledType)
			{
				FError::Throwf(TEXT("Unrecognized type '%s'"), VarType.Identifier );
			}
		}

		if ((Flags & CPF_InstancedReference) && CurrentAccessSpecifier == ACCESS_Private && VariableCategory == EVariableCategory::Member)
		{
			if (((Flags & CPF_Edit) && (Flags & CPF_EditConst) == 0) ||
				((Flags & CPF_BlueprintVisible) && (Flags & CPF_BlueprintReadOnly) == 0))
			{
				FError::Throwf(TEXT("%s: Subobject (instanced) properties can't be editable (use VisibleAnywhere or BlueprintReadOnly instead)."), VarType.Identifier);
			}
		}
	}

	if (VariableCategory != EVariableCategory::Member)
	{
		// const after the variable type support (only for params)
		if (MatchIdentifier(TEXT("const")))
		{
			Flags |= CPF_ConstParm;
		}
	}

	if (bUnconsumedConstKeyword)
	{
		FError::Throwf(TEXT("Inappropriate keyword 'const' on variable of type '%s'"), VarType.Identifier );
	}

	if (bUnconsumedClassKeyword)
	{
		FError::Throwf(TEXT("Inappropriate keyword 'class' on variable of type '%s'"), VarType.Identifier );
	}

	if (bUnconsumedStructKeyword)
	{
		FError::Throwf(TEXT("Inappropriate keyword 'struct' on variable of type '%s'"), VarType.Identifier );
	}

	if (bUnconsumedEnumKeyword)
	{
		FError::Throwf(TEXT("Inappropriate keyword 'enum' on variable of type '%s'"), VarType.Identifier );
	}

	if (MatchSymbol(TEXT("*")))
	{
		FError::Throwf(TEXT("Inappropriate '*' on variable of type '%s', cannot have an exposed pointer to this type."), VarType.Identifier );
	}

	//@TODO: UCREMOVAL: 'const' member variables that will get written post-construction by defaultproperties
	if (Class->HasAnyClassFlags(CLASS_Const) && VariableCategory == EVariableCategory::Member)
	{
		// Eat a 'not quite truthful' const after the type; autogenerated for member variables of const classes.
		MatchIdentifier(TEXT("const"));
	}

	// Arrays are passed by reference but are only implicitly so; setting it explicitly could cause a problem with replicated functions
	if (MatchSymbol(TEXT("&")))
	{
		switch (VariableCategory)
		{
			case EVariableCategory::RegularParameter:
			case EVariableCategory::Return:
			{
				Flags |= CPF_OutParm;

				//@TODO: UCREMOVAL: How to determine if we have a ref param?
				if (Flags & CPF_ConstParm)
				{
					Flags |= CPF_ReferenceParm;
				}
			}
			break;

			case EVariableCategory::ReplicatedParameter:
			{
				if (!(Flags & CPF_ConstParm))
				{
					FError::Throwf(TEXT("Replicated %s parameters cannot be passed by non-const reference"), VarType.Identifier);
				}

				Flags |= CPF_ReferenceParm;
			}
			break;

			default:
			{
			}
			break;
		}

		if (Flags & CPF_ConstParm)
		{
			VarProperty.RefQualifier = ERefQualifier::ConstRef;
		}
		else
		{
			VarProperty.RefQualifier = ERefQualifier::NonConstRef;
		}
	}

	VarProperty.PropertyExportFlags = ExportFlags;

	// Set FPropertyBase info.
	VarProperty.PropertyFlags |= Flags;

	// Set the RepNotify name, if the variable needs it
	if( VarProperty.PropertyFlags & CPF_RepNotify )
	{
		if( RepCallbackName != NAME_None )
		{
			VarProperty.RepNotifyName = RepCallbackName;
		}
		else
		{
			FError::Throwf(TEXT("Must specify a valid function name for replication notifications"));
		}
	}

	// Perform some more specific validation on the property flags
	if ((VarProperty.PropertyFlags & CPF_PersistentInstance) && VarProperty.Type != CPT_ObjectReference)
	{
		FError::Throwf(TEXT("'Instanced' is only allowed on object property (or array of objects)"));
	}

	if ( VarProperty.IsObject() && VarProperty.MetaClass == NULL && (VarProperty.PropertyFlags&CPF_Config) != 0 )
	{
		FError::Throwf(TEXT("Not allowed to use 'config' with object variables"));
	}

	if ((VarProperty.PropertyFlags & CPF_RepRetry) && VarProperty.Type != CPT_Struct)
	{
		FError::Throwf(TEXT("'RepRetry' is only allowed on struct properties"));
	}

	if ((VarProperty.PropertyFlags & CPF_BlueprintAssignable) && VarProperty.Type != CPT_MulticastDelegate)
	{
		FError::Throwf(TEXT("'BlueprintAssignable' is only allowed on multicast delegate properties"));
	}

	if ((VarProperty.PropertyFlags & CPF_BlueprintCallable) && VarProperty.Type != CPT_MulticastDelegate)
	{
		FError::Throwf(TEXT("'BlueprintCallable' is only allowed on a property when it is a multicast delegate"));
	}

	if ((VarProperty.PropertyFlags & CPF_BlueprintAuthorityOnly) && VarProperty.Type != CPT_MulticastDelegate)
	{
		FError::Throwf(TEXT("'BlueprintAuthorityOnly' is only allowed on a property when it is a multicast delegate"));
	}
	
	if (VariableCategory != EVariableCategory::Member)
	{
		// These conditions are checked externally for struct/member variables where the flag can be inferred later on from the variable name itself
		ValidatePropertyIsDeprecatedIfNecessary(VarProperty, OuterPropertyType);
	}

	// Check for invalid transients
	uint64 Transients = VarProperty.PropertyFlags & (CPF_DuplicateTransient | CPF_TextExportTransient | CPF_NonPIEDuplicateTransient);
	if (Transients && !Cast<UClass>(Scope))
	{
		TArray<const TCHAR*> FlagStrs = ParsePropertyFlags(Transients);
		FError::Throwf(TEXT("'%s' specifier(s) are only allowed on class member variables"), *FString::Join(FlagStrs, TEXT(", ")));
	}

	// Make sure the overrides are allowed here.
	if( VarProperty.PropertyFlags & Disallow )
	{
		FError::Throwf(TEXT("Specified type modifiers not allowed here") );
	}

	VarProperty.MetaData = MetaDataFromNewStyle;

	if (ParsedVarIndexRange)
	{
		ParsedVarIndexRange->Count = InputPos - ParsedVarIndexRange->StartIndex;
	}
	return 1;
}

/**
 * If the property has already been seen during compilation, then return add. If not,
 * then return replace so that INI files don't mess with header exporting
 *
 * @param PropertyName the string token for the property
 *
 * @return FNAME_Replace_Not_Safe_For_Threading or FNAME_Add
 */
EFindName FHeaderParser::GetFindFlagForPropertyName(const TCHAR* PropertyName)
{
	static TMap<FString,int32> PreviousNames;
	FString PropertyStr(PropertyName);
	FString UpperPropertyStr = PropertyStr.ToUpper();
	// See if it's in the list already
	if (PreviousNames.Find(UpperPropertyStr))
	{
		return FNAME_Add;
	}
	// Add it to the list for future look ups
	PreviousNames.Add(UpperPropertyStr,1);
	// Check for a mismatch between the INI file and the config property name
	FName CurrentText(PropertyName,FNAME_Find);
	if (CurrentText != NAME_None &&
		FCString::Strcmp(PropertyName,*CurrentText.ToString()) != 0)
	{
		FError::Throwf(
			TEXT("INI file contains an incorrect case for (%s) should be (%s)"),
			*CurrentText.ToString(),
			PropertyName);
	}
	return FNAME_Replace_Not_Safe_For_Threading;
}

UProperty* FHeaderParser::GetVarNameAndDim
(
	UStruct*     Scope,
	FToken&      VarProperty,
	EObjectFlags ObjectFlags,
	bool         NoArrays,
	bool         IsFunction,
	const TCHAR* HardcodedName,
	const TCHAR* HintText
)
{
	check(Scope);

	// Add metadata for the module relative path
	const FString *ModuleRelativePath = GClassModuleRelativePathMap.Find(Class);
	if(ModuleRelativePath != NULL)
	{
		VarProperty.MetaData.Add(TEXT("ModuleRelativePath"), *ModuleRelativePath);
	}

	// Get variable name.
	if (HardcodedName != NULL)
	{
		// Hard-coded variable name, such as with return value.
		VarProperty.TokenType = TOKEN_Identifier;
		FCString::Strcpy( VarProperty.Identifier, HardcodedName );
	}
	else
	{
		FToken VarToken;
		if (!GetIdentifier(VarToken))
		{
			FError::Throwf(TEXT("Missing variable name") );
		}

		VarProperty.TokenType = TOKEN_Identifier;
		FCString::Strcpy(VarProperty.Identifier, VarToken.Identifier);
	}

	// Check to see if the variable is deprecated, and if so set the flag
	{
		FString VarName(VarProperty.Identifier);
		int32 DeprecatedIndex = VarName.Find(TEXT("_DEPRECATED"));
		if (DeprecatedIndex != INDEX_NONE)
		{
			if (DeprecatedIndex != VarName.Len() - 11)
			{
				FError::Throwf(TEXT("Deprecated variables must end with _DEPRECATED"));
			}

			// Warn if a deprecated property is visible
			if (VarProperty.PropertyFlags & (CPF_Edit | CPF_EditConst | CPF_BlueprintVisible | CPF_BlueprintReadOnly))
			{
				UE_LOG(LogCompile, Warning, TEXT("%s: Deprecated property '%s' should not be marked as visible or editable"), HintText, *VarName);
			}

			VarProperty.PropertyFlags |= CPF_Deprecated;
			VarName = VarName.Mid(0, DeprecatedIndex);

			FCString::Strcpy(VarProperty.Identifier, *VarName);
		}
	}

	// Make sure it doesn't conflict.
	int32 OuterContextCount = 0;
	UField* Existing = FindField(Scope, VarProperty.Identifier, true, UField::StaticClass(), NULL);

	if (Existing != NULL)
	{
		if (Existing->GetOuter() == Scope)
		{
			FError::Throwf(TEXT("%s: '%s' already defined"), HintText, VarProperty.Identifier);
		}
		else if ((Cast<UFunction>(Scope) != NULL || Cast<UClass>(Scope) != NULL)	// declaring class member or function local/parm
				&& Cast<UFunction>(Existing) == NULL								// and the existing field isn't a function
				&& Cast<UClass>(Existing->GetOuter()) != NULL )						// and the existing field is a class member (don't care about locals in other functions)
		{
			// don't allow it to obscure class properties either
			if (Existing->IsA(UScriptStruct::StaticClass()))
			{
				FError::Throwf(TEXT("%s: '%s' conflicts with struct defined in %s'%s'"), HintText, VarProperty.Identifier, (OuterContextCount > 0) ? TEXT("'within' class") : TEXT(""), *Existing->GetOuter()->GetName());
			}
			else
			{
				// if this is a property and one of them is deprecated, ignore it since it will be removed soon
				UProperty* ExistingProp = Cast<UProperty>(Existing);
				if ( ExistingProp == NULL
				|| (!ExistingProp->HasAnyPropertyFlags(CPF_Deprecated) && (VarProperty.PropertyFlags & CPF_Deprecated) == 0) )
				{
					FError::Throwf(TEXT("%s: '%s' conflicts with previously defined field in %s'%s'"), HintText, VarProperty.Identifier, (OuterContextCount > 0) ? TEXT("'within' class") : TEXT(""), *Existing->GetOuter()->GetName() );
				}
			}
		}
	}

	// Get optional dimension immediately after name.
	FToken Dimensions;
	if (MatchSymbol(TEXT("[")))
	{
		if (NoArrays)
		{
			FError::Throwf(TEXT("Arrays aren't allowed in this context") );
		}

		if (VarProperty.ArrayType == EArrayType::Dynamic)
		{
			FError::Throwf(TEXT("Static arrays of dynamic arrays are not allowed"));
		}

		if (VarProperty.IsBool())
		{
			FError::Throwf(TEXT("Bool arrays are not allowed") );
		}

		// Ignore how the actual array dimensions are actually defined - we'll calculate those with the compiler anyway.
		if (!GetRawToken(Dimensions, TEXT(']')))
		{
			FError::Throwf(TEXT("%s %s: Missing ']'"), HintText, VarProperty.Identifier );
		}

		// Only static arrays are declared with [].  Dynamic arrays use TArray<> instead.
		VarProperty.ArrayType = EArrayType::Static;

		UEnum* Enum = NULL;

		if (*Dimensions.String)
		{
			UEnum::LookupEnumNameSlow(Dimensions.String, &Enum);
		}

		if (!Enum)
		{
			// If the enum wasn't declared in this scope, then try to find it anywhere we can
			Enum = FindObject<UEnum>(ANY_PACKAGE, Dimensions.String);
		}

		if (Enum)
		{
			// set the ArraySizeEnum if applicable
			VarProperty.MetaData.Add("ArraySizeEnum", Enum->GetPathName());
		}

		MatchSymbol(TEXT("]"));
	}

	// Try gathering metadata for member fields
	if (!IsFunction)
	{
		ParseFieldMetaData(VarProperty.MetaData, VarProperty.Identifier);
		AddFormattedPrevCommentAsTooltipMetaData(VarProperty.MetaData);
	}

	// validate UFunction parameters
	if (IsFunction)
	{
		// UFunctions with a smart pointer as input parameter wont compile anyway, because of missing P_GET_... macro.
		// UFunctions with a smart pointer as return type will crash when called via blueprint, because they are not supported in VM.
		// WeakPointer is supported by VM as return type (see UObject::execLetWeakObjPtr), but there is no P_GET_... macro for WeakPointer.
		if ((VarProperty.Type == CPT_LazyObjectReference) || (VarProperty.Type == CPT_AssetObjectReference))
		{
			FError::Throwf(TEXT("UFunctions cannot take a smart pointer (LazyPtr, AssetPtr, etc) as a parameter."));
		}
	}

	// If this is the first time seeing the property name, then flag it for replace instead of add
	const EFindName FindFlag = VarProperty.PropertyFlags & CPF_Config ? GetFindFlagForPropertyName(VarProperty.Identifier) : FNAME_Add;
	// create the FName for the property, splitting (ie Unnamed_3 -> Unnamed,3)
	FName PropertyName(VarProperty.Identifier, FindFlag, true);

	// Add property.
	UProperty* NewProperty = NULL;

	{
		UProperty* Prev = NULL;
	    for (TFieldIterator<UProperty> It(Scope, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			Prev = *It;
		}

		UProperty* Array    = NULL;
		UObject*   NewScope = Scope;
		int32      ArrayDim = 1; // 1 = not a static array, 2 = static array
		if (VarProperty.ArrayType == EArrayType::Dynamic)
		{
			Array       = new(Scope,PropertyName,ObjectFlags)UArrayProperty(FObjectInitializer());
			NewScope    = Array;
			ObjectFlags = RF_Public;
		}
		else if (VarProperty.ArrayType == EArrayType::Static)
		{
			ArrayDim = 2;
		}

		if (VarProperty.Type == CPT_Byte)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UByteProperty(FObjectInitializer());
			((UByteProperty*)NewProperty)->Enum = VarProperty.Enum;
		}
		else if (VarProperty.Type == CPT_Int8)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UInt8Property(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_Int16)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UInt16Property(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_Int)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UIntProperty(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_Int64)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UInt64Property(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_UInt16)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UUInt16Property(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_UInt32)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UUInt32Property(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_UInt64)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UUInt64Property(FObjectInitializer());
		}
		else if (VarProperty.IsBool())
		{
			UBoolProperty* NewBoolProperty = new(NewScope,PropertyName,ObjectFlags)UBoolProperty(FObjectInitializer());
			NewProperty = NewBoolProperty;
			if (HardcodedName && FCString::Stricmp(HardcodedName, TEXT("ReturnValue")) == 0)
			{
				NewBoolProperty->SetBoolSize(sizeof(bool), true);
			}
			else
			{
				switch( VarProperty.Type )
				{
				case CPT_Bool:
					NewBoolProperty->SetBoolSize(sizeof(bool), true);
					break;
				case CPT_Bool8:
					NewBoolProperty->SetBoolSize(sizeof(uint8));
					break;
				case CPT_Bool16:
					NewBoolProperty->SetBoolSize(sizeof(uint16));
					break;
				case CPT_Bool32:
					NewBoolProperty->SetBoolSize(sizeof(uint32));
					break;
				case CPT_Bool64:
					NewBoolProperty->SetBoolSize(sizeof(uint64));
					break;
				}
			}
		}
		else if (VarProperty.Type == CPT_Float)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UFloatProperty(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_Double)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UDoubleProperty(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_ObjectReference)
		{
			check(VarProperty.PropertyClass);
			if (VarProperty.PropertyClass->IsChildOf(UClass::StaticClass()))
			{
				NewProperty = new(NewScope,PropertyName,ObjectFlags)UClassProperty(FObjectInitializer());
				((UClassProperty*)NewProperty)->MetaClass = VarProperty.MetaClass;
			}
			else
			{
				if ( DoesAnythingInHierarchyHaveDefaultToInstanced( VarProperty.PropertyClass ) )
				{
					VarProperty.PropertyFlags |= CPF_InstancedReference;
					AddEditInlineMetaData(VarProperty.MetaData);
				}
				NewProperty = new(NewScope,PropertyName,ObjectFlags)UObjectProperty(FObjectInitializer());
			}

			((UObjectPropertyBase*)NewProperty)->PropertyClass = VarProperty.PropertyClass;
		}
		else if (VarProperty.Type == CPT_WeakObjectReference)
		{
			check(VarProperty.PropertyClass);
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UWeakObjectProperty(FObjectInitializer());
			((UObjectPropertyBase*)NewProperty)->PropertyClass = VarProperty.PropertyClass;
		}
		else if( VarProperty.Type == CPT_LazyObjectReference )
		{
			check(VarProperty.PropertyClass);
			NewProperty = new(NewScope,PropertyName,ObjectFlags)ULazyObjectProperty(FObjectInitializer());
			((UObjectPropertyBase*)NewProperty)->PropertyClass = VarProperty.PropertyClass;
		}
		else if( VarProperty.Type == CPT_AssetObjectReference )
		{
			check(VarProperty.PropertyClass);
			if (VarProperty.PropertyClass->IsChildOf(UClass::StaticClass()))
			{
				NewProperty = new(NewScope,PropertyName,ObjectFlags)UAssetClassProperty(FObjectInitializer());
				((UAssetClassProperty*)NewProperty)->MetaClass = VarProperty.MetaClass;
			}
			else
			{
				NewProperty = new(NewScope,PropertyName,ObjectFlags)UAssetObjectProperty(FObjectInitializer());
			}
			((UObjectPropertyBase*)NewProperty)->PropertyClass = VarProperty.PropertyClass;
		}
		else if (VarProperty.Type == CPT_Interface)
		{
			check(VarProperty.PropertyClass);
			check(VarProperty.PropertyClass->HasAnyClassFlags(CLASS_Interface));

			UInterfaceProperty* InterfaceProperty = new(NewScope,PropertyName,ObjectFlags) UInterfaceProperty(FObjectInitializer());
			InterfaceProperty->InterfaceClass = VarProperty.PropertyClass;

			NewProperty = InterfaceProperty;
		}
		else if (VarProperty.Type == CPT_Name)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UNameProperty(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_String)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UStrProperty(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_Text)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UTextProperty(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_Struct)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UStructProperty(FObjectInitializer());
			if (VarProperty.Struct->StructFlags & STRUCT_HasInstancedReference)
			{
				VarProperty.PropertyFlags |= CPF_ContainsInstancedReference;
			}
			((UStructProperty*)NewProperty)->Struct = VarProperty.Struct;
		}
		else if (VarProperty.Type == CPT_Delegate)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UDelegateProperty(FObjectInitializer());
		}
		else if (VarProperty.Type == CPT_MulticastDelegate)
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UMulticastDelegateProperty(FObjectInitializer());
		}
		else
		{
			FError::Throwf(TEXT("Unknown property type %i"), (uint8)VarProperty.Type );
		}

		if( Array )
		{
			check(NewProperty);
			CastChecked<UArrayProperty>(Array)->Inner = NewProperty;

			// Copy some of the property flags to the inner property.
			NewProperty->PropertyFlags |= (VarProperty.PropertyFlags&CPF_PropagateToArrayInner);
			// Copy some of the property flags to the array property.
			if (NewProperty->PropertyFlags & (CPF_ContainsInstancedReference | CPF_InstancedReference))
			{
				VarProperty.PropertyFlags |= CPF_ContainsInstancedReference;
				VarProperty.PropertyFlags &= ~(CPF_InstancedReference | CPF_PersistentInstance); //this was propagated to the inner

				if (0 != (NewProperty->PropertyFlags & CPF_PersistentInstance))
				{
					TMap<FName, FString> MetaData;
					AddEditInlineMetaData(MetaData);
					AddMetaDataToClassData(NewProperty, VarProperty.MetaData);
				}
			}
			NewProperty = Array;
		}
		NewProperty->ArrayDim = ArrayDim;
		if (ArrayDim == 2)
		{
			GArrayDimensions.Add(NewProperty, Dimensions.String);
		}
		NewProperty->PropertyFlags = VarProperty.PropertyFlags;
		if (Prev != NULL)
		{
			NewProperty->Next = Prev->Next;
			Prev->Next = NewProperty;
		}
		else
		{
			NewProperty->Next = Scope->Children;
			Scope->Children = NewProperty;
		}
	}

	VarProperty.TokenProperty = NewProperty;
	ClassData->AddProperty(VarProperty);

	// if we had any metadata, add it to the class
	AddMetaDataToClassData(VarProperty.TokenProperty, VarProperty.MetaData);

	return NewProperty;
}

/*-----------------------------------------------------------------------------
	Statement compiler.
-----------------------------------------------------------------------------*/

//
// Compile a declaration in Token. Returns 1 if compiled, 0 if not.
//
bool FHeaderParser::CompileDeclaration( FClasses& AllClasses, FToken& Token )
{
	EAccessSpecifier AccessSpecifier = ParseAccessProtectionSpecifier(Token);
	if (AccessSpecifier)
	{
		if (!IsAllowedInThisNesting(ALLOW_VarDecl) && !IsAllowedInThisNesting(ALLOW_Function))
		{
			FError::Throwf(TEXT("Access specifier %s not allowed here."), Token.Identifier);
		}
		check( TopNest->NestType == NEST_Class || TopNest->NestType == NEST_Interface );
		CurrentAccessSpecifier = AccessSpecifier;
	}
	else if (Token.Matches(TEXT("class")) && (TopNest->NestType == NEST_GlobalScope))
	{
		if (!bHaveSeenFirstInterfaceClass || bFinishedParsingInterfaceClasses)
		{
			return SkipDeclaration(Token);
		}

		// Make sure the previous class ended with valid nesting.
		if (bEncounteredNewStyleClass_UnmatchedBrackets)
		{
			FError::Throwf(TEXT("Missing } at end of class"));
		}

		// Start parsing the second class
		bEncounteredNewStyleClass_UnmatchedBrackets = true;
		bHaveSeenSecondInterfaceClass = true;
		ParseSecondInterfaceClass(AllClasses);
	}
	else if (Token.Matches(TEXT("GENERATED_BODY")) && (TopNest->NestType != NEST_Interface
		|| (bHaveSeenFirstInterfaceClass && !bHaveSeenSecondInterfaceClass)))
	{
		if (TopNest->NestType != NEST_Class && TopNest->NestType != NEST_Interface)
		{
			FError::Throwf(TEXT("%s must occur inside the class or interface definition"), Token.Identifier);
		}
		if (!ClassDefinitionRanges.Contains(Class))
		{
			ClassDefinitionRanges.Add(Class, ClassDefinitionRange());
		}

		ClassDefinitionRanges[Class].bHasGeneratedBody = true;

		RequireSymbol(TEXT("("), Token.Identifier);
		RequireSymbol(TEXT(")"), Token.Identifier);

		GScriptHelper.FindClassData(Class)->GeneratedBodyMacroAccessSpecifier = CurrentAccessSpecifier;

		if (TopNest->NestType == NEST_Class)
		{
			bClassHasGeneratedBody = true;
		}
	}
	else if (Token.Matches(TEXT("GENERATED_UCLASS_BODY")))
	{
		if (TopNest->NestType != NEST_Class)
		{
			FError::Throwf(TEXT("%s must occur inside the class definition"), Token.Identifier);
		}
		RequireSymbol(TEXT("("), Token.Identifier);
		RequireSymbol(TEXT(")"), Token.Identifier);

		// The body implementation macro always ends with a 'public:'
		CurrentAccessSpecifier = ACCESS_Public;

		bClassHasGeneratedBody = true;
	}
	else if (bHaveSeenSecondInterfaceClass && Token.Matches(TEXT("GENERATED_IINTERFACE_BODY")))
	{
		RequireSymbol(TEXT("("), Token.Identifier);
		RequireSymbol(TEXT(")"), Token.Identifier);

		// The body implementation macro always ends with a 'public:'
		CurrentAccessSpecifier = ACCESS_Public;
	}
	else if (bHaveSeenFirstInterfaceClass && !bHaveSeenSecondInterfaceClass && Token.Matches(TEXT("GENERATED_UINTERFACE_BODY")))
	{
		if (TopNest->NestType != NEST_Interface)
		{
			FError::Throwf(TEXT("%s must occur inside the interface definition"), Token.Identifier);
		}
		RequireSymbol(TEXT("("), Token.Identifier);
		RequireSymbol(TEXT(")"), Token.Identifier);

		// The body implementation macro always ends with a 'public:'
		CurrentAccessSpecifier = ACCESS_Public;
	}
	else if (Token.Matches(TEXT("UCLASS"), ESearchCase::CaseSensitive) && (TopNest->Allow & ALLOW_Class))
	{
		if (bHaveSeenUClass)
		{
			FError::Throwf(TEXT("Can only declare one class per file (two for an interface)"));
		}
		bHaveSeenUClass = true;
		bEncounteredNewStyleClass_UnmatchedBrackets = true;		
		CompileClassDeclaration(AllClasses);
	}
	else if (Token.Matches(TEXT("UINTERFACE")) && (TopNest->Allow & ALLOW_Class))
	{
		if (bHaveSeenUClass)
		{
			FError::Throwf(TEXT("Can only declare one class per file (two for an interface)"));
		}
		bHaveSeenUClass = true;
		bEncounteredNewStyleClass_UnmatchedBrackets = true;
		bHaveSeenFirstInterfaceClass = true;
		CompileInterfaceDeclaration(AllClasses);
	}
	else if (Token.Matches(TEXT("UFUNCTION"), ESearchCase::CaseSensitive))
	{
		CompileFunctionDeclaration(AllClasses);
	}
	else if (Token.Matches(TEXT("UDELEGATE")))
	{
		CompileDelegateDeclaration(AllClasses, Token.Identifier, EDelegateSpecifierAction::Parse);
	}
	else if (IsValidDelegateDeclaration(Token)) // Legacy delegate parsing - it didn't need a UDELEGATE
	{
		CompileDelegateDeclaration(AllClasses, Token.Identifier);
	}
	else if (Token.Matches(TEXT("UPROPERTY"), ESearchCase::CaseSensitive))
	{
		CheckAllow( TEXT("'Member variable declaration'"), ALLOW_VarDecl );
		check( TopNest->NestType == NEST_Class );

		CompileVariableDeclaration(AllClasses, TopNode, EPropertyDeclarationStyle::UPROPERTY);
	}
	else if (Token.Matches(TEXT("UENUM")))
	{
		// Enumeration definition.
		CompileEnum(Class);
	}
	else if (Token.Matches(TEXT("USTRUCT")))
	{
		// Struct definition.
		CompileStructDeclaration(AllClasses, Class);
	}
	else if (Token.Matches(TEXT("#")))
	{
		// Compiler directive.
		CompileDirective(Class);
	}
	else if (bEncounteredNewStyleClass_UnmatchedBrackets && Token.Matches(TEXT("}")))
	{
		if (ClassDefinitionRanges.Contains(Class))
		{
			ClassDefinitionRanges[Class].End = &Input[InputPos];
		}
		MatchSemi();

		// Closing brace for class declaration
		//@TODO: This is a very loose approximation of what we really need to do
		// Instead, the whole statement-consumer loop should be in a nest
		bEncounteredNewStyleClass_UnmatchedBrackets = false;

		// Pop nesting here to allow other non UClass declarations in the header file.
		if (Class->ClassFlags & CLASS_Interface)
		{
			if (bHaveSeenSecondInterfaceClass)
			{
				// Remember all interface classes are now parsed so that we can parse other (non-U)classes.
				bFinishedParsingInterfaceClasses = true;
			}
			PopNest( AllClasses, NEST_Interface, TEXT("'Interface'") );
		}
		else
		{
			PopNest( AllClasses, NEST_Class, TEXT("'Class'") );

			// Ensure classes have a GENERATED_UCLASS_BODY declaration
			if (bHaveSeenUClass && !bClassHasGeneratedBody)
			{
				FError::Throwf(TEXT("Expected a GENERATED_UCLASS_BODY() at the start of class"));
			}

			bClassHasGeneratedBody = false;
		}
	}
	else if (Token.Matches(TEXT(";")))
	{
		if( GetToken(Token) )
		{
			FError::Throwf(TEXT("Extra ';' before '%s'"), Token.Identifier );
		}
		else
		{
			FError::Throwf(TEXT("Extra ';' before end of file") );
		}
	}
	else if (Token.Matches(NameLookupCPP.GetNameCPP(Class)))
	{
		if(!TryToMatchConstructorParameterList(Token))
		{
			return SkipDeclaration(Token);
		}
	}
	else
	{
		// Ignore C++ declaration / function definition. 
		return SkipDeclaration(Token);
	}
	return true;
}

bool FHeaderParser::SkipDeclaration(FToken& Token)
{
	// Store the current value of PrevComment so it can be restored after we parsed everything.
	FString OldPrevComment(PrevComment);
	// Consume all tokens until the end of declaration/definition has been found.
	int32 Nest = 0;
	// Check if this is a class/struct declaration in which case it can be followed by member variable declaration.	
	bool bPossiblyClassDeclaration = Token.Matches(TEXT("class")) || Token.Matches(TEXT("struct"));
	// (known) macros can end without ; or } so use () to find the end of the declaration.
	// However, we don't want to use it with DECLARE_FUNCTION, because we need it to be treated like a function.
	bool bMacroDeclaration      = ProbablyAMacro(Token.Identifier) && !Token.Matches("DECLARE_FUNCTION");
	bool bEndOfDeclarationFound = false;
	bool bDefinitionFound       = false;
	const TCHAR* OpeningBracket = bMacroDeclaration ? TEXT("(") : TEXT("{");
	const TCHAR* ClosingBracket = bMacroDeclaration ? TEXT(")") : TEXT("}");
	bool bRetestCurrentToken = false;
	while (bRetestCurrentToken || GetToken(Token))
	{
		// If we find parentheses at top-level and we think it's a class declaration then it's more likely
		// to be something like: class UThing* GetThing();
		if (bPossiblyClassDeclaration && Nest == 0 && Token.Matches(TEXT("(")))
		{
			bPossiblyClassDeclaration = false;
		}

		bRetestCurrentToken = false;
		if (Token.Matches(TEXT(";")) && Nest == 0)
		{
			bEndOfDeclarationFound = true;
			break;
		}

		if (Token.Matches(OpeningBracket))
		{
			// This is a function definition or class declaration.
			bDefinitionFound = true;
			Nest++;
		}
		else if (Token.Matches(ClosingBracket))
		{
			Nest--;
			if (Nest == 0)
			{
				bEndOfDeclarationFound = true;
				break;
			}

			if (Nest < 0)
			{
				FError::Throwf(TEXT("Unexpected '}'. Did you miss a semi-colon?"));
			}
		}
		else if (bMacroDeclaration && Nest == 0)
		{
			bMacroDeclaration = false;
			OpeningBracket = TEXT("{");
			ClosingBracket = TEXT("}");
			bRetestCurrentToken = true;
		}
	}
	if (bEndOfDeclarationFound)
	{
		// Member variable declaration after class declaration (see bPossiblyClassDeclaration).
		if (bPossiblyClassDeclaration && bDefinitionFound)
		{
			// Should syntax errors be also handled when someone declares a variable after function definition?
			// Consume the variable name.
			FToken VariableName;
			if( !GetToken(VariableName, true) )
			{
				return false;
			}
			if (VariableName.TokenType != TOKEN_Identifier)
			{
				// Not a variable name.
				UngetToken(VariableName);
			}
			else if (!SafeMatchSymbol(TEXT(";")))
			{
				FError::Throwf(*FString::Printf(TEXT("Unexpected '%s'. Did you miss a semi-colon?"), VariableName.Identifier));
			}
		}

		// C++ allows any number of ';' after member declaration/definition.
		while (SafeMatchSymbol(TEXT(";")));
	}

	PrevComment = OldPrevComment;
	// clear the current value for comment
	//ClearComment();

	// Successfully consumed C++ declaration unless mismatched pair of brackets has been found.
	return Nest == 0 && bEndOfDeclarationFound;
}

bool FHeaderParser::SafeMatchSymbol( const TCHAR* Match )
{
	FToken Token;

	// Remember the position before the next token (this can include comments before the next symbol).
	FScriptLocation LocationBeforeNextSymbol;
	InitScriptLocation(LocationBeforeNextSymbol);

	if (GetToken(Token, /*bNoConsts=*/ true))
	{
		if (Token.TokenType==TOKEN_Symbol && !FCString::Stricmp(Token.Identifier, Match))
		{
			return true;
		}

		UngetToken(Token);
	}
	// Return to the stored position.
	ReturnToLocation(LocationBeforeNextSymbol);

	return false;
}

void FHeaderParser::ParseClassNameDeclaration(FClasses& AllClasses, FString& DeclaredClassName, FString& RequiredAPIMacroIfPresent)
{
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ DeclaredClassName, /*out*/ RequiredAPIMacroIfPresent, TEXT("class"));

	// Get parent class.
	bool bSpecifiesParentClass = false;

	if (MatchSymbol(TEXT(":")))
	{
		RequireIdentifier(TEXT("public"), TEXT("class inheritance"));
		bSpecifiesParentClass = true;
	}

	if (bSpecifiesParentClass)
	{
		// Set the base class.
		UClass* TempClass = GetQualifiedClass(AllClasses, TEXT("'extends'"));
		// a class cannot 'extends' an interface, use 'implements'
		if (TempClass->ClassFlags & CLASS_Interface)
		{
			FError::Throwf(TEXT("Class '%s' cannot extend interface '%s', use 'implements'"), *Class->GetName(), *TempClass->GetName() );
		}

		UClass* SuperClass = Class->GetSuperClass();
		if( SuperClass == NULL )
		{
			Class->SetSuperStruct(TempClass);
		}
		else if( SuperClass != TempClass )
		{
			FError::Throwf(TEXT("%s's superclass must be %s, not %s"), *Class->GetPathName(), *SuperClass->GetPathName(), *TempClass->GetPathName() );
		}

		Class->ClassCastFlags |= Class->GetSuperClass()->ClassCastFlags;

		// Handle additional inherited interface classes
		while (MatchSymbol(TEXT(",")))
		{
			RequireIdentifier(TEXT("public"), TEXT("Interface inheritance must be public"));

			FToken Token;
			if (!GetIdentifier(Token, true))
				FError::Throwf(TEXT("Failed to get interface class identifier"));

			FString InterfaceName = Token.Identifier;

			// Handle templated native classes
			if (MatchSymbol(TEXT("<")))
			{
				InterfaceName += TEXT('<');

				int32 Nest = 1;
				while (Nest)
				{
					if (!GetToken(Token))
						FError::Throwf(TEXT("Unexpected end of file"));

					if (Token.TokenType == TOKEN_Symbol)
					{
						if (!FCString::Strcmp(Token.Identifier, TEXT("<")))
						{
							++Nest;
						}
						else if (!FCString::Strcmp(Token.Identifier, TEXT(">")))
						{
							--Nest;
						}
					}

					InterfaceName += Token.Identifier;
				}
			}

			HandleOneInheritedClass(AllClasses, *InterfaceName);
		}
	}
	else if( Class->GetSuperClass() )
	{
		FError::Throwf(TEXT("class: missing 'Extends %s'"), *Class->GetSuperClass()->GetName() );
	}
}

void FHeaderParser::HandleOneInheritedClass(FClasses& AllClasses, FString InterfaceName)
{
	// Check for UInterface derived interface inheritance
	if (UClass* Interface = AllClasses.FindScriptClass(InterfaceName))
	{
		// Try to find the interface
		if ( !Interface->HasAnyClassFlags(CLASS_Interface) )
		{
			FError::Throwf(TEXT("Implements: Class %s is not an interface; Can only inherit from non-UObjects or UInterface derived interfaces"), *Interface->GetName() );
		}

		// Make sure this class or a parent of this class didn't already implement the interface
		for (UClass* TestClass = Class; TestClass != NULL; TestClass = TestClass->GetSuperClass())
		{
			for (int32 i = 0; i < TestClass->Interfaces.Num(); i++)
			{
				if (TestClass->Interfaces[i].Class == Interface)
				{
					FError::Throwf(TEXT("Implements: Interface '%s' is already implemented by '%s'"), *Interface->GetName(), *TestClass->GetName());
				}
			}
		}

		// Propagate the inheritable ClassFlags
		Class->ClassFlags |= (Interface->ClassFlags) & CLASS_ScriptInherit;

		new (Class->Interfaces) FImplementedInterface(Interface, 0, false);
		if (Interface->HasAnyClassFlags(CLASS_Native))
		{
			ClassData->AddInheritanceParent(Interface);
		}
	}
	else
	{
		// Non-UObject inheritance
		ClassData->AddInheritanceParent(InterfaceName);
	}
}

/**
 * Compiles a class declaration.
 */
void FHeaderParser::CompileClassDeclaration(FClasses& AllClasses)
{
	// Start of a class block.
	CheckAllow( TEXT("'class'"), ALLOW_Class );

	// Get categories inherited from the parent.
	TArray<FString> HideCategories;
	TArray<FString> ShowSubCatgories;
	TArray<FString> HideFunctions;
	TArray<FString> AutoExpandCategories;
	TArray<FString> AutoCollapseCategories;
	Class->GetHideCategories(HideCategories);
	Class->GetShowCategories(ShowSubCatgories);
	Class->GetHideFunctions(HideFunctions);
	Class->GetAutoExpandCategories(AutoExpandCategories);
	Class->GetAutoCollapseCategories(AutoCollapseCategories);

	// Class attributes.
	FClassMetaData* ClassData = GScriptHelper.FindClassData(Class);
	check(ClassData);

	// New-style UCLASS() syntax
	TMap<FName, FString> MetaData;

	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Class"), MetaData);

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	// New style files have the class name / extends afterwards
	RequireIdentifier(TEXT("class"), TEXT("Class declaration"));

	SkipDeprecatedMacroIfNecessary();

	FString DeclaredClassName;
	FString RequiredAPIMacroIfPresent;
	ParseClassNameDeclaration(AllClasses, /*out*/ DeclaredClassName, /*out*/ RequiredAPIMacroIfPresent);
	ClassDefinitionRanges.Add(Class, ClassDefinitionRange(&Input[InputPos], nullptr));
	// Record that this class is RequiredAPI if the CORE_API style macro was present
	if (!RequiredAPIMacroIfPresent.IsEmpty())
	{
		Class->ClassFlags |= CLASS_RequiredAPI;
	}

	// All classes that are parsed are expected to be native
	UClass* Super = Class->GetSuperClass();
	if (Super && !Super->HasAnyClassFlags(CLASS_Native))
	{
		FError::Throwf(TEXT("Native classes cannot extend non-native classes") );
	}

	Class->SetFlags(RF_Native);
	Class->ClassFlags |= CLASS_Native;

	// Process all of the class specifiers
	TArray<FString> ClassGroupNames;
	bool bWithinSpecified = false;
	bool bDeclaresConfigFile = false;
	for (const FPropertySpecifier& PropSpecifier : SpecifiersFound)
	{
		const FString& Specifier = PropSpecifier.Key;

		if (Specifier == TEXT("noexport"))
		{
			// Don't export to C++ header.
			Class->ClassFlags |= CLASS_NoExport;
		}
		else if (Specifier == TEXT("intrinsic"))
		{
			Class->ClassFlags |= CLASS_Intrinsic;
		}
		else if (Specifier == TEXT("within"))
		{
			FString WithinNameStr = RequireExactlyOneSpecifierValue(PropSpecifier);

			UClass* RequiredWithinClass = AllClasses.FindClass(*WithinNameStr);

			if (!RequiredWithinClass)
			{
				FError::Throwf(TEXT("Within class '%s' not found."), *WithinNameStr);
			}

			if (RequiredWithinClass->IsChildOf(UInterface::StaticClass()))
			{
				FError::Throwf(TEXT("Classes cannot be 'within' interfaces"));
			}
			else if (Class->ClassWithin == NULL || Class->ClassWithin==UObject::StaticClass() || RequiredWithinClass->IsChildOf(Class->ClassWithin))
			{
				Class->ClassWithin = RequiredWithinClass;
			}
			else if (Class->ClassWithin != RequiredWithinClass)
			{
				FError::Throwf(TEXT("%s must be within %s, not %s"), *Class->GetPathName(), *Class->ClassWithin->GetPathName(), *RequiredWithinClass->GetPathName());
			}

			bWithinSpecified = true;
		}
		else if (Specifier == TEXT("editinlinenew"))
		{
			// don't allow actor classes to be declared editinlinenew
			if (IsActorClass(Class))
			{
				FError::Throwf(TEXT("Invalid class attribute: Creating actor instances via the property window is not allowed"));
			}

			// Class can be constructed from the New button in editinline
			Class->ClassFlags |= CLASS_EditInlineNew;
		}
		else if (Specifier == TEXT("noteditinlinenew"))
		{
			// Class cannot be constructed from the New button in editinline
			Class->ClassFlags &= ~CLASS_EditInlineNew;
		}
		else if (Specifier == TEXT("placeable"))
		{
			if (!(Class->ClassFlags & CLASS_NotPlaceable))
			{
				FError::Throwf(TEXT("The placeable specifier is deprecated. Classes are assumed to be placeable by default."));
			}
			Class->ClassFlags &= ~CLASS_NotPlaceable;
		}
		else if (Specifier == TEXT("defaulttoinstanced"))
		{
			// these classed default to instanced.
			Class->ClassFlags |= CLASS_DefaultToInstanced;
		}
		else if (Specifier == TEXT("notplaceable"))
		{
			// Don't allow the class to be placed in the editor.
			Class->ClassFlags |= CLASS_NotPlaceable;
		}
		else if (Specifier == TEXT("hidedropdown"))
		{
			// Prevents class from appearing in class comboboxes in the property window
			Class->ClassFlags |= CLASS_HideDropDown;
		}
		else if (Specifier == TEXT("dependsOn"))
		{
			FError::Throwf(TEXT("The dependsOn specifier is deprecated. Please use proper #include instead."));

			// Make sure the syntax matches but don't do anything with it;
			RequireSpecifierValue(PropSpecifier);
		}
		else if (Specifier == TEXT("MinimalAPI"))
		{
			Class->ClassFlags |= CLASS_MinimalAPI;
		}
		else if (Specifier == TEXT("const"))
		{
			Class->ClassFlags |= CLASS_Const;
		}
		else if (Specifier == TEXT("perObjectConfig"))
		{
			Class->ClassFlags |= CLASS_PerObjectConfig;
		}
		else if (Specifier == TEXT("configdonotcheckdefaults"))
		{
			Class->ClassFlags |= CLASS_ConfigDoNotCheckDefaults;
		}
		else if (Specifier == TEXT("abstract"))
		{
			// Hide all editable properties.
			Class->ClassFlags |= CLASS_Abstract;
		}
		else if (Specifier == TEXT("deprecated"))
		{
			Class->ClassFlags |= CLASS_Deprecated;

			// Don't allow the class to be placed in the editor.
			Class->ClassFlags |= CLASS_NotPlaceable;
		}
		else if (Specifier == TEXT("transient"))
		{
			// Transient class.
			Class->ClassFlags |= CLASS_Transient;
		}
		else if (Specifier == TEXT("nonTransient"))
		{
			// this child of a transient class is not transient - remove the transient flag
			Class->ClassFlags &= ~CLASS_Transient;
		}
		else if (Specifier == TEXT("customConstructor"))
		{
			// we will not export a constructor for this class, assuming it is in the CPP block
			Class->ClassFlags |= CLASS_CustomConstructor;
		}
		else if (Specifier == TEXT("config"))
		{
			// Class containing config properties - parse the name of the config file to use
			FString ConfigNameStr = RequireExactlyOneSpecifierValue(PropSpecifier);

			// if the user specified "inherit", we're just going to use the parent class's config filename
			// this is not actually necessary but it can be useful for explicitly communicating config-ness
			if (ConfigNameStr == TEXT("inherit"))
			{
				UClass* SuperClass = Class->GetSuperClass();
				if (!SuperClass)
					FError::Throwf(TEXT("Cannot inherit config filename: %s has no super class"), *Class->GetName());

				if (SuperClass->ClassConfigName == NAME_None)
					FError::Throwf(TEXT("Cannot inherit config filename: parent class %s is not marked config."), *SuperClass->GetPathName());
			}
			else
			{
				// otherwise, set the config name to the parsed identifier
				Class->ClassConfigName = FName(*ConfigNameStr);
			}

			bDeclaresConfigFile = true;
		}
		else if (Specifier == TEXT("defaultconfig"))
		{
			// Save object config only to Default INIs, never to local INIs.
			Class->ClassFlags |= CLASS_DefaultConfig;
		}
		else if (Specifier == TEXT("showCategories"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				// if we didn't find this specific category path in the HideCategories metadata
				if (HideCategories.Remove(Value) == 0)
				{
					TArray<FString> SubCategoryList;
					Value.ParseIntoArray(&SubCategoryList, TEXT("|"), true);

					FString SubCategoryPath;
					// look to see if any of the parent paths are excluded in the HideCategories list
					for (int32 CategoryPathIndex = 0; CategoryPathIndex < SubCategoryList.Num()-1; ++CategoryPathIndex)
					{
						SubCategoryPath += SubCategoryList[CategoryPathIndex];
						// if we're hiding a parent category, then we need to flag this sub category for show
						if (HideCategories.Contains(SubCategoryPath))
						{
							ShowSubCatgories.AddUnique(Value);
							break;
						}
						SubCategoryPath += "|";
					}
				}
			}
		}
		else if (Specifier == TEXT("hideCategories"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				HideCategories.AddUnique(Value);
			}
		}
		else if (Specifier == TEXT("showFunctions"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				HideFunctions.Remove(Value);
			}
		}
		else if (Specifier == TEXT("hideFunctions"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				HideFunctions.AddUnique(Value);
			}
		}
		else if (Specifier == TEXT("classGroup"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				ClassGroupNames.Add(Value);
			}
		}
		else if (Specifier == TEXT("autoExpandCategories"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				AutoCollapseCategories.Remove   (Value);
				AutoExpandCategories  .AddUnique(Value);
			}
		}
		else if (Specifier == TEXT("autoCollapseCategories"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				AutoExpandCategories  .Remove   (Value);
				AutoCollapseCategories.AddUnique(Value);
			}
		}
		else if (Specifier == TEXT("dontAutoCollapseCategories"))
		{
			RequireSpecifierValue(PropSpecifier);

			for (const FString& Value : PropSpecifier.Values)
			{
				AutoCollapseCategories.Remove(Value);
			}
		}
		else if (Specifier == TEXT("collapseCategories"))
		{
			// Class' properties should not be shown categorized in the editor.
			Class->ClassFlags |= CLASS_CollapseCategories;
		}
		else if (Specifier == TEXT("dontCollapseCategories"))
		{
			// Class' properties should be shown categorized in the editor.
			Class->ClassFlags &= ~CLASS_CollapseCategories;
		}
		else if (Specifier == TEXT("AdvancedClassDisplay"))
		{
			// By default the class properties are shown in advanced sections in UI
			Class->ClassFlags |= CLASS_AdvancedDisplay;
		}
		else if (Specifier == TEXT("ConversionRoot"))
		{
			MetaData.Add(FName(TEXT("IsConversionRoot")), "true");
		}
		else
		{
			FError::Throwf(TEXT("Unknown class specifier '%s'"), *Specifier);
		}
	}

	if (ClassGroupNames       .Num()) { MetaData.Add("ClassGroupNames",        FString::Join(ClassGroupNames,        TEXT(" "))); }
	if (AutoCollapseCategories.Num()) { MetaData.Add("AutoCollapseCategories", FString::Join(AutoCollapseCategories, TEXT(" "))); }
	if (HideCategories        .Num()) { MetaData.Add("HideCategories",         FString::Join(HideCategories,         TEXT(" "))); }
	if (ShowSubCatgories      .Num()) { MetaData.Add("ShowCategories",         FString::Join(ShowSubCatgories,       TEXT(" "))); }
	if (HideFunctions         .Num()) { MetaData.Add("HideFunctions",          FString::Join(HideFunctions,          TEXT(" "))); }
	if (AutoExpandCategories  .Num()) { MetaData.Add("AutoExpandCategories",   FString::Join(AutoExpandCategories,   TEXT(" "))); }

	// Make sure both RequiredAPI and MinimalAPI aren't specified
	if (Class->HasAllClassFlags(CLASS_MinimalAPI | CLASS_RequiredAPI))
	{
		FError::Throwf(TEXT("MinimalAPI cannot be specified when the class is fully exported using a MODULENAME_API macro"));
	}

	// Make sure there is a valid within
	if (!bWithinSpecified)
	{
		// classes always have a ClassWithin
		Class->ClassWithin = Class->GetSuperClass()
			? Class->GetSuperClass()->ClassWithin
			: UObject::StaticClass();
	}

	UClass* ExpectedWithin = Class->GetSuperClass()
		? Class->GetSuperClass()->ClassWithin
		: UObject::StaticClass();

	if( !Class->ClassWithin->IsChildOf(ExpectedWithin) )
	{
		FError::Throwf(TEXT("Parent class declared within '%s'.  Cannot override within class with '%s' since it isn't a child"), *ExpectedWithin->GetName(), *Class->ClassWithin->GetName() );
		///Class->ClassWithin = ExpectedWithin;
	}

	// All classes must start with a valid Unreal prefix
	const FString ExpectedClassName = Class->GetNameWithPrefix();
	if( DeclaredClassName != ExpectedClassName )
	{
		FError::Throwf(TEXT("Class name '%s' is invalid, should be identified as '%s'"), *DeclaredClassName, *ExpectedClassName );
	}

	// Validate.
	if( (Class->ClassFlags&CLASS_NoExport) )
	{
		// if the class's class flags didn't contain CLASS_NoExport before it was parsed, it means either:
		// a) the DECLARE_CLASS macro for this native class doesn't contain the CLASS_NoExport flag (this is an error)
		// b) this is a new native class, which isn't yet hooked up to static registration (this is OK)
		if ( !(Class->ClassFlags&CLASS_Intrinsic) && (PreviousClassFlags & CLASS_NoExport) == 0 &&
			(PreviousClassFlags&CLASS_Native) != 0 )	// a new native class (one that hasn't been compiled into C++ yet) won't have this set
		{
			FError::Throwf(TEXT("'noexport': Must include CLASS_NoExport in native class declaration"));
		}
	}

	if (!Class->HasAnyClassFlags(CLASS_Abstract) && ((PreviousClassFlags & CLASS_Abstract) != 0))
	{
		if (Class->HasAnyClassFlags(CLASS_NoExport))
		{
			FError::Throwf(TEXT("'abstract': NoExport class missing abstract keyword from class declaration (must change C++ version first)"));
			Class->ClassFlags |= CLASS_Abstract;
		}
		else if (Class->HasAnyFlags(RF_Native))
		{
			FError::Throwf(TEXT("'abstract': missing abstract keyword from class declaration - class will no longer be exported as abstract"));
		}
	}

	// Invalidate config name if not specifically declared.
	if (!bDeclaresConfigFile)
	{
		Class->ClassConfigName = NAME_None;
	}

	// Add metadata for the include path
	const FString *IncludePath = GClassIncludePathMap.Find(Class);
	if(IncludePath != NULL)
	{
		MetaData.Add(TEXT("IncludePath"), *IncludePath);
	}

	// Add metadata for the module relative path
	const FString *ModuleRelativePath = GClassModuleRelativePathMap.Find(Class);
	if(ModuleRelativePath != NULL)
	{
		MetaData.Add(TEXT("ModuleRelativePath"), *ModuleRelativePath);
	}

	// Register the metadata
	AddMetaDataToClassData(Class, MetaData);

	// Handle the start of the rest of the class
	RequireSymbol( TEXT("{"), TEXT("'Class'") );

	// Make visible outside the package.
	Class->ClearFlags( RF_Transient );
	check(Class->HasAnyFlags(RF_Public));
	check(Class->HasAnyFlags(RF_Standalone));

	// Copy properties from parent class.
	if (Class->GetSuperClass())
	{
		Class->SetPropertiesSize( Class->GetSuperClass()->GetPropertiesSize() );
	}

	// Push the class nesting.
	PushNest( NEST_Class, Class->GetFName(), NULL );

	// auto-create properties for all of the VFTables needed for the multiple inheritances
	// get the inheritance parents
	auto& InheritanceParents = ClassData->GetInheritanceParents();

	// for all base class types, make a VfTable property
	for (int32 ParentIndex = InheritanceParents.Num() - 1; ParentIndex >= 0; ParentIndex--)
	{
		// if this base class corresponds to an interface class, assign the vtable UProperty in the class's Interfaces map now...
		if (UClass* InheritedInterface = InheritanceParents[ParentIndex].InterfaceClass)
		{
			if (FImplementedInterface* Found = Class->Interfaces.FindByPredicate([=](const FImplementedInterface& Impl) { return Impl.Class == InheritedInterface; }))
			{
				Found->PointerOffset = 1;
			}
			else
			{
				Class->Interfaces.Add(FImplementedInterface(InheritedInterface, 1, false));
			}
		}
	}
}

void FHeaderParser::ParseInterfaceNameDeclaration(FClasses& AllClasses, FString& DeclaredInterfaceName, FString& RequiredAPIMacroIfPresent)
{
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ DeclaredInterfaceName, /*out*/ RequiredAPIMacroIfPresent, TEXT("interface"));

	// Get super interface
	bool bSpecifiesParentClass = MatchSymbol(TEXT(":"));
	if (!bSpecifiesParentClass)
		return;

	RequireIdentifier(TEXT("public"), TEXT("class inheritance"));

	// verify if our super class is an interface class
	// the super class should have been marked as CLASS_Interface at the importing stage, if it were an interface
	UClass* TempClass = GetQualifiedClass(AllClasses, TEXT("'extends'"));
	if( !(TempClass->ClassFlags & CLASS_Interface) )
	{
		// UInterface is special and actually extends from UObject, which isn't an interface
		if (DeclaredInterfaceName != TEXT("UInterface"))
			FError::Throwf(TEXT("Interface class '%s' cannot inherit from non-interface class '%s'"), *DeclaredInterfaceName, *TempClass->GetName() );
	}

	UClass* SuperClass = Class->GetSuperClass();
	if (SuperClass == NULL)
	{
		Class->SetSuperStruct(TempClass);
	}
	else if (SuperClass != TempClass)
	{
		FError::Throwf(TEXT("%s's superclass must be %s, not %s"), *Class->GetPathName(), *SuperClass->GetPathName(), *TempClass->GetPathName() );
	}
}

void FHeaderParser::ParseSecondInterfaceClass(FClasses& AllClasses)
{
	FString ErrorMsg(TEXT("C++ interface mix-in class declaration"));

	// 'class' was already matched by the caller

	// Get a class name
	FString DeclaredInterfaceName;
	FString RequiredAPIMacroIfPresent;
	ParseInterfaceNameDeclaration(AllClasses, /*out*/ DeclaredInterfaceName, /*out*/ RequiredAPIMacroIfPresent);

	// All classes must start with a valid Unreal prefix
	const FString ExpectedInterfaceName = Class->GetNameWithPrefix(EEnforceInterfacePrefix::I);
	if (DeclaredInterfaceName != ExpectedInterfaceName)
	{
		FError::Throwf(TEXT("Interface name '%s' is invalid, the second interface class should be identified as '%s'"), *DeclaredInterfaceName, *ExpectedInterfaceName);
	}

	// Continue parsing the second class as if it were a part of the first (for reflection data purposes, it is)
	RequireSymbol(TEXT("{"), *ErrorMsg);

	// Push the interface class nesting again.
	PushNest( NEST_Interface, Class->GetFName(), NULL );
}

/**
 *  compiles Java or C# style interface declaration
 */
void FHeaderParser::CompileInterfaceDeclaration(FClasses& AllClasses)
{
	// Start of an interface block. Since Interfaces and Classes are always at the same nesting level,
	// whereever a class declaration is allowed, an interface declaration is also allowed.
	CheckAllow( TEXT("'interface'"), ALLOW_Class );

	FString DeclaredInterfaceName;
	FString RequiredAPIMacroIfPresent;
	TMap<FName, FString> MetaData;

	// Build up a list of interface specifiers
	TArray<FPropertySpecifier> SpecifiersFound;

	// New-style UINTERFACE() syntax
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Interface"), MetaData);

	// New style files have the interface name / extends afterwards
	RequireIdentifier(TEXT("class"), TEXT("Interface declaration"));
	ParseInterfaceNameDeclaration(AllClasses, /*out*/ DeclaredInterfaceName, /*out*/ RequiredAPIMacroIfPresent);


	// Record that this interface is RequiredAPI if the CORE_API style macro was present
	if (!RequiredAPIMacroIfPresent.IsEmpty())
	{
		Class->ClassFlags |= CLASS_RequiredAPI;
	}

	// Set the appropriate interface class flags
	Class->ClassFlags |= CLASS_Interface | CLASS_Abstract;
	if (Class->GetSuperStruct() != NULL)
	{
		Class->ClassCastFlags |= Class->GetSuperClass()->ClassCastFlags;
	}

	// All classes that are parsed are expected to be native
	if (Class->GetSuperClass() && !Class->GetSuperClass()->HasAnyClassFlags(CLASS_Native))
	{
		FError::Throwf(TEXT("Native classes cannot extend non-native classes") );
	}

	Class->SetFlags(RF_Native);
	Class->ClassFlags |= CLASS_Native;

	// Process all of the interface specifiers
	for (TArray<FPropertySpecifier>::TIterator SpecifierIt(SpecifiersFound); SpecifierIt; ++SpecifierIt)
	{
		const FString& Specifier = SpecifierIt->Key;

		if (Specifier == TEXT("DependsOn"))
		{
			// Make sure the syntax matches but don't do anything with it
			RequireSpecifierValue(*SpecifierIt);
		}
		else if (Specifier == TEXT("MinimalAPI"))
		{
			Class->ClassFlags |= CLASS_MinimalAPI;
		}
		else if (Specifier == TEXT("ConversionRoot"))
		{
			MetaData.Add(FName(TEXT("IsConversionRoot")), "true");
		}
		else
		{
			FError::Throwf(TEXT("Unknown interface specifier '%s'"), *Specifier);
		}
	}

	// All classes must start with a valid Unreal prefix
	const FString ExpectedInterfaceName = Class->GetNameWithPrefix(EEnforceInterfacePrefix::U);
	if (DeclaredInterfaceName != ExpectedInterfaceName)
	{
		FError::Throwf(TEXT("Interface name '%s' is invalid, the first class should be identified as '%s'"), *DeclaredInterfaceName, *ExpectedInterfaceName );
	}

	// Try parsing metadata for the interface
	FClassMetaData* ClassData = GScriptHelper.FindClassData(Class);
	check(ClassData);

	// Register the metadata
	AddMetaDataToClassData( Class, MetaData );

	// Handle the start of the rest of the interface
	RequireSymbol( TEXT("{"), TEXT("'Class'") );

	// Make visible outside the package.
	Class->ClearFlags( RF_Transient );
	check(Class->HasAnyFlags(RF_Public));
	check(Class->HasAnyFlags(RF_Standalone));

	// Push the interface class nesting.
	// we need a more specific set of allow flags for NEST_Interface, only function declaration is allowed, no other stuff are allowed
	PushNest( NEST_Interface, Class->GetFName(), NULL );
}

// Returns true if the token is a dynamic delegate declaration
bool FHeaderParser::IsValidDelegateDeclaration(const FToken& Token) const
{
	FString TokenStr(Token.Identifier);
	return (Token.TokenType == TOKEN_Identifier) && TokenStr.StartsWith(TEXT("DECLARE_DYNAMIC_"));
}

// Parse the parameter list of a function or delegate declaration
void FHeaderParser::ParseParameterList(FClasses& AllClasses, UFunction* Function, bool bExpectCommaBeforeName, TMap<FName, FString>* MetaData)
{
	// Get parameter list.
	if ( MatchSymbol(TEXT(")")) )
		return;

	FAdvancedDisplayParameterHandler AdvancedDisplay(MetaData);
	do
	{
		// Get parameter type.
		FToken Property(CPT_None);
		EObjectFlags ObjectFlags;
		GetVarType( AllClasses, TopNode, Property, ObjectFlags, ~(CPF_ParmFlags|CPF_AutoWeak|CPF_RepSkip), TEXT("Function parameter"), NULL, EPropertyDeclarationStyle::None, (Function->FunctionFlags & FUNC_Net) ? EVariableCategory::ReplicatedParameter: EVariableCategory::RegularParameter);
		Property.PropertyFlags |= CPF_Parm;

		if (bExpectCommaBeforeName)
		{
			RequireSymbol(TEXT(","), TEXT("Delegate definitions require a , between the parameter type and parameter name"));
		}

		UProperty* Prop = GetVarNameAndDim(TopNode, Property, ObjectFlags, /*NoArrays=*/ false, /*IsFunction=*/ true, NULL, TEXT("Function parameter"));

		Function->NumParms++;

		if( AdvancedDisplay.CanMarkMore() && AdvancedDisplay.ShouldMarkParameter(Prop->GetName()) )
		{
			Prop->PropertyFlags |= CPF_AdvancedDisplay;
		}

		// Check parameters.
		if ((Function->FunctionFlags & FUNC_Net))
		{
			if (!(Function->FunctionFlags & FUNC_NetRequest))
			{
				if (Property.PropertyFlags & CPF_OutParm)
					FError::Throwf(TEXT("Replicated functions cannot contain out parameters"));

				if (Property.PropertyFlags & CPF_RepSkip)
					FError::Throwf(TEXT("Only service request functions cannot contain NoReplication parameters"));

				if ((Prop->GetClass()->ClassCastFlags & CASTCLASS_UDelegateProperty) != 0)
					FError::Throwf(TEXT("Replicated functions cannot contain delegate parameters (this would be insecure)"));

				if (Property.Type == CPT_String && Property.RefQualifier != ERefQualifier::ConstRef && Prop->ArrayDim == 1)
					FError::Throwf(TEXT("Replicated FString parameters must be passed by const reference"));

				if (Property.ArrayType == EArrayType::Dynamic && Property.RefQualifier != ERefQualifier::ConstRef && Prop->ArrayDim == 1)
					FError::Throwf(TEXT("Replicated TArray parameters must be passed by const reference"));
			}
			else
			{
				if (!(Property.PropertyFlags & CPF_RepSkip) && (Property.PropertyFlags & CPF_OutParm))
					FError::Throwf(TEXT("Service request functions cannot contain out parameters, unless marked NotReplicated"));

				if (!(Property.PropertyFlags & CPF_RepSkip) && (Prop->GetClass()->ClassCastFlags & CASTCLASS_UDelegateProperty) != 0)
					FError::Throwf(TEXT("Service request functions cannot contain delegate parameters, unless marked NotReplicated"));
			}
		}

		// Default value.
		if (MatchSymbol( TEXT("=") ))
		{
			// Skip past the native specified default value; we make no attempt to parse it
			FToken SkipToken;
			int32 ParenthesisNestCount=0;
			int32 StartPos=-1;
			int32 EndPos=-1;
			while ( GetToken(SkipToken) )
			{
				if (StartPos == -1)
				{
					StartPos = SkipToken.StartPos;
				}
				if ( ParenthesisNestCount == 0
					&& (SkipToken.Matches(TEXT(")")) || SkipToken.Matches(TEXT(","))) )
				{
					EndPos = SkipToken.StartPos;
					// went too far
					UngetToken(SkipToken);
					break;
				}
				if ( SkipToken.Matches(TEXT("(")) )
				{
					ParenthesisNestCount++;
				}
				else if ( SkipToken.Matches(TEXT(")")) )
				{
					ParenthesisNestCount--;
				}
			}

			// allow exec functions to be added to the metaData, this is so we can have default params for them.
			const bool bStoreCppDefaultValueInMetaData = Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Exec);
				
			if((EndPos > -1) && bStoreCppDefaultValueInMetaData) 
			{
				FString DefaultArgText(EndPos - StartPos, Input + StartPos);
				FString Key(TEXT("CPP_Default_"));
				Key += Prop->GetName();
				FName KeyName = FName(*Key);
				if (!MetaData->Contains(KeyName))
				{
					FString InnerDefaultValue;
					const bool bDefaultValueParsed = DefaultValueStringCppFormatToInnerFormat(Prop, DefaultArgText, InnerDefaultValue);
					if (!bDefaultValueParsed)
						FError::Throwf(TEXT("C++ Default parameter not parsed: %s \"%s\" "), *Prop->GetName(), *DefaultArgText);

					if (InnerDefaultValue.IsEmpty())
					{
						static int32 SkippedCounter = 0;
						UE_LOG(LogCompile, Verbose, TEXT("C++ Default parameter skipped/empty [%i]: %s \"%s\" "), SkippedCounter, *Prop->GetName(), *DefaultArgText );
						++SkippedCounter;
					}
					else
					{
						MetaData->Add(KeyName, InnerDefaultValue);
						UE_LOG(LogCompile, Verbose, TEXT("C++ Default parameter parsed: %s \"%s\" -> \"%s\" "), *Prop->GetName(), *DefaultArgText, *InnerDefaultValue );
					}
				}
			}
		}
	} while( MatchSymbol(TEXT(",")) );
	RequireSymbol( TEXT(")"), TEXT("parameter list") );
}

void FHeaderParser::CompileDelegateDeclaration(FClasses& AllClasses, const TCHAR* DelegateIdentifier, EDelegateSpecifierAction::Type SpecifierAction)
{
	TMap<FName, FString> MetaData;

	// Add metadata for the module relative path
	const FString *ModuleRelativePath = GClassModuleRelativePathMap.Find(Class);
	if(ModuleRelativePath != NULL)
	{
		MetaData.Add(TEXT("ModuleRelativePath"), *ModuleRelativePath);
	}

	FFuncInfo            FuncInfo;

	// If this is a UDELEGATE, parse the specifiers first
	FString DelegateMacro;
	if (SpecifierAction == EDelegateSpecifierAction::Parse)
	{
		TArray<FPropertySpecifier> SpecifiersFound;
		ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Delegate"), MetaData);

		ProcessFunctionSpecifiers(FuncInfo, SpecifiersFound);

		// Get the next token and ensure it looks like a delegate
		FToken Token;
		GetToken(Token);
		if (!IsValidDelegateDeclaration(Token))
			FError::Throwf(TEXT("Unexpected token following UDELEGATE(): %s"), Token.Identifier);

		DelegateMacro = Token.Identifier;
	}
	else
	{
		DelegateMacro = DelegateIdentifier;
	}

	// Make sure we can have a delegate declaration here
	const TCHAR* CurrentScopeName = TEXT("Delegate Declaration");
	CheckAllow(CurrentScopeName, ALLOW_TypeDecl);//@TODO: UCREMOVAL: After the switch: Make this require global scope

	// Break the delegate declaration macro down into parts
	const bool bHasReturnValue = DelegateMacro.Contains(TEXT("_RetVal"));
	const bool bDeclaredConst  = DelegateMacro.Contains(TEXT("_Const"));
	const bool bIsMulticast    = DelegateMacro.Contains(TEXT("_MULTICAST"));

	// Determine the parameter count
	const FString* FoundParamCount = DelegateParameterCountStrings.FindByPredicate([&](const FString& Str){ return DelegateMacro.Contains(Str); });

	// Try reconstructing the string to make sure it matches our expectations
	FString ExpectedOriginalString = FString::Printf(TEXT("DECLARE_DYNAMIC%s_DELEGATE%s%s%s"),
		bIsMulticast ? TEXT("_MULTICAST") : TEXT(""),
		bHasReturnValue ? TEXT("_RetVal") : TEXT(""),
		FoundParamCount ? **FoundParamCount : TEXT(""),
		bDeclaredConst ? TEXT("_Const") : TEXT(""));

	if (DelegateMacro != ExpectedOriginalString)
	{
		FError::Throwf(TEXT("Unable to parse delegate declaration; expected '%s' but found '%s'."), *ExpectedOriginalString, *DelegateMacro);
	}

	// Multi-cast delegate function signatures are not allowed to have a return value
	if (bHasReturnValue && bIsMulticast)
	{
		FError::Throwf(TEXT("Multi-cast delegates function signatures must not return a value"));
	}

	// Delegate signature
	FuncInfo.FunctionFlags |= FUNC_Public | FUNC_Delegate;

	if (bIsMulticast)
	{
		FuncInfo.FunctionFlags |= FUNC_MulticastDelegate;
	}

	// Now parse the macro body
	RequireSymbol(TEXT("("), CurrentScopeName);

	// Parse the return value type
	EObjectFlags ReturnValueFlags = RF_NoFlags;
	FToken ReturnType( CPT_None );

	if (bHasReturnValue)
	{
		GetVarType( AllClasses, TopNode, ReturnType, ReturnValueFlags, 0, NULL, NULL, EPropertyDeclarationStyle::None, EVariableCategory::Return );
		RequireSymbol(TEXT(","), CurrentScopeName);
	}

	// Get the delegate name
	if (!GetIdentifier(FuncInfo.Function))
	{
		FError::Throwf(TEXT("Missing name for %s"), CurrentScopeName );
	}

	// If this is a delegate function then go ahead and mangle the name so we don't collide with
	// actual functions or properties
	{
		//@TODO: UCREMOVAL: Eventually this mangling shouldn't occur

		// Remove the leading F
		FString Name(FuncInfo.Function.Identifier);

		if (!Name.StartsWith(TEXT("F")))
		{
			FError::Throwf(TEXT("Delegate type declarations must start with F"));
		}

		Name = Name.Mid(1);

		// Append the signature goo
		Name += HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX;

		// Replace the name
		FCString::Strcpy( FuncInfo.Function.Identifier, *Name );
	}

	// Allocate local property frame, push nesting level and verify
	// uniqueness at this scope level.
	PushNest( NEST_FunctionDeclaration, FuncInfo.Function.Identifier, NULL );
	UFunction* DelegateSignatureFunction = ((UFunction*)TopNode);

	DelegateSignatureFunction->FunctionFlags |= FuncInfo.FunctionFlags;

	FuncInfo.FunctionReference = DelegateSignatureFunction;
	FuncInfo.SetFunctionNames();
	ClassData->AddFunction(FuncInfo);

	// determine whether this function should be 'const'
	if (bDeclaredConst)
	{
		DelegateSignatureFunction->FunctionFlags |= FUNC_Const;
	}
	else if (Class->HasAnyClassFlags(CLASS_Const))
	{
		// non-static functions in a const class must be const themselves
		//@TODO: UCREMOVAL: Should this really apply to delegate signatures which don't really live in the class?
		FError::Throwf(TEXT("Delegates declared in a const class must also be marked as const"));
	}

	// Get parameter list.
	if (FoundParamCount)
	{
		RequireSymbol(TEXT(","), CurrentScopeName);

		ParseParameterList(AllClasses, DelegateSignatureFunction, /*bExpectCommaBeforeName=*/ true);

		// Check the expected versus actual number of parameters
		int32 ParamCount = FoundParamCount - DelegateParameterCountStrings.GetData() + 1;
		if (DelegateSignatureFunction->NumParms != ParamCount)
			FError::Throwf(TEXT("Expected %d parameters but found %d parameters"), ParamCount, DelegateSignatureFunction->NumParms);
	}
	else
	{
		// Require the closing paren even with no parameter list
		RequireSymbol(TEXT(")"), TEXT("Delegate Declaration"));
	}

	// Create the return value property
	if (bHasReturnValue)
	{
		ReturnType.PropertyFlags |= CPF_Parm | CPF_OutParm | CPF_ReturnParm;
		UProperty* ReturnProp = GetVarNameAndDim(TopNode, ReturnType, ReturnValueFlags, /*NoArrays=*/ true, /*IsFunction=*/ true, TEXT("ReturnValue"), TEXT("Function return type"));

		DelegateSignatureFunction->NumParms++;
	}

	// Try parsing metadata for the function
	ParseFieldMetaData(MetaData, *(DelegateSignatureFunction->GetName()));

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	AddMetaDataToClassData(DelegateSignatureFunction, MetaData);

	// Optionally consume a semicolon, it's not required for the delegate macro since it contains one internally
	MatchSemi();

	// Bind the function.
	DelegateSignatureFunction->Bind();

	// End the nesting
	PopNest( AllClasses, NEST_FunctionDeclaration, CurrentScopeName );

	// Don't allow delegate signatures to be redefined.
	for (TFieldIterator<UFunction> FunctionIt(CastChecked<UStruct>(DelegateSignatureFunction->GetOuter()), EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* TestFunc = *FunctionIt;
		if ((TestFunc->GetFName() == DelegateSignatureFunction->GetFName()) && (TestFunc != DelegateSignatureFunction))
		{
			FError::Throwf(TEXT("Can't override delegate signature function '%s'"), FuncInfo.Function.Identifier);
		}
	}
}

/**
 * Parses and compiles a function declaration
 */
void FHeaderParser::CompileFunctionDeclaration(FClasses& AllClasses)
{
	CheckAllow(TEXT("'Function'"), ALLOW_Function);

	TMap<FName, FString> MetaData;

	// Add metadata for the module relative path
	const FString *ModuleRelativePath = GClassModuleRelativePathMap.Find(Class);
	if(ModuleRelativePath != NULL)
	{
		MetaData.Add(TEXT("ModuleRelativePath"), *ModuleRelativePath);
	}

	// New-style UFUNCTION() syntax 
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Function"), MetaData);

	FScriptLocation FuncNameRetry;
	InitScriptLocation(FuncNameRetry);

	if (!Class->HasAnyClassFlags(CLASS_Native))
	{
		FError::Throwf(TEXT("Should only be here for native classes!"));
	}

	// Process all specifiers.
	const TCHAR* TypeOfFunction = TEXT("function");

	bool bAutomaticallyFinal = true;

	FFuncInfo FuncInfo;
	FuncInfo.FunctionFlags = FUNC_Native;

	// Infer the function's access level from the currently declared C++ access level
	if (CurrentAccessSpecifier == ACCESS_Public)
	{
		FuncInfo.FunctionFlags |= FUNC_Public;
	}
	else if (CurrentAccessSpecifier == ACCESS_Protected)
	{
		FuncInfo.FunctionFlags |= FUNC_Protected;
	}
	else if (CurrentAccessSpecifier == ACCESS_Private)
	{
		FuncInfo.FunctionFlags |= FUNC_Private;
		FuncInfo.FunctionFlags |= FUNC_Final;

		// This is automatically final as well, but in a different way and for a different reason
		bAutomaticallyFinal = false;
	}
	else
	{
		FError::Throwf(TEXT("Unknown access level"));
	}

	// non-static functions in a const class must be const themselves
	if (Class->HasAnyClassFlags(CLASS_Const))
	{
		FuncInfo.FunctionFlags |= FUNC_Const;
	}

	if (MatchIdentifier(TEXT("static")))
	{
		FuncInfo.FunctionFlags |= FUNC_Static;
		FuncInfo.FunctionExportFlags |= FUNCEXPORT_CppStatic;
	}

	ProcessFunctionSpecifiers(FuncInfo, SpecifiersFound);

	if( (FuncInfo.FunctionFlags & FUNC_BlueprintPure) && Class->HasAnyClassFlags(CLASS_Interface) )
	{
		// Until pure interface casts are supported, we don't allow pures in interfaces
		FError::Throwf(TEXT("BlueprintPure specifier is not allowed for interface functions"));
	}

	if (FuncInfo.FunctionFlags & FUNC_Net)
	{
		// Network replicated functions are always events, and are only final if sealed
		TypeOfFunction = TEXT("event");
		bAutomaticallyFinal = false;
	}

	if (FuncInfo.FunctionFlags & FUNC_BlueprintEvent)
	{
		TypeOfFunction = (FuncInfo.FunctionFlags & FUNC_Native) ? TEXT("BlueprintNativeEvent") : TEXT("BlueprintImplementableEvent");
		bAutomaticallyFinal = false;
	}

	bool bSawVirtual = false;

	if (MatchIdentifier(TEXT("virtual")))
	{
		bSawVirtual = true;
	}

	FString*   InternalPtr = MetaData.Find("BlueprintInternalUseOnly"); // FBlueprintMetadata::MD_BlueprintInternalUseOnly
	const bool bDeprecated = MetaData.Contains("DeprecatedFunction");       // FBlueprintMetadata::MD_DeprecatedFunction
	const bool bHasMenuCategory = MetaData.Contains("Category");                 // FBlueprintMetadata::MD_FunctionCategory
	const bool bInternalOnly = InternalPtr && *InternalPtr == TEXT("true");

	// If this function is blueprint callable or blueprint pure, require a category 
	if ((FuncInfo.FunctionFlags & (FUNC_BlueprintCallable | FUNC_BlueprintPure)) != 0) 
	{ 
		if (!bHasMenuCategory && !bInternalOnly && !bDeprecated) 
		{ 
			FError::Throwf(TEXT("Blueprint accessible functions must have a category specified")); 
		} 
	}

	// Verify interfaces with respect to their blueprint accessible functions
	if (Class->HasAnyClassFlags(CLASS_Interface))
	{
		const bool bCanImplementInBlueprints = !Class->HasMetaData(TEXT("CannotImplementInterfaceInBlueprint"));  //FBlueprintMetadata::MD_CannotImplementInterfaceInBlueprint
		if((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
		{
			// Ensure that blueprint events are only allowed in implementable interfaces. Internal only functions allowed
			if (!bCanImplementInBlueprints && !bInternalOnly)
			{
				FError::Throwf(TEXT("Interfaces that are not implementable in blueprints cannot have BlueprintImplementableEvent members."));
			}
		}
		
		if (((FuncInfo.FunctionFlags & FUNC_BlueprintCallable) != 0) && (((~FuncInfo.FunctionFlags) & FUNC_BlueprintEvent) != 0))
		{
			// Ensure that if this interface contains blueprint callable functions that are not blueprint defined, that it must be implemented natively
			if (bCanImplementInBlueprints)
			{
				FError::Throwf(TEXT("Blueprint implementable interfaces cannot contain BlueprintCallable functions that are not BlueprintImplementableEvents.  Use CannotImplementInterfaceInBlueprint on the interface if you wish to keep this function."));
			}
		}
	}

	// Peek ahead to look for a CORE_API style DLL import/export token if present
	{
		FToken Token;
		if (GetToken(Token, true))
		{
			bool bThrowTokenBack = true;
			if (Token.TokenType == TOKEN_Identifier)
			{
				FString RequiredAPIMacroIfPresent(Token.Identifier);
				if (RequiredAPIMacroIfPresent.EndsWith(TEXT("_API")))
				{
					//@TODO: Validate the module name for RequiredAPIMacroIfPresent
					bThrowTokenBack = false;

					if (Class->HasAnyClassFlags(CLASS_RequiredAPI))
					{
						FError::Throwf(TEXT("'%s' must not be used on methods of a class that is marked '%s' itself."), *RequiredAPIMacroIfPresent, *RequiredAPIMacroIfPresent);
					}
					FuncInfo.FunctionFlags |= FUNC_RequiredAPI;
					FuncInfo.FunctionExportFlags |= FUNCEXPORT_RequiredAPI;
				}
			}

			if (bThrowTokenBack)
			{
				UngetToken(Token);
			}
		}
	}

	// Look for virtual again, in case there was an ENGINE_API token first
	if (MatchIdentifier(TEXT("virtual")))
	{
		bSawVirtual = true;
	}

	// Process the virtualness
	if (bSawVirtual)
	{
		// Remove the implicit final, the user can still specifying an explicit final at the end of the declaration
		bAutomaticallyFinal = false;

		// if this is a BlueprintNativeEvent or BlueprintImplementableEvent in an interface, make sure it's not "virtual"
		if ((Class->HasAnyClassFlags(CLASS_Interface)) && (FuncInfo.FunctionFlags & FUNC_BlueprintEvent))
		{
			FError::Throwf(TEXT("BlueprintImplementableEvents in Interfaces must not be declared 'virtual'"));
		}

		// if this is a BlueprintNativeEvent, make sure it's not "virtual"
		if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) && (FuncInfo.FunctionFlags & FUNC_Native))
		{
			FError::Throwf(TEXT("BlueprintNativeEvent functions must be non-virtual."));
		}

		//		@todo: we should consider making BIEs nonvirtual as well, where could simplify these checks to this...
		// 		if ( (FuncInfo.FunctionFlags & FUNC_BlueprintEvent) )
		// 		{
		// 			FError::Throwf(TEXT("Functions marked BlueprintImplementableEvent or BlueprintNativeEvent must not be declared 'virtual'"));
		// 		}
	}
	else
	{
		// if this is a function in an Interface, it must be marked 'virtual' unless it's an event
		if (Class->HasAnyClassFlags(CLASS_Interface) && !(FuncInfo.FunctionFlags & FUNC_BlueprintEvent))
		{
			FError::Throwf(TEXT("Interface functions that are not BlueprintImplementableEvents must be declared 'virtual'"));
		}
	}

	// Handle the initial implicit/explicit final
	// A user can still specify an explicit final after the parameter list as well.
	if (bAutomaticallyFinal || FuncInfo.bSealedEvent)
	{
		FuncInfo.FunctionFlags |= FUNC_Final;
		FuncInfo.FunctionExportFlags |= FUNCEXPORT_Final;

		if (Class->HasAnyClassFlags(CLASS_Interface))
		{
			FError::Throwf(TEXT("Interface functions cannot be declared 'final'"));
		}
	}

	// Get return type.
	EObjectFlags ReturnValueFlags = RF_NoFlags;
	FToken ReturnType( CPT_None );
	bool bHasReturnValue = false;

	// C++ style functions always have a return value type, even if it's void
	if (!MatchIdentifier(TEXT("void")))
	{
		bHasReturnValue = GetVarType( AllClasses, TopNode, ReturnType, ReturnValueFlags, 0, NULL, NULL, EPropertyDeclarationStyle::None, EVariableCategory::Return );
	}

	// Get function or operator name.
	if (!GetIdentifier(FuncInfo.Function))
	{
		FError::Throwf(TEXT("Missing %s name"), TypeOfFunction);
	}

	if ( !MatchSymbol(TEXT("(")) )
	{
		FError::Throwf(TEXT("Bad %s definition"), TypeOfFunction);
	}

	if (FuncInfo.FunctionFlags & FUNC_Net)
	{
		bool bIsNetService = !!(FuncInfo.FunctionFlags & (FUNC_NetRequest | FUNC_NetResponse));
		if (bHasReturnValue && !bIsNetService)
			FError::Throwf(TEXT("Replicated functions can't have return values"));

		if (FuncInfo.RPCId > 0)
		{
			if (FString* ExistingFunc = UsedRPCIds.Find(FuncInfo.RPCId))
				FError::Throwf(TEXT("Function %s already uses identifier %d"), **ExistingFunc, FuncInfo.RPCId);

			UsedRPCIds.Add(FuncInfo.RPCId, FuncInfo.Function.Identifier);
			if (FuncInfo.FunctionFlags & FUNC_NetResponse)
			{
				// Look for another function expecting this response
				if (FString* ExistingFunc = RPCsNeedingHookup.Find(FuncInfo.RPCId))
				{
					// If this list isn't empty at end of class, throw error
					RPCsNeedingHookup.Remove(FuncInfo.RPCId);
				}
			}
		}

		if (FuncInfo.RPCResponseId > 0)
		{
			// Look for an existing response function
			FString* ExistingFunc = UsedRPCIds.Find(FuncInfo.RPCResponseId);
			if (ExistingFunc == NULL)
			{
				// If this list isn't empty at end of class, throw error
				RPCsNeedingHookup.Add(FuncInfo.RPCResponseId, FuncInfo.Function.Identifier);
			}
		}
	}

	// Allocate local property frame, push nesting level and verify
	// uniqueness at this scope level.
	PushNest( NEST_FunctionDeclaration, FuncInfo.Function.Identifier, NULL );
	UFunction* TopFunction = ((UFunction*)TopNode);

	TopFunction->FunctionFlags |= FuncInfo.FunctionFlags;

	FuncInfo.FunctionReference = TopFunction;
	FuncInfo.SetFunctionNames();

	FFunctionData* StoredFuncData = ClassData->AddFunction(FuncInfo);

	// Get parameter list.
	ParseParameterList(AllClasses, TopFunction, false, &MetaData);

	// Get return type, if any.
	if (bHasReturnValue)
	{
		ReturnType.PropertyFlags |= CPF_Parm | CPF_OutParm | CPF_ReturnParm;
		UProperty* ReturnProp = GetVarNameAndDim(TopNode, ReturnType, ReturnValueFlags, /*NoArrays=*/ true, /*IsFunction=*/ true, TEXT("ReturnValue"), TEXT("Function return type"));

		TopFunction->NumParms++;
	}

	// determine if there are any outputs for this function
	bool bHasAnyOutputs = bHasReturnValue;
	if (bHasAnyOutputs == false)
	{
		for (TFieldIterator<UProperty> It(TopFunction); It; ++It)
		{
			UProperty const* const Param = *It;
			if (!(Param->PropertyFlags & CPF_ReturnParm) && (Param->PropertyFlags & CPF_OutParm))
			{
				bHasAnyOutputs = true;
				break;
			}
		}
	}
	if ( (bHasAnyOutputs == false) && (FuncInfo.FunctionFlags & (FUNC_BlueprintPure)) )
	{
		FError::Throwf(TEXT("BlueprintPure specifier is not allowed for functions with no return value and no output parameters."));
	}


	// determine whether this function should be 'const'
	if ( MatchIdentifier(TEXT("const")) )
	{
		if( (TopFunction->FunctionFlags & (FUNC_Native)) == 0 )
		{
			// @TODO: UCREMOVAL Reconsider?
			//FError::Throwf(TEXT("'const' may only be used for native functions"));
		}

		FuncInfo.FunctionFlags |= FUNC_Const;

		// @todo: the presence of const and one or more outputs does not guarantee that there are
		// no side effects. On GCC and clang we could use __attribure__((pure)) or __attribute__((const))
		// or we could just rely on the use marking things BlueprintPure. Either way, checking the C++
		// const identifier to determine purity is not desirable. We should remove the following logic:

		// If its a const BlueprintCallable function with some sort of output, mark it as BlueprintPure as well
		if ( bHasAnyOutputs && ((FuncInfo.FunctionFlags & FUNC_BlueprintCallable) != 0) )
		{
			FuncInfo.FunctionFlags |= FUNC_BlueprintPure;
		}
	}

	// Try parsing metadata for the function
	ParseFieldMetaData(MetaData, *(TopFunction->GetName()));

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	AddMetaDataToClassData(TopFunction, MetaData);
	

	// Handle C++ style functions being declared as abstract
	if (MatchSymbol(TEXT("=")))
	{
		if (bHaveSeenSecondInterfaceClass)
		{
			int32 ZeroValue = 1;
			bool bGotZero = GetConstInt(/*out*/ZeroValue);
			bGotZero = bGotZero && (ZeroValue == 0);

			if (!bGotZero)
			{
				FError::Throwf(TEXT("Expected 0 to indicate function is abstract"));
			}
		}
		else
		{
			FError::Throwf(TEXT("Only functions in the second interface class can be declared abstract"));
		}
	}

	// Look for the final keyword to indicate this function is sealed
	if (MatchIdentifier(TEXT("final")))
	{
		// This is a final (prebinding, non-overridable) function
		FuncInfo.FunctionFlags |= FUNC_Final;
		FuncInfo.FunctionExportFlags |= FUNCEXPORT_Final;
		if (Class->HasAnyClassFlags(CLASS_Interface))
		{
			FError::Throwf(TEXT("Interface functions cannot be declared 'final'"));
		}
		else if (FuncInfo.FunctionFlags & FUNC_BlueprintEvent)
		{
			FError::Throwf(TEXT("Blueprint events cannot be declared 'final'"));
		}
	}

	// Make sure any new flags made it to the function
	//@TODO: UCREMOVAL: Ideally the flags didn't get copied midway thru parsing the function declaration, and we could avoid this
	TopFunction->FunctionFlags |= FuncInfo.FunctionFlags;
	StoredFuncData->UpdateFunctionData(FuncInfo);

	// Verify parameter list and return type compatibility within the
	// function, if any, that it overrides.
	for( int32 i=NestLevel-2; i>=1; i-- )
	{
		for (UFunction* Function : TFieldRange<UFunction>(Nest[i].Node))
		{
			if (Function->GetFName() != TopNode->GetFName() || Function == TopNode)
				continue;

			// Don't allow private functions to be redefined.
			if (Function->FunctionFlags & FUNC_Private)
				FError::Throwf(TEXT("Can't override private function '%s'"), FuncInfo.Function.Identifier);

			// see if they both either have a return value or don't
			if ((TopFunction->GetReturnProperty() != NULL) != (Function->GetReturnProperty() != NULL))
			{
				ReturnToLocation(FuncNameRetry);
				FError::Throwf(TEXT("Redefinition of '%s %s' differs from original: return value mismatch"), TypeOfFunction, FuncInfo.Function.Identifier );
			}

			// See if all parameters match.
			if (TopFunction->NumParms!=Function->NumParms)
			{
				ReturnToLocation(FuncNameRetry);
				FError::Throwf(TEXT("Redefinition of '%s %s' differs from original; different number of parameters"), TypeOfFunction, FuncInfo.Function.Identifier );
			}

			// Check all individual parameters.
			int32 Count=0;
			for( TFieldIterator<UProperty> CurrentFuncParam(TopFunction),SuperFuncParam(Function); Count<Function->NumParms; ++CurrentFuncParam,++SuperFuncParam,++Count )
			{
				if( !FPropertyBase(*CurrentFuncParam).MatchesType(FPropertyBase(*SuperFuncParam), 1) )
				{
					if( CurrentFuncParam->PropertyFlags & CPF_ReturnParm )
					{
						ReturnToLocation(FuncNameRetry);
						FError::Throwf(TEXT("Redefinition of %s %s differs only by return type"), TypeOfFunction, FuncInfo.Function.Identifier );
					}
					else
					{
						ReturnToLocation(FuncNameRetry);
						FError::Throwf(TEXT("Redefinition of '%s %s' differs from original"), TypeOfFunction, FuncInfo.Function.Identifier );
					}
					break;
				}
				else if ( CurrentFuncParam->HasAnyPropertyFlags(CPF_OutParm) != SuperFuncParam->HasAnyPropertyFlags(CPF_OutParm) )
				{
					ReturnToLocation(FuncNameRetry);
					FError::Throwf(TEXT("Redefinition of '%s %s' differs from original - 'out' mismatch on parameter %i"), TypeOfFunction, FuncInfo.Function.Identifier, Count + 1);
				}
				else if ( CurrentFuncParam->HasAnyPropertyFlags(CPF_ReferenceParm) != SuperFuncParam->HasAnyPropertyFlags(CPF_ReferenceParm) )
				{
					ReturnToLocation(FuncNameRetry);
					FError::Throwf(TEXT("Redefinition of '%s %s' differs from original - 'ref' mismatch on parameter %i"), TypeOfFunction, FuncInfo.Function.Identifier, Count + 1);
				}
			}

			if( Count<TopFunction->NumParms )
			{
				continue;
			}

			// if super version is event, overridden version must be defined as event (check before inheriting FUNC_Event)
			if ( (Function->FunctionFlags & FUNC_Event) && !(FuncInfo.FunctionFlags & FUNC_Event) )
			{
				FError::Throwf(TEXT("Superclass version is defined as an event so '%s' should be!"), FuncInfo.Function.Identifier);
			}
			// Function flags to copy from parent.
			FuncInfo.FunctionFlags |= (Function->FunctionFlags & FUNC_FuncInherit);

			// Make sure the replication conditions aren't being redefined
			if ((FuncInfo.FunctionFlags & FUNC_NetFuncFlags) != (Function->FunctionFlags & FUNC_NetFuncFlags))
			{
				FError::Throwf(TEXT("Redefinition of replication conditions for function '%s'"), FuncInfo.Function.Identifier);
			}
			FuncInfo.FunctionFlags |= (Function->FunctionFlags & FUNC_NetFuncFlags);

			// Are we overriding a function?
			if( TopNode==Function->GetOuter() )
			{
				// Duplicate.
				ReturnToLocation( FuncNameRetry );
				FError::Throwf(TEXT("Duplicate function '%s'"), *Function->GetName() );
			}
			// Overriding an existing function.
			else if( Function->FunctionFlags & FUNC_Final )
			{
				ReturnToLocation(FuncNameRetry);
				FError::Throwf(TEXT("%s: Can't override a 'final' function"), *Function->GetName() );
			}
			// Native function overrides should be done in CPP text, not in a UFUNCTION() declaration (you can't change flags, and it'd otherwise be a burden to keep them identical)
			else if( Cast<UClass>(TopFunction->GetOuter()) != NULL )
			{
				//ReturnToLocation(FuncNameRetry);
				FError::Throwf(TEXT("%s: An override of a function cannot have a UFUNCTION() declaration above it; it will use the same parameters as the original base declaration."), *Function->GetName() );
			}

			// Balk if required specifiers differ.
			if ((Function->FunctionFlags & FUNC_FuncOverrideMatch) != (FuncInfo.FunctionFlags & FUNC_FuncOverrideMatch))
			{
				FError::Throwf(TEXT("Function '%s' specifiers differ from original"), *Function->GetName());
			}

			// Here we have found the original.
			TopNode->SetSuperStruct(Function);
			goto Found;
		}
	}
Found:

	// Bind the function.
	TopFunction->Bind();
	
	// Make sure that the replication flags set on an overridden function match the parent function
	if (UFunction* SuperFunc = TopFunction->GetSuperFunction())
	{
		if ((TopFunction->FunctionFlags & FUNC_NetFuncFlags) != (SuperFunc->FunctionFlags & FUNC_NetFuncFlags))
		{
			FError::Throwf(TEXT("Overridden function '%s': Cannot specify different replication flags when overriding a function."), *TopFunction->GetName());
		}
	}

	// if this function is an RPC in state scope, verify that it is an override
	// this is required because the networking code only checks the class for RPCs when initializing network data, not any states within it
	if ((TopFunction->FunctionFlags & FUNC_Net) && (TopFunction->GetSuperFunction() == NULL) && Cast<UClass>(TopFunction->GetOuter()) == NULL)
	{
		FError::Throwf(TEXT("Function '%s': Base implementation of RPCs cannot be in a state. Add a stub outside state scope."), *TopFunction->GetName());
	}

	if (TopFunction->FunctionFlags & (FUNC_BlueprintCallable | FUNC_BlueprintEvent))
	{
		for (TFieldIterator<UProperty> It(TopFunction); It; ++It)
		{
			UProperty const* const Param = *It;
			if (Param->ArrayDim > 1)
			{
				FError::Throwf(TEXT("Static array cannot be exposed to blueprint. Function: %s Parameter %s\n"), *TopFunction->GetName(), *Param->GetName());
			}
		}
	}

	// Just declaring a function, so end the nesting.
	PopNest( AllClasses, NEST_FunctionDeclaration, TypeOfFunction );

	// Optionally consume a semicolon
	// This is optional to allow inline function definitions, as long as the function body is inside #if CPP ... #endif
	MatchSymbol(TEXT(";"));
}

/**
 * Ensures at script compile time that the metadata formatting is correct
 * @param	InKey			the metadata key being added
 * @param	InValue			the value string that will be associated with the InKey
 */
void FHeaderParser::ValidateMetaDataFormat(UField* Field, const FString& InKey, const FString& InValue)
{
	if ((InKey == TEXT("UIMin")) || (InKey == TEXT("UIMax")) || (InKey == TEXT("ClampMin")) || (InKey == TEXT("ClampMax")))
	{
		if (!InValue.IsNumeric())
		{
			FError::Throwf(TEXT("Metadata value for '%s' is non-numeric : '%s'"), *InKey, *InValue);
		}
	}
	else if (InKey == /*FBlueprintMetadata::MD_Protected*/ TEXT("BlueprintProtected"))
	{
		if (UFunction* Function = Cast<UFunction>(Field))
		{
			if (Function->HasAnyFunctionFlags(FUNC_Static))
			{
				// Determine if it's a function library
				UClass* Class = Function->GetOuterUClass();
				while ((Class->GetSuperClass() != UObject::StaticClass()) && (Class != NULL))
				{
					Class = Class->GetSuperClass();
				}

				if ((Class != NULL) && (Class->GetName() == TEXT("BlueprintFunctionLibrary")))
				{
					FError::Throwf(TEXT("%s doesn't make sense on static method '%s' in a blueprint function library"), *InKey, *Function->GetName());
				}
			}
		}
	}
	else if (InKey == TEXT("DevelopmentStatus"))
	{
		const FString EarlyAccessValue(TEXT("EarlyAccess"));
		const FString ExperimentalValue(TEXT("Experimental"));
		if ((InValue != EarlyAccessValue) && (InValue != ExperimentalValue))
		{
			FError::Throwf(TEXT("'%s' metadata was '%s' but it must be %s or %s"), *InKey, *InValue, *ExperimentalValue, *EarlyAccessValue);
		}
	}
}

// Validates the metadata, then adds it to the class data
void FHeaderParser::AddMetaDataToClassData(UField* Field, const TMap<FName, FString>& InMetaData)
{
	// Evaluate any key redirects on the passed in pairs
	TMap<FName, FString> RemappedPairs;
	RemappedPairs.Empty(InMetaData.Num());

	for (TMap<FName, FString>::TConstIterator It(InMetaData); It; ++It)
	{
		FName CurrentKey = It.Key();
		FName NewKey = UMetaData::GetRemappedKeyName(CurrentKey);

		if (NewKey != NAME_None)
		{
			UE_LOG(LogCompile, Warning, TEXT("Remapping old metadata key '%s' to new key '%s', please update the declaration."), *CurrentKey.ToString(), *NewKey.ToString());
			CurrentKey = NewKey;
		}

		RemappedPairs.Add(CurrentKey, It.Value());
	}

	// Finish validating and associate the metadata with the field
	ValidateMetaDataFormat(Field, RemappedPairs);
	ClassData->AddMetaData(Field, RemappedPairs);
}

// Ensures at script compile time that the metadata formatting is correct
void FHeaderParser::ValidateMetaDataFormat(UField* Field, const TMap<FName, FString>& MetaData)
{
	for (TMap<FName, FString>::TConstIterator It(MetaData); It; ++It)
	{
		ValidateMetaDataFormat(Field, It.Key().ToString(), It.Value());
	}
}

/** Parses optional metadata text. */
void FHeaderParser::ParseFieldMetaData(TMap<FName, FString>& MetaData, const TCHAR* FieldName)
{
	FToken PropertyMetaData;
	bool bMetadataPresent = false;
	 if (MatchIdentifier(TEXT("UMETA")))
	{
		bMetadataPresent = true;
		RequireSymbol( TEXT("("),*FString::Printf(TEXT("' %s metadata'"), FieldName) );
		if (!GetRawTokenRespectingQuotes(PropertyMetaData, TCHAR(')')))
		{
			FError::Throwf(TEXT("'%s': No metadata specified"), FieldName);
		}
		RequireSymbol( TEXT(")"),*FString::Printf(TEXT("' %s metadata'"), FieldName) );
	}

	if (bMetadataPresent)
	{
		// parse apart the string
		TArray<FString> Pairs;

		//@TODO: UCREMOVAL: Convert to property token reading
		// break apart on | to get to the key/value pairs
		FString NewData(PropertyMetaData.String);
		bool bInString = false;
		int32 LastStartIndex = 0;
		int32 CharIndex;
		for (CharIndex = 0; CharIndex < NewData.Len(); ++CharIndex)
		{
			TCHAR Ch = NewData.GetCharArray()[CharIndex];
			if (Ch == '"')
			{
				bInString = !bInString;
			}

			if ((Ch == ',') && !bInString)
			{
				if (LastStartIndex != CharIndex)
				{
					Pairs.Add(NewData.Mid(LastStartIndex, CharIndex - LastStartIndex));
				}
				LastStartIndex = CharIndex + 1;
			}
		}

		if (LastStartIndex != CharIndex)
		{
			Pairs.Add(NewData.Mid(LastStartIndex, CharIndex - LastStartIndex));
		}

		// go over all pairs
		for (int32 PairIndex = 0; PairIndex < Pairs.Num(); PairIndex++)
		{
			// break the pair into a key and a value
			FString Token = Pairs[PairIndex];
			FString Key = Token;
			// by default, not value, just a key (allowed)
			FString Value;

			// look for a value after an =
			int32 Equals = Token.Find(TEXT("="));
			// if we have an =, break up the string
			if (Equals != -1)
			{
				Key = Token.Left(Equals);
				Value = Token.Right((Token.Len() - Equals) - 1);
			}

			InsertMetaDataPair(MetaData, Key, Value);
		}
	}
}

bool FHeaderParser::IsBitfieldProperty()
{
	bool bIsBitfield = false;

	// The current token is the property type (uin32, uint16, etc).
	// Check the property name and then check for ':'
	FToken TokenVarName;
	if (GetToken(TokenVarName, /*bNoConsts=*/ true))
	{
		FToken Token;
		if (GetToken(Token, /*bNoConsts=*/ true))
		{
			if (Token.TokenType == TOKEN_Symbol && FCString::Stricmp(Token.Identifier, TEXT(":")) == 0)
			{
				bIsBitfield = true;
			}
			UngetToken(Token);
		}
		UngetToken(TokenVarName);
	}

	return bIsBitfield;
}

void FHeaderParser::ValidatePropertyIsDeprecatedIfNecessary(FPropertyBase& VarProperty, FToken* OuterPropertyType)
{
	// check to see if we have a UClassProperty using a deprecated class
	if ( VarProperty.MetaClass != NULL && VarProperty.MetaClass->HasAnyClassFlags(CLASS_Deprecated) && !(VarProperty.PropertyFlags & CPF_Deprecated) &&
		(OuterPropertyType == NULL || !(OuterPropertyType->PropertyFlags & CPF_Deprecated)) )
	{
		FError::Throwf(TEXT("Property is using a deprecated class: %s.  Property should be marked deprecated as well."), *VarProperty.MetaClass->GetPathName());
	}

	// check to see if we have a UObjectProperty using a deprecated class.
	// PropertyClass is part of a union, so only check PropertyClass if this token represents an object property
	if ( (VarProperty.Type == CPT_ObjectReference || VarProperty.Type == CPT_WeakObjectReference || VarProperty.Type == CPT_LazyObjectReference || VarProperty.Type == CPT_AssetObjectReference) && VarProperty.PropertyClass != NULL
		&&	VarProperty.PropertyClass->HasAnyClassFlags(CLASS_Deprecated)	// and the object class being used has been deprecated
		&& (VarProperty.PropertyFlags&CPF_Deprecated) == 0					// and this property isn't marked deprecated as well
		&& (OuterPropertyType == NULL || !(OuterPropertyType->PropertyFlags & CPF_Deprecated)) ) // and this property isn't in an array that was marked deprecated either
	{
		FError::Throwf(TEXT("Property is using a deprecated class: %s.  Property should be marked deprecated as well."), *VarProperty.PropertyClass->GetPathName());
	}
}

struct FExposeOnSpawnValidator
{
	// Keep this function synced with UEdGraphSchema_K2::FindSetVariableByNameFunction
	static bool IsSupported(const FPropertyBase& Property)
	{
		bool ProperNativeType = false;
		switch (Property.Type)
		{
		case CPT_Int:
		case CPT_Byte:
		case CPT_Float:
		case CPT_Bool:
		case CPT_ObjectReference:
		case CPT_String:
		case CPT_Text:
		case CPT_Name:
		case CPT_Vector:
		case CPT_Rotation:
			ProperNativeType = true;
		}

		if (!ProperNativeType && (CPT_Struct == Property.Type) && Property.Struct)
		{
			static const FName BlueprintTypeName(TEXT("BlueprintType"));
			ProperNativeType |= Property.Struct->GetBoolMetaData(BlueprintTypeName);
		}

		return ProperNativeType;
	}
};

void FHeaderParser::CompileVariableDeclaration(FClasses& AllClasses, UStruct* Struct, EPropertyDeclarationStyle::Type PropertyDeclarationStyle)
{
	uint64 DisallowFlags = CPF_ParmFlags;
	uint64 EdFlags       = 0;

	// Get variable type.
	FPropertyBase OriginalProperty(CPT_None);
	EObjectFlags ObjectFlags = RF_NoFlags;

	FIndexRange TypeRange;
	GetVarType( AllClasses, Struct, OriginalProperty, ObjectFlags, DisallowFlags, TEXT("Member variable declaration"), /*OuterPropertyType=*/ NULL, PropertyDeclarationStyle, EVariableCategory::Member, &TypeRange );
	OriginalProperty.PropertyFlags |= EdFlags;

	FString* Category = OriginalProperty.MetaData.Find("Category");

	if (PropertyDeclarationStyle == EPropertyDeclarationStyle::UPROPERTY)
	{
		// First check if the category was specified at all and if the property was exposed to the editor.
		if (!Category && (OriginalProperty.PropertyFlags & (CPF_Edit|CPF_BlueprintVisible)))
		{
			static const FString AbsoluteEngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
			FString SourceFilename = GClassSourceFileMap[Class];
			FPaths::NormalizeFilename(SourceFilename);
			if (Struct->GetOutermost() != nullptr && !SourceFilename.StartsWith(AbsoluteEngineDir))
			{
				OriginalProperty.MetaData.Add("Category", Struct->GetFName().ToString());
				Category = OriginalProperty.MetaData.Find("Category");
			}
			else
			{
				FError::Throwf(TEXT("Property is exposed to the editor or blueprints but has no Category specified."));
			}
		}

		// Validate that pointer properties are not interfaces (which are not GC'd and so will cause runtime errors)
		if (OriginalProperty.PointerType == EPointerType::Native && OriginalProperty.Struct->IsChildOf(UInterface::StaticClass()))
		{
			// Get the name of the type, removing the asterisk representing the pointer
			FString TypeName = FString(TypeRange.Count, Input + TypeRange.StartIndex).Trim().TrimTrailing().LeftChop(1).TrimTrailing();
			FError::Throwf(TEXT("UPROPERTY pointers cannot be interfaces - did you mean TScriptInterface<%s>?"), *TypeName);
		}
	}

	// If the category was specified explicitly, it wins
	if (Category && !(OriginalProperty.PropertyFlags & (CPF_Edit|CPF_BlueprintVisible|CPF_BlueprintAssignable|CPF_BlueprintCallable)))
	{
		FError::Throwf(TEXT("Property has a Category set but is not exposed to the editor or Blueprints with EditAnywhere, BlueprintReadWrite, VisibleAnywhere, BlueprintReadOnly, BlueprintAssignable, BlueprintCallable keywords.\r\n"));
	}

	// Make sure that editblueprint variables are editable
	if(!(OriginalProperty.PropertyFlags & CPF_Edit))
	{
		if (OriginalProperty.PropertyFlags & CPF_DisableEditOnInstance)
		{
			FError::Throwf(TEXT("Property cannot have 'DisableEditOnInstance' without being editable"));
		}

		if (OriginalProperty.PropertyFlags & CPF_DisableEditOnTemplate)
		{
			FError::Throwf(TEXT("Property cannot have 'DisableEditOnTemplate' without being editable"));
		}
	}

	// Validate.
	if (OriginalProperty.PropertyFlags & CPF_ParmFlags)
	{
		FError::Throwf(TEXT("Illegal type modifiers in member variable declaration") );
	}

	if (FString* ExposeOnSpawnValue = OriginalProperty.MetaData.Find(TEXT("ExposeOnSpawn")))
	{
		if ((*ExposeOnSpawnValue == TEXT("true")) && !FExposeOnSpawnValidator::IsSupported(OriginalProperty))
		{
			FError::Throwf(TEXT("ExposeOnSpawn - Property cannoty be exposed"));
		}
	}

	// Process all variables of this type.
	TArray<UProperty*> NewProperties;
	do
	{
		FToken     Property    = OriginalProperty;
		UProperty* NewProperty = GetVarNameAndDim(Struct, Property, ObjectFlags, /*NoArrays=*/ false, /*IsFunction=*/ false, NULL, TEXT("Member variable declaration"));

		// Optionally consume the :1 at the end of a bitfield boolean declaration
		if (Property.IsBool() && MatchSymbol(TEXT(":")))
		{
			int32 BitfieldSize = 0;
			if (!GetConstInt(/*out*/ BitfieldSize) || (BitfieldSize != 1))
			{
				FError::Throwf(TEXT("Bad or missing bitfield size for '%s', must be 1."), *NewProperty->GetName());
			}
		}

		// Deprecation validation
		ValidatePropertyIsDeprecatedIfNecessary(Property, NULL);

		if (TopNest->NestType != NEST_FunctionDeclaration)
		{
			if (NewProperties.Num())
			{
				FError::Throwf(TEXT("Comma delimited properties cannot be converted %s.%s\n"), *Struct->GetName(), *NewProperty->GetName());
			}
		}

		NewProperties.Add( NewProperty );
		// we'll need any metadata tags we parsed later on when we call ConvertEOLCommentToTooltip() so the tags aren't clobbered
		OriginalProperty.MetaData = Property.MetaData;

		if (NewProperty->HasAnyPropertyFlags(CPF_RepNotify))
		{
			NewProperty->RepNotifyFunc = OriginalProperty.RepNotifyName;
		}

		if (UScriptStruct* StructBeingBuilt = Cast<UScriptStruct>(Struct))
		{
			if (NewProperty->ContainsInstancedObjectProperty())
			{
				StructBeingBuilt->StructFlags = EStructFlags(StructBeingBuilt->StructFlags | STRUCT_HasInstancedReference);
			}
		}

		if (NewProperty->HasAnyPropertyFlags(CPF_BlueprintVisible) && (NewProperty->ArrayDim > 1))
		{
			FError::Throwf(TEXT("Static array cannot be exposed to blueprint %s.%s"), *Struct->GetName(), *NewProperty->GetName());
		}

	} while( MatchSymbol(TEXT(",")) );

	// Optional member initializer.
	if (MatchSymbol(TEXT("=")))
	{
		// Skip past the specified member initializer; we make no attempt to parse it
		FToken SkipToken;
		while (GetToken(SkipToken))
		{
			if (SkipToken.Matches(TEXT(";")))
			{
				// went too far
				UngetToken(SkipToken);
				break;
			}
		}
	}

	// Expect a semicolon.
	RequireSymbol( TEXT(";"), TEXT("'variable declaration'") );
}

//
// Compile a statement: Either a declaration or a command.
// Returns 1 if success, 0 if end of file.
//
bool FHeaderParser::CompileStatement(FClasses& AllClasses)
{
	// Get a token and compile it.
	FToken Token;
	if( !GetToken(Token, true) )
	{
		// End of file.
		return false;
	}
	else if (!CompileDeclaration( AllClasses, Token ))
	{
		FError::Throwf(TEXT("'%s': Bad command or expression"), Token.Identifier );
	}
	return true;
}

//
// Compute the function parameter size and save the return offset
//
//@TODO: UCREMOVAL: Need to rename ComputeFunctionParametersSize to reflect the additional work it's doing
void FHeaderParser::ComputeFunctionParametersSize( UClass* Class )
{
	// Recurse with all child states in this class.
	for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* ThisFunction = *FuncIt;

		// Fix up any structs that were used as a parameter in a delegate before being defined
		if (ThisFunction->HasAnyFunctionFlags(FUNC_Delegate))
		{
			for (TFieldIterator<UProperty> It(ThisFunction); It; ++It)
			{
				UProperty* Param = *It;
				if (UStructProperty* StructProp = Cast<UStructProperty>(Param))
				{
					if (StructProp->Struct->StructFlags & STRUCT_HasInstancedReference)
					{
						StructProp->PropertyFlags |= CPF_ContainsInstancedReference;
					}
				}
			}
			ThisFunction->StaticLink(true);
		}

		// Compute the function parameter size, propagate some flags to the outer function, and save the return offset
		// Must be done in a second phase, as StaticLink resets various fields again!
		ThisFunction->ParmsSize = 0;
		for (TFieldIterator<UProperty> It(ThisFunction); It; ++It)
		{
			UProperty* Param = *It;

			if (!(Param->PropertyFlags & CPF_ReturnParm) && (Param->PropertyFlags & CPF_OutParm))
			{
				ThisFunction->FunctionFlags |= FUNC_HasOutParms;
			}
				
			if (UStructProperty* StructProp = Cast<UStructProperty>(Param))
			{
				if (StructProp->Struct->HasDefaults())
				{
					ThisFunction->FunctionFlags |= FUNC_HasDefaults;
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Code skipping.
-----------------------------------------------------------------------------*/

/**
 * Skip over code, honoring { and } pairs.
 *
 * @param	NestCount	number of nest levels to consume. if 0, consumes a single statement
 * @param	ErrorTag	text to use in error message if EOF is encountered before we've done
 */
void FHeaderParser::SkipStatements( int32 NestCount, const TCHAR* ErrorTag  )
{
	FToken Token;

	int32 OriginalNestCount = NestCount;

	while( GetToken( Token, true ) )
	{
		if ( Token.Matches(TEXT("{")) )
		{
			NestCount++;
		}
		else if	( Token.Matches(TEXT("}")) )
		{
			NestCount--;
		}
		else if ( Token.Matches(TEXT(";")) && OriginalNestCount == 0 )
		{
			break;
		}

		if ( NestCount < OriginalNestCount || NestCount < 0 )
			break;
	}

	if( NestCount > 0 )
	{
		FError::Throwf(TEXT("Unexpected end of file at end of %s"), ErrorTag );
	}
	else if ( NestCount < 0 )
	{
		FError::Throwf(TEXT("Extraneous closing brace found in %s"), ErrorTag);
	}
}

/*-----------------------------------------------------------------------------
	Main script compiling routine.
-----------------------------------------------------------------------------*/

//
// Finalize any script-exposed functions in the specified class
//
void FHeaderParser::FinalizeScriptExposedFunctions(UClass* Class)
{
	check(Class->ClassFlags & CLASS_Parsed);

	// Finalize all of the children introduced in this class
	for (TFieldIterator<UStruct> ChildIt(Class, EFieldIteratorFlags::ExcludeSuper); ChildIt; ++ChildIt)
	{
		UStruct* ChildStruct = *ChildIt;

		if (UFunction* Function = Cast<UFunction>(ChildStruct))
		{
			// Add this function to the function map of it's parent class
			Class->AddFunctionToFunctionMap(Function);
		}
		else if (ChildStruct->IsA(UScriptStruct::StaticClass()))
		{
			// Ignore embedded structs
		}
		else
		{
			UE_LOG(LogCompile, Warning, TEXT("Unknown and unexpected child named %s of type %s in %s\n"), *ChildStruct->GetName(), *ChildStruct->GetClass()->GetName(), *Class->GetName());
			check(false);
		}
	}
}

//
// Parses the header associated with the specified class.
// Returns result enumeration.
//
ECompilationResult::Type FHeaderParser::ParseHeaderForOneClass(FClasses& AllClasses, FClass* InClass)
{
	// Early-out if this class has previously failed some aspect of parsing
	if (FailedClassesAnnotation.Get(InClass))
	{
		return ECompilationResult::OtherCompilationError;
	}

	// Reset the parser to begin a new class
	Class                                       = InClass;
	bEncounteredNewStyleClass_UnmatchedBrackets = false;
	bSpottedAutogeneratedHeaderInclude          = false;
	bHaveSeenFirstInterfaceClass                = false;
	bHaveSeenSecondInterfaceClass               = false;
	bFinishedParsingInterfaceClasses            = false;
	bHaveSeenUClass                             = false;
	bClassHasGeneratedBody                      = false;

	ECompilationResult::Type Result = ECompilationResult::OtherCompilationError;

	ClassData = GScriptHelper.AddClassData(Class);

	// Message.
	if (FParse::Param(FCommandLine::Get(), TEXT("VERBOSE")))
	{
		// Message.
		Warn->Logf( TEXT("Parsing %s"), *Class->GetName() );
	}

	// Make sure our parent classes is parsed.
	for (UClass* Temp = Class->GetSuperClass(); Temp; Temp=Temp->GetSuperClass())
	{
		if (!(Temp->ClassFlags & (CLASS_Parsed | CLASS_Intrinsic)))
		{
			FError::Throwf(TEXT("'%s' can't be compiled: Parent class '%s' has errors"), *Class->GetName(), *Temp->GetName() );
		}
	}

	// First pass.

	//@fixme - reset class default object state?

	Class->PropertiesSize = 0;

	// Set class flags and within.
	PreviousClassFlags = Class->ClassFlags;
	Class->ClassFlags &= ~CLASS_RecompilerClear;

	UClass* SuperClass = Class->GetSuperClass();
	if (SuperClass != NULL)
	{
		Class->ClassFlags |= (SuperClass->ClassFlags) & CLASS_ScriptInherit;
		Class->ClassConfigName = SuperClass->ClassConfigName;
		check(SuperClass->ClassWithin);
		if (Class->ClassWithin == NULL)
		{
			Class->ClassWithin = SuperClass->ClassWithin;
		}

		// Copy special categories from parent
		if (SuperClass->HasMetaData(TEXT("HideCategories")))
		{
			Class->SetMetaData(TEXT("HideCategories"), *SuperClass->GetMetaData("HideCategories"));
		}
		if (SuperClass->HasMetaData(TEXT("ShowCategories")))
		{
			Class->SetMetaData(TEXT("ShowCategories"), *SuperClass->GetMetaData("ShowCategories"));
		}
		if (SuperClass->HasMetaData(TEXT("HideFunctions")))
		{
			Class->SetMetaData(TEXT("HideFunctions"), *SuperClass->GetMetaData("HideFunctions"));
		}
		if (SuperClass->HasMetaData(TEXT("AutoExpandCategories")))
		{
			Class->SetMetaData(TEXT("AutoExpandCategories"), *SuperClass->GetMetaData("AutoExpandCategories"));
		}
		if (SuperClass->HasMetaData(TEXT("AutoCollapseCategories")))
		{
			Class->SetMetaData(TEXT("AutoCollapseCategories"), *SuperClass->GetMetaData("AutoCollapseCategories"));
		}
	}

	check(Class->ClassWithin);

	// Init compiler variables.
	ResetParser(*GClassStrippedHeaderTextMap[Class]);

	// Init nesting.
	NestLevel = 0;
	TopNest = NULL;
	PushNest( NEST_GlobalScope, TEXT(""), Class );

	// Verify class variables haven't been filled in
	check(Class->Children == NULL);
	check(Class->Next == NULL);
	check(Class->NetFields.Num() == 0);

	// C++ classes default to private access level
	CurrentAccessSpecifier = ACCESS_Private; 

	// Try to compile it, and catch any errors.
	bool bEmptyFile = true;
#if !PLATFORM_EXCEPTIONS_DISABLED
	try
#endif
	{
		// Parse entire program.
		while (CompileStatement(AllClasses))
		{
			bEmptyFile = false;

			// Clear out the previous comment in anticipation of the next statement.
			ClearComment();
			StatementsParsed++;
		}

		// Precompute info for runtime optimization.
		LinesParsed += InputLine;

		Class->ClassFlags |= CLASS_Parsed;

		if (RPCsNeedingHookup.Num() > 0)
		{
			FString ErrorMsg(TEXT("Request functions missing response pairs:\r\n"));
			for (TMap<int32, FString>::TConstIterator It(RPCsNeedingHookup); It; ++It)
			{
				ErrorMsg += FString::Printf(TEXT("%s missing id %d\r\n"), *It.Value(), It.Key());
			}

			RPCsNeedingHookup.Empty();
			FError::Throwf(*ErrorMsg);
		}

		// Make sure both classes were declared for interfaces
		if (bHaveSeenFirstInterfaceClass && !bHaveSeenSecondInterfaceClass)
		{
			FError::Throwf(TEXT("Expected two class declarations to complete an interface (UMyInterface and IMyInterface)"));
		}

		// Make sure the compilation ended with valid nesting.
		if (bEncounteredNewStyleClass_UnmatchedBrackets)
		{
			FError::Throwf(TEXT("Missing } at end of class") );
		}

		if (NestLevel == 0)
		{
			FError::Throwf(TEXT("Internal nest inconsistency") );
		}
		else if (NestLevel > 1)
		{
			FError::Throwf(TEXT("Unexpected end of script in '%s' block"), NestTypeName(TopNest->NestType) );
		}

		// Cleanup after first pass.
		ComputeFunctionParametersSize( Class );

		// Set all optimization ClassFlags based on property types
		for (TFieldIterator<UProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->IsLocalized())
			{
				Class->ClassFlags |= CLASS_Localized;
			}

			if ((It->PropertyFlags & CPF_Config) != 0)
			{
				Class->ClassFlags |= CLASS_Config;
			}

			if (It->ContainsInstancedObjectProperty())
			{
				Class->ClassFlags |= CLASS_HasInstancedReference;
			}
		}

		// Class needs to specify which ini file is going to be used if it contains config variables.
		if( (Class->ClassFlags & CLASS_Config) && (Class->ClassConfigName == NAME_None) )
		{
			// Inherit config setting from base class.
			Class->ClassConfigName = Class->GetSuperClass() ? Class->GetSuperClass()->ClassConfigName : NAME_None;
			if( Class->ClassConfigName == NAME_None )
			{
				FError::Throwf(TEXT("Classes with config / globalconfig member variables need to specify config file.") );
				Class->ClassConfigName = NAME_Engine;
			}
		}

		// mark temporary classes as native
		if (FClassUtils::IsTemporaryClass(Class))
		{
			Class->ClassFlags |= CLASS_Native;
		}

		// First-pass success.
		Result = ECompilationResult::Succeeded;
	}
#if !PLATFORM_EXCEPTIONS_DISABLED
	catch( TCHAR* ErrorMsg )
	{
		// Handle compiler error.
		{
			TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);
			FString FormattedErrorMessage            = FString::Printf(TEXT("Error: In %s: %s\r\n"), *Class->GetName(), ErrorMsg);
			FString FormattedErrorMessageWithContext = FString::Printf(TEXT("%s: %s"), *GetContext(), *FormattedErrorMessage);

			UE_LOG(LogCompile, Log,  TEXT("%s"), *FormattedErrorMessageWithContext );
			Warn->Log(ELogVerbosity::Error, FormattedErrorMessage );
		}

		FailedClassesAnnotation.Set(Class);
		Result = GCompilationResult;
	}
#endif

	// Clean up and exit.
	Class->Bind();

	// Finalize functions
	if (Result == ECompilationResult::Succeeded)
	{
		FinalizeScriptExposedFunctions( Class );
	}

	if (!bSpottedAutogeneratedHeaderInclude && !bEmptyFile && !Class->HasAnyClassFlags(CLASS_NoExport))
	{
		const FString ExpectedHeaderName = FString::Printf(TEXT("%s.generated.h"), *GClassHeaderNameWithNoPathMap[Class]);
		FError::Throwf(TEXT("Expected an include at the top of the header: '#include \"%s\"'"), *ExpectedHeaderName);
	}

	return Result; //@TODO: UCREMOVAL: This function is always returning succeeded even on a compiler error; should this continue?
}

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

// Parse Class's annotated headers and optionally its child classes.  Marks the class as CLASS_Parsed.
ECompilationResult::Type FHeaderParser::ParseHeaders(FClasses& AllClasses, FHeaderParser& HeaderParser, FClass* Class, bool bParseSubclasses)
{
	if (!GClassStrippedHeaderTextMap.Contains(Class))
		return ECompilationResult::Succeeded;

	ECompilationResult::Type Result = ECompilationResult::Succeeded;

	// Handle all dependencies of the class.
	auto& DependentOn = *GClassDependentOnMap.FindOrAdd(Class);
	for (int32 NameIndex = 0; NameIndex < DependentOn.Num(); NameIndex++)
	{
		FString DependentClassName = DependentOn[NameIndex].ToString();

		// Check if the dependent class name came from #include directive in which case we allow it to fail.
		DependentClassNameFromHeader(*DependentClassName, DependentClassName);

		FClass* DependsOnClass = AllClasses.FindScriptClass(DependentClassName);
		if (!DependsOnClass)
		{
			// Try again with actor class name
			const FString DependentClassNameStripped  = GetClassNameWithoutPrefix(DependentClassName);
			const FString ActorDependentClassName = FString(TEXT("A")) + DependentClassNameStripped;

			DependsOnClass = AllClasses.FindScriptClass(ActorDependentClassName);
			if (!DependsOnClass)
			{
				// Try again with temporary class name. This may be struct only header.
				const FString TemporaryDependentClassName = FString(TEXT("U")) + GenerateTemporaryClassName(*DependentClassNameStripped);

				DependsOnClass = AllClasses.FindScriptClass(TemporaryDependentClassName);
				if (!DependsOnClass)
				{
					// Ingore the error and remove this entry from DependentOn array.
					DependentOn.RemoveAt(NameIndex--);
					continue;
				}
			}
		}

		// Detect potentially unnecessary usage of dependson.
		if (DependsOnClass->HasAnyClassFlags(CLASS_Parsed))
			continue;

		// Check for circular dependency. If the DependsOnClass is dependent on the SubClass, there is one.
		if (DependsOnClass != Class && AllClasses.IsDependentOn(DependsOnClass, Class))
		{
			HeaderParser.Class = Class;
			HeaderParser.InputLine = GClassDeclarationLineNumber[Class];
			FError::Throwf(TEXT("Error: Class %s DependsOn(%s) is a circular dependency."),*DependsOnClass->GetName(),*Class->GetName());
		}

		// Consider all children of the suspect if any of them are dependent on the source, the suspect is
		// too because it itself is dependent on its children.
		if (!DependsOnClass->HasAnyClassFlags(CLASS_Interface) && !AllClasses.ContainsClass(DependsOnClass))
			FError::Throwf(TEXT("Unparsed class '%s' found while validating DependsOn entries for '%s'"), *DependsOnClass->GetName(), *Class->GetName());

		// Find first base class of DependsOnClass that is not a base class of Class.
		TArray<FClass*> ClassesToParse;
		ClassesToParse.Add(DependsOnClass);

		for ( FClass* ParentClass = DependsOnClass->GetSuperClass(); ParentClass && !ParentClass->HasAnyClassFlags(CLASS_Parsed|CLASS_Intrinsic); ParentClass = ParentClass->GetSuperClass() )
		{
			ClassesToParse.Add(ParentClass);
		}

		while (ClassesToParse.Num() > 0)
		{
			FClass* NextClass = ClassesToParse.Pop();

			ECompilationResult::Type ParseResult = ParseHeaders(AllClasses, HeaderParser, NextClass, true);

			if (ParseResult == ECompilationResult::Succeeded)
			{
				break;
			}

			ParseResult = ParseHeaders(AllClasses, HeaderParser, NextClass, false);

			if (ParseResult != ECompilationResult::Succeeded)
			{
				Result = ParseResult;
				break;
			}
		}
	}

	// Parse the class
	if (!(Class->ClassFlags & CLASS_Parsed))
	{
		UClass* CurrentSuperClass = Class->GetSuperClass();
		ECompilationResult::Type OneClassResult = HeaderParser.ParseHeaderForOneClass(AllClasses, Class);
		if (OneClassResult != ECompilationResult::Succeeded)
		{
			// if we couldn't parse this class, we won't be able to parse its children
			return OneClassResult;
		}

		if (CurrentSuperClass != Class->GetSuperClass())
		{
			// detect a native class that has changed parents and update the tree
			AllClasses.ChangeParentClass(Class);
			AllClasses.Validate();
		}
	}

	// Parse subclasses if instructed to do so.
	if (bParseSubclasses)
	{
		for (auto SubClass : AllClasses.GetDerivedClasses(Class))
		{
			//note: you must always pass in the root tree node here, since we may add new classes to the tree
			// if an manual dependency (through dependson()) is encountered
			ECompilationResult::Type ParseResult = ParseHeaders(AllClasses, HeaderParser, SubClass, bParseSubclasses);

			if (ParseResult == ECompilationResult::FailedDueToHeaderChange)
			{
				return ParseResult;
			}

			if (ParseResult == ECompilationResult::OtherCompilationError)
			{
				Result = ECompilationResult::OtherCompilationError;
			}
		}
	}

	// Success.
	return Result;
}

bool FHeaderParser::DependentClassNameFromHeader(const TCHAR* HeaderFilename, FString& OutClassName)
{
	FString DependentClassName(HeaderFilename);
	const int32 ExtensionIndex = DependentClassName.Find(TEXT("."));
	if (ExtensionIndex != INDEX_NONE)
	{
		// Generate UHeaderName name for this header.
		OutClassName = FString(TEXT("U")) + FPaths::GetBaseFilename(*DependentClassName);
		return true;
	}
	return false;
}

// Begins the process of exporting C++ class declarations for native classes in the specified package
void FHeaderParser::ExportNativeHeaders( UPackage* CurrentPackage, FClasses& AllClasses, bool bAllowSaveExportedHeaders )
{
	// Build a list of header filenames
	TArray<FString>	ClassHeaderFilenames;
	new(ClassHeaderFilenames) FString(TEXT(""));

	bool bExportingHeaders = false;

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Cls = *ClassIt;
		if( (CurrentPackage == NULL || Cls->GetOuter()==CurrentPackage) && GClassStrippedHeaderTextMap.Contains(Cls) && Cls->HasAnyClassFlags(CLASS_Native) && !Cls->HasAnyClassFlags(CLASS_Intrinsic) )
		{
			bExportingHeaders = true;
			break;
		}
	}

	if (bExportingHeaders)
	{
		const static bool bQuiet = !FParse::Param(FCommandLine::Get(),TEXT("VERBOSE"));
		if ( CurrentPackage != NULL )
		{
			if ( bQuiet )
			{
				UE_LOG(LogCompile, Log, TEXT("Exporting native class declarations for %s"), *CurrentPackage->GetName());
			}
			else
			{
				UE_LOG(LogCompile, Warning, TEXT("Exporting native class declarations for %s"), *CurrentPackage->GetName());
			}
		}
		else
		{
			if ( bQuiet )
			{
				UE_LOG(LogCompile, Log, TEXT("Exporting native class declarations"));
			}
			else
			{
				UE_LOG(LogCompile, Warning, TEXT("Exporting native class declarations"));
			}
		}

		// Export native class definitions to package header files.
		FNativeClassHeaderGenerator(CurrentPackage, AllClasses, bAllowSaveExportedHeaders);
	}
}

FHeaderParser::FHeaderParser(FFeedbackContext* InWarn)
: FBaseParser                       ()
, Warn                              (InWarn)
, Class                             (NULL)
, bSpottedAutogeneratedHeaderInclude(false)
, TopNest                           (NULL)
, TopNode                           (NULL)
{
	FScriptLocation::Compiler = this;

	// This should be moved to some sort of config
	StructsWithNoPrefix.Add("uint64");
	StructsWithNoPrefix.Add("uint32");
	StructsWithNoPrefix.Add("double");

	StructsWithTPrefix.Add("IndirectArray");
	StructsWithTPrefix.Add("BitArray");
	StructsWithTPrefix.Add("SparseArray");
	StructsWithTPrefix.Add("Set");
	StructsWithTPrefix.Add("Map");
	StructsWithTPrefix.Add("MultiMap");
	StructsWithTPrefix.Add("SharedPtr");

	// List of legal variable specifiers
	LegalVariableSpecifiers.Add(TEXT("Const"));
	LegalVariableSpecifiers.Add(TEXT("BlueprintReadOnly"));
	LegalVariableSpecifiers.Add(TEXT("Config"));
	LegalVariableSpecifiers.Add(TEXT("GlobalConfig"));
	LegalVariableSpecifiers.Add(TEXT("Localized"));
	LegalVariableSpecifiers.Add(TEXT("EditBlueprint"));
	LegalVariableSpecifiers.Add(TEXT("Transient"));
	LegalVariableSpecifiers.Add(TEXT("Native"));
	LegalVariableSpecifiers.Add(TEXT("DuplicateTransient"));
	LegalVariableSpecifiers.Add(TEXT("Ref"));
	LegalVariableSpecifiers.Add(TEXT("Export"));
	LegalVariableSpecifiers.Add(TEXT("NoClear"));
	LegalVariableSpecifiers.Add(TEXT("EditFixedSize"));
	LegalVariableSpecifiers.Add(TEXT("Replicated"));
	LegalVariableSpecifiers.Add(TEXT("ReplicatedUsing"));
	LegalVariableSpecifiers.Add(TEXT("RepRetry"));
	LegalVariableSpecifiers.Add(TEXT("Interp"));
	LegalVariableSpecifiers.Add(TEXT("NonTransactional"));
	LegalVariableSpecifiers.Add(TEXT("Deprecated"));
	LegalVariableSpecifiers.Add(TEXT("Instanced"));
	LegalVariableSpecifiers.Add(TEXT("BlueprintAssignable"));
	LegalVariableSpecifiers.Add(TEXT("Category"));
	LegalVariableSpecifiers.Add(TEXT("AssetRegistrySearchable"));

	// List of legal delegate parameter counts
	DelegateParameterCountStrings.Add(TEXT("_OneParam"));
	DelegateParameterCountStrings.Add(TEXT("_TwoParams"));
	DelegateParameterCountStrings.Add(TEXT("_ThreeParams"));
	DelegateParameterCountStrings.Add(TEXT("_FourParams"));
	DelegateParameterCountStrings.Add(TEXT("_FiveParams"));
	DelegateParameterCountStrings.Add(TEXT("_SixParams"));
	DelegateParameterCountStrings.Add(TEXT("_SevenParams"));
	DelegateParameterCountStrings.Add(TEXT("_EightParams"));
}

// Returns true if the token is a variable specifier
bool FHeaderParser::IsValidVariableSpecifier(const FToken& Token) const
{
	return (Token.TokenType == TOKEN_Identifier) && LegalVariableSpecifiers.Contains(Token.Identifier);
}

// Throws if a specifier value wasn't provided
void FHeaderParser::RequireSpecifierValue(const FPropertySpecifier& Specifier, bool bRequireExactlyOne)
{
	if (Specifier.Values.Num() == 0)
	{
		FError::Throwf(TEXT("The specifier '%s' must be given a value"), *Specifier.Key);
	}
	else if ((Specifier.Values.Num() != 1) && bRequireExactlyOne)
	{
		FError::Throwf(TEXT("The specifier '%s' must be given exactly one value"), *Specifier.Key);
	}
}

// Throws if a specifier value wasn't provided
FString FHeaderParser::RequireExactlyOneSpecifierValue(const FPropertySpecifier& Specifier)
{
	RequireSpecifierValue(Specifier, /*bRequireExactlyOne*/ true);
	return Specifier.Values[0];
}

// Exports the class to all vailable plugins
void ExportClassToScriptPlugins(UClass* Class, const FManifestModule& Module, IScriptGeneratorPluginInterface& ScriptPlugin)
{
	auto ClassHeaderInfo = GClassGeneratedFileMap.FindRef(Class);
	ScriptPlugin.ExportClass(Class, ClassHeaderInfo.SourceFilename, ClassHeaderInfo.GeneratedFilename, ClassHeaderInfo.bHasChanged);
}

// Exports class tree to all available plugins
void ExportClassTreeToScriptPlugins(const FClassTree* Node, const FManifestModule& Module, IScriptGeneratorPluginInterface& ScriptPlugin)
{
	for (int32 ChildIndex = 0; ChildIndex < Node->NumChildren(); ++ChildIndex)
	{
		auto ChildNode = Node->GetChild(ChildIndex);
		ExportClassToScriptPlugins(ChildNode->GetClass(), Module, ScriptPlugin);
	}

	for (int32 ChildIndex = 0; ChildIndex < Node->NumChildren(); ++ChildIndex)
	{
		auto ChildNode = Node->GetChild(ChildIndex);
		ExportClassTreeToScriptPlugins(ChildNode, Module, ScriptPlugin);
	}
}

// Parse all headers for classes that are inside CurrentPackage.
ECompilationResult::Type FHeaderParser::ParseAllHeadersInside(FClasses& ModuleClasses, FFeedbackContext* Warn, UPackage* CurrentPackage, const FManifestModule& Module, TArray<IScriptGeneratorPluginInterface*>& ScriptPlugins)
{
	// Disable loading of objects outside of this package (or more exactly, objects which aren't UFields, CDO, or templates)
	TGuardValue<bool> AutoRestoreVerifyObjectRefsFlag(GVerifyObjectReferencesOnly, true);

	// Create the header parser and register it as the warning context.
	// Note: This must be declared outside the try block, since the catch block will log into it.
	FHeaderParser HeaderParser(Warn);
	Warn->SetContext(&HeaderParser);

	// Set up a filename for the error context if we don't even get as far parsing a class
	HeaderParser.Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*GClassSourceFileMap[ModuleClasses.GetRootClass()]);

	// Hierarchically parse all classes.
	ECompilationResult::Type Result = ECompilationResult::Succeeded;
#if !PLATFORM_EXCEPTIONS_DISABLED
	try
#endif
	{
		// Parse the headers
		const bool bParseSubclasses = true;

		Result = FHeaderParser::ParseHeaders(ModuleClasses, HeaderParser, ModuleClasses.GetRootClass(), bParseSubclasses);

		// Export the autogenerated code wrappers
		if (Result == ECompilationResult::Succeeded)
		{
			// At this point all headers have been parsed and the header parser will
			// no longer have up to date info about what's being done so unregister it 
			// from the feedback context.
			Warn->SetContext(NULL);

			ExportNativeHeaders(CurrentPackage, ModuleClasses, Module.SaveExportedHeaders);

			// Done with header generation
			if (HeaderParser.LinesParsed > 0)
			{
				UE_LOG(LogCompile, Log,  TEXT("Success: Parsed %i line(s), %i statement(s).\r\n"), HeaderParser.LinesParsed, HeaderParser.StatementsParsed );
			}
			else
			{
				UE_LOG(LogCompile, Log,  TEXT("Success: Everything is up to date") );
			}
		}
	}
#if !PLATFORM_EXCEPTIONS_DISABLED
	catch( TCHAR* ErrorMsg )
	{
		Warn->Log(ELogVerbosity::Error, ErrorMsg);
		Result = GCompilationResult;
	}
#endif
	// Unregister the header parser from the feedback context
	Warn->SetContext(NULL);

	if (Result == ECompilationResult::Succeeded && ScriptPlugins.Num())
	{
		auto RootNode = &ModuleClasses.GetClassTree();
		for (auto Plugin : ScriptPlugins)
		{
			if (Plugin->ShouldExportClassesForModule(Module.Name, Module.ModuleType, Module.GeneratedIncludeDirectory))
			{
				ExportClassToScriptPlugins(RootNode->GetClass(), Module, *Plugin);
				ExportClassTreeToScriptPlugins(RootNode, Module, *Plugin);
			}
		}
	}

	return Result;
}

/** 
 * Returns True if the given class name includes a valid Unreal prefix and matches up with the given original class.
 *
 * @param InNameToCheck - Name w/ potential prefix to check
 * @param OriginalClassName - Name of class w/ no prefix to check against
 */
bool FHeaderParser::ClassNameHasValidPrefix(const FString InNameToCheck, const FString OriginalClassName)
{
	bool bIsLabledDeprecated;
	const FString ClassPrefix = GetClassPrefix( InNameToCheck, bIsLabledDeprecated );

	// If the class is labeled deprecated, don't try to resolve it during header generation, valid results can't be guaranteed.
	if (bIsLabledDeprecated)
	{
		return true;
	}

	if (ClassPrefix.IsEmpty())
	{
		return false;
	}

	FString TestString = FString::Printf(TEXT("%s%s"), *ClassPrefix, *OriginalClassName);

	const bool bNamesMatch = ( InNameToCheck == *TestString );

	return bNamesMatch;
}

void FHeaderParser::ParseClassName(const TCHAR* Temp, FString& ClassName)
{
	// Skip leading whitespace
	while (FChar::IsWhitespace(*Temp))
	{
		++Temp;
	}

	// Run thru characters (note: relying on later code to reject the name for a leading number, etc...)
	const TCHAR* StringStart = Temp;
	while (FChar::IsAlnum(*Temp) || FChar::IsUnderscore(*Temp))
	{
		++Temp;
	}

	ClassName = FString(Temp - StringStart, StringStart);
	if (ClassName.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		// RequiresAPI token for a given module

		//@TODO: UCREMOVAL: Validate the module name
		FString RequiresAPISymbol = ClassName;

		// Now get the real class name
		ClassName.Empty();
		ParseClassName(Temp, ClassName);
	}
}

// Performs a preliminary parse of the text in the specified buffer, pulling out useful information for the header generation process
void FHeaderParser::SimplifiedClassParse(const TCHAR* InBuffer, bool& bIsInterface, TArray<FName>& DependentOn, FString& out_ClassName, FString& out_ParentClassName, int32& out_ClassDeclLine, FStringOutputDevice& ClassHeaderTextStrippedOfCppText)
{
	FHeaderPreParser Parser;
	FString StrLine;
	FString ClassName;
	FString BaseClassName;

	out_ClassDeclLine = 1;

	// Two passes, preprocessor, then looking for the class stuff

	// The layer of multi-line comment we are in.
	int32 CommentDim = 0;
	int32 CurrentLine = 0;
	const TCHAR* Buffer = InBuffer;

	// Preprocessor pass
	while (FParse::Line(&Buffer, StrLine, true))
	{
		CurrentLine++;
		const TCHAR* Str = *StrLine;
		bool bProcess = CommentDim <= 0;	// for skipping nested multi-line comments
		int32 BraceCount = 0;

		if( !bProcess )
		{
			ClassHeaderTextStrippedOfCppText.Logf( TEXT("%s\r\n"), *StrLine );
			continue;
		}

		bool bIf = FParse::Command(&Str,TEXT("#if"));
		if( bIf || FParse::Command(&Str,TEXT("#ifdef")) || FParse::Command(&Str,TEXT("#ifndef")) )
		{
			FStringOutputDevice TextDumpDummy;
			int32 PreprocessorNest = 1;
			FStringOutputDevice* Target = NULL;
			FStringOutputDevice* SpacerTarget = NULL;
			bool bKeepPreprocessorDirectives = true;
			bool bNotCPP = false;
			bool bCPP = false;
			bool bUnknownDirective = false;

			if( bIf && FParse::Command(&Str,TEXT("CPP")) )
			{
				Target = &TextDumpDummy;
				SpacerTarget = &ClassHeaderTextStrippedOfCppText;
				bCPP = true;
			}
			else if( bIf && FParse::Command(&Str,TEXT("!CPP")) )
			{
				Target = &ClassHeaderTextStrippedOfCppText;
				bKeepPreprocessorDirectives = false;
				bNotCPP = true;
			}
			else if (bIf && (FParse::Command(&Str,TEXT("WITH_EDITORONLY_DATA")) || FParse::Command(&Str,TEXT("WITH_EDITOR"))))
			{
				Target = &ClassHeaderTextStrippedOfCppText;
				bUnknownDirective = true;
			}
			else
			{
				// Unknown directives or #ifdef or #ifndef are always treated as CPP
				bUnknownDirective = true;
				Target = &TextDumpDummy;
				SpacerTarget = &ClassHeaderTextStrippedOfCppText;
			}

			if (bKeepPreprocessorDirectives)
			{
				Target->Logf( TEXT("%s\r\n"), *StrLine );
			}
			else
			{
				Target->Logf( TEXT("\r\n") );
			}

			if (SpacerTarget != NULL)
			{
				// Make sure script line numbers don't get out of whack if there is an inline CPP block in there
				SpacerTarget->Logf( TEXT("\r\n") );
			}

			while ((PreprocessorNest > 0) && FParse::Line(&Buffer, StrLine, 1))
			{
				if (SpacerTarget != NULL)
				{
					// Make sure script line numbers don't get out of whack if there is an inline CPP block in there
					SpacerTarget->Logf( TEXT("\r\n") );
				}

				CurrentLine++;
				const TCHAR* Str = *StrLine;
				bool bIsPrep = false;
				if( FParse::Command(&Str,TEXT("#endif")) )
				{
					PreprocessorNest--;
					bIsPrep = true;
				}
				else if( FParse::Command(&Str,TEXT("#if")) || FParse::Command(&Str,TEXT("#ifdef")) || FParse::Command(&Str,TEXT("#ifndef")) )
				{
					PreprocessorNest++;
					bIsPrep = true;
				}
				else if( FParse::Command(&Str,TEXT("#elif")) )
				{
					bIsPrep = true;
				}
				else if(PreprocessorNest == 1 && FParse::Command(&Str,TEXT("#else")))
				{
					if (!bUnknownDirective)
					{
						if (!bNotCPP && !bCPP)
						{
							FError::Throwf(TEXT("Bad preprocessor directive in metadata declaration: %s; Only 'CPP' can have ! or #else directives"),*ClassName);
						}
						Swap(bNotCPP,bCPP);
						if( bCPP)
						{
							Target = &TextDumpDummy;
							SpacerTarget = &ClassHeaderTextStrippedOfCppText;
							bKeepPreprocessorDirectives = true;
							StrLine = TEXT("#if CPP\r\n");
						}
						else
						{
							bKeepPreprocessorDirectives = false;
							Target = &ClassHeaderTextStrippedOfCppText;
							SpacerTarget = NULL;
						}
					}
					bIsPrep = true;
				}

				if (bKeepPreprocessorDirectives || !bIsPrep)
				{
					Target->Logf( TEXT("%s\r\n"), *StrLine );
				}
				else
				{
					Target->Logf( TEXT("\r\n") );
				}
			}
		}
		else if ( FParse::Command(&Str,TEXT("#include")) )
		{
			ClassHeaderTextStrippedOfCppText.Logf( TEXT("%s\r\n"), *StrLine );
		}
		else
		{
			ClassHeaderTextStrippedOfCppText.Logf( TEXT("%s\r\n"), *StrLine );
		}
	}

	// now start over go look for the class

	CommentDim  = 0;
	CurrentLine = 0;
	Buffer      = *ClassHeaderTextStrippedOfCppText;

	const TCHAR* StartOfLine            = Buffer;
	bool         bFoundGeneratedInclude = false;

	while (FParse::Line(&Buffer, StrLine, true))
	{
		CurrentLine++;

		const TCHAR* Str = *StrLine;
		bool bProcess = CommentDim <= 0;	// for skipping nested multi-line comments

		int32 BraceCount = 0;
		if( bProcess && FParse::Command(&Str,TEXT("#if")) )
		{
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#include")) )
		{
			if (bFoundGeneratedInclude)
			{
				out_ClassDeclLine = CurrentLine;
				FError::Throwf(TEXT("#include found after .generated.h file - the .generated.h file should always be the last #include in a header"));
			}

			// Handle #include directives as if they were 'dependson' keywords.
			FString DependsOnHeaderName = Str;

			bFoundGeneratedInclude = DependsOnHeaderName.Contains(TEXT(".generated.h"));
			if (!bFoundGeneratedInclude && DependsOnHeaderName.Len())
			{
				bool  bIsQuotedInclude  = DependsOnHeaderName[0] == '\"';
				int32 HeaderFilenameEnd = DependsOnHeaderName.Find(bIsQuotedInclude ? TEXT("\"") : TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);

				if (HeaderFilenameEnd != INDEX_NONE)
				{
					// Include the extension in the name so that we later know where this entry came from.
					DependentOn.Add(*FPaths::GetCleanFilename(DependsOnHeaderName.Mid(1, HeaderFilenameEnd - 1)));
				}
			}
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#else")) )
		{
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#elif")) )
		{
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#endif")) )
		{
		}
		else
		{
			int32 Pos = INDEX_NONE;
			int32 EndPos = INDEX_NONE;
			int32 StrBegin = INDEX_NONE;
			int32 StrEnd = INDEX_NONE;
				
			bool bEscaped = false;
			for ( int32 CharPos = 0; CharPos < StrLine.Len(); CharPos++ )
			{
				if ( bEscaped )
				{
					bEscaped = false;
				}
				else if ( StrLine[CharPos] == TEXT('\\') )
				{
					bEscaped = true;
				}
				else if ( StrLine[CharPos] == TEXT('\"') )
				{
					if ( StrBegin == INDEX_NONE )
					{
						StrBegin = CharPos;
					}
					else
					{
						StrEnd = CharPos;
						break;
					}
				}
			}

			// Stub out the comments, ignoring anything inside literal strings.
			Pos = StrLine.Find(TEXT("//"));
			if (Pos >= 0)
			{
				if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
					StrLine = StrLine.Left( Pos );

				if (StrLine == TEXT(""))
					continue;
			}

			// look for a / * ... * / block, ignoring anything inside literal strings
			Pos = StrLine.Find(TEXT("/*"));
			EndPos = StrLine.Find(TEXT("*/"));
			if (Pos >= 0)
			{
				if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
				{
					if (EndPos != INDEX_NONE && (EndPos < StrBegin || EndPos > StrEnd))
					{
						StrLine = StrLine.Left(Pos) + StrLine.Mid(EndPos + 2);
						EndPos = INDEX_NONE;
					}
					else 
					{
						StrLine = StrLine.Left( Pos );
						CommentDim++;
					}
				}
				bProcess = CommentDim <= 1;
			}

			if (EndPos >= 0)
			{
				if (StrBegin == INDEX_NONE || EndPos < StrBegin || EndPos > StrEnd)
				{
					StrLine = StrLine.Mid( EndPos+2 );
					CommentDim--;
				}

				bProcess = CommentDim <= 0;
			}

			if (!bProcess || StrLine == TEXT(""))
				continue;

			Str = *StrLine;

			// Get class or interface name
			if (ClassName.IsEmpty())
			{
				if (const TCHAR* UInterfaceMacroDecl = FCString::Strfind(Str, TEXT("UINTERFACE(")))
				{
					out_ClassDeclLine = CurrentLine;
					Parser.ParseClassDeclaration(StartOfLine + (UInterfaceMacroDecl - Str), CurrentLine, TEXT("UINTERFACE"), /*out*/ ClassName, /*out*/ BaseClassName, /*inout*/ DependentOn);
					bIsInterface = true;
				}

				if (const TCHAR* UClassMacroDecl = FCString::Strfind(Str, TEXT("UCLASS(")))
				{
					out_ClassDeclLine = CurrentLine;
					Parser.ParseClassDeclaration(StartOfLine + (UClassMacroDecl - Str), CurrentLine, TEXT("UCLASS"), /*out*/ ClassName, /*out*/ BaseClassName, /*inout*/ DependentOn);
				}
			}
		}
	
		StartOfLine = Buffer;
	}

	out_ClassName = ClassName;
	out_ParentClassName = BaseClassName;
}

/////////////////////////////////////////////////////
// FHeaderPreParser

void FHeaderPreParser::ParseClassDeclaration(const TCHAR* InputText, int32 InLineNumber, const TCHAR* StartingMatchID, FString& out_ClassName, FString& out_BaseClassName, TArray<FName>& inout_ClassNames)
{
	FString ErrorMsg = TEXT("Class declaration");

	ResetParser(InputText, InLineNumber);

	// Require 'UCLASS' or 'UINTERFACE'
	RequireIdentifier(StartingMatchID, *ErrorMsg);

	// New-style UCLASS() syntax
	TMap<FName, FString> MetaData;
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, ErrorMsg, MetaData);

	// Require 'class'
	RequireIdentifier(TEXT("class"), *ErrorMsg);

	// Read the class name
	FString RequiredAPIMacroIfPresent;
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ out_ClassName, /*out*/ RequiredAPIMacroIfPresent, StartingMatchID);

	// Handle inheritance
	if (MatchSymbol(TEXT(":")))
	{
		// Require 'public'
		RequireIdentifier(TEXT("public"), *ErrorMsg);

		// Inherits from something
		FToken BaseClassNameToken;
		if (!GetIdentifier(BaseClassNameToken, true))
		{
			FError::Throwf(TEXT("Expected a base class name"));
		}

		out_BaseClassName = BaseClassNameToken.Identifier;

		// Get additional inheritance links and rack them up as dependencies if they're UObject derived
		while (MatchSymbol(TEXT(",")))
		{
			// Require 'public'
			RequireIdentifier(TEXT("public"), *ErrorMsg);

			FToken InterfaceClassNameToken;
			if (!GetIdentifier(InterfaceClassNameToken, true))
			{
				FError::Throwf(TEXT("Expected an interface class name"));
			}

			FName InterfaceClassName(InterfaceClassNameToken.Identifier);
			inout_ClassNames.Add(InterfaceClassName);
		}
	}

	// Run thru the specifier list looking for dependency links
	for (const auto& Specifier : SpecifiersFound)
	{
		if (Specifier.Key == TEXT("DependsOn"))
		{
			for (auto It2 = Specifier.Values.CreateConstIterator(); It2; ++It2)
			{
				const FString& Value = *It2;

				FName DependentOnClassName = FName(*Value);
				inout_ClassNames.Add(DependentOnClassName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FHeaderParser::DefaultValueStringCppFormatToInnerFormat(const UProperty* Property, const FString& CppForm, FString &OutForm)
{
	OutForm = FString();
	if (!Property || CppForm.IsEmpty())
		return false;

	if (Property->IsA(UClassProperty::StaticClass()) || Property->IsA(UObjectPropertyBase::StaticClass()))
		return FDefaultValueHelper::Is(CppForm, TEXT("NULL")) || FDefaultValueHelper::Is(CppForm, TEXT("nullptr")) || FDefaultValueHelper::Is(CppForm, TEXT("0"));

	if( !Property->IsA(UStructProperty::StaticClass()) )
	{
		if( Property->IsA(UIntProperty::StaticClass()) )
		{
			int32 Value;
			if( FDefaultValueHelper::ParseInt( CppForm, Value) )
			{
				OutForm = FString::FromInt(Value);
			}
		}
		else if( Property->IsA(UByteProperty::StaticClass()) )
		{
			const UEnum* Enum = CastChecked<UByteProperty>(Property)->Enum;
			if( NULL != Enum )
			{
				OutForm = FDefaultValueHelper::GetUnqualifiedEnumValue(FDefaultValueHelper::RemoveWhitespaces(CppForm));
				return ( INDEX_NONE != Enum->FindEnumIndex( *OutForm ) );
			}
			int32 Value;
			if( FDefaultValueHelper::ParseInt( CppForm, Value) )
			{
				OutForm = FString::FromInt(Value);
				return ( 0 <= Value ) && ( 255 >= Value );
			}
		}
		else if( Property->IsA(UFloatProperty::StaticClass()) )
		{
			float Value;
			if( FDefaultValueHelper::ParseFloat( CppForm, Value) )
			{
				OutForm = FString::Printf( TEXT("%f"), Value) ;
			}
		}
		else if( Property->IsA(UDoubleProperty::StaticClass()) )
		{
			double Value;
			if( FDefaultValueHelper::ParseDouble( CppForm, Value) )
			{
				OutForm = FString::Printf( TEXT("%f"), Value) ;
			}
		}
		else if( Property->IsA(UBoolProperty::StaticClass()) )
		{
			if( FDefaultValueHelper::Is(CppForm, TEXT("true")) || 
				FDefaultValueHelper::Is(CppForm, TEXT("false")) )
			{
				OutForm = FDefaultValueHelper::RemoveWhitespaces( CppForm );
			}
		}
		else if( Property->IsA(UNameProperty::StaticClass()) )
		{
			if(FDefaultValueHelper::Is( CppForm, TEXT("NAME_None") ))
			{
				OutForm = TEXT("None");
				return true;
			}
			return FDefaultValueHelper::StringFromCppString(CppForm, TEXT("FName"), OutForm);
		}
		else if( Property->IsA(UTextProperty::StaticClass()) )
		{
			return FDefaultValueHelper::StringFromCppString(CppForm, TEXT("FText"), OutForm);
		}
		else if( Property->IsA(UStrProperty::StaticClass()) )
		{
			return FDefaultValueHelper::StringFromCppString(CppForm, TEXT("FString"), OutForm);
		}
	}
	else 
	{
		// Cache off the struct types, in case we need them later
		static const UScriptStruct* VectorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Vector"));
		static const UScriptStruct* Vector2DStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Vector2D"));
		static const UScriptStruct* RotatorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Rotator"));
		static const UScriptStruct* LinearColorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("LinearColor"));

		const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
		if( StructProperty->Struct == VectorStruct )
		{
			if(FDefaultValueHelper::Is( CppForm, TEXT("FVector::ZeroVector") ))
			{
				return true;
			}
			if(FDefaultValueHelper::Is(CppForm, TEXT("FVector::UpVector")))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"),
					FVector::UpVector.X, FVector::UpVector.Y, FVector::UpVector.Z);
			}
			FString Parameters;
			if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector"), Parameters) )
			{
				if( FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")) )
				{
					return true;
				}
				FVector Vector;
				if(FDefaultValueHelper::ParseVector(Parameters, Vector))
				{
					OutForm = FString::Printf(TEXT("%f,%f,%f"),
						Vector.X, Vector.Y, Vector.Z);
				}
			}
		}
		else if( StructProperty->Struct == RotatorStruct )
		{
			if(FDefaultValueHelper::Is( CppForm, TEXT("FRotator::ZeroRotator") ))
			{
				return true;
			}
			FString Parameters;
			if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FRotator"), Parameters) )
			{
				if( FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")) )
				{
					return true;
				}
				FRotator Rotator;
				if(FDefaultValueHelper::ParseRotator(Parameters, Rotator))
				{
					OutForm = FString::Printf(TEXT("%f,%f,%f"),
						Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
				}
			}
		}
		else if( StructProperty->Struct == Vector2DStruct )
		{
			if(FDefaultValueHelper::Is( CppForm, TEXT("FVector2D::ZeroVector") ))
			{
				return true;
			}
			if(FDefaultValueHelper::Is(CppForm, TEXT("FVector2D::UnitVector")))
			{
				OutForm = FString::Printf(TEXT("(X=%3.3f,Y=%3.3f)"),
					FVector2D::UnitVector.X, FVector2D::UnitVector.Y);
			}
			FString Parameters;
			if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector2D"), Parameters) )
			{
				if( FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")) )
				{
					return true;
				}
				FVector2D Vector2D;
				if(FDefaultValueHelper::ParseVector2D(Parameters, Vector2D))
				{
					OutForm = FString::Printf(TEXT("(X=%3.3f,Y=%3.3f)"),
						Vector2D.X, Vector2D.Y);
				}
			}
		}
		else if( StructProperty->Struct == LinearColorStruct )
		{
			if( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::White") ) )
			{
				OutForm = FLinearColor::White.ToString();
			}
			else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::Gray") ) )
			{
				OutForm = FLinearColor::Gray.ToString();
			}
			else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::Black") ) )
			{
				OutForm = FLinearColor::Black.ToString();
			}
			else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::Transparent") ) )
			{
				OutForm = FLinearColor::Transparent.ToString();
			}
			else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::Red") ) )
			{
				OutForm = FLinearColor::Red.ToString();
			}
			else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::Green") ) )
			{
				OutForm = FLinearColor::Green.ToString();
			}
			else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::Blue") ) )
			{
				OutForm = FLinearColor::Blue.ToString();
			}
			else if ( FDefaultValueHelper::Is( CppForm, TEXT("FLinearColor::Yellow") ) )
			{
				OutForm = FLinearColor::Yellow.ToString();
			}
			else
			{
				FString Parameters;
				if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FLinearColor"), Parameters) )
				{
					if( FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")) )
					{
						return true;
					}
					FLinearColor Color;
					if( FDefaultValueHelper::ParseLinearColor(Parameters, Color) )
					{
						OutForm = Color.ToString();
					}
				}
			}
		}
	}

	return !OutForm.IsEmpty();
}

bool FHeaderParser::TryToMatchConstructorParameterList(FToken Token)
{
	FToken PotentialParenthesisToken;
	if (!GetToken(PotentialParenthesisToken))
	{
		return false;
	}

	if (!PotentialParenthesisToken.Matches(TEXT("(")))
	{
		UngetToken(PotentialParenthesisToken);
		return false;
	}

	auto* ClassData = GScriptHelper.FindClassData(Class);

	ClassData->bConstructorDeclared = true;

	if (!ClassData->bDefaultConstructorDeclared && MatchSymbol(TEXT(")")))
	{
		ClassData->bDefaultConstructorDeclared = true;
	}
	else if (!ClassData->bObjectInitializerConstructorDeclared)
	{
		FToken ObjectInitializerParamParsingToken;

		bool bIsConst = false;
		bool bIsRef = false;
		int32 ParenthesesNestingLevel = 1;

		while (ParenthesesNestingLevel && GetToken(ObjectInitializerParamParsingToken))
		{
			// Template instantiation or additional parameter excludes ObjectInitializer constructor.
			if (ObjectInitializerParamParsingToken.Matches(TEXT(",")) || ObjectInitializerParamParsingToken.Matches(TEXT("<")))
			{
				ClassData->bObjectInitializerConstructorDeclared = false;
				break;
			}

			if (ObjectInitializerParamParsingToken.Matches(TEXT("(")))
			{
				ParenthesesNestingLevel++;
				continue;
			}

			if (ObjectInitializerParamParsingToken.Matches(TEXT(")")))
			{
				ParenthesesNestingLevel--;
				continue;
			}

			if (ObjectInitializerParamParsingToken.Matches(TEXT("const")))
			{
				bIsConst = true;
				continue;
			}

			if (ObjectInitializerParamParsingToken.Matches(TEXT("&")))
			{
				bIsRef = true;
				continue;
			}

			if (ObjectInitializerParamParsingToken.Matches(TEXT("FObjectInitializer"))
				|| ObjectInitializerParamParsingToken.Matches(TEXT("FPostConstructInitializeProperties")) // Deprecated, but left here, so it won't break legacy code.
				)
			{
				ClassData->bObjectInitializerConstructorDeclared = true;
			}
		}

		// Parse until finish.
		while (ParenthesesNestingLevel && GetToken(ObjectInitializerParamParsingToken))
		{
			if (ObjectInitializerParamParsingToken.Matches(TEXT("(")))
			{
				ParenthesesNestingLevel++;
				continue;
			}

			if (ObjectInitializerParamParsingToken.Matches(TEXT(")")))
			{
				ParenthesesNestingLevel--;
				continue;
			}
		}

		ClassData->bObjectInitializerConstructorDeclared = ClassData->bObjectInitializerConstructorDeclared && bIsRef && bIsConst;
	}

	// Optionally match semicolon.
	if (!MatchSymbol(TEXT(";")))
	{
		// If not matched a semicolon, this is inline constructor definition. We have to skip it.
		UngetToken(Token); // Resets input stream to the initial token.
		GetToken(Token); // Re-gets the initial token to start constructor definition skip.
		return SkipDeclaration(Token);
	}

	return true;
}

void FHeaderParser::SkipDeprecatedMacroIfNecessary()
{
	if (!MatchIdentifier(TEXT("DEPRECATED")))
	{
		return;
	}

	FToken Token;
	// DEPRECATED(Version, "Message")
	RequireSymbol(TEXT("("), TEXT("DEPRECATED macro"));
	if (GetToken(Token) && (Token.Type != CPT_Float || Token.TokenType != TOKEN_Const))
	{
		FError::Throwf(TEXT("Expected engine version in DEPRECATED macro"));
	}

	RequireSymbol(TEXT(","), TEXT("DEPRECATED macro"));
	if (GetToken(Token) && (Token.Type != CPT_String || Token.TokenType != TOKEN_Const))
	{
		FError::Throwf(TEXT("Expected deprecation message in DEPRECATED macro"));
	}

	RequireSymbol(TEXT(")"), TEXT("DEPRECATED macro"));
}
