// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObjectThreadContext.h"

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
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_TIGHTLY_PACKED_ENUMS)
	{
		TArray<FName> TempNames;
		Ar << TempNames;
		uint8 Value = 0;
		for (const FName& TempName : TempNames)
		{
			Names.Add(TPairInitializer<FName, uint8>(TempName, Value++));
		}
	}
	else
	{
	Ar << Names;
	}

	if (Ar.UE4Ver() < VER_UE4_ENUM_CLASS_SUPPORT)
	{
		bool bIsNamespace;
		Ar << bIsNamespace;
		CppForm = bIsNamespace ? ECppForm::Namespaced : ECppForm::Regular;
	}
	else
	{
		uint8 EnumTypeByte = (uint8)CppForm;
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
	FString BaseEnumName = GetNameByIndex(Names.Num() - 1).ToString();

	// Double check we have a fully qualified name.
	auto DoubleColonPos = BaseEnumName.Find(TEXT("::"));
	check(DoubleColonPos != INDEX_NONE);

	// Get actual base name.
	BaseEnumName = BaseEnumName.LeftChop(BaseEnumName.Len() - DoubleColonPos);

	return BaseEnumName;
}

void UEnum::RenameNamesAfterDuplication()
{
	if (Names.Num() != 0)
	{
		// Get name of base enum, from which we're duplicating.
		FString BaseEnumName = GetBaseEnumNameOnDuplication();

		// Get name of duplicated enum.
		FString ThisName = GetName();

		// Replace all usages of base class name to the duplicated one.
		for (TPair<FName, uint8>& Kvp : Names)
		{
			FString NameString = Kvp.Key.ToString();
			NameString.ReplaceInline(*BaseEnumName, *ThisName);
			Kvp.Key = FName(*NameString);
		}
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

FName UEnum::GetNameByIndex(uint8 Index) const
{
	if (Names.IsValidIndex(Index))
	{
		return Names[Index].Key;
	}

	return NAME_None;
}

uint8 UEnum::GetValueByIndex(uint8 Index) const
{
	check(Names.IsValidIndex(Index));
	return Names[Index].Value;
}

FName UEnum::GetNameByValue(uint8 InValue) const
{
	for (TPair<FName, uint8> Kvp : Names)
	{
		if (Kvp.Value == InValue)
		{
			return Kvp.Key;
		}
	}

	return NAME_None;
}

int32 UEnum::GetIndexByName(FName InName) const
{
	int32 Count = Names.Num();
	for (int32 Counter = 0; Counter < Count; ++Counter)
	{
		if (Names[Counter].Key == InName)
		{
			return Counter;
		}
	}

	return INDEX_NONE;
}

int32 UEnum::GetValueByName(FName InName) const
{
	FString InNameString = InName.ToString();
	FString DoubleColon = TEXT("::");
	int32 DoubleColonPosition = InNameString.Find(DoubleColon);
	bool bIsInNameFullyQualified = DoubleColonPosition != INDEX_NONE;
	bool bIsEnumFullyQualified = CppForm != ECppForm::Regular;

	if (!bIsInNameFullyQualified && bIsEnumFullyQualified)
	{
		// Make InName fully qualified.
		InName = FName(*GetName().Append(DoubleColon).Append(InNameString));
	}
	else if (bIsInNameFullyQualified && !bIsEnumFullyQualified)
	{
		// Make InName unqualified.
		InName = FName(*InNameString.Mid(DoubleColonPosition + 2, InNameString.Len() - DoubleColonPosition + 2));
	}

	for (TPair<FName, uint8> Kvp : Names)
	{
		if (Kvp.Key == InName)
		{
			return Kvp.Value;
		}
	}

	return INDEX_NONE;
}


uint8 UEnum::GetMaxEnumValue() const
{
	int32 NamesNum = Names.Num();
	if (NamesNum == 0)
	{
		return 0;
	}

	uint8 MaxValue = Names[0].Value;
	for (int32 i = 0; i < NamesNum; ++i)
	{
		uint8 CurrentValue = Names[i].Value;
		if (CurrentValue > MaxValue)
		{
			MaxValue = CurrentValue;
		}
	}

	return MaxValue;
}

bool UEnum::IsValidEnumValue(uint8 InValue) const
{
	int32 NamesNum = Names.Num();
	for (int32 i = 0; i < NamesNum; ++i)
	{
		uint8 CurrentValue = Names[i].Value;
		if (CurrentValue == InValue)
		{
			return true;
		}
	}

	return false;
}

bool UEnum::IsValidEnumName(FName InName) const
{
	int32 NamesNum = Names.Num();
	for (int32 i = 0; i < NamesNum; ++i)
	{
		FName CurrentName = Names[i].Key;
		if (CurrentName == InName)
		{
			return true;
		}
	}

	return false;
}

FString UEnum::GenerateFullEnumName(const UEnum* InEnum, const TCHAR* InEnumName)
{
	if (InEnum->GetCppForm() == ECppForm::Regular || IsFullEnumName(InEnumName))
	{
		return InEnumName;
	}

	return FString::Printf(TEXT("%s::%s"), *InEnum->GetName(), InEnumName);
}

void UEnum::AddNamesToMasterList()
{
	for (TPair<FName, uint8> Kvp : Names)
	{
		UEnum* Enum = AllEnumNames.FindRef(Kvp.Key);
		if (Enum == nullptr)
		{
			AllEnumNames.Add(Kvp.Key, this);
		}
		else if (Enum != this && Enum->GetOutermost() != GetTransientPackage())
		{
			UE_LOG(LogEnum, Warning, TEXT("Enum name collision: '%s' is in both '%s' and '%s'"), *Kvp.Key.ToString(), *GetPathName(), *Enum->GetPathName());
		}
	}
}

void UEnum::RemoveNamesFromMasterList()
{
	for (TPair<FName, uint8> Kvp : Names)
	{
		UEnum* Enum = AllEnumNames.FindRef(Kvp.Key);
		if (Enum == this)
		{
			AllEnumNames.Remove(Kvp.Key);
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
		Prefix = Names[0].Key.ToString();

		// For each item in the enumeration, trim the prefix as much as necessary to keep it a prefix.
		// This ensures that once all items have been processed, a common prefix will have been constructed.
		// This will be the longest common prefix since as little as possible is trimmed at each step.
		for (int32 NameIdx = 1; NameIdx < Names.Num(); NameIdx++)
		{
			FString EnumItemName = *Names[NameIdx].Key.ToString();

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

FString UEnum::GetEnumName(int32 InIndex) const
{
	if (Names.IsValidIndex(InIndex))
	{
		if (CppForm == ECppForm::Regular)
		{
			return GetNameByIndex(InIndex).ToString();
		}

		// Strip the namespace from the name.
		FString EnumName(GetNameByIndex(InIndex).ToString());
		int32 ScopeIndex = EnumName.Find(TEXT("::"), ESearchCase::CaseSensitive);
		if (ScopeIndex != INDEX_NONE)
		{
			return EnumName.Mid(ScopeIndex + 2);
		}
	}
	return FName(NAME_None).ToString();
}

FText UEnum::GetEnumText(int32 InIndex) const
{
#if WITH_EDITOR
	//@todo These values should be properly localized [9/24/2013 justin.sargent]
	FText LocalizedDisplayName = GetDisplayNameText(InIndex);
	if(!LocalizedDisplayName.IsEmpty())
	{
		return LocalizedDisplayName;
	}
#endif

	return FText::FromString( GetEnumName(InIndex) );
}

int32 UEnum::FindEnumIndex(FName InName) const
{
	int32 EnumIndex = GetIndexByName(InName);
	if (EnumIndex == INDEX_NONE && CppForm != ECppForm::Regular && !IsFullEnumName(*InName.ToString()))
	{
		// Try the long enum name if InName doesn't have a namespace specified and this is a namespace enum.
		FName LongName(*GenerateFullEnumName(*InName.ToString()));
		EnumIndex = GetIndexByName(LongName);
	}

	// If that still didn't work - try the redirect table
	if(EnumIndex == INDEX_NONE)
	{
		EnumIndex = FindEnumRedirects(this, InName);
	}

	// None is passed in by blueprints at various points, isn't an error. Any other failed resolve should be fixed
	if ((EnumIndex == INDEX_NONE) && (InName != NAME_None))
	{
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		
		UE_LOG(LogEnum, Warning, TEXT("In asset '%s', there is an enum property of type '%s' with an invalid value of '%s'"), *GetPathNameSafe(ThreadContext.SerializedObject), *GetName(), *InName.ToString());
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
	if (ThisEnumRedirects != nullptr)
	{
		// first look for default name
		const FName* NewEntryName = ThisEnumRedirects->Find(EnumEntryName);
		if (NewEntryName != nullptr)
		{
			return Enum->GetIndexByName(*NewEntryName);
		}
		// if not found, look for long name
		else
		{
			// Note: This can pollute the name table if the ini is set up incorrectly
			FName LongName(*GenerateFullEnumName(Enum, *EnumEntryName.ToString()));
			NewEntryName = ThisEnumRedirects->Find(LongName);
			if (NewEntryName != nullptr)
			{
				return Enum->GetIndexByName(*NewEntryName);
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

				int32 FoundIndex = Enum->GetIndexByName(NewName);

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
 * @return	true unless the MAX enum already exists.
 */
bool UEnum::SetEnums(TArray<TPair<FName, uint8>>& InNames, UEnum::ECppForm InCppForm, bool bAddMaxKeyIfMissing)
{
	if (Names.Num() > 0)
	{
		RemoveNamesFromMasterList();
	}
	Names   = InNames;
	CppForm = InCppForm;

	if (bAddMaxKeyIfMissing)
	{
		const FString EnumPrefix = GenerateEnumPrefix();
		checkSlow(EnumPrefix.Len());

		const FName MaxEnumItem = *GenerateFullEnumName(*(EnumPrefix + TEXT("_MAX")));
		const int32 MaxEnumItemIndex = GetIndexByName(MaxEnumItem);

		if (MaxEnumItemIndex == INDEX_NONE)
		{
			if (LookupEnumName(MaxEnumItem) != INDEX_NONE)
			{
				// the MAX identifier is already being used by another enum
				return false;
			}

#if HACK_HEADER_GENERATOR
			if (GetMaxEnumValue() == 255)
			{
				FError::Throwf(TEXT("Enum %s doesn't contain user defined %s_MAX entry and its maximum entry value equals 255. Autogenerated %s_MAX enum entry will equal 0. Change maximum entry value to 254 or define %s_MAX entry in enum."), *GetName(), *EnumPrefix, *EnumPrefix, *EnumPrefix, *EnumPrefix);
			}
#endif
			Names.Add(TPairInitializer<FName, uint8>(MaxEnumItem, GetMaxEnumValue() + 1));
		}
	}
	AddNamesToMasterList();

	return true;
}

#if WITH_EDITOR
FText UEnum::GetDisplayNameTextByValue(int32 Value) const
{
	int32 Index = GetIndexByValue(Value);
	return GetDisplayNameText(Index);
}
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
		
	if ( !FText::FindText( Namespace, Key, /*OUT*/LocalizedToolTip, &NativeToolTip ) )
	{
		static const FString DoxygenSee(TEXT("@see"));
		static const FString TooltipSee(TEXT("See:"));
		if (NativeToolTip.ReplaceInline(*DoxygenSee, *TooltipSee) > 0)
		{
			NativeToolTip.TrimTrailing();
		}

		LocalizedToolTip = FText::FromString(NativeToolTip);
	}

	return LocalizedToolTip;
}

#endif

#if WITH_EDITOR || HACK_HEADER_GENERATOR

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
		KeyString = GetEnumName(NameIndex);
		KeyString.AppendChar(TEXT('.'));
		KeyString.Append(Key);
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
				const FString& ConfigValue = It.Value().GetValue();
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
