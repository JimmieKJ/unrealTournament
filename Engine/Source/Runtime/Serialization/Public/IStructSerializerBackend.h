// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// forward declarations
class UProperty;
class UStruct;


/**
 * Interface for UStruct serializer backends.
 */
class IStructSerializerBackend
{
public:

	/**
	 * Signals the beginning of an array.
	 *
	 * @param Property The property that holds the array.
	 */
	virtual void BeginArray( UProperty* Property ) = 0;

	/**
	 * Signals the beginning of a child structure.
	 *
	 * @param Property The property that holds the object.
	 */
	virtual void BeginStructure( UProperty* Property ) = 0;

	/**
	 * Signals the beginning of a root structure.
	 *
	 * @param TypeInfo The object's type information.
	 */
	virtual void BeginStructure( UStruct* TypeInfo ) = 0;

	/**
	 * Signals the end of an array.
	 *
	 * @param Property The property that holds the array.
	 */
	virtual void EndArray( UProperty* Property ) = 0;

	/**
	 * Signals the end of an object.
	 */
	virtual void EndStructure() = 0;

	/**
	 * Writes a comment to the output stream.
	 *
	 * @param Comment The comment text.
	 */
	virtual void WriteComment( const FString& Comment ) = 0;

	/**
	 * Writes a property to the output stream.
	 *
	 * Depending on the context, properties to be written can be either object properties or array elements.
	 *
	 * @param Property The property that holds the data to write.
	 * @param Data The property's data to write.
	 * @param TypeInfo The property data's type information.
	 * @param ArrayIndex An index into the property array (for static arrays).
	 */
	virtual void WriteProperty( UProperty* Property, const void* Data, UStruct* TypeInfo, int32 ArrayIndex = 0 ) = 0;

public:

	/** Virtual destructor. */
	virtual ~IStructSerializerBackend() { }
};
