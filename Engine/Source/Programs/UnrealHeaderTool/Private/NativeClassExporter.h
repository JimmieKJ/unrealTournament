// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ParserClass.h"
#include "Classes.h"

//
//	FNativeClassHeaderGenerator
//

namespace EExportFunctionHeaderStyle
{
	enum Type
	{
		Definition,
		Declaration
	};
}

namespace EExportFunctionType
{
	enum Type
	{
		Interface,
		Function,
		Event
	};
}

class FClass;
class FClasses;
class FModuleClasses;

struct FNativeClassHeaderGenerator
{
private:
	FClass*				CurrentClass;
	TArray<FClass*>		Classes;
	FString				API;
	FString			ClassesHeaderPath;
	/** the name of the cpp file where the implementation functions are generated, without the .cpp extension */
	FString				GeneratedCPPFilenameBase;
	/** the name of the proto file where proto definitions are generated */
	FString				GeneratedProtoFilenameBase;
	/** the name of the java file where mcp definitions are generated */
	FString				GeneratedMCPFilenameBase;
	UPackage*			Package;
	FStringOutputDevice	PreHeaderText;
	FStringOutputDevice	EnumForeachText;
	FStringOutputDevice ListOfPublicClassesUObjectHeaderGroupIncludes;  // This is built up each time from scratch each time for each header groups
	FStringOutputDevice ListOfPublicClassesUObjectHeaderModuleIncludes; // This is built up for the entire module, and for all processed headers
	FStringOutputDevice ListOfAllUObjectHeaderIncludes;
	FStringOutputDevice FriendText;
	FStringOutputDevice	GeneratedPackageCPP;
	FStringOutputDevice	GeneratedHeaderText;
	FStringOutputDevice	GeneratedHeaderTextBeforeForwardDeclarations;
	FStringOutputDevice	GeneratedForwardDeclarations;
    /** output generated for a .proto file */
	FStringOutputDevice GeneratedProtoText;
	/** output generated for a mcp .java file */
	FStringOutputDevice GeneratedMCPText;
	FStringOutputDevice	PrologMacroCalls;
	FStringOutputDevice	InClassMacroCalls;
	FStringOutputDevice	InClassNoPureDeclsMacroCalls;
	FStringOutputDevice	StandardUObjectConstructorsMacroCall;
	FStringOutputDevice	EnhancedUObjectConstructorsMacroCall;
	FStringOutputDevice	AllConstructors;
	FStringOutputDevice StaticChecks;

	/** Generated function implementations that belong in the cpp file, split into multiple files base on line count **/
	TArray<FStringOutputDeviceCountLines> GeneratedFunctionBodyTextSplit;
	/** Declarations of generated functions for this module**/
	FStringOutputDevice	GeneratedFunctionDeclarations;
	/** Declarations of generated functions for cross module**/
	FStringOutputDevice	CrossModuleGeneratedFunctionDeclarations;
	/** Set of already exported cross-module references, to prevent duplicates */
	TSet<FString> UniqueCrossModuleReferences;

	/** Names that have been exported so far **/
	TSet<FName> ReferencedNames;

	/** the existing disk version of the header for this package's names */
	FString				OriginalNamesHeader;
	/** the existing disk version of this header */
	FString				OriginalHeader;

	/** Array of temp filenames that for files to overwrite headers */
	TArray<FString>		TempHeaderPaths;

	/** Array of all header filenames from the current package. */
	TArray<FString>		PackageHeaderPaths;

	/** Array of all generated Classes headers for the current package */
	TArray<FString>		ClassesHeaders;
	
	/** if we are exporting a struct for offset determination only, replace some types with simpler ones (delegates) */
	bool bIsExportingForOffsetDeterminationOnly;

	/** If false, exported headers will not be saved to disk */
	bool bAllowSaveExportedHeaders;

	/** If true, any change in the generated headers will result in UHT failure. */
	bool bFailIfGeneratedCodeChanges;

	/** All properties that need to be forward declared. */
	TArray<UProperty*>	ForwardDeclarations;

	// This exists because it makes debugging much easier on VC2010, since the visualizers can't properly understand templates with templated args
	struct HeaderDependents : TArray<const FString*>
	{
	};

	/**
	 * Sorts the list of header files being exported from a package according to their dependency on each other.
	 *
	 * @param	HeaderDependencyMap		A map of headers and their dependencies. Each header is represented as the actual filename string.
	 * @param	SortedHeaderFilenames	[out] receives the sorted list of header filenames.
	 * @return	true upon success, false if there were circular dependencies which made it impossible to sort properly.
	 */
	bool SortHeaderDependencyMap(const TMap<const FString*, HeaderDependents>& HeaderDependencyMap, TArray<const FString*>& SortedHeaderFilenames ) const;

	/**
	 * Finds to headers that are dependent on each other.
	 * Wrapper for FindInterDependencyRecursive().
	 *
	 * @param  HeaderDependencyMap A map of headers and their dependencies. Each header is represented as an index into a TArray of the actual filename strings.
	 * @param  Header              A header to scan for any inter-dependency.
	 * @param  OutHeader1          [out] Receives the first inter-dependent header index.
	 * @param  OutHeader2          [out] Receives the second inter-dependent header index.
	 * @return true if an inter-dependency was found.
	 */
	bool FindInterDependency( TMap<const FString*, HeaderDependents>& HeaderDependencyMap, const FString* Header, const FString*& OutHeader1, const FString*& OutHeader2 );

	/**
	 * Finds to headers that are dependent on each other.
	 *
	 * @param  HeaderDependencyMap A map of headers and their dependencies. Each header is represented as an index into a TArray of the actual filename strings.
	 * @param  Header              A header to scan for any inter-dependency.
	 * @param  VisitedHeaders      Must be filled with false values before the first call (must be large enough to be indexed by all headers).
	 * @param  OutHeader1          [out] Receives the first inter-dependent header index.
	 * @param  OutHeader2          [out] Receives the second inter-dependent header index.
	 * @return true if an inter-dependency was found.
	 */
	bool FindInterDependencyRecursive( TMap<const FString*, HeaderDependents>& HeaderDependencyMap, const FString* HeaderIndex, TSet<const FString*>& VisitedHeaders, const FString*& OutHeader1, const FString*& OutHeader2 );

	/**
	 * Determines whether the glue version of the specified native function
	 * should be exported
	 * 
	 * @param	Function	the function to check
	 * @return	true if the glue version of the function should be exported.
	 */
	bool ShouldExportFunction( UFunction* Function );

	/**
	 * Returns a C++ code string for declaring a class or variable as an export, if the class is configured for that
	 *
	 * @return	The API declaration string
	 */
	FString MakeAPIDecl() const;

	/**
	 * Exports the struct's C++ properties to the HeaderText output device and adds special
	 * compiler directives for GCC to pack as we expect.
	 *
	 * @param	Struct				UStruct to export properties
	 * @param	TextIndent			Current text indentation
	 * @param	Output	alternate output device
	 */
	void ExportProperties( UStruct* Struct, int32 TextIndent, bool bAccessSpecifiers = true, class FStringOutputDevice* Output = NULL );

	/** Return the name of the singleton function that returns the UObject for Item **/
	FString GetSingletonName(UField* Item, bool bRequiresValidObject=true);
	FString GetSingletonName(FClass* Item, bool bRequiresValidObject=true);

	/** 
	 * Export functions used to find and call C++ or script implementation of a script function in the interface 
	 */
	void ExportInterfaceCallFunctions( const TArray<UFunction*>& CallbackFunctions, FStringOutputDevice& HeaderOutput );

	/**
	 * Exports the C++ class declarations for a native interface class.
	 */
	void ExportInterfaceClassDeclaration( FClass* Class );

	/**
	 * Appends the header definition for an inheritance hierarchy of classes to the header.
	 * Wrapper for ExportClassHeaderRecursive().
	 *
	 * @param	AllClasses			The classes being processed.
	 * @param	Class				The class to be exported.
	 */
	void ExportClassHeader( FClasses& AllClasses, FClass* Class );

	/**
	 * After all of the dependency checking, and setup for isolating the generated code, actually export the class
	 *
	 * @param	Class				The class to be exported.
	 * @param	bValidNonTemporaryClass	Is the class permanent, or a temporary shell?
	 */
	void ExportClassHeaderInner( FClass* Class, bool bValidNonTemporaryClass );

	/**
	 * After all of the dependency checking, but before actually exporting the class, set up the generated code
	 *
	 * @param	Class				The class to be exported.
	 * @param	bIsExportClass		true if this is an export class
	 */
	void ExportClassHeaderWrapper( FClass* Class, bool bIsExportClass );

	/**
	 * Appends the header definition for an inheritance hierarchy of classes to the header.
	 *
	 * @param	AllClasses				The classes being processed.
	 * @param	Class					The class to be exported.
	 * @param	DependencyChain			Used for finding errors. Must be empty before the first call.
	 * @param	VisitedSet				The set of classes visited so far. Must be empty before the first call.
	 * @param	bCheckDependenciesOnly	Whether we should just keep checking for dependency errors, without exporting anything.
	 */
	void ExportClassHeaderRecursive( FClasses& AllClasses, FClass* Class, TArray<FClass*>& DependencyChain, TSet<const UClass*>& VisitedSet, bool bCheckDependenciesOnly );

	/**
	 * Returns a string in the format CLASS_Something|CLASS_Something which represents all class flags that are set for the specified
	 * class which need to be exported as part of the DECLARE_CLASS macro
	 */
	FString GetClassFlagExportText( UClass* Class );

	/**
	 * Iterates through all fields of the specified class, and separates fields that should be exported with this class into the appropriate array.
	 * 
	 * @param	Class				the class to pull fields from
	 * @param	Enums				[out] all enums declared in the specified class
	 * @param	Structs				[out] list of structs declared in the specified class
	 * @param	CallbackFunctions	[out] list of delegates and events declared in the specified class
	 * @param	NativeFunctions		[out] list of native functions declared in the specified class
	 * @param	DelegateProperties	[out] list of delegate properties declared in the specified class
	 */
	void RetrieveRelevantFields(UClass* Class, TArray<UEnum*>& Enums, TArray<UScriptStruct*>& Structs, TArray<UFunction*>& CallbackFunctions, TArray<UFunction*>& NativeFunctions, TArray<UFunction*>& DelegateProperties);

	/**
	 * Exports the header text for the names needed
	 */
	void ExportNames();

	/**
	 * Exports the header text for the list of enums specified
	 * 
	 * @param	Enums	the enums to export
	 */
	void ExportEnums( const TArray<UEnum*>& Enums );

	/**
	 * Exports the inl text for enums declared in non-UClass headers.
	 * 
	 * @param	Enums	the enums to export
	 */
	void ExportGeneratedEnumsInitCode(const TArray<UEnum*>& Enums);

	/**
	 * Exports the macro declarations for GENERATED_USTRUCT_BODY() for each Foo in the list of structs specified
	 * 
	 * @param	Structs		the structs to export
	 */
	void ExportGeneratedStructBodyMacros(const TArray<UScriptStruct*>& NativeStructs);

	/**
	 * Exports a local mirror of the specified struct; used to get offsets for noexport structs
	 * 
	 * @param	Structs		the structs to export
	 * @param	TextIndent	the current indentation of the header exporter
	 * @param	HeaderOutput	the output device for the mirror struct
	 */
	void ExportMirrorsForNoexportStructs( const TArray<UScriptStruct*>& NativeStructs, int32 TextIndent, FStringOutputDevice& HeaderOutput);

	/**
	 * Exports the parameter struct declarations for the list of functions specified
	 * 
	 * @param	Function	the function that (may) have parameters which need to be exported
	 * @return	true		if the structure generated is not completely empty
	 */
	bool WillExportEventParms( UFunction* Function );

	/**
	 * Exports C++ type definitions for delegates
	 *
	 * @param	DelegateFunctions	the functions that have parameters which need to be exported
	 * @param	bWrapperImplementationsOnly	 True if only function implementations for delegate wrappers should be exported
	 */
	void ExportDelegateDefinitions( const TArray<UFunction*>& DelegateFunctions, const bool bWrapperImplementationsOnly );

	/**
	 * Exports the parameter struct declarations for the list of functions specified
	 * 
	 * @param	CallbackFunctions	the functions that have parameters which need to be exported
	 * @param	Indent				number of spaces to put before each line
	 * @param	bOutputConstructor	If true, output a constructor for the parm struct
	 * @param	Output				ALternate output device
	 */
	void ExportEventParms( const TArray<UFunction*>& CallbackFunctions, const TCHAR* MacroSuffix, int32 Indent = 0, bool bOutputConstructor=true, class FStringOutputDevice* Output = NULL );

	/**
	 * Generate a .proto message declaration for any functions marked as requiring one
	 * 
	 * @param InCallbackFunctions array of functions for consideration to generate .proto definitions
	 * @param Indent starting indentation level
	 * @param Output optional output redirect
	 */
	void ExportProtoMessage(const TArray<UFunction*>& InCallbackFunctions, int32 Indent = 0, class FStringOutputDevice* Output = NULL);

	/**
	 * Generate a .java message declaration for any functions marked as requiring one
	 * 
	 * @param InCallbackFunctions array of functions for consideration to generate .proto definitions
	 * @param Indent starting indentation level
	 * @param Output optional output redirect
	 */
	void ExportMCPMessage(const TArray<UFunction*>& InCallbackFunctions, int32 Indent = 0, class FStringOutputDevice* Output = NULL);

	/** 
	* Exports the temp header files into the .h files, then deletes the temp files.
	* 
	* @param	PackageName	Name of the package being saved
	*/
	void ExportUpdatedHeaders( FString PackageName  );

	/**
	 * Exports the generated cpp file for all functions/events/delegates in package.
	 */
	void ExportGeneratedCPP();

	/**
	 * Exports protobuffer definitions from boilerplate that was generated for a package.
	 * They are exported to a file using the name <PackageName>.generated.proto
	 */
	void ExportGeneratedProto();

	/**
	 * Exports mcp definitions from boilerplate that was generated for a package.
	 * They are exported to a file using the name <PackageName>.generated.java
	 */
	void ExportGeneratedMCP();

	/**
	 * Get the intrinsic null value for this property
	 * 
	 * @param	Prop				the property to get the null value for
	 * @param	bMacroContext		true when exporting the P_GET* macro, false when exporting the friendly C++ function header
	 * @param	bInitializer		if true, will just return ForceInit instead of FStruct(ForceInit)
	 *
	 * @return	the intrinsic null value for the property (0 for ints, TEXT("") for strings, etc.)
	 */
	FString GetNullParameterValue( UProperty* Prop, bool bMacroContext, bool bInitializer = false );

	/**
	 * Exports a native function prototype
	 * 
	 * @param	FunctionData		data representing the function to export
	 * @param	HeaderOutput		Where to write the exported function prototype to.
	 * @param	FunctionType		Whether to export this function prototype as an event stub, an interface or a native function stub.
	 * @param	FunctionHeaderStyle	Whether we're outputting a declaration or definition.
	 * @param	ExtraParam			Optional extra parameter that will be added to the declaration as the first argument
	 */
	void ExportNativeFunctionHeader( const FFuncInfo& FunctionData, FStringOutputDevice& HeaderOutput, EExportFunctionType::Type FunctionType, EExportFunctionHeaderStyle::Type FunctionHeaderStyle, const TCHAR* ExtraParam = NULL );

	/**
	* Exports checks if function exists
	*
	* @param	FunctionData			Data representing the function to export.
	* @param	CheckClasses			Where to write the member check classes.
	* @param	StaticChecks			Where to write the static asserts throwing errors when function doesn't exist.
	* @param	ClassName				Name of currently parsed class.
	*/
	void ExportFunctionChecks(const FFuncInfo& FunctionData, FStringOutputDevice& CheckClasses, FStringOutputDevice& StaticChecks, const FString& ClassName);

	/**
	 * Exports the native stubs for the list of functions specified
	 * 
	 * @param	NativeFunctions	the functions to export
	 */
	void ExportNativeFunctions(const TArray<UFunction*>& NativeFunctions);

	/**
	 * Export the actual internals to a standard thunk function
	 *
	 * @param RPCWrappers output device for writing
	 * @param FunctionData function data for the current function
	 * @param Parameters list of parameters in the function
	 * @param Return return parameter for the function
	 */
	void ExportFunctionThunk(FStringOutputDevice& RPCWrappers, const FFuncInfo& FunctionData, const TArray<UProperty*>& Parameters, UProperty* Return);

	/** Exports the native function registration code for the given class. */
	void ExportNatives(FClass* Class);

	/**
	 * Exports generated singleton functions for UObjects that used to be stored in .u files.
	 * 
	 * @param	Class			Class to export
	**/
	void ExportNativeGeneratedInitCode(FClass* Class);

	/**
	 * Exports a generated singleton function to setup the package for compiled-in classes.
	 * 
	 * @param	Package		Package to export code for.
	**/
	void ExportGeneratedPackageInitCode(UPackage* Package);

	/**
	 * Function to output the C++ code necessary to set up the given array of properties
	 * 
	 * @param	Meta			Returned string of meta data generator code
	 * @param	OutputDevice	String output device to send the generated code to
	 * @param	Properties		Array of properties to export
	 * @param	Spaces			String of spaces to use as an indent
	 */
	void OutputProperties(FString& Meta, FOutputDevice& OutputDevice, const FString& OuterString, TArray<UProperty*>& Properties, const TCHAR* Spaces);

	/**
	 * Function to output the C++ code necessary to set up a property
	 * 
	 * @param	Meta			Returned string of meta data generator code
	 * @param	OutputDevice	String output device to send the generated code to
	 * @param	Prop			Property to export
	 * @param	Spaces			String of spaces to use as an indent
	**/
	void OutputProperty(FString& Meta, FOutputDevice& OutputDevice, const FString& OuterString, UProperty* Prop, const TCHAR* Spaces);

	/**
	 * Function to output the C++ code necessary to set up a property, including an array property and its inner, array dimensions, etc.
	 * 
	 * @param	Meta			Returned string of meta data generator code
	 * @param	Prop			Property to export
	 * @param	OuterString		String that specifies the outer to add the properties to
	 * @param	PropMacro		String specifying the macro to call to get the property offset
	 * @param	NameSuffix		Suffix for the generated variable name
	 * @param	Spaces			String of spaces to use as an indent
	 * @param	SourceStruct	Structure that the property offset is relative to
	**/
	FString PropertyNew(FString& Meta, UProperty* Prop, const FString& OuterString, const FString& PropMacro, const TCHAR* NameSuffix, const TCHAR* Spaces, const TCHAR* SourceStruct = NULL);

	/**
	 * Exports the proxy definitions for the list of enums specified
	 * 
	 * @param	CallbackFunctions	the functions to export
	 */
	void ExportCallbackFunctions( const TArray<UFunction*>& CallbackFunctions);

	/**
	 * Determines if the property has alternate export text associated with it and if so replaces the text in PropertyText with the
	 * alternate version. (for example, structs or properties that specify a native type using export-text).  Should be called immediately
	 * after ExportCppDeclaration()
	 *
	 * @param	Prop			the property that is being exported
	 * @param	PropertyText	the string containing the text exported from ExportCppDeclaration
	 */
	void ApplyAlternatePropertyExportText( UProperty* Prop, FStringOutputDevice& PropertyText );

	/**
	* Create a temp header file name from the header name
	*
	* @param	CurrentFilename		The filename off of which the current filename will be generated
	* @param	bReverseOperation	Get the header from the temp file name instead
	*
	* @return	The generated string
	*/
	FString GenerateTempHeaderName( FString CurrentFilename, bool bReverseOperation = false );

	/**
	 * Saves a generated header if it has changed. 
	 *
	 * @param HeaderPath	Header Filename
	 * @param NewHeaderContents	Contents of the generated header.
	 * @return True if the header contents has changed, false otherwise.
	 */
	bool SaveHeaderIfChanged(const TCHAR* HeaderPath, const TCHAR* NewHeaderContents);

	/**
	 * Deletes all .generated.h files which do not correspond to any of the classes.
	 */
	void DeleteUnusedGeneratedHeaders();

	/**
	 * Exports macros that manages UObject constructors.
	 * 
	 * @param Class Class for which to export macros.
	 */
	void ExportConstructorsMacros(FClass* Class);

public:

	// Constructor
	FNativeClassHeaderGenerator( UPackage* InPackage, FClasses& AllClasses, bool InAllowSaveExportedHeaders );

	/**
	 * Gets string with function return type.
	 * 
	 * @param Function Function to get return type of.
	 * @return FString with function return type.
	 */
	FString GetFunctionReturnString(UFunction* Function);

	/**
	* Gets string with function parameters (with names).
	*
	* @param Function Function to get parameters of.
	* @return FString with function parameters.
	*/
	FString GetFunctionParameterString(UFunction* Function);
};
