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
class FScope;

// These are declared in this way to allow swapping out the classes for something more optimized in the future
typedef FStringOutputDevice FUHTStringBuilder;
typedef FStringOutputDeviceCountLines FUHTStringBuilderLineCounter;

struct FNativeClassHeaderGenerator
{
private:
	FString API;

	/**
	 * Gets API string for this header.
	 */
	FString GetAPIString()
	{
		return FString::Printf(TEXT("%s_API "), *API);
	}

	FString ClassesHeaderPath;
	/** the name of the cpp file where the implementation functions are generated, without the .cpp extension */
	FString GeneratedCPPFilenameBase;
	/** the name of the proto file where proto definitions are generated */
	FString GeneratedProtoFilenameBase;
	/** the name of the java file where mcp definitions are generated */
	FString GeneratedMCPFilenameBase;
	UPackage* Package;
	FUHTStringBuilder PreHeaderText;
	FUHTStringBuilder EnumForeachText;
	TArray<FUnrealSourceFile*> ListOfPublicHeaderGroupIncludes;			// This is built up each time from scratch each time for each header groups
	FUHTStringBuilder ListOfPublicClassesUObjectHeaderModuleIncludes; // This is built up for the entire module, and for all processed headers
	FUHTStringBuilder ListOfAllUObjectHeaderIncludes;
	FUHTStringBuilder GeneratedPackageCPP;
	FUHTStringBuilder GeneratedHeaderText;
	FUHTStringBuilder GeneratedHeaderTextBeforeForwardDeclarations;
	FUHTStringBuilder GeneratedForwardDeclarations;
    /** output generated for a .proto file */
	FUHTStringBuilder GeneratedProtoText;
	/** output generated for a mcp .java file */
	FUHTStringBuilder GeneratedMCPText;
	FUHTStringBuilder PrologMacroCalls;
	FUHTStringBuilder InClassMacroCalls;
	FUHTStringBuilder InClassNoPureDeclsMacroCalls;
	FUHTStringBuilder StandardUObjectConstructorsMacroCall;
	FUHTStringBuilder EnhancedUObjectConstructorsMacroCall;

	/** Generated function implementations that belong in the cpp file, split into multiple files base on line count **/
	TArray<TUniqueObj<FUHTStringBuilderLineCounter>> GeneratedFunctionBodyTextSplit;
	/** Declarations of generated functions for this module**/
	FUHTStringBuilder GeneratedFunctionDeclarations;
	/** Declarations of generated functions for cross module**/
	FUHTStringBuilder CrossModuleGeneratedFunctionDeclarations;
	/** Set of already exported cross-module references, to prevent duplicates */
	TSet<FString> UniqueCrossModuleReferences;

	/** Names that have been exported so far **/
	TSet<FName> ReferencedNames;

	/** the existing disk version of the header for this package's names */
	FString OriginalNamesHeader;
	/** the existing disk version of this header */
	FString OriginalHeader;

	/** Array of temp filenames that for files to overwrite headers */
	TArray<FString> TempHeaderPaths;

	/** Array of all header filenames from the current package. */
	TArray<FString> PackageHeaderPaths;

	/** Array of all generated Classes headers for the current package */
	TArray<FString> ClassesHeaders;
	
	/** if we are exporting a struct for offset determination only, replace some types with simpler ones (delegates) */
	bool bIsExportingForOffsetDeterminationOnly;

	/** If false, exported headers will not be saved to disk */
	bool bAllowSaveExportedHeaders;

	/** If true, any change in the generated headers will result in UHT failure. */
	bool bFailIfGeneratedCodeChanges;

#if WITH_HOT_RELOAD_CTORS
	/** If true, then generate vtable constructors and it's callers. */
	bool bExportVTableConstructors;
#endif // WITH_HOT_RELOAD_CTORS

	/** All properties that need to be forward declared. */
	TArray<UProperty*>	ForwardDeclarations;

	// This exists because it makes debugging much easier on VC2010, since the visualizers can't properly understand templates with templated args
	struct HeaderDependents : TArray<const FString*>
	{
	};

	/**
	 * Gets generated function text device.
	 */
	FUHTStringBuilder& GetGeneratedFunctionTextDevice();

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
	 * Exports the struct's C++ properties to the HeaderText output device and adds special
	 * compiler directives for GCC to pack as we expect.
	 *
	 * @param	Struct				UStruct to export properties
	 * @param	TextIndent			Current text indentation
	 * @param	Output	alternate output device
	 */
	void ExportProperties(UStruct* Struct, int32 TextIndent, bool bAccessSpecifiers = true, FUHTStringBuilder* Output = NULL);

	/** Return the name of the singleton function that returns the UObject for Item */
	FString GetSingletonName(UField* Item, bool bRequiresValidObject=true);
	/** Return the name of the singleton function that returns the UObject for Item */
	FString GetSingletonName(FClass* Item, bool bRequiresValidObject=true);

	/** 
	 * Export functions used to find and call C++ or script implementation of a script function in the interface 
	 */
	void ExportInterfaceCallFunctions(const TArray<UFunction*>& CallbackFunctions, UClass* Class, FClassMetaData* ClassData, FUHTStringBuilder& HeaderOutput);

	/** 
	 * Export UInterface boilerplate.
	 *
	 * @param UInterfaceBoilerplate Device to export to.
	 * @param Class Interface to export.
	 * @param FriendText Friend text for this boilerplate.
	 */
	void ExportUInterfaceBoilerplate(FUHTStringBuilder& UInterfaceBoilerplate, FClass* Class, const FString& FriendText);

	/**
	 * Appends the header definition for an inheritance hierarchy of classes to the header.
	 * Wrapper for ExportClassHeaderRecursive().
	 *
	 * @param	AllClasses			The classes being processed.
	 * @param	SourceFile			The source file to export header for.
	 */
	void ExportSourceFileHeader(FClasses& AllClasses, FUnrealSourceFile* SourceFile);

	/**
	 * After all of the dependency checking, and setup for isolating the generated code, actually export the class
	 *
	 * @param SourceFile Source file to export.
	 */
	void ExportClassesFromSourceFileInner(FUnrealSourceFile& SourceFile);

	/**
	 * After all of the dependency checking, but before actually exporting the class, set up the generated code
	 *
	 * @param SourceFile Source file to export.
	 */
	void ExportClassesFromSourceFileWrapper(FUnrealSourceFile& SourceFile);

	/**
	 * Appends the header definition for an inheritance hierarchy of classes to the header.
	 *
	 * @param	AllClasses				The classes being processed.
	 * @param	SourceFile				The source file for which to export header.
	 * @param	VisitedSet				The set of source files visited so far. Must be empty before the first call.
	 * @param	bCheckDependenciesOnly	Whether we should just keep checking for dependency errors, without exporting anything.
	 */
	void ExportSourceFileHeaderRecursive(FClasses& AllClasses, FUnrealSourceFile* SourceFile, TSet<const FUnrealSourceFile*>& VisitedSet, bool bCheckDependenciesOnly);

	/**
	 * Returns a string in the format CLASS_Something|CLASS_Something which represents all class flags that are set for the specified
	 * class which need to be exported as part of the DECLARE_CLASS macro
	 */
	FString GetClassFlagExportText( UClass* Class );

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
	 * Exports the macro declarations for GENERATED_BODY() for each Foo in the list of structs specified
	 * 
	 * @param	SourceFile	the source file
	 * @param	Structs		the structs to export
	 */
	void ExportGeneratedStructBodyMacros(FUnrealSourceFile& SourceFile, const TArray<UScriptStruct*>& NativeStructs);

	/**
	 * Exports a local mirror of the specified struct; used to get offsets for noexport structs
	 * 
	 * @param	Structs		the structs to export
	 * @param	TextIndent	the current indentation of the header exporter
	 * @param	HeaderOutput	the output device for the mirror struct
	 */
	void ExportMirrorsForNoexportStructs(const TArray<UScriptStruct*>& NativeStructs, int32 TextIndent, FUHTStringBuilder& HeaderOutput);

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
	 * @param	SourceFile			Source file of the delegate.
	 * @param	DelegateFunctions	the functions that have parameters which need to be exported
	 * @param	bWrapperImplementationsOnly	 True if only function implementations for delegate wrappers should be exported
	 */
	void ExportDelegateDefinitions(FUnrealSourceFile& SourceFile, const TArray<UDelegateFunction*>& DelegateFunctions, const bool bWrapperImplementationsOnly);

	/**
	 * Exports the parameter struct declarations for the list of functions specified
	 * 
	 * @param	Scope				Current scope.
	 * @param	CallbackFunctions	the functions that have parameters which need to be exported
	 * @param	Output				ALternate output device
	 * @param	Indent				number of spaces to put before each line
	 * @param	bOutputConstructor	If true, output a constructor for the parm struct
	 */
	void ExportEventParms(FScope& Scope, const TArray<UFunction*>& CallbackFunctions, FUHTStringBuilder& Output, int32 Indent = 0, bool bOutputConstructor = true);

	/**
	 * Exports the parameter struct declarations for the given function.
	 *
	 * @param	Function			the function that have parameters which need to be exported
	 * @param	HeaderOutput		output device
	 * @param	Indent				number of spaces to put before each line
	 * @param	bOutputConstructor	If true, output a constructor for the param struct
	 */
	void ExportEventParm(UFunction* Function, FUHTStringBuilder& HeaderOutput, int32 Indent = 0, bool bOutputConstructor = true);

	/**
	 * Generate a .proto message declaration for any functions marked as requiring one
	 * 
	 * @param InCallbackFunctions array of functions for consideration to generate .proto definitions
	 * @param ClassData class data
	 * @param Indent starting indentation level
	 * @param Output optional output redirect
	 */
	void ExportProtoMessage(const TArray<UFunction*>& InCallbackFunctions, FClassMetaData* ClassData, int32 Indent = 0, FUHTStringBuilder* Output = NULL);

	/**
	 * Generate a .java message declaration for any functions marked as requiring one
	 * 
	 * @param InCallbackFunctions array of functions for consideration to generate .proto definitions
	 * @param ClassData class data
	 * @param Indent starting indentation level
	 * @param Output optional output redirect
	 */
	void ExportMCPMessage(const TArray<UFunction*>& InCallbackFunctions, FClassMetaData* ClassData, int32 Indent = 0, FUHTStringBuilder* Output = NULL);

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
	void ExportNativeFunctionHeader(const FFuncInfo& FunctionData, FUHTStringBuilder& HeaderOutput, EExportFunctionType::Type FunctionType, EExportFunctionHeaderStyle::Type FunctionHeaderStyle, const TCHAR* ExtraParam = NULL);

	/**
	* Runs checks whether necessary RPC functions exist for function described by FunctionData.
	*
	* @param	FunctionData			Data representing the function to export.
	* @param	ClassName				Name of currently parsed class.
	* @param	ImplementationPosition	Position in source file of _Implementation function for function described by FunctionData.
	* @param	ValidatePosition		Position in source file of _Validate function for function described by FunctionData.
	* @param	SourceFile				Currently analyzed source file.
	*/
	void CheckRPCFunctions(const FFuncInfo& FunctionData, const FString& ClassName, int32 ImplementationPosition, int32 ValidatePosition, const FUnrealSourceFile& SourceFile);

	/**
	 * Exports the native stubs for the list of functions specified
	 * 
	 * @param SourceFile	current source file
	 * @param Class			class
	 * @param ClassData		class data
	 */
	void ExportNativeFunctions(FUnrealSourceFile& SourceFile, UClass* Class, FClassMetaData* ClassData);

	/**
	 * Export the actual internals to a standard thunk function
	 *
	 * @param RPCWrappers output device for writing
	 * @param Function given function
	 * @param FunctionData function data for the current function
	 * @param Parameters list of parameters in the function
	 * @param Return return parameter for the function
	 * @param DeprecationWarningOutputDevice Device to output deprecation warnings for _Validate and _Implementation functions.
	 */
	void ExportFunctionThunk(FUHTStringBuilder& RPCWrappers, UFunction* Function, const FFuncInfo& FunctionData, const TArray<UProperty*>& Parameters, UProperty* Return, FUHTStringBuilder& DeprecationWarningOutputDevice);

	/** Exports the native function registration code for the given class. */
	void ExportNatives(FClass* Class);

	/**
	 * Exports generated singleton functions for UObjects that used to be stored in .u files.
	 * 
	 * @param	Class			Class to export
	 * @param	OutFriendText	(Output parameter) Friend text
	 */
	void ExportNativeGeneratedInitCode(FClass* Class, FUHTStringBuilder& OutFriendText);

	/**
	 * Export given function.
	 *
	 * @param Function Given function.
	 * @param Scope Scope for this function.
	 * @param bIsNoExport Is in NoExport class.
	 */
	void ExportFunction(UFunction* Function, FScope* Scope, bool bIsNoExport);

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
	 * @param SourceFile	current source file
	 * @param Class			current class
	 * @param ClassData		current class data
	 */
	TArray<UFunction*> ExportCallbackFunctions(FUnrealSourceFile& SourceFile, UClass* Class, FClassMetaData* ClassData);

	/**
	 * Determines if the property has alternate export text associated with it and if so replaces the text in PropertyText with the
	 * alternate version. (for example, structs or properties that specify a native type using export-text).  Should be called immediately
	 * after ExportCppDeclaration()
	 *
	 * @param	Prop			the property that is being exported
	 * @param	PropertyText	the string containing the text exported from ExportCppDeclaration
	 */
	void ApplyAlternatePropertyExportText(UProperty* Prop, FUHTStringBuilder& PropertyText);

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
	 * @param ConstructorsMacroPrefix Prefix for constructors macro.
	 * @param Class Class for which to export macros.
	 */
	void ExportConstructorsMacros(const FString& ConstructorsMacroPrefix, FClass* Class);

	/**
	 * Gets list of public headers for the given package in the form of string of includes.
	 */
	FString GetListOfPublicHeaderGroupIncludesString(UPackage* Package);

public:

	// Constructor
	FNativeClassHeaderGenerator(
		UPackage* InPackage,
		const TArray<FUnrealSourceFile*>& SourceFiles,
		FClasses& AllClasses,
		bool InAllowSaveExportedHeaders
#if WITH_HOT_RELOAD_CTORS
		, bool bExportVTableConstructors
#endif // WITH_HOT_RELOAD_CTORS
	);

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

	/**
	 * Checks if function is missing "virtual" specifier.
	 * 
	 * @param SourceFile SourceFile where function is declared.
	 * @param FunctionNamePosition Position of name of function in SourceFile.
	 * @return true if function misses "virtual" specifier, false otherwise.
	 */
	bool IsMissingVirtualSpecifier(const FString& SourceFile, int32 FunctionNamePosition) const;
};
