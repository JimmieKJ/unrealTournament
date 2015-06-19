// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FString;

/** 
 * Symbol information associated with a program counter. 
 * FString version.
 * To be used by external tools.
 */
struct CORE_API FProgramCounterSymbolInfoEx
{
	/** Module name. */
	FString	ModuleName;

	/** Function name. */
	FString	FunctionName;

	/** Filename. */
	FString	Filename;

	/** Line number in file. */
	uint32	LineNumber;

	/** Symbol displacement of address.	*/
	uint64	SymbolDisplacement;

	/** Program counter offset into module. */
	uint64	OffsetInModule;

	/** Program counter. */
	uint64	ProgramCounter;

	/** Default constructor. */
	FProgramCounterSymbolInfoEx( FString InModuleName, FString InFunctionName, FString InFilename, uint32 InLineNumber, uint64 InSymbolDisplacement, uint64 InOffsetInModule, uint64 InProgramCounter );
};


/** Enumerates crash description versions. */
enum class ECrashDescVersions
{
	/** Introduces a new crash description format. */
	VER_1_NewCrashFormat,

	/** Added misc properties (CPU,GPU,OS,etc), memory related stats and platform specific properties as generic payload. */
	VER_2_AddedNewProperties,
};

/**
 *	Contains a runtime crash's properties that are common for all platforms.
 *	This may change in the future.
 */
struct CORE_API FGenericCrashContext
{
public:
	static const ANSICHAR* CrashContextRuntimeXMLNameA;
	static const TCHAR* CrashContextRuntimeXMLNameW;

	/**	Whether the Initialize() has been called */
	static bool bIsInitialized;

	/** Initializes crash context related platform specific data that can be impossible to obtain after a crash. */
	static void Initialize();

	/**
	 * @return true, if the generic crash context has been initialized.
	 */
	static bool IsInitalized()
	{
		return bIsInitialized;
	}

	/** Default constructor. */
	FGenericCrashContext();

	/** Serializes all data to the buffer. */
	void SerializeContentToBuffer();

	const FString& GetBuffer() const
	{
		return CommonBuffer;
	}

	/**
	 * @return a globally unique crash name.
	 */
	const FString& GetUniqueCrashName();

	/** Serializes crash's informations to the specified filename. */
	void SerializeAsXML( const TCHAR* Filename );

	/** Writes a common property to the buffer. */
	void AddCrashProperty( const TCHAR* PropertyName, const TCHAR* PropertyValue );

	/** Writes a common property to the buffer. */
	template <typename Type>
	void AddCrashProperty( const TCHAR* PropertyName, const Type& Value )
	{
		AddCrashProperty( PropertyName, *TTypeToString<Type>::ToString( Value ) );
	}

private:
	/** Serializes platform specific properties to the buffer. */
	virtual void AddPlatformSpecificProperties();

	/** Writes header information to the buffer. */
	void AddHeader();

	/** Writes footer to the buffer. */
	void AddFooter();

	void BeginSection( const TCHAR* SectionName );
	void EndSection( const TCHAR* SectionName );

	/** The buffer used to store the crash's properties. */
	FString CommonBuffer;

	// FNoncopyable
	FGenericCrashContext( const FGenericCrashContext& );
	FGenericCrashContext& operator=(const FGenericCrashContext&);
};

struct CORE_API FGenericMemoryWarningContext
{};
