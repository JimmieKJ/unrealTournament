// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogInternationalization, Log, All);

/**
 * Represents all the types a LocMetadata Value can be.
 */
namespace ELocMetadataType
{
	enum Type
	{
		None,
		Boolean,
		String,
		Array,
		Object,
	};
}

class FLocMetadataObject;

/**
 * A Metadata Value is a structure that can be a number of types.
 */
class CORE_API FLocMetadataValue
{
public:

	/** Returns this value as a number, throwing an error if this is not a Metadata String */
	virtual FString AsString() {ErrorMessage(TEXT("String")); return FString();}

	/** Returns this value as a boolean, throwing an error if this is not an Metadata Bool */
	virtual bool AsBool() {ErrorMessage(TEXT("Boolean")); return false;}

	/** Returns this value as an array, throwing an error if this is not an Metadata Array */
	virtual TArray< TSharedPtr<FLocMetadataValue> > AsArray() {ErrorMessage(TEXT("Array")); return TArray< TSharedPtr<FLocMetadataValue> >();}

	/** Returns this value as an object, throwing an error if this is not an Metadata Object */
	virtual TSharedPtr<FLocMetadataObject> AsObject() {ErrorMessage(TEXT("Object")); return TSharedPtr<FLocMetadataObject>();}

	virtual TSharedRef<FLocMetadataValue> Clone() const = 0;

	bool operator==( const FLocMetadataValue& Other ) { return ( (Type == Other.Type) && EqualTo( Other ) ); }
	bool operator<( const FLocMetadataValue& Other ) { return (Type == Other.Type) ? LessThan( Other ) : (Type < Other.Type); }

	ELocMetadataType::Type Type;

protected:

	FLocMetadataValue() : Type(ELocMetadataType::None) {}
	virtual ~FLocMetadataValue() {}

	virtual bool EqualTo( const FLocMetadataValue& Other ) const = 0;
	virtual bool LessThan( const FLocMetadataValue& Other ) const = 0;

	virtual FString GetType() const = 0;

	void ErrorMessage(const FString& InType);

private:
	
	/** Copying is only supported using Clone() function */
	FLocMetadataValue( const FLocMetadataValue& ); 
	FLocMetadataValue& operator=( const FLocMetadataValue& );
};


/**
 * A LocMetadata Object is a structure holding an unordered set of name/value pairs.
 */
class CORE_API FLocMetadataObject
{
public:
	FLocMetadataObject()
		: Values()
	{
	}

	/** Copy ctor */
	FLocMetadataObject( const FLocMetadataObject& Other );
	
	template<ELocMetadataType::Type LocMetadataType>
	TSharedPtr<FLocMetadataValue> GetField( const FString& FieldName )
	{
		const TSharedPtr<FLocMetadataValue>* Field = Values.Find(FieldName);
		if ( ensureMsgf(Field && Field->IsValid(), TEXT("Field %s was not found."), *FieldName) )
		{
			if ( ensureMsgf( (*Field)->Type == LocMetadataType, TEXT("Field %s is of the wrong type."), *FieldName) )
			{
				return (*Field);
			}
		}

		return TSharedPtr<FLocMetadataValue>();
	}

	/** Checks to see if the FieldName exists in the object. */
	bool HasField( const FString& FieldName)
	{
		const TSharedPtr<FLocMetadataValue>* Field = Values.Find(FieldName);
		if(Field && Field->IsValid())
		{
			return true;
		}

		return false;
	}
	
	/** Checks to see if the FieldName exists in the object, and has the specified type. */
	template<ELocMetadataType::Type LocMetadataType>
	bool HasTypedField(const FString& FieldName)
	{
		const TSharedPtr<FLocMetadataValue>* Field = Values.Find(FieldName);
		if(Field && Field->IsValid() && ((*Field)->Type == LocMetadataType))
		{
			return true;
		}

		return false;
	}

	void SetField( const FString& FieldName, const TSharedPtr<FLocMetadataValue>& Value );

	void RemoveField(const FString& FieldName);

	/** Get the field named FieldName as a string. Ensures that the field is present and is of type LocMetadata string. */
	FString GetStringField(const FString& FieldName);

	/** Add a field named FieldName with value of StringValue */
	void SetStringField( const FString& FieldName, const FString& StringValue );

	/** Get the field named FieldName as a boolean. Ensures that the field is present and is of type LocMetadata boolean. */
	bool GetBoolField(const FString& FieldName);

	/** Set a boolean field named FieldName and value of InValue */
	void SetBoolField( const FString& FieldName, bool InValue );

	/** Get the field named FieldName as an array. Ensures that the field is present and is of type LocMetadata Array. */
	TArray< TSharedPtr<FLocMetadataValue> > GetArrayField(const FString& FieldName);

	/** Set an array field named FieldName and value of Array */
	void SetArrayField( const FString& FieldName, const TArray< TSharedPtr<FLocMetadataValue> >& Array );

	/** Get the field named FieldName as a LocMetadata object. Ensures that the field is present and is a LocMetadata object*/
	TSharedPtr<FLocMetadataObject> GetObjectField(const FString& FieldName);

	/** Set an ObjectField named FieldName and value of LocMetadataObject */
	void SetObjectField( const FString& FieldName, const TSharedPtr<FLocMetadataObject>& LocMetadataObject );

	FLocMetadataObject& operator=( const FLocMetadataObject& Other );

	bool operator==(const FLocMetadataObject& Other) const;
	bool operator<( const FLocMetadataObject& Other ) const;

	/** Similar functionality to == operator but ensures everything matches(ignores COMPARISON_MODIFIER_PREFIX). */
	bool IsExactMatch( const FLocMetadataObject& Other ) const;

	static bool IsMetadataExactMatch( TSharedPtr<FLocMetadataObject> MetadataA, TSharedPtr<FLocMetadataObject> MetadataB );

public:
	/** Stores the name/value pairs for the metadata object */
	TMap< FString, TSharedPtr<FLocMetadataValue> > Values;

	/** Special reserved character.  When encountered as a prefix metadata name(the key in the Values map), it will adjust the way comparisons are done. */
	static const FString COMPARISON_MODIFIER_PREFIX;
};


/** A LocMetadata String Value. */
class CORE_API FLocMetadataValueString : public FLocMetadataValue
{
public:
	FLocMetadataValueString(const FString& InString) : Value(InString) {Type = ELocMetadataType::String;}
	virtual FString AsString() override {return Value;}

	virtual TSharedRef<FLocMetadataValue> Clone() const override;
	void SetString( const FString& InString ) { Value = InString; }

protected:
	FString Value;

	virtual bool EqualTo( const FLocMetadataValue& Other ) const override;
	virtual bool LessThan( const FLocMetadataValue& Other ) const override;

	virtual FString GetType() const override {return TEXT("String");}
};


/** A LocMetadata Boolean Value. */
class CORE_API FLocMetadataValueBoolean : public FLocMetadataValue
{
public:
	FLocMetadataValueBoolean(bool InBool) : Value(InBool) {Type = ELocMetadataType::Boolean;}
	virtual bool AsBool() override {return Value;}

	virtual TSharedRef<FLocMetadataValue> Clone() const override;
	
protected:
	bool Value;

	virtual bool EqualTo( const FLocMetadataValue& Other ) const override;
	virtual bool LessThan( const FLocMetadataValue& Other ) const override;

	virtual FString GetType() const override {return TEXT("Boolean");}
};

/** A LocMetadata Array Value. */
class CORE_API FLocMetadataValueArray : public FLocMetadataValue
{
public:
	FLocMetadataValueArray(const TArray< TSharedPtr<FLocMetadataValue> >& InArray) : Value(InArray) {Type = ELocMetadataType::Array;}
	virtual TArray< TSharedPtr<FLocMetadataValue> > AsArray() override {return Value;}

	virtual TSharedRef<FLocMetadataValue> Clone() const override;
	
protected:
	TArray< TSharedPtr<FLocMetadataValue> > Value;

	virtual bool EqualTo( const FLocMetadataValue& Other ) const override;
	virtual bool LessThan( const FLocMetadataValue& Other ) const override;

	virtual FString GetType() const override {return TEXT("Array");}
};

/** A LocMetadata Object Value. */
class CORE_API FLocMetadataValueObject : public FLocMetadataValue
{
public:
	FLocMetadataValueObject(TSharedPtr<FLocMetadataObject> InObject) : Value(InObject) {Type = ELocMetadataType::Object;}
	virtual TSharedPtr<FLocMetadataObject> AsObject() override {return Value;}

	virtual TSharedRef<FLocMetadataValue> Clone() const override;
	
protected:
	TSharedPtr<FLocMetadataObject> Value;

	virtual bool EqualTo( const FLocMetadataValue& Other ) const override;
	virtual bool LessThan( const FLocMetadataValue& Other ) const override;

	virtual FString GetType() const override {return TEXT("Object");}

private:
	// Hidden default ctor
	FLocMetadataValueObject()
	{}
};
