// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Wildcard string used to search for shader format modules. */
#define SHADERFORMAT_MODULE_WILDCARD TEXT("*ShaderFormat*")


/**
 * IShaderFormat, shader pre-compilation abstraction
 */
class IShaderFormat
{
public:

	/** 
	 * Compile the specified shader.
	 *
	 * @param Format The desired format
	 * @param Input The input to the shader compiler.
	 * @param Output The output from shader compiler.
	 * @param WorkingDirectory The working directory.
	 */
	virtual void CompileShader( FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory ) const = 0;

	/**
	 * Gets the current version of the specified shader format.
	 *
	 * @param Format The format to get the version for.
	 * @return Version number.
	 */
	virtual uint16 GetVersion( FName Format ) const = 0;

	/**
	 * Gets the list of supported formats.
	 *
	 * @param OutFormats Will hold the list of formats.
	 */
	virtual void GetSupportedFormats( TArray<FName>& OutFormats ) const = 0;

public:

	/** Virtual destructor. */
	virtual ~IShaderFormat() { }
};
