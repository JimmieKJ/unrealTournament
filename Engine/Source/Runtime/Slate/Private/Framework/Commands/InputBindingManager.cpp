// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/* FUserDefinedChords helper class
 *****************************************************************************/

/** An identifier for a user defined chord */
struct FUserDefinedChordKey
{
	FUserDefinedChordKey()
	{
	}

	FUserDefinedChordKey(const FName InBindingContext, const FName InCommandName)
		: BindingContext(InBindingContext)
		, CommandName(InCommandName)
	{
	}

	bool operator==(const FUserDefinedChordKey& Other) const
	{
		return BindingContext == Other.BindingContext && CommandName == Other.CommandName;
	}

	bool operator!=(const FUserDefinedChordKey& Other) const
	{
		return !(*this == Other);
	}

	FName BindingContext;
	FName CommandName;
};

uint32 GetTypeHash( const FUserDefinedChordKey& Key )
{
	return GetTypeHash(Key.BindingContext) ^ GetTypeHash(Key.CommandName);
}

class FUserDefinedChords
{
public:
	void LoadChords();
	void SaveChords() const;
	bool GetUserDefinedChord( const FName BindingContext, const FName CommandName, FInputChord& OutUserDefinedChord ) const;
	void SetUserDefinedChord( const FUICommandInfo& CommandInfo );
	/** Remove all user defined chords */
	void RemoveAll();
private:
	/* Mapping from a chord key to the user defined chord */
	typedef TMap<FUserDefinedChordKey, FInputChord> FChordsMap;
	TSharedPtr<FChordsMap> Chords;
};

void FUserDefinedChords::LoadChords()
{
	if( !Chords.IsValid() )
	{
		Chords = MakeShareable( new FChordsMap );

		// First, try and load the chords from their new location in the ini file
		// Failing that, try and load them from the older txt file
		TArray<FString> ChordJsonArray;
		bool bFoundChords = (GConfig->GetArray(TEXT("UserDefinedChords"), TEXT("UserDefinedChords"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
		if (!bFoundChords)
		{
			// Backwards compat for when it was named gestures
			bFoundChords = (GConfig->GetArray(TEXT("UserDefinedGestures"), TEXT("UserDefinedGestures"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
		}

		if(bFoundChords)
		{
			// This loads an array of JSON strings representing the FUserDefinedChordKey and FInputChord in a single JSON object
			for(const FString& ChordJson : ChordJsonArray)
			{
				const FString UnescapedContent = FRemoteConfig::ReplaceIniSpecialCharWithChar(ChordJson).ReplaceEscapedCharWithChar();

				TSharedPtr<FJsonObject> ChordInfoObj;
				auto JsonReader = TJsonReaderFactory<>::Create( UnescapedContent );
				if( FJsonSerializer::Deserialize( JsonReader, ChordInfoObj ) )
				{
					const TSharedPtr<FJsonValue> BindingContextObj = ChordInfoObj->Values.FindRef( TEXT("BindingContext") );
					const TSharedPtr<FJsonValue> CommandNameObj = ChordInfoObj->Values.FindRef( TEXT("CommandName") );
					const TSharedPtr<FJsonValue> CtrlObj = ChordInfoObj->Values.FindRef( TEXT("Control") );
					const TSharedPtr<FJsonValue> AltObj = ChordInfoObj->Values.FindRef( TEXT("Alt") );
					const TSharedPtr<FJsonValue> ShiftObj = ChordInfoObj->Values.FindRef( TEXT("Shift") );
					const TSharedPtr<FJsonValue> CmdObj = ChordInfoObj->Values.FindRef( TEXT("Command") );
					const TSharedPtr<FJsonValue> KeyObj = ChordInfoObj->Values.FindRef( TEXT("Key") );

					const FName BindingContext = *BindingContextObj->AsString();
					const FName CommandName = *CommandNameObj->AsString();

					const FUserDefinedChordKey ChordKey(BindingContext, CommandName);
					FInputChord& UserDefinedChord = Chords->FindOrAdd(ChordKey);

					UserDefinedChord = FInputChord(*KeyObj->AsString(), EModifierKey::FromBools(
																				CtrlObj->AsBool(), 
																				AltObj->AsBool(), 
																				ShiftObj->AsBool(), 
																				CmdObj.IsValid() ? CmdObj->AsBool() : false // Old config files may not have this
																			));
				}
			}
		}
		else
		{
			TSharedPtr<FJsonObject> ChordsObj;

			// This loads a JSON object containing BindingContexts, containing objects of CommandNames, containing the FInputChord information
			FString Content;
			bool bFoundContent = (GConfig->GetArray(TEXT("UserDefinedChords"), TEXT("Content"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
			if (!bFoundContent)
			{
				// Backwards compat for when it was named gestures
				bFoundChords = (GConfig->GetArray(TEXT("UserDefinedGestures"), TEXT("Content"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
			}
			if (bFoundContent)
			{
				const FString UnescapedContent = FRemoteConfig::ReplaceIniSpecialCharWithChar(Content).ReplaceEscapedCharWithChar();

				auto JsonReader = TJsonReaderFactory<>::Create( UnescapedContent );
				FJsonSerializer::Deserialize( JsonReader, ChordsObj );
			}

			if (!ChordsObj.IsValid())
			{
				// Chords have not been loaded from the ini file, try reading them from the txt file now
				TSharedPtr<FArchive> Ar = MakeShareable( IFileManager::Get().CreateFileReader( *( FPaths::GameSavedDir() / TEXT("Preferences/EditorKeyBindings.txt") ) ) );
				if( Ar.IsValid() )
				{
					auto TextReader = TJsonReaderFactory<ANSICHAR>::Create( Ar.Get() );
					FJsonSerializer::Deserialize( TextReader, ChordsObj );
				}
			}

			if (ChordsObj.IsValid())
			{
				for(const auto& BindingContextInfo : ChordsObj->Values)
				{
					const FName BindingContext = *BindingContextInfo.Key;
					TSharedPtr<FJsonObject> BindingContextObj = BindingContextInfo.Value->AsObject();
					for(const auto& CommandInfo : BindingContextObj->Values)
					{
						const FName CommandName = *CommandInfo.Key;
						TSharedPtr<FJsonObject> CommandObj = CommandInfo.Value->AsObject();

						const TSharedPtr<FJsonValue> CtrlObj = CommandObj->Values.FindRef( TEXT("Control") );
						const TSharedPtr<FJsonValue> AltObj = CommandObj->Values.FindRef( TEXT("Alt") );
						const TSharedPtr<FJsonValue> ShiftObj = CommandObj->Values.FindRef( TEXT("Shift") );
						const TSharedPtr<FJsonValue> CmdObj = CommandObj->Values.FindRef( TEXT("Command") );
						const TSharedPtr<FJsonValue> KeyObj = CommandObj->Values.FindRef( TEXT("Key") );

						const FUserDefinedChordKey ChordKey(BindingContext, CommandName);
						FInputChord& UserDefinedChord = Chords->FindOrAdd(ChordKey);

						UserDefinedChord = FInputChord(*KeyObj->AsString(), EModifierKey::FromBools(
																					CtrlObj->AsBool(), 
																					AltObj->AsBool(), 
																					ShiftObj->AsBool(), 
																					CmdObj.IsValid() ? CmdObj->AsBool() : false // Old config files may not have this
																				));
					}
				}
			}
		}
	}
}

void FUserDefinedChords::SaveChords() const
{
	if( Chords.IsValid() )
	{
		FString ChordRawJsonContent;
		TArray<FString> ChordJsonArray;
		for(const auto& ChordInfo : *Chords)
		{
			TSharedPtr<FJsonValueObject> ChordInfoValueObj = MakeShareable( new FJsonValueObject( MakeShareable( new FJsonObject ) ) );
			TSharedPtr<FJsonObject> ChordInfoObj = ChordInfoValueObj->AsObject();

			// Set the chord values for the command
			ChordInfoObj->Values.Add( TEXT("BindingContext"), MakeShareable( new FJsonValueString( ChordInfo.Key.BindingContext.ToString() ) ) );
			ChordInfoObj->Values.Add( TEXT("CommandName"), MakeShareable( new FJsonValueString( ChordInfo.Key.CommandName.ToString() ) ) );
			ChordInfoObj->Values.Add( TEXT("Control"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsControl() ) ) );
			ChordInfoObj->Values.Add( TEXT("Alt"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsAlt() ) ) );
			ChordInfoObj->Values.Add( TEXT("Shift"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsShift() ) ) );
			ChordInfoObj->Values.Add( TEXT("Command"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsCommand() ) ) );
			ChordInfoObj->Values.Add( TEXT("Key"), MakeShareable( new FJsonValueString( ChordInfo.Value.Key.ToString() ) ) );

			auto JsonWriter = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create( &ChordRawJsonContent );
			FJsonSerializer::Serialize( ChordInfoObj.ToSharedRef(), JsonWriter );

			const FString EscapedContent = FRemoteConfig::ReplaceIniCharWithSpecialChar(ChordRawJsonContent).ReplaceCharWithEscapedChar();
			ChordJsonArray.Add(EscapedContent);
		}

		GConfig->SetArray(TEXT("UserDefinedChords"), TEXT("UserDefinedChords"), ChordJsonArray, GEditorKeyBindingsIni);

		// Clean up old keys (if they still exist)
		GConfig->RemoveKey(TEXT("UserDefinedGestures"), TEXT("UserDefinedGestures"), GEditorKeyBindingsIni);
		GConfig->RemoveKey(TEXT("UserDefinedChords"), TEXT("Content"), GEditorKeyBindingsIni);
		GConfig->RemoveKey(TEXT("UserDefinedChords"), TEXT("Content"), GEditorKeyBindingsIni);
	}
}

bool FUserDefinedChords::GetUserDefinedChord( const FName BindingContext, const FName CommandName, FInputChord& OutUserDefinedChord ) const
{
	bool bResult = false;

	if( Chords.IsValid() )
	{
		const FUserDefinedChordKey ChordKey(BindingContext, CommandName);
		const FInputChord* const UserDefinedChordPtr = Chords->Find(ChordKey);
		if( UserDefinedChordPtr )
		{
			OutUserDefinedChord = *UserDefinedChordPtr;
			bResult = true;
		}
	}

	return bResult;
}

void FUserDefinedChords::SetUserDefinedChord( const FUICommandInfo& CommandInfo )
{
	if( Chords.IsValid() )
	{
		const FName BindingContext = CommandInfo.GetBindingContext();
		const FName CommandName = CommandInfo.GetCommandName();

		// Find or create the command context
		const FUserDefinedChordKey ChordKey(BindingContext, CommandName);
		FInputChord& UserDefinedChord = Chords->FindOrAdd(ChordKey);

		// Save an empty invalid chord if one was not set
		// This is an indication that the user doesn't want this bound and not to use the default chord
		const TSharedPtr<const FInputChord> InputChord = CommandInfo.GetActiveChord();
		UserDefinedChord = (InputChord.IsValid()) ? *InputChord : FInputChord();
	}
}

void FUserDefinedChords::RemoveAll()
{
	Chords = MakeShareable( new FChordsMap );
}


/* FInputBindingManager structors
 *****************************************************************************/

FInputBindingManager& FInputBindingManager::Get()
{
	static FInputBindingManager* Instance= NULL;
	if( Instance == NULL )
	{
		Instance = new FInputBindingManager();
	}

	return *Instance;
}


/* FInputBindingManager interface
 *****************************************************************************/

/**
 * Gets the user defined chord (if any) from the provided command name
 * 
 * @param InBindingContext	The context in which the command is active
 * @param InCommandName		The name of the command to get the chord from
 */
bool FInputBindingManager::GetUserDefinedChord( const FName InBindingContext, const FName InCommandName, FInputChord& OutUserDefinedChord )
{
	if( !UserDefinedChords.IsValid() )
	{
		UserDefinedChords = MakeShareable( new FUserDefinedChords );
		UserDefinedChords->LoadChords();
	}

	return UserDefinedChords->GetUserDefinedChord( InBindingContext, InCommandName, OutUserDefinedChord );
}

void FInputBindingManager::CheckForDuplicateDefaultChords( const FBindingContext& InBindingContext, TSharedPtr<FUICommandInfo> InCommandInfo ) const
{
	const bool bCheckDefault = true;
	TSharedPtr<FUICommandInfo> ExistingInfo = GetCommandInfoFromInputChord( InBindingContext.GetContextName(), InCommandInfo->DefaultChord, bCheckDefault );
	if( ExistingInfo.IsValid() )
	{
		if( ExistingInfo->CommandName != InCommandInfo->CommandName )
		{
			// Two different commands with the same name in the same context or parent context
			UE_LOG(LogSlate, Fatal, TEXT("The command '%s.%s' has the same default chord as '%s.%s' [%s]"), 
				*InCommandInfo->BindingContext.ToString(),
				*InCommandInfo->CommandName.ToString(), 
				*ExistingInfo->BindingContext.ToString(),
				*ExistingInfo->CommandName.ToString(), 
				*InCommandInfo->DefaultChord.GetInputText().ToString() );
		}
	}
}


void FInputBindingManager::NotifyActiveChordChanged( const FUICommandInfo& CommandInfo )
{
	FContextEntry& ContextEntry = ContextMap.FindChecked( CommandInfo.GetBindingContext() );

	// Slow but doesn't happen frequently
	for( FChordMap::TIterator It( ContextEntry.ChordToCommandInfoMap ); It; ++It )
	{
		// Remove the currently active chord from the map if one exists
		if( It.Value() == CommandInfo.GetCommandName() )
		{
			It.RemoveCurrent();
			// There should only be one active chord
			break;
		}
	}

	if( CommandInfo.GetActiveChord()->IsValidChord() )
	{
		checkSlow( !ContextEntry.ChordToCommandInfoMap.Contains( *CommandInfo.GetActiveChord() ) )
		ContextEntry.ChordToCommandInfoMap.Add( *CommandInfo.GetActiveChord(), CommandInfo.GetCommandName() );
	}
	

	// The user defined chords should have already been created
	check( UserDefinedChords.IsValid() );

	UserDefinedChords->SetUserDefinedChord( CommandInfo );

	// Broadcast the chord event when a new one is added
	OnUserDefinedChordChanged.Broadcast(CommandInfo);
}

void FInputBindingManager::SaveInputBindings()
{
	if( UserDefinedChords.IsValid() )
	{
		UserDefinedChords->SaveChords();
	}
}

void FInputBindingManager::RemoveUserDefinedChords()
{
	if( UserDefinedChords.IsValid() )
	{
		UserDefinedChords->RemoveAll();
		UserDefinedChords->SaveChords();
	}
}

void FInputBindingManager::GetCommandInfosFromContext( const FName InBindingContext, TArray< TSharedPtr<FUICommandInfo> >& OutCommandInfos ) const
{
	ContextMap.FindRef( InBindingContext ).CommandInfoMap.GenerateValueArray( OutCommandInfos );
}

void FInputBindingManager::CreateInputCommand( const TSharedRef<FBindingContext>& InBindingContext, TSharedRef<FUICommandInfo> InCommandInfo )
{
	check( InCommandInfo->BindingContext == InBindingContext->GetContextName() );

	// The command name should be valid
	check( InCommandInfo->CommandName != NAME_None );

	// Should not have already created a chord for this command
	check( !InCommandInfo->ActiveChord->IsValidChord() );
	
	const FName ContextName = InBindingContext->GetContextName();

	FContextEntry& ContextEntry = ContextMap.FindOrAdd( ContextName );
	
	// Our parent context must exist.
	check( InBindingContext->GetContextParent() == NAME_None || ContextMap.Find( InBindingContext->GetContextParent() ) != NULL );

	FCommandInfoMap& CommandInfoMap = ContextEntry.CommandInfoMap;

	if( !ContextEntry.BindingContext.IsValid() )
	{
		ContextEntry.BindingContext = InBindingContext;
	}

	if( InBindingContext->GetContextParent() != NAME_None  )
	{
		check( InBindingContext->GetContextName() != InBindingContext->GetContextParent() );
		// Set a mapping from the parent of the current context to the current context
		ParentToChildMap.AddUnique( InBindingContext->GetContextParent(), InBindingContext->GetContextName() );
	}

	if( InCommandInfo->DefaultChord.IsValidChord() )
	{
		CheckForDuplicateDefaultChords( *InBindingContext, InCommandInfo );
	}
	
	{
		TSharedPtr<FUICommandInfo> ExistingInfo = CommandInfoMap.FindRef( InCommandInfo->CommandName );
		ensureMsgf( !ExistingInfo.IsValid(), TEXT("A command with name %s already exists in context %s"), *InCommandInfo->CommandName.ToString(), *InBindingContext->GetContextName().ToString() );
	}

	// Add the command info to the list of known infos.  It can only exist once.
	CommandInfoMap.Add( InCommandInfo->CommandName, InCommandInfo );

	// See if there are user defined chords for this command
	FInputChord UserDefinedChord;
	bool bFoundUserDefinedChord = GetUserDefinedChord( ContextName, InCommandInfo->CommandName, UserDefinedChord );

	
	if( !bFoundUserDefinedChord && InCommandInfo->DefaultChord.IsValidChord() )
	{
		// Find any existing command with the same chord 
		// This is for inconsistency between default and user defined chord.  We need to make sure that if default chords are changed to a chord that a user set to a different command, that the default chord doesn't replace
		// the existing commands chord. Note: Duplicate default chords are found above in CheckForDuplicateDefaultChords
		FName ExisingCommand = ContextEntry.ChordToCommandInfoMap.FindRef( InCommandInfo->DefaultChord );

		if( ExisingCommand == NAME_None )
		{
			// No existing command has a user defined chord and no user defined chord is available for this command 
			TSharedRef<FInputChord> NewChord = MakeShareable( new FInputChord( InCommandInfo->DefaultChord ) );
			InCommandInfo->ActiveChord = NewChord;
		}

	}
	else if( bFoundUserDefinedChord )
	{
		// Find any existing command with the same chord 
		// This is for inconsistency between default and user defined chord.  We need to make sure that if default chords are changed to a chord that a user set to a different command, that the default chord doesn't replace
		// the existing commands chord.
		FName ExisingCommandName = ContextEntry.ChordToCommandInfoMap.FindRef( UserDefinedChord );

		if( ExisingCommandName != NAME_None )
		{
			// Get the command with using the same chord
			TSharedPtr<FUICommandInfo> ExistingInfo = CommandInfoMap.FindRef( ExisingCommandName );
			if( *ExistingInfo->ActiveChord != ExistingInfo->DefaultChord )
			{
				// two user defined chords are the same within a context.  If the keybinding editor was used this wont happen so this must have been directly a modified user setting file
				UE_LOG(LogSlate, Error, TEXT("Duplicate user defined chords found: [%s,%s].  Chord for %s being removed"), *InCommandInfo->GetLabel().ToString(), *ExistingInfo->GetLabel().ToString(), *ExistingInfo->GetLabel().ToString() );
			}
			ContextEntry.ChordToCommandInfoMap.Remove( *ExistingInfo->ActiveChord );
			// Remove the existing chord. 
			ExistingInfo->ActiveChord = MakeShareable( new FInputChord() );
		
		}

		TSharedRef<FInputChord> NewChord = MakeShareable( new FInputChord( UserDefinedChord ) );
		// Set the active chord on the command info
		InCommandInfo->ActiveChord = NewChord;
	}

	// If the active chord is valid, map the chord to the map for fast lookup when processing bindings
	if( InCommandInfo->ActiveChord->IsValidChord() )
	{
		checkSlow( !ContextEntry.ChordToCommandInfoMap.Contains( *InCommandInfo->GetActiveChord() ) );
		ContextEntry.ChordToCommandInfoMap.Add( *InCommandInfo->GetActiveChord(), InCommandInfo->GetCommandName() );
	}
}

const TSharedPtr<FUICommandInfo> FInputBindingManager::FindCommandInContext( const FName InBindingContext, const FInputChord& InChord, bool bCheckDefault ) const
{
	const FContextEntry& ContextEntry = ContextMap.FindRef( InBindingContext );
	
	TSharedPtr<FUICommandInfo> FoundCommand = NULL;

	if( bCheckDefault )
	{
		const FCommandInfoMap& InfoMap = ContextEntry.CommandInfoMap;
		for( FCommandInfoMap::TConstIterator It(InfoMap); It && !FoundCommand.IsValid(); ++It )
		{
			const FUICommandInfo& CommandInfo = *It.Value();
			if( CommandInfo.DefaultChord == InChord )
			{
				FoundCommand = It.Value();
			}	
		}
	}
	else
	{
		// faster lookup for active chords
		FName CommandName = ContextEntry.ChordToCommandInfoMap.FindRef( InChord );
		if( CommandName != NAME_None )
		{
			FoundCommand = ContextEntry.CommandInfoMap.FindChecked( CommandName );
		}
	}

	return FoundCommand;
}

const TSharedPtr<FUICommandInfo> FInputBindingManager::FindCommandInContext( const FName InBindingContext, const FName CommandName ) const 
{
	const FContextEntry& ContextEntry = ContextMap.FindRef( InBindingContext );
	
	return ContextEntry.CommandInfoMap.FindRef( CommandName );
}


void FInputBindingManager::GetAllChildContexts( const FName InBindingContext, TArray<FName>& AllChildren ) const
{
	AllChildren.Add( InBindingContext );

	TArray<FName> TempChildren;
	ParentToChildMap.MultiFind( InBindingContext, TempChildren );
	for( int32 ChildIndex = 0; ChildIndex < TempChildren.Num(); ++ChildIndex )
	{
		GetAllChildContexts( TempChildren[ChildIndex], AllChildren );
	}
}

const TSharedPtr<FUICommandInfo> FInputBindingManager::GetCommandInfoFromInputChord( const FName InBindingContext, const FInputChord& InChord, bool bCheckDefault ) const
{
	TSharedPtr<FUICommandInfo> FoundCommand = NULL;

	FName CurrentContext = InBindingContext;
	while( CurrentContext != NAME_None && !FoundCommand.IsValid() )
	{
		const FContextEntry& ContextEntry = ContextMap.FindRef( CurrentContext );

		FoundCommand = FindCommandInContext( CurrentContext, InChord, bCheckDefault );

		CurrentContext = ContextEntry.BindingContext->GetContextParent();
	}
	
	if( !FoundCommand.IsValid() )
	{
		TArray<FName> Children;
		GetAllChildContexts( InBindingContext, Children );

		for( int32 ContextIndex = 0; ContextIndex < Children.Num() && !FoundCommand.IsValid(); ++ContextIndex )
		{
			FoundCommand = FindCommandInContext( Children[ContextIndex], InChord, bCheckDefault );
		}
	}
	

	return FoundCommand;
}

/**
 * Returns a list of all known input contexts
 *
 * @param OutInputContexts	The generated list of contexts
 * @return A list of all known input contexts                   
 */
void FInputBindingManager::GetKnownInputContexts( TArray< TSharedPtr<FBindingContext> >& OutInputContexts ) const
{
	for( TMap< FName, FContextEntry >::TConstIterator It(ContextMap); It; ++It )
	{
		OutInputContexts.Add( It.Value().BindingContext );
	}
}

TSharedPtr<FBindingContext> FInputBindingManager::GetContextByName( const FName& InContextName )
{
	FContextEntry* FindResult = ContextMap.Find( InContextName );
	if ( FindResult == NULL )
	{
		return TSharedPtr<FBindingContext>();
	}
	else
	{
		return FindResult->BindingContext;
	}
}

void FInputBindingManager::RemoveContextByName( const FName& InContextName )
{
	ContextMap.Remove(InContextName);
}
