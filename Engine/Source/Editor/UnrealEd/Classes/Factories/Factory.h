// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Base class for all factories
 * An object responsible for creating and importing new objects.
 * 
 */

#pragma once
#include "Factory.generated.h"

UCLASS(abstract)
class UNREALED_API UFactory : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The class manufactured by this factory. */
	UPROPERTY()
	TSubclassOf<class UObject>  SupportedClass;

	/** Class of the context object used to help create the object. */
	UPROPERTY()
	TSubclassOf<class UObject>  ContextClass;

	/** List of formats supported by the factory. Each entry is of the form "ext;Description" where ext is the file extension. */
	UPROPERTY()
	TArray<FString> Formats;

protected:
	/** The default value to return from CanCreateNew() */
	UPROPERTY()
	uint32 bCreateNew:1;

public:
	/** true if the associated editor should be opened after creating a new object. */
	UPROPERTY()
	uint32 bEditAfterNew:1;

	/** true if the factory imports objects from files. */
	UPROPERTY()
	uint32 bEditorImport:1;

	/** true if the factory imports objects from text. */
	UPROPERTY()
	uint32 bText:1;

	/** Determines the order in which factories are tried when importing an object. */
	DEPRECATED(4.8, "AutoPriority has been replaced with ImportPriority")
	int32 AutoPriority;

	/** Determines the order in which factories are tried when importing or reimporting an object.
		Factories with higher priority values will go first. Factories with negative priorities will be excluded. */
	UPROPERTY()
	int32 ImportPriority;

	/** This is the import priority that all factories are given in the default constructor. */
	static const int32 DefaultImportPriority;

	static FString GetCurrentFilename() { return CurrentFilename; }

	static FString CurrentFilename;

	/** For interactive object imports, this value indicates whether the user wants objects to be automatically
		overwritten (See EAppReturnType), or -1 if the user should be prompted. */
	static int32 OverwriteYesOrNoToAllState;

	/** If this value is true, warning messages will be shown once for all objects being imported at the same time.  
		This value will be reset to false each time a new import operation is started. */
	DEPRECATED(4.8, "bAllowOneTimeWarningMessages is due to be removed in future.")
	static bool bAllowOneTimeWarningMessages;

	// Begin UObject interface.
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject interface.

	/** Returns true if this factory should be shown in the New Asset menu (by default calls CanCreateNew). */
	virtual bool ShouldShowInNewMenu() const;

	/** Returns an optional override brush name for the new asset menu. If this is not specified, the thumbnail for the supported class will be used. */
	virtual FName GetNewAssetThumbnailOverride() const { return NAME_None; }

	/** Returns the name of the factory for menus */
	virtual FText GetDisplayName() const;

	/** When shown in menus, this is the category containing this factory. Return type is a BitFlag mask using EAssetTypeCategories. */
	virtual uint32 GetMenuCategories() const;

	/** Returns the tooltip text description of this factory */
	virtual FText GetToolTip() const;

	/** Returns the documentation page that should be use for the rich tool tip for this factory */
	virtual FString GetToolTipDocumentationPage() const;

	/** Returns the documentation excerpt that should be use for the rich tool tip for this factory */
	virtual FString GetToolTipDocumentationExcerpt() const;

	/**
	 * @return		The object class supported by this factory.
	 */
	UClass* GetSupportedClass() const;

	/**
	 * @return true if it supports this class 
	 */
	virtual bool DoesSupportClass(UClass* Class);

	/**
	 * Resolves SupportedClass for factories which support multiple classes.
	 * Such factories will have a NULL SupportedClass member.
	 */
	virtual UClass* ResolveSupportedClass();

	/**
	 * Resets the saved state of this factory.  The states are used to suppress messages during multiple object import. 
	 * It needs to be reset each time a new import is started
	 */
	static void ResetState();

	/**
	 * Pop up message to the user asking whether they wish to overwrite existing state or not
	 */
	static void DisplayOverwriteOptionsDialog(const FText& Message);

	/** Opens a dialog to configure the factory properties. Return false if user opted out of configuring properties */
	virtual bool ConfigureProperties() { return true; }

	/**
	 * @return true if the factory can currently create a new object from scratch.
	 */
	virtual bool CanCreateNew() const { return bCreateNew; }

	// @todo document
	virtual UObject* FactoryCreateText( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn ) {return NULL;}

	// @todo document
	// @param Type must not be 0, e.g. TEXT("TGA")
	virtual UObject* FactoryCreateBinary( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) {return NULL;}
	// @param Type must not be 0, e.g. TEXT("TGA")
	virtual UObject* FactoryCreateBinary( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) { return FactoryCreateBinary(InClass, InParent, InName, Flags, Context, Type, Buffer, BufferEnd, Warn); }

	// @todo document
	virtual UObject* FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext )
	{
		return FactoryCreateNew(InClass, InParent, InName, Flags, Context, Warn);
	}
	virtual UObject* FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn ) {return NULL;}

	/**
	 * @return	true if this factory can deal with the file sent in.
	 */
	virtual bool FactoryCanImport( const FString& Filename );


	// @todo document
	virtual bool ImportUntypedBulkDataFromText(const TCHAR*& Buffer, FUntypedBulkData& BulkData);


	// @todo document
	static UObject* StaticImportObject( UClass* Class, UObject* InOuter, FName Name, EObjectFlags Flags, const TCHAR* Filename=TEXT(""), UObject* Context=NULL, UFactory* Factory=NULL, const TCHAR* Parms=NULL, FFeedbackContext* Warn=GWarn, int32 MaxImportFileSize = 0xC100000 );
	static UObject* StaticImportObject( UClass* Class, UObject* InOuter, FName Name, EObjectFlags Flags, bool& bOutOperationCanceled, const TCHAR* Filename=TEXT(""), UObject* Context=NULL, UFactory* Factory=NULL, const TCHAR* Parms=NULL, FFeedbackContext* Warn=GWarn, int32 MaxImportFileSize = 0xC100000 );

	/** Creates a list of file extensions supported by this factory */
	void GetSupportedFileExtensions( TArray<FString>& OutExtensions ) const;

	/** 
	 * Do clean up after importing is done. Will be called once for multi batch import
	 */
	virtual void CleanUp() {}

	/**
	 * Creates an asset if it doesn't exist. If it does exist then it overwrites it if possible. If it can not overwrite then it will delete and replace. If it can not delete, it will return NULL.
	 * 
	 * @param	InClass		the class of the asset to create
	 * @param	InPackage	the package to create this object within.
	 * @param	Name		the name to give the new asset. If no value (NAME_None) is specified, the asset will be given a unique name in the form of ClassName_#.
	 * @param	InFlags		the ObjectFlags to assign to the new asset.
	 * @param	Template	if specified, the property values from this object will be copied to the new object, and the new object's ObjectArchetype value will be set to this object.
	 *						If NULL, the class default object is used instead.
	 * @return	A pointer to a new asset of the specified type or null if the creation failed.
	 */
	UObject* CreateOrOverwriteAsset(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InTemplate = NULL) const;

	/** Returns a new starting point name for newly created assets in the content browser */
	virtual FString GetDefaultNewAssetName() const;
};
