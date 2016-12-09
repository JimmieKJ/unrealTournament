#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Curves/KeyFrameManipulator.h"

/**
 * Proxy type used to reference a key's time and value
 */
template<typename KeyValueType, typename TimeType>
struct TKeyFrameProxy
{
	TKeyFrameProxy(TimeType InTime, KeyValueType& InValue)
		: Time(InTime)
		, Value(InValue)
	{}

	/** The key time */
	TimeType Time;

	/** (potentially const) Reference to the key's value */
	KeyValueType& Value;
};

/**
 * Templated interface to externally owned curve data data
 */
template<typename KeyValueType, typename TimeType>
class TCurveInterface : public TKeyFrameManipulator<TimeType>
{
public:
	// Pass by value/ref parameter types
	typedef typename TCallTraits<TimeType>::ParamType TimeTypeRef;
	typedef typename TCallTraits<KeyValueType>::ParamType KeyValueTypeRef;

	/**
	 * Construction from externally owned curve data
	 *
	 * @param KeyTimesParam			Ptr to array of key times
	 * @param KeyTimesParam			Ptr to array of key values
	 * @param ExternalKeyHandleLUT	Optional external look-up-table for key handles. If not supplied, an internal temporary LUT will be used.
	 */
	TCurveInterface(TArray<TimeType>* KeyTimesParam, TArray<KeyValueType>* KeyValuesParam, FKeyHandleLookupTable* ExternalKeyHandleLUT = nullptr)
		: TKeyFrameManipulator<TimeType>(KeyTimesParam, ExternalKeyHandleLUT)
		, KeyValues(KeyValuesParam)
	{
	}

	TCurveInterface(const TCurveInterface&) = default;
	TCurveInterface& operator=(const TCurveInterface&) = default;

#if PLATFORM_COMPILER_HAS_DEFAULTED_CONSTRUCTORS
	TCurveInterface(TCurveInterface&&) = default;
	TCurveInterface& operator=(TCurveInterface&&) = default;
#endif

	/**
	  * Add a new key to the curve with the supplied Time and Value. Returns the handle of the new key.
	  * 
	  * @param InTime				The time at which to add the key.
	  * @param InValue				The value of the key.
	  * @return A unique identifier for the newly added key
	  */
	FKeyHandle AddKeyValue(TimeTypeRef InTime, KeyValueTypeRef InValue)
	{
		int32 InsertIndex = this->ComputeInsertIndex(InTime);
		FKeyHandle NewHandle = this->InsertKeyImpl(InTime, InsertIndex);

		KeyValues->Insert(InValue, InsertIndex);
		return NewHandle;
	}

	/**
	 * Attempt to retrieve a key from its handle
	 *
	 * @param 						KeyHandle Handle to the key to get.
	 * @return A proxy to the key, or empty container if it was not found
	 */
	TOptional<TKeyFrameProxy<const KeyValueType, TimeType>> GetKey(FKeyHandle KeyHandle) const
	{
		int32 Index = this->GetIndex(KeyHandle);
		if (Index != INDEX_NONE)
		{
			return TKeyFrameProxy<const KeyValueType, const TimeType>(this->GetKeyTimeChecked(Index), (*KeyValues)[Index]);
		}
		return TOptional<TKeyFrameProxy<const KeyValueType, TimeType>>();
	}

	/**
	 * Attempt to retrieve a key from its handle
	 *
	 * @param 						KeyHandle Handle to the key to get.
	 * @return A proxy to the key, or empty container if it was not found
	 */
	TOptional<TKeyFrameProxy<KeyValueType, TimeType>> GetKey(FKeyHandle KeyHandle)
	{
		int32 Index = this->GetIndex(KeyHandle);
		if (Index != INDEX_NONE)
		{
			return TKeyFrameProxy<KeyValueType, TimeType>(this->GetKeyTimeChecked(Index), (*KeyValues)[Index]);
		}
		return TOptional<TKeyFrameProxy<KeyValueType, TimeType>>();
	}

	/**
	 * Update the key at the specified time with a new value, or add one if none is present
	 *
	 * @param InTime				The time at which to update/add the value
	 * @param Value					The new key value
	 * @param KeyTimeTolerance		Tolerance to use when looking for an existing key
	 * @return The key's handle
	 */
	FKeyHandle UpdateOrAddKey(TimeTypeRef InTime, KeyValueTypeRef Value, TimeTypeRef KeyTimeTolerance)
	{
		TOptional<FKeyHandle> Handle = this->FindKey(
			[=](TimeType ExistingTime)
			{
				return FMath::IsNearlyEqual(InTime, ExistingTime, KeyTimeTolerance);
			});
		
		if (!Handle.IsSet())
		{
			return AddKeyValue(InTime, Value);
		}
		else
		{
			TOptional<TKeyFrameProxy<KeyValueType, TimeType>> Key = GetKey(Handle.GetValue());

			if (ensure(Key.IsSet()))
			{
				Key->Value = Value;
			}

			return Handle.GetValue();
		}
	}

protected:

	virtual void OnKeyAdded(int32 Index) override
	{
		KeyValueType NewValue;
		KeyValues->Insert(MoveTemp(NewValue), Index);
	}

	virtual void OnKeyRelocated(int32 OldIndex, int32 NewIndex) override
	{
		KeyValueType Value = (*KeyValues)[OldIndex];

		KeyValues->RemoveAtSwap(OldIndex, 1, false);
		KeyValues->Insert(MoveTemp(Value), NewIndex);
	}

	virtual void OnKeyRemoved(int32 Index) override
	{
		KeyValues->RemoveAtSwap(Index, 1, false);
	}

private:

	/** Array of associated key data */
	TArray<KeyValueType>* KeyValues;
};
