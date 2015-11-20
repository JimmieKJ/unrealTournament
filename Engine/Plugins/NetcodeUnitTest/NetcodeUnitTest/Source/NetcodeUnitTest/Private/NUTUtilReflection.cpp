// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "NUTUtilReflection.h"


/**
 * Wrapper macro for cast operator returns
 *
 * @return	The value the operator should return
 */
#define CAST_RETURN(ReturnVal) \
	NotifyCastReturn(); \
	return ReturnVal;


/**
 * FVMReflection
 */

FVMReflection::FVMReflection()
	: FieldAddress(NULL)
	, bVerifiedFieldType(false)
	, bSetArrayElement(false)
	, bNextActionMustBeCast(false)
	, NextActionError()
	, bOutError(NULL)
	, History()
	, OutHistoryPtr(NULL)
{
}

FVMReflection::FVMReflection(UObject* InBaseObject, EVMRefWarning InWarnLevel/*=EVMRefWarning::Warn*/)
	: FVMReflection()
{
	WarnLevel = InWarnLevel;

	if (InBaseObject != NULL)
	{
		BaseAddress = InBaseObject;
		FieldInstance = InBaseObject->GetClass();
		bIsError = false;
	}
	else
	{
		BaseAddress = NULL;
		FieldInstance = NULL;
		bIsError = true;
	}
}

FVMReflection::FVMReflection(FStructOnScope& InStruct, EVMRefWarning InWarnLevel/*=EVMRefWarning::Warn*/)
	: FVMReflection()
{
	WarnLevel = InWarnLevel;

	UScriptStruct* TargetStruct = (InStruct.IsValid() ? (UScriptStruct*)Cast<UScriptStruct>(InStruct.GetStruct()) : NULL);

	if (TargetStruct != NULL)
	{
		BaseAddress = InStruct.GetStructMemory();
		FieldInstance = TargetStruct;
		bIsError = false;
	}
	else
	{
		BaseAddress = NULL;
		FieldInstance = NULL;
		bIsError = true;
	}
}

FVMReflection::FVMReflection(const FVMReflection& ToCopy)
	: BaseAddress(ToCopy.BaseAddress)
	, FieldInstance(ToCopy.FieldInstance)
	, FieldAddress(ToCopy.FieldAddress)
	, bVerifiedFieldType(ToCopy.bVerifiedFieldType)
	, bSetArrayElement(ToCopy.bSetArrayElement)
	, bNextActionMustBeCast(ToCopy.bNextActionMustBeCast)
	, NextActionError(ToCopy.NextActionError)
	, bIsError(ToCopy.bIsError)
	, bOutError(NULL)
	, History()
	, OutHistoryPtr(NULL)
	, WarnLevel(EVMRefWarning::Warn)
{
}

FVMReflection& FVMReflection::operator = (const FVMReflection& ToCopy)
{
	UNIT_ASSERT(FString(TEXT("This should never be called.")) == TEXT(""));
	return *this;
}

FVMReflection& FVMReflection::operator ->*(const ANSICHAR* InPropertyName)
{
	FString PropertyName(ANSI_TO_TCHAR(InPropertyName));

	NotifyOperator();
	AddHistory(FString::Printf(TEXT("->*\"%s\""), *PropertyName));

	if (!bIsError && FieldInstance != NULL)
	{
		/**
		 * Property Context
		 *
		 * Before evaluating an actual property, first parse it from within the current reflection helper context;
		 * the context is most often the current object the reflection helper is pointed at
		 */

		// UClass
		if (FieldInstance->IsA(UClass::StaticClass()))
		{
			UClass* ClassInstance = Cast<UClass>(FieldInstance);
			UProperty* FoundProperty = FindField<UProperty>(ClassInstance, *PropertyName);

			if (FoundProperty != NULL)
			{
				FieldInstance = FoundProperty;
				SetFieldAddress(FoundProperty->ContainerPtrToValuePtr<void>(BaseAddress));
			}
			else
			{
				FString Error = FString::Printf(TEXT("Property '%s' not found in class '%s'"), *PropertyName,
										(ClassInstance != NULL ? *ClassInstance->GetFullName() : TEXT("NULL")));

				SetError(Error);
			}
		}
		// UStruct
		else if (FieldInstance->IsA(UStruct::StaticClass()))
		{
			if (!IsPropertyArray() || (bVerifiedFieldType && bSetArrayElement))
			{
				UStruct* InnerStruct = Cast<UStruct>(FieldInstance);
				UProperty* FoundProperty = (InnerStruct != NULL ? FindField<UProperty>(InnerStruct, *PropertyName) : NULL);

				if (FoundProperty != NULL)
				{
					FieldInstance = FoundProperty;
					SetFieldAddress(FoundProperty->ContainerPtrToValuePtr<void>(BaseAddress));
				}
				else
				{
					FString Error = FString::Printf(TEXT("Property '%s' not found in struct '%s'"), *PropertyName,
											(InnerStruct != NULL ? *InnerStruct->GetFullName() : TEXT("NULL")));

					SetError(Error);
				}
			}
			else if (!bVerifiedFieldType)
			{
				SetError(TEXT("Can't access struct array without verifying array type."));
			}
			else // if (!bSetArrayElement)
			{
				SetError(TEXT("Can't access struct array without selecting element."));
			}
		}


		/**
		 * Property
		 *
		 * Now that the context of the property has been setup, handle some special cases which change BaseAddress,
		 * i.e. change the object being pointed to
		 */

		// These types are supported without any extra effort from within this function
		// (though some, such as arrays, need extra support elsewhere)
		static const TArray<UClass*> SupportedTypes = TArrayBuilder<UClass*>()
														.Add(UClass::StaticClass())
														.Add(UByteProperty::StaticClass())
														.Add(UUInt16Property::StaticClass())
														.Add(UUInt32Property::StaticClass())
														.Add(UUInt64Property::StaticClass())
														.Add(UInt8Property::StaticClass())
														.Add(UInt16Property::StaticClass())
														.Add(UIntProperty::StaticClass())
														.Add(UInt64Property::StaticClass())
														.Add(UFloatProperty::StaticClass())
														.Add(UDoubleProperty::StaticClass())
														.Add(UBoolProperty::StaticClass())
														.Add(UNameProperty::StaticClass())
														.Add(UStrProperty::StaticClass())
														.Add(UTextProperty::StaticClass())
														.Add(UArrayProperty::StaticClass());

		auto IsSupportedType =
			[&](UClass* InClass)
			{
				return SupportedTypes.ContainsByPredicate(
					[&](UClass* CurEntry)
					{
						return InClass->IsChildOf(CurEntry);
					});
			};


		// UObjectProperty
		// This is a special circumstance, context changes to the object, but FieldAddress is kept pointing to the object property
		if (FieldInstance->IsA(UObjectProperty::StaticClass()))
		{
			ProcessObjectProperty();
		}
		// UStructProperty
		// Same principle as UObjectProperty, but changing context to a UStruct
		else if (FieldInstance->IsA(UStructProperty::StaticClass()))
		{
			ProcessStructProperty();
		}
		else if (!IsSupportedType(FieldInstance->GetClass()))
		{
			FString Error = FString::Printf(TEXT("Support for field type '%s' has not been implemented."),
									*FieldInstance->GetClass()->GetFullName());

			SetError(Error);
		}
	}
	else if (!bIsError && FieldInstance == NULL)
	{
		SetError(TEXT("FieldInstance is NULL"));
	}

	return *this;
}

FVMReflection& FVMReflection::operator [](int32 ArrayElement)
{
	NotifyOperator();
	AddHistory(FString::Printf(TEXT("[%i]"), ArrayElement));

	UProperty* FieldProp = Cast<UProperty>(FieldInstance);
	UArrayProperty* ArrayProp = Cast<UArrayProperty>(FieldInstance);

	if (!bIsError && FieldProp != NULL && FieldAddress != NULL && !bSetArrayElement)
	{
		// Static arrays
		if (FieldProp->ArrayDim > 1)
		{
			if (ArrayElement >= 0 && ArrayElement < FieldProp->ArrayDim)
			{
				SetFieldAddress(FieldProp->ContainerPtrToValuePtr<void>(BaseAddress, ArrayElement), true);
			}
			else
			{
				SetError(FString::Printf(TEXT("Tried to access array element '%i' of '%i'."), ArrayElement, FieldProp->ArrayDim));
			}
		}
		// Dynamic arrays
		else if (ArrayProp != NULL)
		{
			FScriptArrayHelper DynArray(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(BaseAddress));

			if (ArrayElement >= 0 && ArrayElement < DynArray.Num())
			{
				// Update the field type, to the arrays inner property (i.e. uint8 for a uint8 array)
				FieldInstance = ArrayProp->Inner;

				SetFieldAddress(DynArray.GetRawPtr(ArrayElement), true);
			}
			else
			{
				SetError(FString::Printf(TEXT("Tried to access array element '%i' of '%i'."), ArrayElement, FieldProp->ArrayDim));
			}
		}
		else
		{
			SetError(FString::Printf(TEXT("Property '%s' is not an array."), *FieldProp->GetName()));
		}


		// Handle context changes from UObjectProperty
		// This is a special circumstance, context changes to the object, but FieldAddress is kept pointing to the object property
		if (FieldInstance->IsA(UObjectProperty::StaticClass()))
		{
			ProcessObjectProperty();
		}
		// Handle context changes from UStructProperty (same principle as above)
		else if (FieldInstance->IsA(UStructProperty::StaticClass()))
		{
			ProcessStructProperty();
		}
	}
	else if (!bIsError)
	{
		if (FieldInstance == NULL)
		{
			SetError(TEXT("FieldInstance is NULL."));
		}
		else if (FieldProp == NULL)
		{
			SetError(FString::Printf(TEXT("Field '%s' is not a property."), *FieldInstance->GetName()));
		}
		else if (FieldAddress == NULL)
		{
			SetError(TEXT("FieldAddress is NULL (should already be pointing to base property address)."));
		}
		else // if (bSetArrayElement)
		{
			SetError(TEXT("Array element was already set."));
		}
	}

	return *this;
}

FVMReflection& FVMReflection::operator [](const ANSICHAR* InFieldType)
{
	FString ExpectedFieldType = ANSI_TO_TCHAR(InFieldType);

	NotifyOperator();
	AddHistory(FString::Printf(TEXT("[\"%s\"]"), *ExpectedFieldType));

	UProperty* FieldProp = Cast<UProperty>(FieldInstance);
	UArrayProperty* ArrayProp = Cast<UArrayProperty>(FieldProp);
	UStruct* StructField = Cast<UStruct>(FieldInstance);

	if (!bIsError && FieldInstance != NULL && FieldAddress != NULL && !bVerifiedFieldType)
	{
		UField* ActualFieldType = NULL;
		FString ActualFieldTypeStr = TEXT("");
		const TCHAR* CheckType = TEXT("");

		// Static arrays
		if (FieldProp != NULL && FieldProp->ArrayDim > 1)
		{
			ActualFieldType = FieldProp;
			CheckType = TEXT("array");
		}
		// Dynamic arrays
		else if (ArrayProp != NULL)
		{
			ActualFieldType = ArrayProp->Inner;
			CheckType = TEXT("array");
		}
		// Structs
		else if (StructField != NULL)
		{
			ActualFieldType = StructField;
			CheckType = TEXT("struct");
		}
		else
		{
			SetError(FString::Printf(TEXT("Property '%s' is not a %s."), *FieldInstance->GetName(), CheckType));
		}

		// Handle special case of struct-arrays
		UStructProperty* StructProp = Cast<UStructProperty>(ActualFieldType);

		if (StructProp != NULL)
		{
			ActualFieldType = StructProp->Struct;
			CheckType = TEXT("struct array");
		}


		if (ActualFieldType != NULL)
		{
			bool bTypeValid = false;

			ActualFieldTypeStr = ActualFieldType->GetClass()->GetName();

			#define FIELD_TYPE_CHECK_FIRST(TypeString, TypeProperty) \
				if (ExpectedFieldType == TypeString) \
				{ \
					if (ActualFieldType->IsA(TypeProperty##::StaticClass())) \
					{ \
						bTypeValid = true; \
					} \
				}

			#define FIELD_TYPE_CHECK(TypeString, TypeProperty)	else FIELD_TYPE_CHECK_FIRST(TypeString, TypeProperty)

			FIELD_TYPE_CHECK_FIRST(TEXT("bool"), UBoolProperty)
			FIELD_TYPE_CHECK(TEXT("FName"), UNameProperty)
			FIELD_TYPE_CHECK(TEXT("uint8"), UByteProperty)
			FIELD_TYPE_CHECK(TEXT("double"), UDoubleProperty)
			FIELD_TYPE_CHECK(TEXT("float"), UFloatProperty)
			FIELD_TYPE_CHECK(TEXT("int16"), UInt16Property)
			FIELD_TYPE_CHECK(TEXT("int64"), UInt64Property)
			FIELD_TYPE_CHECK(TEXT("int8"), UInt8Property)
			FIELD_TYPE_CHECK(TEXT("int32"), UIntProperty)
			FIELD_TYPE_CHECK(TEXT("uint16"), UUInt16Property)
			FIELD_TYPE_CHECK(TEXT("uint32"), UUInt32Property)
			FIELD_TYPE_CHECK(TEXT("uint64"), UUInt64Property)
			FIELD_TYPE_CHECK(TEXT("FString"), UStrProperty)
			FIELD_TYPE_CHECK(TEXT("FText"), UTextProperty)
			// UObject and subclasses
			else if (ExpectedFieldType.Len() > 2 &&
						(ExpectedFieldType.Left(1) == TEXT("U") || ExpectedFieldType.Left(1) == TEXT("A")) &&
						ExpectedFieldType.Right(1) == TEXT("*"))
			{
				UObjectProperty* ObjProp = Cast<UObjectProperty>(ActualFieldType);

				if (ObjProp != NULL)
				{
					FString ClassName = ExpectedFieldType.Mid(1, ExpectedFieldType.Len()-2);

					ActualFieldTypeStr = ObjProp->PropertyClass->GetName();

					if (ActualFieldTypeStr == ClassName)
					{
						bTypeValid = true;
					}
					else
					{
						SetError(FString::Printf(TEXT("Expected object %s of type '%s', but got %s of type '%s%s*'"),
									CheckType, *ExpectedFieldType, CheckType, *ActualFieldTypeStr, *ClassName));
					}
				}
			}
			// UStruct
			else if (ExpectedFieldType.Len() > 1 && ExpectedFieldType.Left(1) == TEXT("F"))
			{
				UScriptStruct* StructRef = Cast<UScriptStruct>(ActualFieldType);

				if (StructRef != NULL)
				{
					FString ClassName = ExpectedFieldType.Mid(1);

					ActualFieldTypeStr = StructRef->GetName();

					if (ActualFieldTypeStr == ClassName)
					{
						bTypeValid = true;
					}
					else
					{
						SetError(FString::Printf(TEXT("Expected %s of type '%s', but got %s of type 'F%s'"),
									CheckType, *ExpectedFieldType, CheckType, *ActualFieldTypeStr));
					}
				}
			}

			#undef FIELD_TYPE_CHECK
			#undef FIELD_TYPE_CHECK_FIRST


			if (bTypeValid)
			{
				bVerifiedFieldType = true;
			}
			else
			{
				SetError(FString::Printf(TEXT("Tried to verify %s as being of type '%s', but it is of type '%s' instead."),
							CheckType, *ExpectedFieldType, *ActualFieldTypeStr));
			}
		}
		else
		{
			SetError(FString::Printf(TEXT("Could not determine inner %s type."), CheckType));
		}
	}
	else if (!bIsError)
	{
		if (FieldInstance == NULL)
		{
			SetError(TEXT("FieldInstance is NULL."));
		}
		else if (FieldInstance == NULL)
		{
			SetError(FString::Printf(TEXT("Field '%s' is NULL."), *FieldInstance->GetName()));
		}
		else if (FieldAddress == NULL)
		{
			SetError(TEXT("FieldAddress is NULL (should already be pointing to base property address)."));
		}
		else //if (bVerifiedFieldType)
		{
			SetError(TEXT("Field type already verified."));
		}
	}

	return *this;
}

void FVMReflection::ProcessObjectProperty()
{
	if (!IsPropertyArray() || (bVerifiedFieldType && bSetArrayElement))
	{
		if (FieldInstance->IsA(UObjectProperty::StaticClass()))
		{
			UObject* PropValue = *(UObject**)FieldAddress;

			if (PropValue != NULL)
			{
				BaseAddress = PropValue;
				FieldInstance = PropValue->GetClass();
			}
			else
			{
				bNextActionMustBeCast = true;
				NextActionError = FString::Printf(TEXT("UObjectProperty '%s' was NULL."), *FieldInstance->GetFullName());
			}
		}
		else
		{
			FString Error = FString::Printf(
							TEXT("ProcessObjectProperty called with field '%s' of type '%s', instead of type 'UObjectProperty'"),
							*FieldInstance->GetName(), *FieldInstance->GetClass()->GetName());

			SetError(Error);
		}
	}
}

void FVMReflection::ProcessStructProperty()
{
	if (!IsPropertyArray() || (bVerifiedFieldType && bSetArrayElement))
	{
		UStructProperty* StructProp = Cast<UStructProperty>(FieldInstance);

		if (StructProp != NULL && FieldAddress != NULL)
		{
			BaseAddress = FieldAddress;
			FieldInstance = StructProp->Struct;
		}
		else if (FieldAddress == NULL)
		{
			SetError(TEXT("ProcessStructProperty called with FieldAddress == NULL"));
		}
		else
		{
			bNextActionMustBeCast = true;

			NextActionError = FString::Printf(
							TEXT("ProcessStructProperty called with field '%s' of type '%s', instead of type 'UStructProperty'"),
							*FieldInstance->GetName(), *FieldInstance->GetClass()->GetName());
		}
	}
}


template<typename InType, class InTypeClass>
InType* FVMReflection::GetWritableCast(const TCHAR* InTypeStr, bool bDoingUpCast/*=false*/)
{
	InType* ReturnVal = NULL;

	AddCastHistory(FString::Printf(TEXT("(%s*)"), InTypeStr));

	if (CanCastProperty())
	{
		if (FieldInstance->IsA(InTypeClass::StaticClass()))
		{
			ReturnVal = (InType*)FieldAddress;
		}
		else if (!bDoingUpCast)
		{
			FString Error = FString::Printf(TEXT("Tried to cast type '%s' to type '%s'."), *FieldInstance->GetClass()->GetName(),
						*InTypeClass::StaticClass()->GetName());

			SetCastError(Error);
		}
	}
	else
	{
		if (IsPropertyArray() && (!bVerifiedFieldType || !bSetArrayElement))
		{
			if (!bVerifiedFieldType)
			{
				SetCastError(TEXT("Can't cast array property, verification type not set."));
			}
			else // if (!bSetArrayElement)
			{
				SetCastError(TEXT("Can't cast array property, element not set."));
			}
		}
		else
		{
			SetCastError(TEXT("Can't cast property."));
		}
	}

	CAST_RETURN(ReturnVal);
}

template<typename InType, class InTypeClass>
InType FVMReflection::GetNumericTypeCast(const TCHAR* InTypeStr, const TArray<UClass*>& SupportedUpCasts)
{
	InType ReturnVal = 0;
	InType* ValuePtr = GetWritableCast<InType, InTypeClass>(InTypeStr, (SupportedUpCasts.Num() > 0));

	AddCastHistory(FString::Printf(TEXT("(%s)"), InTypeStr));

	if (ValuePtr != NULL)
	{
		ReturnVal = *ValuePtr;
	}
	// NOTE: Can't check SupportedUpCasts against FieldInstance->GetClass(), as FieldInstance may be a child of a support cast
	else if (CanCastProperty())
	{
		#define NUMERIC_UPCAST_FIRST(CastType, CastTypeClass) \
			if (FieldInstance->IsA(CastTypeClass::StaticClass())) \
			{ \
				if (SupportedUpCasts.Contains(CastTypeClass::StaticClass())) \
				{ \
					ReturnVal = (InType)(GetTypeCast<CastType>(InTypeStr)); \
				} \
				else \
				{ \
					FString Error = FString::Printf(TEXT("Type '%s' does not support upcasting to type '%s'."), \
										*FieldInstance->GetClass()->GetName(), *InTypeClass::StaticClass()->GetName()); \
					SetCastError(Error); \
				} \
			}

		#define NUMERIC_UPCAST(CastType, CastTypeClass) else NUMERIC_UPCAST_FIRST(CastType, CastTypeClass)

		{
			NUMERIC_UPCAST_FIRST(uint8, UByteProperty)
			NUMERIC_UPCAST(uint16, UUInt16Property)
			NUMERIC_UPCAST(uint32, UUInt32Property)
			NUMERIC_UPCAST(int8, UInt8Property)
			NUMERIC_UPCAST(int16, UInt16Property)
			NUMERIC_UPCAST(int32, UIntProperty)
			NUMERIC_UPCAST(float, UFloatProperty)
			else
			{
				FString Error = FString::Printf(TEXT("No upcast possible from type '%s' to type '%s'."),
									*FieldInstance->GetClass()->GetName(), *InTypeClass::StaticClass()->GetName());

				SetCastError(Error);
			}
		}


		#undef NUMERIC_UPCAST
		#undef NUMERIC_UPCAST_FIRST
	}
	else
	{
		if (IsPropertyArray() && (!bVerifiedFieldType || !bSetArrayElement))
		{
			if (!bVerifiedFieldType)
			{
				SetCastError(TEXT("Can't cast array property, verification type not set."));
			}
			else // if (!bSetArrayElement)
			{
				SetCastError(TEXT("Can't cast array property, element not set."));
			}
		}
		else
		{
			SetCastError(TEXT("Can't cast property."));
		}
	}

	CAST_RETURN(ReturnVal);
}

template<typename InType>
FORCEINLINE InType FVMReflection::GetTypeCast(const TCHAR* InTypeStr)
{
	InType ReturnVal = 0;
	InType* ValuePtr = (InType*)(*this);

	AddCastHistory(FString::Printf(TEXT("(%s)"), InTypeStr));

	if (ValuePtr != NULL)
	{
		ReturnVal = *ValuePtr;
	}
	else
	{
		SetCastError(TEXT("Failed to get writable cast result."));
	}

	CAST_RETURN(ReturnVal);
}


/**
 * Implements a pointer cast operator (can't figure out how to do this with just the template)
 *
 * @param InType		The type being cast to
 * @param InTypeClass	The UProperty class being cast to
 */
#define IMPLEMENT_GENERIC_POINTER_CAST(InType, InTypeClass) \
	FVMReflection::operator InType*() \
	{ \
		return GetWritableCast<InType, InTypeClass>(TEXT(#InType)); \
	}

#define IMPLEMENT_NUMERIC_POINTER_CAST(InType, InTypeClass) IMPLEMENT_GENERIC_POINTER_CAST(InType, InTypeClass)

/**
 * Implements a readonly cast operator, for a numeric type
 *
 * @param InType		The type being cast to
 * @param InTypeClass	The UProperty class being cast to (not used here, but keep for clarity/consistency)
 */
#define IMPLEMENT_NUMERIC_CAST_BASIC(InType, InTypeClass) \
	FVMReflection::operator InType() \
	{ \
		return GetTypeCast<InType>(TEXT(#InType)); \
	}

/**
 * Implements a readonly cast operator, for a numeric type, with support for additional upcast types
 *
 * @param InType				The type being cast to
 * @param InTypeClass			The UProperty class being cast to
 * @param InSupportedUpCasts	A TArrayBuilder listing additional supported cast types
 */
#define IMPLEMENT_NUMERIC_CAST(InType, InTypeClass, InSupportedUpCasts) \
	FVMReflection::operator InType() \
	{ \
		static const TArray<UClass*> InType##InTypeClass##UpCasts = InSupportedUpCasts; \
		return GetNumericTypeCast<InType, InTypeClass>(TEXT(#InType), InType##InTypeClass##UpCasts); \
	}

// Implement numeric pointer casts
IMPLEMENT_NUMERIC_POINTER_CAST(uint8, UByteProperty);
IMPLEMENT_NUMERIC_POINTER_CAST(uint16, UUInt16Property);
IMPLEMENT_NUMERIC_POINTER_CAST(uint32, UUInt32Property);
IMPLEMENT_NUMERIC_POINTER_CAST(uint64, UUInt64Property);
IMPLEMENT_NUMERIC_POINTER_CAST(int8, UInt8Property);
IMPLEMENT_NUMERIC_POINTER_CAST(int16, UInt16Property);
IMPLEMENT_NUMERIC_POINTER_CAST(int32, UIntProperty);
IMPLEMENT_NUMERIC_POINTER_CAST(int64, UInt64Property);
IMPLEMENT_NUMERIC_POINTER_CAST(float, UFloatProperty);
IMPLEMENT_NUMERIC_POINTER_CAST(double, UDoubleProperty);


// Implement readonly numeric casts
IMPLEMENT_NUMERIC_CAST_BASIC(uint8, UByteProperty);

IMPLEMENT_NUMERIC_CAST(uint16, UUInt16Property, TArrayBuilder<UClass*>()
													.Add(UByteProperty::StaticClass()));

IMPLEMENT_NUMERIC_CAST(uint32, UUInt32Property, TArrayBuilder<UClass*>()
													.Add(UByteProperty::StaticClass())
													.Add(UUInt16Property::StaticClass()));

IMPLEMENT_NUMERIC_CAST(uint64, UUInt64Property, TArrayBuilder<UClass*>()
													.Add(UByteProperty::StaticClass())
													.Add(UUInt16Property::StaticClass())
													.Add(UUInt32Property::StaticClass()));

IMPLEMENT_NUMERIC_CAST_BASIC(int8, UInt8Property);

IMPLEMENT_NUMERIC_CAST(int16, UInt16Property, TArrayBuilder<UClass*>()
													.Add(UInt8Property::StaticClass()));

IMPLEMENT_NUMERIC_CAST(int32, UIntProperty, TArrayBuilder<UClass*>()
													.Add(UInt8Property::StaticClass())
													.Add(UInt16Property::StaticClass()));

IMPLEMENT_NUMERIC_CAST(int64, UInt64Property, TArrayBuilder<UClass*>()
													.Add(UInt8Property::StaticClass())
													.Add(UInt16Property::StaticClass())
													.Add(UIntProperty::StaticClass()));

IMPLEMENT_NUMERIC_CAST_BASIC(float, UFloatProperty);

IMPLEMENT_NUMERIC_CAST(double, UDoubleProperty, TArrayBuilder<UClass*>()
													.Add(UFloatProperty::StaticClass()));


// Implement generic pointer casts
IMPLEMENT_GENERIC_POINTER_CAST(FName, UNameProperty);
IMPLEMENT_GENERIC_POINTER_CAST(FString, UStrProperty);
IMPLEMENT_GENERIC_POINTER_CAST(FText, UTextProperty);


FVMReflection::operator bool()
{
	bool bReturnVal = false;
	UBoolProperty* BoolProp = Cast<UBoolProperty>(FieldInstance);

	AddCastHistory(TEXT("(bool)"));

	if (CanCastProperty())
	{
		if (BoolProp != NULL)
		{
			bReturnVal = BoolProp->GetPropertyValue(FieldAddress);
		}
		else
		{
			FString Error = FString::Printf(TEXT("FieldInstance is of type '%s', not 'UBoolProperty'."),
								*FieldInstance->GetClass()->GetName());

			SetCastError(Error);
		}
	}
	else
	{
		SetCastError(TEXT("Can't cast property."));
	}

	CAST_RETURN(bReturnVal);
}

FVMReflection::operator FName()
{
	FName ReturnVal = NAME_None;
	FName* ValuePtr = (FName*)(*this);

	AddCastHistory(TEXT("(FName)"));

	if (ValuePtr != NULL)
	{
		ReturnVal = *ValuePtr;
	}
	else
	{
		SetCastError(TEXT("Failed to get writable cast result."));
	}

	CAST_RETURN(ReturnVal);
}

FVMReflection::operator FString()
{
	FString ReturnVal = TEXT("");
	FString* ValuePtr = (FString*)(*this);

	AddCastHistory(TEXT("(FString)"));

	if (ValuePtr != NULL)
	{
		ReturnVal = *ValuePtr;
	}
	else
	{
		SetCastError(TEXT("Failed to get writable cast result."));
	}

	CAST_RETURN(ReturnVal);
}

FVMReflection::operator FText()
{
	FText ReturnVal = FText::GetEmpty();
	FText* ValuePtr = (FText*)(*this);

	AddCastHistory(TEXT("(FText)"));

	if (ValuePtr != NULL)
	{
		ReturnVal = *ValuePtr;
	}
	else
	{
		SetCastError(TEXT("Failed to get writable cast result."));
	}

	CAST_RETURN(ReturnVal);
}

FVMReflection::operator UObject**()
{
	UObject** ReturnVal = NULL;

	AddCastHistory(TEXT("(UObject**)"));

	if (CanCastObject())
	{
		if (FieldAddress != NULL)
		{
			ReturnVal = (UObject**)FieldAddress;
		}
		else
		{
			SetCastError(TEXT("FieldAddress is NULL"));
		}
	}
	else
	{
		SetCastError(TEXT("Can't cast object."));
	}

	CAST_RETURN(ReturnVal);
}

FVMReflection::operator UObject*()
{
	UObject* ReturnVal = NULL;

	AddCastHistory(TEXT("(UObject*)"));

	if (CanCastObject())
	{
		ReturnVal = (UObject*)BaseAddress;
	}
	else
	{
		SetCastError(TEXT("Can't cast object"));
	}

	CAST_RETURN(ReturnVal);
}

FVMReflection::operator FScriptArray*()
{
	FScriptArray* ReturnVal = NULL;

	AddCastHistory(TEXT("(FScriptArray*)"));

	if (CanCastArray())
	{
		if (FieldAddress != NULL)
		{
			ReturnVal = (FScriptArray*)FieldAddress;
		}
		else
		{
			SetCastError(TEXT("FieldAddress is NULL"));
		}
	}
	else if (!bVerifiedFieldType)
	{
		SetCastError(TEXT("Can't cast to array, without specifying an array type for verification."));
	}
	else if (bSetArrayElement)
	{
		SetCastError(TEXT("Can't cast to array, after selecting an array element"));
	}
	else
	{
		SetCastError(TEXT("Can't cast array."));
	}

	CAST_RETURN(ReturnVal);
}

FVMReflection::operator void*()
{
	void* ReturnVal = NULL;

	if (CanCastStruct())
	{
		if (FieldAddress != NULL)
		{
			ReturnVal = FieldAddress;
		}
		else
		{
			SetError(TEXT("FieldAddress is NULL"));
		}
	}
	else if (!bVerifiedFieldType)
	{
		SetCastError(TEXT("Can't cast to struct, without specifying a struct type for verification."));
	}
	else
	{
		SetCastError(TEXT("Can't cast struct."));
	}

	CAST_RETURN(ReturnVal);
}


FVMReflection& FVMReflection::operator ,(bool* bErrorPointer)
{
	bOutError = bErrorPointer;

	if (bOutError != NULL)
	{
		*bOutError = bIsError;
	}

	return *this;
}

FVMReflection& FVMReflection::operator ,(FString* OutHistory)
{
	OutHistoryPtr = OutHistory;

	if (OutHistoryPtr != NULL)
	{
		*OutHistoryPtr = TEXT("");

		for (auto CurString : History)
		{
			*OutHistoryPtr += CurString;
		}
	}

	return *this;
}


void FVMReflection::DebugDump()
{
	UE_LOG(LogUnitTest, Log, TEXT("FVMReflection Dump:"));

	UE_LOG(LogUnitTest, Log, TEXT("     - BaseAddress: %s"), (BaseAddress != NULL ? TEXT("Valid") : TEXT("NULL")));

	UE_LOG(LogUnitTest, Log, TEXT("     - FieldInstance: %s"),
			(FieldInstance != NULL ? *FieldInstance->GetFullName() : TEXT("NULL")));
	UE_LOG(LogUnitTest, Log, TEXT("     - FieldAddress: %s"), (FieldAddress != NULL ? TEXT("Valid") : TEXT("NULL")));
	UE_LOG(LogUnitTest, Log, TEXT("     - bVerifiedFieldType: %i"), (int32)bVerifiedFieldType);
	UE_LOG(LogUnitTest, Log, TEXT("     - bSetArrayElement: %i"), (int32)bSetArrayElement);

	UE_LOG(LogUnitTest, Log, TEXT("     - bNextActionMustBeCast: %i"), (int32)!!bNextActionMustBeCast);
	UE_LOG(LogUnitTest, Log, TEXT("     - NextActionError: %s"), *NextActionError);

	UE_LOG(LogUnitTest, Log, TEXT("     - bIsError: %i"), (int32)!!bIsError);
	UE_LOG(LogUnitTest, Log, TEXT("     - bOutError: %s"), (bOutError != NULL ? TEXT("Valid") : TEXT("NULL")));

	FString FullHistory = TEXT("");

	for (auto CurEntry : History)
	{
		FullHistory += CurEntry;
	}

	UE_LOG(LogUnitTest, Log, TEXT("     - History: %s"), *FullHistory);
	UE_LOG(LogUnitTest, Log, TEXT("     - OutHistoryPtr: %s"), (OutHistoryPtr != NULL ? TEXT("Valid") : TEXT("NULL")));

	UE_LOG(LogUnitTest, Log, TEXT("     - WarnLevel: %i (%s)"),
			(int32)WarnLevel, (WarnLevel == EVMRefWarning::Warn ? TEXT("Warn") : TEXT("NoWarn")));


	bIsError = true;
}


void FVMReflection::SetFieldAddress(void* InFieldAddress, bool bSettingArrayElement/*=false*/)
{
	bool bWasAtArrayElement = bSetArrayElement && !bSettingArrayElement;

	FieldAddress = InFieldAddress;
	bSetArrayElement = bSettingArrayElement;

	// If we were at an array element, and are traversing past it now, make sure the array type was verified (a tiny bit hacky)
	if (bWasAtArrayElement && !bVerifiedFieldType)
	{
		SetError(TEXT("Array type was not specified for verification."));
	}

	// Whenever we set the FieldAddress for a non-array, reset array type verification status
	if (!bSettingArrayElement)
	{
		bVerifiedFieldType = false;
	}
}


void FVMReflection::SetError(FString InError, bool bCastError/*=false*/)
{
	if (!bIsError)
	{
		bIsError = true;

		if (bOutError != NULL)
		{
			*bOutError = bIsError;
		}

		if (InError.Len() > 0)
		{
			FString HistoryStr = FString::Printf(TEXT(" (ERROR: %s)"), *InError);

			if (bCastError)
			{
				// The cast should already be in history at this point, at position 0, so add the error to position 1
				History.Insert(HistoryStr, 1);
			}
			else
			{
				History.Add(HistoryStr);
			}
		}

		if (OutHistoryPtr != NULL)
		{
			*OutHistoryPtr = TEXT("");

			for (auto CurStr : History)
			{
				*OutHistoryPtr += CurStr;
			}
		}


		if (WarnLevel == EVMRefWarning::Warn)
		{
			FString CurHistory;

			for (auto CurString : History)
			{
				CurHistory += CurString;
			}

			UE_LOG(LogUnitTest, Log, TEXT("Reflection Error: History dump: %s"), *CurHistory);
		}
	}
}


/**
 * NUTUtilRefl
 */

FString NUTUtilRefl::FunctionParmsToString(UFunction* InFunction, void* Parms)
{
	FString ReturnVal = TEXT("");

	for (TFieldIterator<UProperty> It(InFunction); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm)) == CPF_Parm; ++It)
	{
		FString CurPropText;
			
		It->ExportTextItem(CurPropText, It->ContainerPtrToValuePtr<void>(Parms), NULL, NULL, PPF_None);

		if (ReturnVal.Len() > 0)
		{
			ReturnVal += TEXT(", ");
		}

		ReturnVal += It->GetName() + TEXT(" = ") + CurPropText;
	}

	return ReturnVal;
}

