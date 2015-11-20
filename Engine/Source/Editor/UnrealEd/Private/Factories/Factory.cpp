// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


// Editor includes.
#include "UnrealEd.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"
#include "EditorClassUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogFactory, Log, All);

/*----------------------------------------------------------------------------
	UFactory.
----------------------------------------------------------------------------*/
FString UFactory::CurrentFilename(TEXT(""));

// This needs to be greater than 0 to allow factories to have both higher and lower priority than the default
const int32 UFactory::DefaultImportPriority = 100;

int32 UFactory::OverwriteYesOrNoToAllState = -1;

bool UFactory::bAllowOneTimeWarningMessages = true;

UFactory::UFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ImportPriority = DefaultImportPriority;
}

void UFactory::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UFactory* This = CastChecked<UFactory>(InThis);
	UClass* SupportedClass = *This->SupportedClass;
	UClass* ContextClass = *This->ContextClass;
	Collector.AddReferencedObject( SupportedClass, This );
	Collector.AddReferencedObject( ContextClass, This );

	Super::AddReferencedObjects( This, Collector );
}


bool UFactory::FactoryCanImport( const FString& Filename )
{
	//check extension (only do the following for t3d)
	if (FPaths::GetExtension(Filename) == TEXT("t3d"))
	{
		//Open file
		FString Data;
		if( FFileHelper::LoadFileToString( Data, *Filename ) )
		{
			const TCHAR* Str= *Data;

			if( FParse::Command(&Str, TEXT("BEGIN") ) && FParse::Command(&Str, TEXT("OBJECT")) )
			{
				FString strClass;
				if (FParse::Value(Str, TEXT("CLASS="), strClass))
				{
					//we found the right syntax, so no error if we don't match
					if (strClass == SupportedClass->GetName())
					{
						return true;
					}
					return false;
				}
			}
			UE_LOG(LogFactory, Warning, TEXT("Factory import failed due to invalid format: %s"), *Filename);
		}
		else
		{
			UE_LOG(LogFactory, Warning, TEXT("Factory import failed due to inability to load file %s"), *Filename);
		}
	}

	return false;
}

bool UFactory::ShouldShowInNewMenu() const
{
	return CanCreateNew();
}

FText UFactory::GetDisplayName() const
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UClass* LocalSupportedClass = GetSupportedClass();
	if ( LocalSupportedClass )
	{
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(LocalSupportedClass);
		if ( AssetTypeActions.IsValid() )
		{
			// @todo AssetTypeActions should be returning a FText so we dont have to do this conversion here.
			return AssetTypeActions.Pin()->GetName();
		}

		// Factories whose classes do not have asset type actions should just display the sanitized class name
		return FText::FromString( FName::NameToDisplayString(*LocalSupportedClass->GetName(), false) );
	}

	// Factories that have no supported class have no display name.
	return FText();
}

uint32 UFactory::GetMenuCategories() const
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UClass* LocalSupportedClass = GetSupportedClass();
	if ( LocalSupportedClass )
	{
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(LocalSupportedClass);
		if ( AssetTypeActions.IsValid() )
		{
			return AssetTypeActions.Pin()->GetCategories();
		}
	}

	// Factories whose classes do not have asset type actions fall in the misc category
	return EAssetTypeCategories::Misc;
}

FText UFactory::GetToolTip() const
{
	return GetSupportedClass()->GetToolTipText();
}

FString UFactory::GetToolTipDocumentationPage() const
{
	return FEditorClassUtils::GetDocumentationPage(GetSupportedClass());
}

FString UFactory::GetToolTipDocumentationExcerpt() const
{
	return FEditorClassUtils::GetDocumentationExcerpt(GetSupportedClass());
}

UClass* UFactory::GetSupportedClass() const
{
	return SupportedClass;
}


bool UFactory::DoesSupportClass(UClass * Class)
{
	return (Class == GetSupportedClass());
}

UClass* UFactory::ResolveSupportedClass()
{
	// This check forces factories which support multiple classes to overload this method.
	// In other words, you can't have a SupportedClass of NULL and not overload this method.
	check( SupportedClass );
	return SupportedClass;
}

void UFactory::ResetState()
{
	// Resets the state of the 'Yes To All / No To All' prompt for overwriting existing objects on import.
	// After the reset, the next import collision will always display the prompt.
	OverwriteYesOrNoToAllState = -1;
}

void UFactory::DisplayOverwriteOptionsDialog(const FText& Message)
{
	// Prompt the user for what to do if a 'To All' response wasn't already given.
	if (OverwriteYesOrNoToAllState != EAppReturnType::YesAll && OverwriteYesOrNoToAllState != EAppReturnType::NoAll)
	{
		OverwriteYesOrNoToAllState = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAllCancel, FText::Format(
			NSLOCTEXT("UnrealEd", "ImportedAssetAlreadyExists", "{0} Would you like to overwrite the existing settings?\n\nYes or Yes to All: Overwrite the existing settings.\nNo or No to All: Preserve the existing settings.\nCancel: Abort the operation."),
			Message));
	}
}

bool UFactory::SortFactoriesByPriority(const UFactory& A, const UFactory& B)
{
	// First sort so that higher priorities are earlier in the list
	if( A.ImportPriority > B.ImportPriority )
	{
		return true;
	}
	else if( A.ImportPriority < B.ImportPriority )
	{
		return false;
	}

	// Then sort so that factories that only create new assets are tried after those that actually import the file data (when they have an equivalent priority)
	const bool bFactoryAImportsFiles = !A.CanCreateNew();
	const bool bFactoryBImportsFiles = !B.CanCreateNew();
	if( bFactoryAImportsFiles && !bFactoryBImportsFiles )
	{
		return true;
	}

	return false;
}

UObject* UFactory::StaticImportObject
(
UClass*				Class,
UObject*			InOuter,
FName				Name,
EObjectFlags		Flags,
const TCHAR*		Filename,
UObject*			Context,
UFactory*			InFactory,
const TCHAR*		Parms,
FFeedbackContext*	Warn,
int32				MaxImportFileSize
)
{
	bool bOperationCanceled = false;
	return StaticImportObject(Class, InOuter, Name, Flags, bOperationCanceled, Filename, Context, InFactory, Parms, Warn, MaxImportFileSize);
}

UObject* UFactory::StaticImportObject
(
	UClass*				Class,
	UObject*			InOuter,
	FName				Name,
	EObjectFlags		Flags,
	bool&				bOutOperationCanceled,
	const TCHAR*		Filename,
	UObject*			Context,
	UFactory*			InFactory,
	const TCHAR*		Parms,
	FFeedbackContext*	Warn,
	int32				MaxImportFileSize
)
{
	check(Class);

	CurrentFilename = Filename;

	// Make list of all applicable factories.
	TArray<UFactory*> Factories;
	if( InFactory )
	{
		// Use just the specified factory.
		if (ensureMsgf( !InFactory->SupportedClass || Class->IsChildOf(InFactory->SupportedClass), 
			TEXT("Factory is (%s), SupportedClass is (%s) and Class name is (%s)"), *InFactory->GetName(), (InFactory->SupportedClass)? *InFactory->SupportedClass->GetName() : TEXT("None"), *Class->GetName() ))
		{
			Factories.Add( InFactory );
		}
	}
	else
	{
		auto TransientPackage = GetTransientPackage();
		// Try all automatic factories, sorted by priority.
		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->IsChildOf( UFactory::StaticClass() ) )
			{
				UFactory* Default = It->GetDefaultObject<UFactory>();
				if (Class->IsChildOf(Default->SupportedClass) && Default->ImportPriority >= 0)
				{
					Factories.Add(NewObject<UFactory>(TransientPackage, *It));
				}
			}
		}

		Factories.Sort(&UFactory::SortFactoriesByPriority);
	}

	bool bLoadedFile = false;

	// Try each factory in turn.
	for( int32 i=0; i<Factories.Num(); i++ )
	{
		UFactory* Factory = Factories[i];
		UObject* Result = NULL;
		if( Factory->CanCreateNew() )
		{
			UE_LOG(LogFactory, Log,  TEXT("FactoryCreateNew: %s with %s (%i %i %s)"), *Class->GetName(), *Factories[i]->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
			Factory->ParseParms( Parms );
			Result = Factory->FactoryCreateNew( Class, InOuter, Name, Flags, NULL, Warn );
		}
		else if( FCString::Stricmp(Filename,TEXT(""))!=0 )
		{
			if( Factory->bText )
			{
				//UE_LOG(LogFactory, Log,  TEXT("FactoryCreateText: %s with %s (%i %i %s)"), *Class->GetName(), *Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				FString Data;
				if( FFileHelper::LoadFileToString( Data, Filename ) )
				{
					bLoadedFile = true;
					const TCHAR* Ptr = *Data;
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateText( Class, InOuter, Name, Flags, NULL, *FPaths::GetExtension(Filename), Ptr, Ptr+Data.Len(), Warn );
				}
			}
			else
			{
				UE_LOG(LogFactory, Log,  TEXT("FactoryCreateBinary: %s with %s (%i %i %s)"), *Class->GetName(), *Factories[i]->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				
				// Sanity check the file size of the impending import and prompt the user if they wish to continue if the file size is very large
				const int32 FileSize = IFileManager::Get().FileSize( Filename );
				bool bValidFileSize = true;

				if ( FileSize == INDEX_NONE )
				{
					UE_LOG(LogFactory, Error,TEXT("File '%s' does not exist"), Filename );
					bValidFileSize = false;
				}

				TArray<uint8> Data;
				if( bValidFileSize && FFileHelper::LoadFileToArray( Data, Filename ) )
				{
					bLoadedFile = true;
					Data.Add( 0 );
					const uint8* Ptr = &Data[ 0 ];
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateBinary( Class, InOuter, Name, Flags, NULL, *FPaths::GetExtension(Filename), Ptr, Ptr+Data.Num()-1, Warn, bOutOperationCanceled );
				}
			}
		}
		if( Result )
		{
			// prevent UTextureCube created from UTextureFactory			check(Result->IsA(Class));
			Result->MarkPackageDirty();
			ULevel::LevelDirtiedEvent.Broadcast();
			Result->PostEditChange();

			CurrentFilename = TEXT("");
			return Result;
		}
	}

	if ( !bLoadedFile && !bOutOperationCanceled )
	{
		Warn->Logf( *FText::Format( NSLOCTEXT( "UnrealEd", "NoFindImport", "Can't find file '{0}' for import" ), FText::FromString( FString(Filename) ) ).ToString() );
	}

	CurrentFilename = TEXT("");

	return NULL;
}


void UFactory::GetSupportedFileExtensions(TArray<FString>& OutExtensions) const
{
	for ( int32 FormatIdx = 0; FormatIdx < Formats.Num(); ++FormatIdx )
	{
		const FString& Format = Formats[FormatIdx];
		const int32 DelimiterIdx = Format.Find(TEXT(";"));

		if ( DelimiterIdx != INDEX_NONE )
		{
			OutExtensions.Add( Format.Left(DelimiterIdx) );
		}
	}
}

bool UFactory::ImportUntypedBulkDataFromText(const TCHAR*& Buffer, FUntypedBulkData& BulkData)
{
	FString StrLine;
	int32 ElementCount = 0;
	int32 ElementSize = 0;
	bool bBulkDataIsLocked = false;

	while(FParse::Line(&Buffer,StrLine))
	{
		FString ParsedText;
		const TCHAR* Str = *StrLine;

		if (FParse::Value(Str, TEXT("ELEMENTCOUNT="), ParsedText))
		{
			/** Number of elements in bulk data array */
			ElementCount = FCString::Atoi(*ParsedText);
		}
		else
		if (FParse::Value(Str, TEXT("ELEMENTSIZE="), ParsedText))
		{
			/** Serialized flags for bulk data */
			ElementSize = FCString::Atoi(*ParsedText);
		}
		else
		if (FParse::Value(Str, TEXT("BEGIN "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARYBLOB")))
		{
			uint8* RawData = NULL;
			/** The bulk data... */
			while(FParse::Line(&Buffer,StrLine))
			{
				Str = *StrLine;

				if (FParse::Value(Str, TEXT("SIZE="), ParsedText))
				{
					int32 Size = FCString::Atoi(*ParsedText);

					check(Size == (ElementSize *ElementCount));

					BulkData.Lock(LOCK_READ_WRITE);
					void* RawBulkData = BulkData.Realloc(ElementCount);
					RawData = (uint8*)RawBulkData;
					bBulkDataIsLocked = true;
				}
				else
				if (FParse::Value(Str, TEXT("BEGIN "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARY")))
				{
					uint8* BulkDataPointer = RawData;
					while(FParse::Line(&Buffer,StrLine))
					{
						Str = *StrLine;
						TCHAR* ParseStr = (TCHAR*)(Str);

						if (FParse::Value(Str, TEXT("END "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARY")))
						{
							break;
						}

						// Clear whitespace
						while ((*ParseStr == L' ') || (*ParseStr == L'\t'))
						{
							ParseStr++;
						}

						// Parse the values into the bulk data...
						while ((*ParseStr != L'\n') && (*ParseStr != L'\r') && (*ParseStr != 0))
						{
							int32 Value;
							if (!FCString::Strnicmp(ParseStr, TEXT("0x"), 2))
							{
								ParseStr +=2;
							}
							Value = FParse::HexDigit(ParseStr[0]) * 16 + FParse::HexDigit(ParseStr[1]);
							*BulkDataPointer = (uint8)Value;
							BulkDataPointer++;
							ParseStr += 2;
							ParseStr++;
						}
					}
				}
				else
				if (FParse::Value(Str, TEXT("END "), ParsedText) && (ParsedText.ToUpper() == TEXT("BINARYBLOB")))
				{
					BulkData.Unlock();
					bBulkDataIsLocked = false;
					break;
				}
			}
		}
		else
		if (FParse::Value(Str, TEXT("END "), ParsedText) && (ParsedText.ToUpper() == TEXT("UNTYPEDBULKDATA")))
		{
			break;
		}
	}

	if (bBulkDataIsLocked == true)
	{
		BulkData.Unlock();
	}

	return true;
}

UObject* UFactory::CreateOrOverwriteAsset(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InTemplate) const
{
	// Creates an asset if it doesn't exist.
	UObject* ExistingAsset = StaticFindObject(NULL, InParent, *InName.ToString());
	if ( !ExistingAsset )
	{
		return NewObject<UObject>(InParent, InClass, InName, InFlags, InTemplate);
	}

	// If it does exist then it overwrites it if possible.
	if ( ExistingAsset->GetClass()->IsChildOf(InClass) )
	{
		return NewObject<UObject>(InParent, InClass, InName, InFlags, InTemplate);
	}
	
	// If it can not overwrite then it will delete and replace.
	if ( ObjectTools::DeleteSingleObject( ExistingAsset ) )
	{
		// Keep InPackage alive through the GC, in case ExistingAsset was the only reason it was around.
		const bool bRootedPackage = InParent->IsRooted();
		if ( !bRootedPackage )
		{
			InParent->AddToRoot();
		}

		// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		if ( !bRootedPackage )
		{
			InParent->RemoveFromRoot();
		}

		// Try to find the existing asset again now that the GC has occurred
		ExistingAsset = StaticFindObject(NULL, InParent, *InName.ToString());
		if ( ExistingAsset )
		{
			// Even after the delete and GC, the object is still around. Fail this operation.
			return NULL;
		}
		else
		{
			// We can now create the asset in the package
			return NewObject<UObject>(InParent, InClass, InName, InFlags, InTemplate);
		}
	}
	else
	{
		// The delete did not succeed. There are still references to the old content.
		return NULL;
	}
}

FString UFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("New")) + GetSupportedClass()->GetName();
}