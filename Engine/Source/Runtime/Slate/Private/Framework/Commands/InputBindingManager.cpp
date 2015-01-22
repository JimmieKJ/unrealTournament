// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/* FUserDefinedGestures helper class
 *****************************************************************************/

/** An identifier for a user defined gesture */
struct FUserDefinedGestureKey
{
	FUserDefinedGestureKey()
	{
	}

	FUserDefinedGestureKey(const FName InBindingContext, const FName InCommandName)
		: BindingContext(InBindingContext)
		, CommandName(InCommandName)
	{
	}

	bool operator==(const FUserDefinedGestureKey& Other) const
	{
		return BindingContext == Other.BindingContext && CommandName == Other.CommandName;
	}

	bool operator!=(const FUserDefinedGestureKey& Other) const
	{
		return !(*this == Other);
	}

	FName BindingContext;
	FName CommandName;
};

uint32 GetTypeHash( const FUserDefinedGestureKey& Key )
{
	return GetTypeHash(Key.BindingContext) ^ GetTypeHash(Key.CommandName);
}

class FUserDefinedGestures
{
public:
	void LoadGestures();
	void SaveGestures() const;
	bool GetUserDefinedGesture( const FName BindingContext, const FName CommandName, FInputGesture& OutUserDefinedGesture ) const;
	void SetUserDefinedGesture( const FUICommandInfo& CommandInfo );
	/** Remove all user defined gestures */
	void RemoveAll();
private:
	/* Mapping from a gesture key to the user defined gesture */
	typedef TMap<FUserDefinedGestureKey, FInputGesture> FGesturesMap;
	TSharedPtr<FGesturesMap> Gestures;
};

void FUserDefinedGestures::LoadGestures()
{
	if( !Gestures.IsValid() )
	{
		Gestures = MakeShareable( new FGesturesMap );

		// First, try and load the gestures from their new location in the ini file
		// Failing that, try and load them from the older txt file
		TArray<FString> GestureJsonArray;
		if( GConfig->GetArray(TEXT("UserDefinedGestures"), TEXT("UserDefinedGestures"), GestureJsonArray, GEditorKeyBindingsIni) )
		{
			// This loads an array of JSON strings representing the FUserDefinedGestureKey and FInputGesture in a single JSON object
			for(const FString& GestureJson : GestureJsonArray)
			{
				const FString UnescapedContent = FRemoteConfig::ReplaceIniSpecialCharWithChar(GestureJson).ReplaceEscapedCharWithChar();

				TSharedPtr<FJsonObject> GestureInfoObj;
				auto JsonReader = TJsonReaderFactory<>::Create( UnescapedContent );
				if( FJsonSerializer::Deserialize( JsonReader, GestureInfoObj ) )
				{
					const TSharedPtr<FJsonValue> BindingContextObj = GestureInfoObj->Values.FindRef( TEXT("BindingContext") );
					const TSharedPtr<FJsonValue> CommandNameObj = GestureInfoObj->Values.FindRef( TEXT("CommandName") );
					const TSharedPtr<FJsonValue> CtrlObj = GestureInfoObj->Values.FindRef( TEXT("Control") );
					const TSharedPtr<FJsonValue> AltObj = GestureInfoObj->Values.FindRef( TEXT("Alt") );
					const TSharedPtr<FJsonValue> ShiftObj = GestureInfoObj->Values.FindRef( TEXT("Shift") );
					const TSharedPtr<FJsonValue> CmdObj = GestureInfoObj->Values.FindRef( TEXT("Command") );
					const TSharedPtr<FJsonValue> KeyObj = GestureInfoObj->Values.FindRef( TEXT("Key") );

					const FName BindingContext = *BindingContextObj->AsString();
					const FName CommandName = *CommandNameObj->AsString();

					const FUserDefinedGestureKey GestureKey(BindingContext, CommandName);
					FInputGesture& UserDefinedGesture = Gestures->FindOrAdd(GestureKey);

					UserDefinedGesture = FInputGesture(*KeyObj->AsString(), EModifierKey::FromBools(
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
			TSharedPtr<FJsonObject> GesturesObj;

			// This loads a JSON object containing BindingContexts, containing objects of CommandNames, containing the FInputGesture information
			FString Content;
			if( GConfig->GetString(TEXT("UserDefinedGestures"), TEXT("Content"), Content, GEditorKeyBindingsIni) )
			{
				const FString UnescapedContent = FRemoteConfig::ReplaceIniSpecialCharWithChar(Content).ReplaceEscapedCharWithChar();

				auto JsonReader = TJsonReaderFactory<>::Create( UnescapedContent );
				FJsonSerializer::Deserialize( JsonReader, GesturesObj );
			}

			if (!GesturesObj.IsValid())
			{
				// Gestures have not been loaded from the ini file, try reading them from the txt file now
				TSharedPtr<FArchive> Ar = MakeShareable( IFileManager::Get().CreateFileReader( *( FPaths::GameSavedDir() / TEXT("Preferences/EditorKeyBindings.txt") ) ) );
				if( Ar.IsValid() )
				{
					auto TextReader = TJsonReaderFactory<ANSICHAR>::Create( Ar.Get() );
					FJsonSerializer::Deserialize( TextReader, GesturesObj );
				}
			}

			if (GesturesObj.IsValid())
			{
				for(const auto& BindingContextInfo : GesturesObj->Values)
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

						const FUserDefinedGestureKey GestureKey(BindingContext, CommandName);
						FInputGesture& UserDefinedGesture = Gestures->FindOrAdd(GestureKey);

						UserDefinedGesture = FInputGesture(*KeyObj->AsString(), EModifierKey::FromBools(
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

void FUserDefinedGestures::SaveGestures() const
{
	if( Gestures.IsValid() )
	{
		FString GestureRawJsonContent;
		TArray<FString> GestureJsonArray;
		for(const auto& GestureInfo : *Gestures)
		{
			TSharedPtr<FJsonValueObject> GestureInfoValueObj = MakeShareable( new FJsonValueObject( MakeShareable( new FJsonObject ) ) );
			TSharedPtr<FJsonObject> GestureInfoObj = GestureInfoValueObj->AsObject();

			// Set the gesture values for the command
			GestureInfoObj->Values.Add( TEXT("BindingContext"), MakeShareable( new FJsonValueString( GestureInfo.Key.BindingContext.ToString() ) ) );
			GestureInfoObj->Values.Add( TEXT("CommandName"), MakeShareable( new FJsonValueString( GestureInfo.Key.CommandName.ToString() ) ) );
			GestureInfoObj->Values.Add( TEXT("Control"), MakeShareable( new FJsonValueBoolean( GestureInfo.Value.NeedsControl() ) ) );
			GestureInfoObj->Values.Add( TEXT("Alt"), MakeShareable( new FJsonValueBoolean( GestureInfo.Value.NeedsAlt() ) ) );
			GestureInfoObj->Values.Add( TEXT("Shift"), MakeShareable( new FJsonValueBoolean( GestureInfo.Value.NeedsShift() ) ) );
			GestureInfoObj->Values.Add( TEXT("Command"), MakeShareable( new FJsonValueBoolean( GestureInfo.Value.NeedsCommand() ) ) );
			GestureInfoObj->Values.Add( TEXT("Key"), MakeShareable( new FJsonValueString( GestureInfo.Value.Key.ToString() ) ) );

			auto JsonWriter = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create( &GestureRawJsonContent );
			FJsonSerializer::Serialize( GestureInfoObj.ToSharedRef(), JsonWriter );

			const FString EscapedContent = FRemoteConfig::ReplaceIniCharWithSpecialChar(GestureRawJsonContent).ReplaceCharWithEscapedChar();
			GestureJsonArray.Add(EscapedContent);
		}

		GConfig->SetArray(TEXT("UserDefinedGestures"), TEXT("UserDefinedGestures"), GestureJsonArray, GEditorKeyBindingsIni);

		// Clean up the old Content key (if it still exists)
		GConfig->RemoveKey(TEXT("UserDefinedGestures"), TEXT("Content"), GEditorKeyBindingsIni);
	}
}

bool FUserDefinedGestures::GetUserDefinedGesture( const FName BindingContext, const FName CommandName, FInputGesture& OutUserDefinedGesture ) const
{
	bool bResult = false;

	if( Gestures.IsValid() )
	{
		const FUserDefinedGestureKey GestureKey(BindingContext, CommandName);
		const FInputGesture* const UserDefinedGesturePtr = Gestures->Find(GestureKey);
		if( UserDefinedGesturePtr )
		{
			OutUserDefinedGesture = *UserDefinedGesturePtr;
			bResult = true;
		}
	}

	return bResult;
}

void FUserDefinedGestures::SetUserDefinedGesture( const FUICommandInfo& CommandInfo )
{
	if( Gestures.IsValid() )
	{
		const FName BindingContext = CommandInfo.GetBindingContext();
		const FName CommandName = CommandInfo.GetCommandName();

		// Find or create the command context
		const FUserDefinedGestureKey GestureKey(BindingContext, CommandName);
		FInputGesture& UserDefinedGesture = Gestures->FindOrAdd(GestureKey);

		// Save an empty invalid gesture if one was not set
		// This is an indication that the user doesn't want this bound and not to use the default gesture
		const TSharedPtr<const FInputGesture> InputGesture = CommandInfo.GetActiveGesture();
		UserDefinedGesture = (InputGesture.IsValid()) ? *InputGesture : FInputGesture();
	}
}

void FUserDefinedGestures::RemoveAll()
{
	Gestures = MakeShareable( new FGesturesMap );
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
 * Gets the user defined gesture (if any) from the provided command name
 * 
 * @param InBindingContext	The context in which the command is active
 * @param InCommandName		The name of the command to get the gesture from
 */
bool FInputBindingManager::GetUserDefinedGesture( const FName InBindingContext, const FName InCommandName, FInputGesture& OutUserDefinedGesture )
{
	if( !UserDefinedGestures.IsValid() )
	{
		UserDefinedGestures = MakeShareable( new FUserDefinedGestures );
		UserDefinedGestures->LoadGestures();
	}

	return UserDefinedGestures->GetUserDefinedGesture( InBindingContext, InCommandName, OutUserDefinedGesture );
}

void FInputBindingManager::CheckForDuplicateDefaultGestures( const FBindingContext& InBindingContext, TSharedPtr<FUICommandInfo> InCommandInfo ) const
{
	const bool bCheckDefault = true;
	TSharedPtr<FUICommandInfo> ExistingInfo = GetCommandInfoFromInputGesture( InBindingContext.GetContextName(), InCommandInfo->DefaultGesture, bCheckDefault );
	if( ExistingInfo.IsValid() )
	{
		if( ExistingInfo->CommandName != InCommandInfo->CommandName )
		{
			// Two different commands with the same name in the same context or parent context
			UE_LOG(LogSlate, Fatal, TEXT("The command '%s.%s' has the same default gesture as '%s.%s' [%s]"), 
				*InCommandInfo->BindingContext.ToString(),
				*InCommandInfo->CommandName.ToString(), 
				*ExistingInfo->BindingContext.ToString(),
				*ExistingInfo->CommandName.ToString(), 
				*InCommandInfo->DefaultGesture.GetInputText().ToString() );
		}
	}
}


void FInputBindingManager::NotifyActiveGestureChanged( const FUICommandInfo& CommandInfo )
{
	FContextEntry& ContextEntry = ContextMap.FindChecked( CommandInfo.GetBindingContext() );

	// Slow but doesn't happen frequently
	for( FGestureMap::TIterator It( ContextEntry.GestureToCommandInfoMap ); It; ++It )
	{
		// Remove the currently active gesture from the map if one exists
		if( It.Value() == CommandInfo.GetCommandName() )
		{
			It.RemoveCurrent();
			// There should only be one active gesture
			break;
		}
	}

	if( CommandInfo.GetActiveGesture()->IsValidGesture() )
	{
		checkSlow( !ContextEntry.GestureToCommandInfoMap.Contains( *CommandInfo.GetActiveGesture() ) )
		ContextEntry.GestureToCommandInfoMap.Add( *CommandInfo.GetActiveGesture(), CommandInfo.GetCommandName() );
	}
	

	// The user defined gestures should have already been created
	check( UserDefinedGestures.IsValid() );

	UserDefinedGestures->SetUserDefinedGesture( CommandInfo );

	// Broadcast the gesture event when a new one is added
	OnUserDefinedGestureChanged.Broadcast(CommandInfo);
}

void FInputBindingManager::SaveInputBindings()
{
	if( UserDefinedGestures.IsValid() )
	{
		UserDefinedGestures->SaveGestures();
	}
}

void FInputBindingManager::RemoveUserDefinedGestures()
{
	if( UserDefinedGestures.IsValid() )
	{
		UserDefinedGestures->RemoveAll();
		UserDefinedGestures->SaveGestures();
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

	// Should not have already created a gesture for this command
	check( !InCommandInfo->ActiveGesture->IsValidGesture() );
	
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

	if( InCommandInfo->DefaultGesture.IsValidGesture() )
	{
		CheckForDuplicateDefaultGestures( *InBindingContext, InCommandInfo );
	}
	
	{
		TSharedPtr<FUICommandInfo> ExistingInfo = CommandInfoMap.FindRef( InCommandInfo->CommandName );
		ensureMsgf( !ExistingInfo.IsValid(), TEXT("A command with name %s already exists in context %s"), *InCommandInfo->CommandName.ToString(), *InBindingContext->GetContextName().ToString() );
	}

	// Add the command info to the list of known infos.  It can only exist once.
	CommandInfoMap.Add( InCommandInfo->CommandName, InCommandInfo );

	// See if there are user defined gestures for this command
	FInputGesture UserDefinedGesture;
	bool bFoundUserDefinedGesture = GetUserDefinedGesture( ContextName, InCommandInfo->CommandName, UserDefinedGesture );

	
	if( !bFoundUserDefinedGesture && InCommandInfo->DefaultGesture.IsValidGesture() )
	{
		// Find any existing command with the same gesture 
		// This is for inconsistency between default and user defined gesture.  We need to make sure that if default gestures are changed to a gesture that a user set to a different command, that the default gesture doesn't replace
		// the existing commands gesture. Note: Duplicate default gestures are found above in CheckForDuplicateDefaultGestures
		FName ExisingCommand = ContextEntry.GestureToCommandInfoMap.FindRef( InCommandInfo->DefaultGesture );

		if( ExisingCommand == NAME_None )
		{
			// No existing command has a user defined gesture and no user defined gesture is available for this command 
			TSharedRef<FInputGesture> NewGesture = MakeShareable( new FInputGesture( InCommandInfo->DefaultGesture ) );
			InCommandInfo->ActiveGesture = NewGesture;
		}

	}
	else if( bFoundUserDefinedGesture )
	{
		// Find any existing command with the same gesture 
		// This is for inconsistency between default and user defined gesture.  We need to make sure that if default gestures are changed to a gesture that a user set to a different command, that the default gesture doesn't replace
		// the existing commands gesture.
		FName ExisingCommandName = ContextEntry.GestureToCommandInfoMap.FindRef( UserDefinedGesture );

		if( ExisingCommandName != NAME_None )
		{
			// Get the command with using the same gesture
			TSharedPtr<FUICommandInfo> ExistingInfo = CommandInfoMap.FindRef( ExisingCommandName );
			if( *ExistingInfo->ActiveGesture != ExistingInfo->DefaultGesture )
			{
				// two user defined gestures are the same within a context.  If the keybinding editor was used this wont happen so this must have been directly a modified user setting file
				UE_LOG(LogSlate, Error, TEXT("Duplicate user defined gestures found: [%s,%s].  Gesture for %s being removed"), *InCommandInfo->GetLabel().ToString(), *ExistingInfo->GetLabel().ToString(), *ExistingInfo->GetLabel().ToString() );
			}
			ContextEntry.GestureToCommandInfoMap.Remove( *ExistingInfo->ActiveGesture );
			// Remove the existing gesture. 
			ExistingInfo->ActiveGesture = MakeShareable( new FInputGesture() );
		
		}

		TSharedRef<FInputGesture> NewGesture = MakeShareable( new FInputGesture( UserDefinedGesture ) );
		// Set the active gesture on the command info
		InCommandInfo->ActiveGesture = NewGesture;
	}

	// If the active gesture is valid, map the gesture to the map for fast lookup when processing bindings
	if( InCommandInfo->ActiveGesture->IsValidGesture() )
	{
		checkSlow( !ContextEntry.GestureToCommandInfoMap.Contains( *InCommandInfo->GetActiveGesture() ) );
		ContextEntry.GestureToCommandInfoMap.Add( *InCommandInfo->GetActiveGesture(), InCommandInfo->GetCommandName() );
	}
}

const TSharedPtr<FUICommandInfo> FInputBindingManager::FindCommandInContext( const FName InBindingContext, const FInputGesture& InGesture, bool bCheckDefault ) const
{
	const FContextEntry& ContextEntry = ContextMap.FindRef( InBindingContext );
	
	TSharedPtr<FUICommandInfo> FoundCommand = NULL;

	if( bCheckDefault )
	{
		const FCommandInfoMap& InfoMap = ContextEntry.CommandInfoMap;
		for( FCommandInfoMap::TConstIterator It(InfoMap); It && !FoundCommand.IsValid(); ++It )
		{
			const FUICommandInfo& CommandInfo = *It.Value();
			if( CommandInfo.DefaultGesture == InGesture )
			{
				FoundCommand = It.Value();
			}	
		}
	}
	else
	{
		// faster lookup for active gestures
		FName CommandName = ContextEntry.GestureToCommandInfoMap.FindRef( InGesture );
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

const TSharedPtr<FUICommandInfo> FInputBindingManager::GetCommandInfoFromInputGesture( const FName InBindingContext, const FInputGesture& InGesture, bool bCheckDefault ) const
{
	TSharedPtr<FUICommandInfo> FoundCommand = NULL;

	FName CurrentContext = InBindingContext;
	while( CurrentContext != NAME_None && !FoundCommand.IsValid() )
	{
		const FContextEntry& ContextEntry = ContextMap.FindRef( CurrentContext );

		FoundCommand = FindCommandInContext( CurrentContext, InGesture, bCheckDefault );

		CurrentContext = ContextEntry.BindingContext->GetContextParent();
	}
	
	if( !FoundCommand.IsValid() )
	{
		TArray<FName> Children;
		GetAllChildContexts( InBindingContext, Children );

		for( int32 ContextIndex = 0; ContextIndex < Children.Num() && !FoundCommand.IsValid(); ++ContextIndex )
		{
			FoundCommand = FindCommandInContext( Children[ContextIndex], InGesture, bCheckDefault );
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
