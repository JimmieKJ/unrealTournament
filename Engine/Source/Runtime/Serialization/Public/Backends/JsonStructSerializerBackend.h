// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStructSerializerBackend.h"
#include "Json.h"


// forward declarations
class UProperty;
class UStruct;


/**
 * Implements a writer for UStruct serialization using Json.
 *
 * Note: The underlying Json serializer is currently hard-coded to use UCS2CHAR and pretty-print.
 * This is because the current JsonWriter API does not allow writers to be substituted since it's
 * all based on templates. At some point we will refactor the low-level Json API to provide more
 * flexibility for serialization.
 */
class SERIALIZATION_API FJsonStructSerializerBackend
	: public IStructSerializerBackend
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param Archive The archive to serialize into.
	 */
	FJsonStructSerializerBackend( FArchive& Archive )
		: JsonWriter(TJsonWriter<UCS2CHAR>::Create(&Archive))
	{ }

public:

	// IStructSerializerBackend interface

	virtual void BeginArray( UProperty* Property ) override;
	virtual void BeginStructure( UProperty* Property ) override;
	virtual void BeginStructure( UStruct* TypeInfo ) override;
	virtual void EndArray( UProperty* Property ) override;
	virtual void EndStructure() override;
	virtual void WriteComment( const FString& Comment ) override;
	virtual void WriteProperty( UProperty* Property, const void* Data, UStruct* TypeInfo, int32 ArrayIndex ) override;

private:

	/** Holds the Json writer used for the actual serialization. */
	TSharedRef<TJsonWriter<UCS2CHAR>> JsonWriter;
};
