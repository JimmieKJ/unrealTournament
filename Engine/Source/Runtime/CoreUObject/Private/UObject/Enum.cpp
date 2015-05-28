// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnum, Log, All);

/*-----------------------------------------------------------------------------
	UEnum implementation.
-----------------------------------------------------------------------------*/

TMap<FName, UEnum*> UEnum::AllEnumNames;
TMap<FName,TMap<FName,FName> > UEnum::EnumRedirects;
TMap<FName,TMap<FString,FString> > UEnum::EnumSubstringRedirects;

UEnum::~UEnum()
{
	RemoveNamesFromMasterList();
}

void UEnum::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Names;

	if (Ar.UE4Ver() < VER_UE4_ENUM_CLASS_SUPPORT)
	{
		bool bIsNamespace;
		Ar << bIsNamespace;
		CppForm = bIsNamespace ? ECppForm::Namespaced : ECppForm::Regular;
	}
	else
	{
		int8 EnumTypeByte = (int8)CppForm;
		Ar << EnumTypeByte;
		CppForm = (ECppForm)EnumTypeByte;
	}

	if (Ar.IsLoading() || Ar.IsSaving())
	{
		// We're duplicating this enum.
		if ((Ar.GetPortFlags() & PPF_Duplicate)
			// and we're loading it from already serialized base.
			&& Ar.IsLoading())
		{
			// Rename enum names to reflect new class.
			RenameNamesAfterDuplication();
		}
		AddNamesToMasterList();
	}
}

FString UEnum::GetBaseEnumNameOnDuplication() const
{
	// Last name is always fully qualified, in form EnumName::Prefix_MAX.
	FString BaseEnumName = Names[Names.Num() - 1].ToString();

	// Double check we have a fully qualified name.
	auto DoubleColonPos = BaseEnumName.Find(TEXT("::"));
	check(DoubleColonPos != INDEX_NONE);

	// Get actual base name.
	BaseEnumName = BaseEnumName.LeftChop(BaseEnumName.Len() - DoubleColonPos);

	return BaseEnumName;
}

void UEnum::RenameNamesAfterDuplication()
{
	// Get name of base enum, from which we're duplicating.
	FString BaseEnumName = GetBaseEnumNameOnDuplication();

	// Get name of duplicated enum.
	auto ThisName = GetName();

	// Replace all usages of base class name to the duplicated one.
	for (auto& Name : Names)
	{
		auto NameString = Name.ToString();
		NameString.ReplaceInline(*BaseEnumName, *ThisName);
		Name = FName(*NameString);
	}
}

int32 UEnum::ResolveEnumerator(FArchive& Ar, int32 EnumeratorIndex) const
{
	return EnumeratorIndex;
}

FString UEnum::GenerateFullEnumName(const TCHAR* InEnumName) const
{
	return (CppForm != ECppForm::Regular) ? GenerateFullEnumName(this, InEnumName) : InEnumName;
}

void UEnum::AddNamesToMasterList()
{
	for (int32 NameIndex = 0; NameIndex < Names.Num(); NameIndex++)
	{
		UEnum* Enum = AllEnumNames.FindRef(Names[NameIndex]);
		if (Enum == NULL)
		{
			AllEnumNames.Add(Names[NameIndex], this);
		}
		else if (Enum != this)
		{
			UE_LOG(LogEnum, Warning, TEXT("Enum name collision: '%s' is in both '%s' and '%s'"), *Names[NameIndex].ToString(), *GetPathName(), *Enum->GetPathName());
		}
	}
}

void UEnum::RemoveNamesFromMasterList()
{
	for (int32 NameIndex = 0; NameIndex < Names.Num(); NameIndex++)
	{
		UEnum* Enum = AllEnumNames.FindRef(Names[NameIndex]);
		if (Enum == this)
		{
			AllEnumNames.Remove(Names[NameIndex]);
		}
	}
}

/**
 * Find the longest common prefix of all items in the enumeration.
 * 
 * @return	the longest common prefix between all items in the enum.  If a common prefix
 *			cannot be found, returns the full name of the enum.
 */
FString UEnum::GenerateEnumPrefix() const
{
	FString Prefix;
	if (Names.Num() > 0)
	{
		Prefix = Names[0].ToString();

		// For each item in the enumeration, trim the prefix as much as necessary to keep it a prefix.
		// This ensures that once all items have been processed, a common prefix will have been constructed.
		// This will be the longest common prefix since as little as possible is trimmed at each step.
		for (int32 NameIdx = 1; NameIdx < Names.Num(); NameIdx++)
		{
			FString EnumItemName = *Names[NameIdx].ToString();

			// Find the length of the longest common prefix of Prefix and EnumItemName.
			int32 PrefixIdx = 0;
			while (PrefixIdx < Prefix.Len() && PrefixIdx < EnumItemName.Len() && Prefix[PrefixIdx] == EnumItemName[PrefixIdx])
			{
				PrefixIdx++;
			}

			// Trim the prefix to the length of the common prefix.
			Prefix = Prefix.Left(PrefixIdx);
		}

		// Find the index of the rightmost underscore in the prefix.
		int32 UnderscoreIdx = Prefix.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		// If an underscore was found, trim the prefix so only the part before the rightmost underscore is included.
		if (UnderscoreIdx > 0)
		{
			Prefix = Prefix.Left(UnderscoreIdx);
		}
		else
		{
			// no underscores in the common prefix - this probably indicates that the names
			// for this enum are not using Epic's notation, so just empty the prefix so that
			// the max item will use the full name of the enum
			Prefix.Empty();
		}
	}

	// If no common prefix was found, or if the enum does not contain any entries,
	// use the name of the enumeration instead.
	if (Prefix.Len() == 0)
	{
		Prefix = GetName();
	}
	return Prefix;
}

int32 UEnum::FindEnumIndex(FName InName) const
{
	int32 EnumIndex = Names.Find( InName );
	if (EnumIndex == INDEX_NONE && CppForm != ECppForm::Regular && !IsFullEnumName(*InName.ToString()))
	{
		// Try the long enum name if InName doesn't have a namespace specified and this is a namespace enum.
		FName LongName(*GenerateFullEnumName(*InName.ToString()));
		EnumIndex = Names.Find( LongName );
	}

	// If that still didn't work - try the redirect table
	if(EnumIndex == INDEX_NONE)
	{
		EnumIndex = FindEnumRedirects(this, InName);
	}

	if (EnumIndex == INDEX_NONE && InName != NAME_None)
	{
		// None is passed in by blueprints at various points, isn't an error. Any other failed resolve should be fixed
		UE_LOG(LogEnum, Warning, TEXT("Enum Text %s for Enum %s failed to resolve to any value"), *InName.ToString(), *GetName());
	}

	return EnumIndex;
}

int32 UEnum::FindEnumRedirects(const UEnum* Enum, FName EnumEntryName) 
{
	check (Enum);

	// Init redirect map from ini file
	static bool bAlreadyInitialized_EnumRedirectsMap = false;
	if( !bAlreadyInitialized_EnumRedirectsMap )
	{
		InitEnumRedirectsMap();
		bAlreadyInitialized_EnumRedirectsMap = true;
	}

	// See if we have an entry for this enum
	const TMap<FName, FName>* ThisEnumRedirects = EnumRedirects.Find(Enum->GetFName());
	if (ThisEnumRedirects != NULL)
	{
		// first look for default name
		const FName* NewEntryName = ThisEnumRedirects->Find(EnumEntryName);
		if (NewEntryName != NULL)
		{
			return Enum->Names.Find(*NewEntryName);
		}
		// if not found, look for long name
		else
		{
			// Note: This can pollute the name table if the ini is set up incorrectly
			FName LongName(*GenerateFullEnumName(Enum, *EnumEntryName.ToString()));
			NewEntryName = ThisEnumRedirects->Find(LongName);
			if (NewEntryName != NULL)
			{
				return Enum->Names.Find(*NewEntryName);
			}
		}
	}

	// Now try the substring replacement
	const TMap<FString, FString>* ThisEnumSubstringRedirects = EnumSubstringRedirects.Find(Enum->GetFName());
	if (ThisEnumSubstringRedirects != NULL)
	{
		FString EntryString = EnumEntryName.ToString();
		for (TMap<FString, FString>::TConstIterator It(*ThisEnumSubstringRedirects); It; ++It)
		{
			if (EntryString.Contains(*It.Key()))
			{
				// Note: This can pollute the name table if the ini is set up incorrectly
				FName NewName = FName(*EntryString.Replace(*It.Key(), *It.Value()));

				int32 FoundIndex = Enum->Names.Find(NewName);

				if (FoundIndex == INDEX_NONE)
				{
					UE_LOG(LogEnum, Warning, TEXT("Matched substring redirect %s in Enum %s failed to resolve %s to valid enum"), *It.Key(), *Enum->GetName(), *NewName.ToString());
				}
				else
				{
					return FoundIndex;
				}
			}
		}
	}

	return INDEX_NONE;
}
/**
 * Sets the array of enums.
 *
 * @return	true unless the MAX enum already exists and isn't the last enum.
 */
bool UEnum::SetEnums(TArray<FName>& InNames, UEnum::ECppForm InCppForm)
{
	if (Names.Num() > 0)
	{
		RemoveNamesFromMasterList();
	}
	Names   = InNames;
	CppForm = InCppForm;
	return GenerateMaxEnum();
}

/**
 * Adds a virtual _MAX entry to the enum's list of names, unless the
 * enum already contains one.
 *
 * @return	true unless the MAX enum already exists and isn't the last enum.
 */
bool UEnum::GenerateMaxEnum()
{
	const FString EnumPrefix = GenerateEnumPrefix();
	checkSlow(EnumPrefix.Len());

	const FName MaxEnumItem = *GenerateFullEnumName(*(EnumPrefix + TEXT("_MAX")));
	const int32 MaxEnumItemIndex = Names.Find(MaxEnumItem);
	if ( MaxEnumItemIndex == INDEX_NONE )
	{
		if (LookupEnumName(MaxEnumItem) != INDEX_NONE)
		{
			// the MAX identifier is already being used by another enum
			return false;
		}

		Names.Add(MaxEnumItem);
	}

	AddNamesToMasterList();

	if ( MaxEnumItemIndex != INDEX_NONE && MaxEnumItemIndex != Names.Num() - 1 )
	{
		// The MAX enum already exists, but isn't the last enum.
		return false;
	}

	return true;
}

#if WITH_EDITOR
/**
 * Finds the localized display name or native display name as a fallback.
 *
 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
 *
 * @return The display name for this object.
 */
FText UEnum::GetDisplayNameText(int32 NameIndex) const
{
	FText LocalizedDisplayName;

	static const FString Namespace = TEXT("UObjectDisplayNames");
	const FString Key =	NameIndex == INDEX_NONE
		?			GetFullGroupName(false)
		:			GetFullGroupName(false) + TEXT(".") + GetEnumName(NameIndex);


	FString NativeDisplayName;
	if( HasMetaData( TEXT("DisplayName"), NameIndex ) )
	{
		NativeDisplayName = GetMetaData( TEXT("DisplayName"), NameIndex );
	}
	else
	{
		NativeDisplayName = FName::NameToDisplayString(GetEnumName(NameIndex), false);
	}

	if ( !( FText::FindText( Namespace, Key, /*OUT*/LocalizedDisplayName, &NativeDisplayName) ) )
	{
		LocalizedDisplayName = FText::FromString(NativeDisplayName);
	}

	return LocalizedDisplayName;
}

/**
 * Finds the localized tooltip or native tooltip as a fallback.
 *
 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
 *
 * @return The tooltip for this object.
 */
FText UEnum::GetToolTipText(int32 NameIndex) const
{
	FText LocalizedToolTip;
	FString NativeToolTip = GetMetaData( TEXT("ToolTip"), NameIndex );

	static const FString Namespace = TEXT("UObjectToolTips");
	FString Key =	NameIndex == INDEX_NONE
		?			GetFullGroupName(false)
		:			GetFullGroupName(false) + TEXT(".") + GetEnumName(NameIndex);
		
	if ( !(FText::FindText( Namespace, Key, /*OUT*/LocalizedToolTip )) || *FTextInspector::GetSourceString(LocalizedToolTip) != NativeToolTip)
	{
		static const FString DoxygenSee(TEXT("@see"));
		if (NativeToolTip.Split(DoxygenSee, &NativeToolTip, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			NativeToolTip.TrimTrailing();
		}

		LocalizedToolTip = FText::FromString(NativeToolTip);
	}

	return LocalizedToolTip;
}

/**
 * Wrapper method for easily determining whether this enum has metadata associated with it.
 * 
 * @param	Key			the metadata tag to check for
 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
 *
 * @return true if the specified key exists in the list of metadata for this enum, even if the value of that key is empty
 */
bool UEnum::HasMetaData( const TCHAR* Key, int32 NameIndex/*=INDEX_NONE*/ ) const
{
	bool bResult = false;

	UPackage* Package = GetOutermost();
	check(Package);

	UMetaData* MetaData = Package->GetMetaData();
	check(MetaData);

	FString KeyString;

	// If an index was specified, search for metadata linked to a specified value
	if ( NameIndex != INDEX_NONE )
	{
		check(Names.IsValidIndex(NameIndex));
		KeyString = GetEnumName(NameIndex) + TEXT(".") + Key;
	}
	// If no index was specified, search for metadata for the enum itself
	else
	{
		KeyString = Key;
	}
	bResult = MetaData->HasValue( this, *KeyString );
	
	return bResult;
}

/**
 * Return the metadata value associated with the specified key.
 * 
 * @param	Key			the metadata tag to find the value for
 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
 *
 * @return	the value for the key specified, or an empty string if the key wasn't found or had no value.
 */
const FString& UEnum::GetMetaData( const TCHAR* Key, int32 NameIndex/*=INDEX_NONE*/ ) const
{
	UPackage* Package = GetOutermost();
	check(Package);

	UMetaData* MetaData = Package->GetMetaData();
	check(MetaData);

	FString KeyString;

	// If an index was specified, search for metadata linked to a specified value
	if ( NameIndex != INDEX_NONE )
	{
		check(Names.IsValidIndex(NameIndex));
		KeyString = GetEnumName(NameIndex) + TEXT(".") + Key;
	}
	// If no index was specified, search for metadata for the enum itself
	else
	{
		KeyString = Key;
	}

	const FString& ResultString = MetaData->GetValue( this, *KeyString );
	
	return ResultString;
}

/**
 * Set the metadata value associated with the specified key.
 * 
 * @param	Key			the metadata tag to find the value for
 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
 * @param	InValue		Value of the metadata for the key
 *
 */
void UEnum::SetMetaData( const TCHAR* Key, const TCHAR* InValue, int32 NameIndex ) const
{
	UPackage* Package = GetOutermost();
	check(Package);

	UMetaData* MetaData = Package->GetMetaData();
	check(MetaData);

	FString KeyString;

	// If an index was specified, search for metadata linked to a specified value
	if ( NameIndex != INDEX_NONE )
	{
		check(Names.IsValidIndex(NameIndex));
		KeyString = GetEnumName(NameIndex) + TEXT(".") + Key;
	}
	// If no index was specified, search for metadata for the enum itself
	else
	{
		KeyString = Key;
	}

	MetaData->SetValue( this, *KeyString, InValue );
}

void UEnum::RemoveMetaData( const TCHAR* Key, int32 NameIndex/*=INDEX_NONE*/) const
{
	UPackage* Package = GetOutermost();
	check(Package);

	UMetaData* MetaData = Package->GetMetaData();
	check(MetaData);

	FString KeyString;

	// If an index was specified, search for metadata linked to a specified value
	if ( NameIndex != INDEX_NONE )
	{
		check(Names.IsValidIndex(NameIndex));
		KeyString = GetEnumName(NameIndex) + TEXT(".") + Key;
	}
	// If no index was specified, search for metadata for the enum itself
	else
	{
		KeyString = Key;
	}

	MetaData->RemoveValue( this, *KeyString );
}

#endif

int32 UEnum::ParseEnum(const TCHAR*& Str)
{
	FString Token;
	const TCHAR* ParsedStr = Str;
	if (FParse::AlnumToken(ParsedStr, Token))
	{
		FName TheName = FName(*Token, FNAME_Find);
		int32 Result = LookupEnumName(TheName);
		if (Result != INDEX_NONE)
		{
			Str = ParsedStr;
		}
		return Result;
	}
	else
	{
		return 0;
	}
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UEnum, UField,
	{
	}
);

void UEnum::InitEnumRedirectsMap()
{
	if( GConfig )
	{
		FConfigSection* EngineSection = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIni );
		for( FConfigSection::TIterator It(*EngineSection); It; ++It )
		{
			if( It.Key() == TEXT("EnumRedirects") )
			{
				FString ConfigValue = It.Value();
				FName EnumName = NAME_None;
				FName OldEnumEntry = NAME_None;
				FName NewEnumEntry = NAME_None;

				FString OldEnumSubstring;
				FString NewEnumSubstring;

				FParse::Value( *ConfigValue, TEXT("EnumName="), EnumName );
				if (FParse::Value( *ConfigValue, TEXT("OldEnumEntry="), OldEnumEntry ) )
				{
					FParse::Value( *ConfigValue, TEXT("NewEnumEntry="), NewEnumEntry );
					check(EnumName != NAME_None && OldEnumEntry != NAME_None && NewEnumEntry != NAME_None );
					EnumRedirects.FindOrAdd(EnumName).Add(OldEnumEntry, NewEnumEntry);
				}
				else if (FParse::Value( *ConfigValue, TEXT("OldEnumSubstring="), OldEnumSubstring ))
				{
					FParse::Value( *ConfigValue, TEXT("NewEnumSubstring="), NewEnumSubstring );
					check(EnumName != NAME_None && !OldEnumSubstring.IsEmpty() && !NewEnumSubstring.IsEmpty());
					EnumSubstringRedirects.FindOrAdd(EnumName).Add(OldEnumSubstring, NewEnumSubstring);
				}
			}			
		}
	}
	else
	{
		UE_LOG(LogEnum, Warning, TEXT(" **** ENUM REDIRECTS UNABLE TO INITIALIZE! **** "));
	}
}
