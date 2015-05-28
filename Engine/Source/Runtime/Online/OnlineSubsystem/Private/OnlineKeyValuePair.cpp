// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPrivatePCH.h"
#include "OnlineKeyValuePair.h"
#include "Json.h"

/**
 * Copy constructor. Copies the other into this object
 *
 * @param Other the other structure to copy
 */
FVariantData::FVariantData(const FVariantData& Other) :
	Type(EOnlineKeyValuePairDataType::Empty)
{
	// Use common methods for doing deep copy or just do a simple shallow copy
	if (Other.Type == EOnlineKeyValuePairDataType::String)
	{
		SetValue(Other.Value.AsTCHAR);
	}
	else if (Other.Type == EOnlineKeyValuePairDataType::Blob)
	{
		SetValue(Other.Value.AsBlob.BlobSize, Other.Value.AsBlob.BlobData);
	}
	else
	{
		// Shallow copy is safe
		FMemory::Memcpy(this, &Other, sizeof(FVariantData));
	}
}

/**
 * Assignment operator. Copies the other into this object
 *
 * @param Other the other structure to copy
 */
FVariantData& FVariantData::operator=(const FVariantData& Other)
{
	if (this != &Other)
	{
		// Use common methods for doing deep copy or just do a simple shallow copy
		if (Other.Type == EOnlineKeyValuePairDataType::String)
		{
			SetValue(Other.Value.AsTCHAR);
		}
		else if (Other.Type == EOnlineKeyValuePairDataType::Blob)
		{
			SetValue(Other.Value.AsBlob.BlobSize, Other.Value.AsBlob.BlobData);
		}
		else
		{
			Empty();
			// Shallow copy is safe
			FMemory::Memcpy(this, &Other, sizeof(FVariantData));
		}
	}
	return *this;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(const TCHAR* InData)
{
	Empty();
	Type = EOnlineKeyValuePairDataType::String;
	if (InData != NULL)
	{	
		int32 StrLen = FCString::Strlen(InData);
		// Allocate a buffer for the string plus terminator
		Value.AsTCHAR = new TCHAR[StrLen + 1];
		if (StrLen > 0)
		{
			// Copy the data
			FCString::Strcpy(Value.AsTCHAR, StrLen + 1, InData);
		}
		else
		{
			Value.AsTCHAR[0] = TEXT('\0');
		}
	}
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(const FString& InData)
{
	SetValue(*InData);
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(int32 InData)
{
	Empty();
	Type = EOnlineKeyValuePairDataType::Int32;
	Value.AsInt = InData;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(bool InData)
{
	Empty();
	Type = EOnlineKeyValuePairDataType::Bool;
	Value.AsBool = InData;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(double InData)
{
	Empty();
	Type = EOnlineKeyValuePairDataType::Double;
	Value.AsDouble = InData;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(float InData)
{
	Empty();
	Type = EOnlineKeyValuePairDataType::Float;
	Value.AsFloat = InData;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(const TArray<uint8>& InData)
{
	SetValue((uint32)InData.Num(), (const uint8*)InData.GetData());
}

/**
 * Copies the data and sets the type
 *
 * @param Size the length of the buffer to copy
 * @param InData the new data to assign
 */
void FVariantData::SetValue(uint32 Size, const uint8* InData)
{
	Empty();
	Type = EOnlineKeyValuePairDataType::Blob;
	if (Size > 0)
	{
		// Deep copy the binary data
		Value.AsBlob.BlobSize = (int32)Size;
		Value.AsBlob.BlobData = new uint8[Size];
		FMemory::Memcpy(Value.AsBlob.BlobData, InData, Size);
	}
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FVariantData::SetValue(uint64 InData)
{
	Empty();
	Type = EOnlineKeyValuePairDataType::Int64;
	Value.AsInt64 = InData;
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(FString& OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::String && Value.AsTCHAR != NULL)
	{
		OutData = Value.AsTCHAR;
	}
	else
	{
		OutData = TEXT("");
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(int32& OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::Int32)
	{
		OutData = Value.AsInt;
	}
	else
	{
		OutData = 0;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(bool& OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::Bool)
	{
		OutData = Value.AsBool;
	}
	else
	{
		OutData = false;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(uint64& OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::Int64)
	{
		OutData = Value.AsInt64;
	}
	else
	{
		OutData = 0;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(float& OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::Float)
	{
		OutData = Value.AsFloat;
	}
	else
	{
		OutData = 0.f;
	}
}

/**
 * Copies the data after verifying the type
 * NOTE: Performs a deep copy so you are responsible for freeing the data
 *
 * @param OutSize out value that receives the size of the copied data
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(uint32& OutSize, uint8** OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::Blob)
	{
		OutSize = Value.AsBlob.BlobSize;
		// Need to perform a deep copy
		*OutData = new uint8[OutSize];
		FMemory::Memcpy(*OutData, Value.AsBlob.BlobData, OutSize);
	}
	else
	{
		OutSize = 0;
		*OutData = NULL;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(TArray<uint8>& OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::Blob)
	{
		// Presize the array so it only allocates what's needed
		OutData.Empty(Value.AsBlob.BlobSize);
		OutData.AddUninitialized(Value.AsBlob.BlobSize);
		// Copy into the array
		FMemory::Memcpy(OutData.GetData(), Value.AsBlob.BlobData, Value.AsBlob.BlobSize);
	}
	else
	{
		OutData.Empty();
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FVariantData::GetValue(double& OutData) const
{
	if (Type == EOnlineKeyValuePairDataType::Double)
	{
		OutData = Value.AsDouble;
	}
	else
	{
		OutData = 0.0;
	}
}

/**
 * Cleans up the existing data and sets the type to EOnlineKeyValuePairDataType::Empty
 */
void FVariantData::Empty()
{
	// Be sure to delete deep allocations
	switch (Type)
	{
	case EOnlineKeyValuePairDataType::String:
		delete [] Value.AsTCHAR;
		break;
	case EOnlineKeyValuePairDataType::Blob:
		delete [] Value.AsBlob.BlobData;
		break;
	}

	Type = EOnlineKeyValuePairDataType::Empty;
	FMemory::Memset(&Value, 0, sizeof(ValueUnion));
}

/**
 * Converts the data into a string representation
 */
FString FVariantData::ToString() const
{
	switch (Type)
	{
		case EOnlineKeyValuePairDataType::Bool:
		{
			bool BoolVal;
			GetValue(BoolVal);
			return FString::Printf(TEXT("%s"), BoolVal ? TEXT("true") : TEXT("false"));
		}
		case EOnlineKeyValuePairDataType::Float:
		{
			// Convert the float to a string
			float FloatVal;
			GetValue(FloatVal);
			return FString::Printf(TEXT("%f"), (double)FloatVal);
		}
		case EOnlineKeyValuePairDataType::Int32:
		{
			// Convert the int to a string
			int32 Val;
			GetValue(Val);
			return FString::Printf(TEXT("%d"), Val);
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			// Convert the int to a string
			uint64 Val;
			GetValue(Val);
			return FString::Printf(TEXT("%lld"),Val);
		}
		case EOnlineKeyValuePairDataType::Double:
		{
			// Convert the double to a string
			double Val;
			GetValue(Val);
			return FString::Printf(TEXT("%f"),Val);
		}
		case EOnlineKeyValuePairDataType::String:
		{
			// Copy the string out
			FString StringVal;
			GetValue(StringVal);
			return StringVal;
		}
		case EOnlineKeyValuePairDataType::Blob:
		{
			return FString::Printf(TEXT("%d byte blob"), Value.AsBlob.BlobSize);
		}
	}
	return TEXT("");
}

/**
 * Converts the string to the specified type of data for this setting
 *
 * @param NewValue the string value to convert
 */
bool FVariantData::FromString(const FString& NewValue)
{
	switch (Type)
	{
		case EOnlineKeyValuePairDataType::Float:
		{
			// Convert the string to a float
			float FloatVal = FCString::Atof(*NewValue);
			SetValue(FloatVal);
			return true;
		}
		case EOnlineKeyValuePairDataType::Int32:
		{
			// Convert the string to a int
			int32 IntVal = FCString::Atoi(*NewValue);
			SetValue(IntVal);
			return true;
		}
		case EOnlineKeyValuePairDataType::Double:
		{
			// Convert the string to a double
			double Val = FCString::Atod(*NewValue);
			SetValue(Val);
			return true;
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			uint64 Val = FCString::Strtoui64(*NewValue, NULL, 10);
			SetValue(Val);
			return true;
		}
		case EOnlineKeyValuePairDataType::String:
		{
			// Copy the string
			SetValue(NewValue);
			return true;
		}
		case EOnlineKeyValuePairDataType::Bool:
		{
			bool Val = NewValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? true : false;
			SetValue(Val);
			return true;
		}
		case EOnlineKeyValuePairDataType::Blob:
		case EOnlineKeyValuePairDataType::Empty:
			break;
	}
	return false;
}

TSharedRef<class FJsonObject> FVariantData::ToJson() const
{
	TSharedRef<FJsonObject> JsonObject(new FJsonObject());

	const TCHAR* TypeStr = TEXT("Type");
	const TCHAR* ValueStr = TEXT("Value");

	JsonObject->SetStringField(TypeStr, EOnlineKeyValuePairDataType::ToString(Type));
	
	switch (Type)
	{
		case EOnlineKeyValuePairDataType::Int32:
		{
			int32 FieldValue;
			GetValue(FieldValue);
			JsonObject->SetNumberField(ValueStr, (double)FieldValue);
			break;
		}
		case EOnlineKeyValuePairDataType::Float:
		{
			float FieldValue;
			GetValue(FieldValue);
			JsonObject->SetNumberField(ValueStr, (double)FieldValue);
			break;
		}
		case EOnlineKeyValuePairDataType::String:
		{
			FString FieldValue;
			GetValue(FieldValue);
			JsonObject->SetStringField(ValueStr, FieldValue);
			break;
		}
		case EOnlineKeyValuePairDataType::Bool:
		{
			bool FieldValue;
			GetValue(FieldValue);
			JsonObject->SetBoolField(ValueStr, FieldValue);
			break;
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			JsonObject->SetStringField(ValueStr, ToString());
			break;
		}
		case EOnlineKeyValuePairDataType::Double:
		{
			double FieldValue;
			GetValue(FieldValue);
			JsonObject->SetNumberField(ValueStr, (double)FieldValue);
			break;
		}
		case EOnlineKeyValuePairDataType::Empty:
		case EOnlineKeyValuePairDataType::Blob:
		default:
		{
			JsonObject->SetStringField(ValueStr, FString());
			break;
		}	
	};

	return JsonObject;
}

bool FVariantData::FromJson(const TSharedRef<FJsonObject>& JsonObject)
{
	bool bResult = false;

	const TCHAR* TypeStr = TEXT("Type");
	const TCHAR* ValueStr = TEXT("Value");

	FString VariantTypeStr;
	if (JsonObject->TryGetStringField(TypeStr, VariantTypeStr) &&
		!VariantTypeStr.IsEmpty())
	{
		if (VariantTypeStr.Equals(EOnlineKeyValuePairDataType::ToString(EOnlineKeyValuePairDataType::Int32)))
		{
			int32 FieldValue;
			if (JsonObject->TryGetNumberField(ValueStr, FieldValue))
			{
				SetValue(FieldValue);
				bResult = true;
			}
		}
		else if (VariantTypeStr.Equals(EOnlineKeyValuePairDataType::ToString(EOnlineKeyValuePairDataType::Float)))
		{
			double FieldValue;
			if (JsonObject->TryGetNumberField(ValueStr, FieldValue))
			{
				SetValue((float)FieldValue);
				bResult = true;
			}
		}
		else if (VariantTypeStr.Equals(EOnlineKeyValuePairDataType::ToString(EOnlineKeyValuePairDataType::String)))
		{
			FString FieldValue;
			if (JsonObject->TryGetStringField(ValueStr, FieldValue))
			{
				SetValue(FieldValue);
				bResult = true;
			}
		}
		else if (VariantTypeStr.Equals(EOnlineKeyValuePairDataType::ToString(EOnlineKeyValuePairDataType::Bool)))
		{
			bool FieldValue;
			if (JsonObject->TryGetBoolField(ValueStr, FieldValue))
			{
				SetValue(FieldValue);
				bResult = true;
			}
		}
		else if (VariantTypeStr.Equals(EOnlineKeyValuePairDataType::ToString(EOnlineKeyValuePairDataType::Int64)))
		{
			FString FieldValue;
			if (JsonObject->TryGetStringField(ValueStr, FieldValue))
			{
				Type = EOnlineKeyValuePairDataType::Int64;
				bResult = FromString(FieldValue);
			}
		}
		else if (VariantTypeStr.Equals(EOnlineKeyValuePairDataType::ToString(EOnlineKeyValuePairDataType::Double)))
		{
			double FieldValue;
			if (JsonObject->TryGetNumberField(ValueStr, FieldValue))
			{
				SetValue(FieldValue);
				bResult = true;
			}
		}
	}

	return bResult;
}

/**
 * Comparison of two settings data classes
 *
 * @param Other the other settings data to compare against
 *
 * @return true if they are equal, false otherwise
 */
bool FVariantData::operator==(const FVariantData& Other) const
{
	if (Type == Other.Type)
	{
		switch (Type)
		{
		case EOnlineKeyValuePairDataType::Float:
			{
				return Value.AsFloat == Other.Value.AsFloat;
			}
		case EOnlineKeyValuePairDataType::Int32:
			{
				return Value.AsInt == Other.Value.AsInt;
			}
		case EOnlineKeyValuePairDataType::Int64:
			{
				return Value.AsInt64 == Other.Value.AsInt64;
			}
		case EOnlineKeyValuePairDataType::Double:
			{
				return Value.AsDouble == Other.Value.AsDouble;
			}
		case EOnlineKeyValuePairDataType::String:
			{
				return FCString::Strcmp(Value.AsTCHAR, Other.Value.AsTCHAR) == 0;
			}
		case EOnlineKeyValuePairDataType::Blob:
			{
				return (Value.AsBlob.BlobSize == Other.Value.AsBlob.BlobSize) && 
						 (FMemory::Memcmp(Value.AsBlob.BlobData, Other.Value.AsBlob.BlobData, Value.AsBlob.BlobSize) == 0);
			}
		}
	}
	return false;
}
bool FVariantData::operator!=(const FVariantData& Other) const
{
	return !(operator==(Other));
}
