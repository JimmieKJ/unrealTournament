// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

COREUOBJECT_API void CastLogError(const TCHAR* FromType, const TCHAR* ToType);

/**
 * Metafunction which detects whether or not a class is an IInterface.  Rules:
 *
 * 1. A UObject is not an IInterface.
 * 2. A type without a UClassType typedef member is not an IInterface.
 * 3. A type whose UClassType::StaticClassFlags does not have CLASS_Interface set is not an IInterface.
 *
 * Otherwise, assume it's an IInterface.
 */
template <typename T, bool bIsAUObject_IMPL = CanConvertPointerFromTo<T, UObject>::Result>
struct TIsIInterface
{
	enum { Value = false };
};

template <typename T>
struct TIsIInterface<T, false>
{
	template <typename U> static char (&Resolve(typename U::UClassType*))[(U::UClassType::StaticClassFlags & CLASS_Interface) ? 2 : 1];
	template <typename U> static char (&Resolve(...))[1];

	enum { Value = sizeof(Resolve<T>(0)) - 1 };
};

template <typename T>
struct TIsCastable
{
	// It's from-castable if it's an interface or a UObject-derived type
	enum { Value = TIsIInterface<T>::Value || CanConvertPointerFromTo<T, const volatile UObject>::Result };
};

template <typename T>
struct TIsCastableToPointer : TOr<TIsVoidType<T>, TIsCastable<T>>
{
	// It's to-pointer-castable if it's from-castable or void
};

template <typename T>
FORCEINLINE typename TEnableIf<TIsIInterface<T>::Value, FString>::Type GetTypeName()
{
	return T::UClassType::StaticClass()->GetName();
}

template <typename T>
FORCEINLINE typename TEnableIf<!TIsIInterface<T>::Value, FString>::Type GetTypeName()
{
	return T::StaticClass()->GetName();
}

enum class ECastType
{
	UObjectToUObject,
	InterfaceToUObject,
	UObjectToInterface,
	InterfaceToInterface,
	FromCastFlags
};

template <typename Type>
struct TCastFlags
{
	static const uint64 Value = CASTCLASS_None;
};

template <typename From, typename To, bool bFromInterface = TIsIInterface<From>::Value, bool bToInterface = TIsIInterface<To>::Value, uint64 CastClass = TCastFlags<To>::Value>
struct TGetCastType
{
	static const ECastType Value = ECastType::FromCastFlags;
};

template <typename From, typename To                  > struct TGetCastType<From, To, false, false, CASTCLASS_None> { static const ECastType Value = ECastType::UObjectToUObject;     };
template <typename From, typename To                  > struct TGetCastType<From, To, false, true , CASTCLASS_None> { static const ECastType Value = ECastType::UObjectToInterface;   };
template <typename From, typename To, uint64 CastClass> struct TGetCastType<From, To, true,  false, CastClass     > { static const ECastType Value = ECastType::InterfaceToUObject;   };
template <typename From, typename To, uint64 CastClass> struct TGetCastType<From, To, true,  true , CastClass     > { static const ECastType Value = ECastType::InterfaceToInterface; };

template <typename To, typename From>
To* Cast(From* Src);

template <typename From, typename To, ECastType CastType = TGetCastType<From, To>::Value>
struct TCastImpl
{
	// This is the cast flags implementation
	FORCEINLINE static To* DoCast( UObject* Src )
	{
		return Src->GetClass()->HasAnyCastFlag(TCastFlags<To>::Value) ? (To*)Src : nullptr;
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::UObjectToUObject>
{
	FORCEINLINE static To* DoCast( UObject* Src )
	{
		return Src->IsA<To>() ? (To*)Src : nullptr;
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::InterfaceToUObject>
{
	FORCEINLINE static To* DoCast( From* Src )
	{
		return Cast<To>(Src->_getUObject());
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::UObjectToInterface>
{
	FORCEINLINE static To* DoCast( UObject* Src )
	{
		return (To*)Src->GetInterfaceAddress(To::UClassType::StaticClass());
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::InterfaceToInterface>
{
	FORCEINLINE static To* DoCast( From* Src )
	{
		return Cast<To>(Src->_getUObject());
	}
};

// Dynamically cast an object type-safely.
template <typename To, typename From>
FORCEINLINE To* Cast(From* Src)
{
	return Src ? TCastImpl<From, To>::DoCast(Src) : nullptr;
}

template< class T >
FORCEINLINE T* ExactCast( UObject* Src )
{
	return Src && (Src->GetClass() == T::StaticClass()) ? (T*)Src : nullptr;
}

template <typename To, typename From>
To* CastChecked(From* Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked)
{
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		if (Src)
		{
			To* Result = Cast<To>(Src);
			if (!Result)
			{
				CastLogError(*Cast<UObject>(Src)->GetFullName(), *GetTypeName<To>());
			}

			return Result;
		}

		if (CheckType == ECastCheckedType::NullChecked)
		{
			CastLogError(TEXT("nullptr"), *GetTypeName<To>());
		}

		return nullptr;

	#else

		return (To*)Src;

	#endif
}


template <typename InterfaceType>
DEPRECATED(4.6, "InterfaceCast is deprecated, use Cast or dynamic_cast instead.")
FORCEINLINE InterfaceType* InterfaceCast(UObject* Src)
{
	return Cast<InterfaceType>(Src);
}

template <typename InterfaceType>
DEPRECATED(4.6, "InterfaceCast is deprecated, use Cast or dynamic_cast instead.")
FORCEINLINE InterfaceType* InterfaceCast(const UObject* Src)
{
	return Cast<InterfaceType>(const_cast<UObject*>(Src));
}

// auto weak versions
template< class T, class U > FORCEINLINE T* Cast       ( const TAutoWeakObjectPtr<U>& Src                                                                   ) { return Cast       <T>(Src.Get()); }
template< class T, class U > FORCEINLINE T* ExactCast  ( const TAutoWeakObjectPtr<U>& Src                                                                   ) { return ExactCast  <T>(Src.Get()); }
template< class T, class U > FORCEINLINE T* CastChecked( const TAutoWeakObjectPtr<U>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(Src.Get(), CheckType); }

// FSubobjectPtr versions
template< class T > FORCEINLINE T* Cast       ( const FSubobjectPtr& Src                                                                   ) { return Cast       <T>(Src.Get()); }
template< class T > FORCEINLINE T* ExactCast  ( const FSubobjectPtr& Src                                                                   ) { return ExactCast  <T>(Src.Get()); }
template< class T > FORCEINLINE T* CastChecked( const FSubobjectPtr& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(Src.Get(), CheckType); }

// TSubclassOf versions
template< class T, class U > FORCEINLINE T* Cast       ( const TSubclassOf<U>& Src                                                                   ) { return Cast       <T>(*Src); }
template< class T, class U > FORCEINLINE T* CastChecked( const TSubclassOf<U>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(*Src, CheckType); }

// Const versions of the casts
template< class T, class U > FORCEINLINE const T* Cast       ( const U      * Src                                                                   ) { return Cast       <T>(const_cast<U      *>(Src)); }
template< class T          > FORCEINLINE const T* ExactCast  ( const UObject* Src                                                                   ) { return ExactCast  <T>(const_cast<UObject*>(Src)); }
template< class T, class U > FORCEINLINE const T* CastChecked( const U      * Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(const_cast<U      *>(Src), CheckType); }

#define DECLARE_CAST_BY_FLAG_FWD(ClassName) class ClassName;
#define DECLARE_CAST_BY_FLAG_CAST(ClassName) \
	template <> \
	struct TCastFlags<ClassName> \
	{ \
		static const uint64 Value = CASTCLASS_##ClassName; \
	};

#define DECLARE_CAST_BY_FLAG(ClassName) \
	DECLARE_CAST_BY_FLAG_FWD(ClassName) \
	DECLARE_CAST_BY_FLAG_CAST(ClassName)

DECLARE_CAST_BY_FLAG(UField)
DECLARE_CAST_BY_FLAG(UEnum)
DECLARE_CAST_BY_FLAG(UStruct)
DECLARE_CAST_BY_FLAG(UScriptStruct)
DECLARE_CAST_BY_FLAG(UClass)
DECLARE_CAST_BY_FLAG(UProperty)
DECLARE_CAST_BY_FLAG(UObjectPropertyBase)
DECLARE_CAST_BY_FLAG(UObjectProperty)
DECLARE_CAST_BY_FLAG(UWeakObjectProperty)
DECLARE_CAST_BY_FLAG(ULazyObjectProperty)
DECLARE_CAST_BY_FLAG(UAssetObjectProperty)
DECLARE_CAST_BY_FLAG(UAssetClassProperty)
DECLARE_CAST_BY_FLAG(UBoolProperty)
DECLARE_CAST_BY_FLAG(UFunction)
DECLARE_CAST_BY_FLAG(UStructProperty)
DECLARE_CAST_BY_FLAG(UByteProperty)
DECLARE_CAST_BY_FLAG(UIntProperty)
DECLARE_CAST_BY_FLAG(UFloatProperty)
DECLARE_CAST_BY_FLAG(UDoubleProperty)
DECLARE_CAST_BY_FLAG(UClassProperty)
DECLARE_CAST_BY_FLAG(UInterfaceProperty)
DECLARE_CAST_BY_FLAG(UNameProperty)
DECLARE_CAST_BY_FLAG(UStrProperty)
DECLARE_CAST_BY_FLAG(UTextProperty)
DECLARE_CAST_BY_FLAG(UArrayProperty)
DECLARE_CAST_BY_FLAG(UDelegateProperty)
DECLARE_CAST_BY_FLAG(UMulticastDelegateProperty)

#undef DECLARE_CAST_BY_FLAG
#undef DECLARE_CAST_BY_FLAG_CASTCHECKED
#undef DECLARE_CAST_BY_FLAG_CAST
#undef DECLARE_CAST_BY_FLAG_FWD

namespace UE4Casts_Private
{
	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<TAnd<TIsPointerType<To>, TAnd<TIsCastableToPointer<typename TRemovePointer<To>::Type>, TIsCastable<From>>>::Value, To>::Type DynamicCast(From* Arg)
	{
		typedef typename TRemovePointer<To  >::Type ToValueType;
		typedef typename TRemovePointer<From>::Type FromValueType;

		// Casting away const/volatile
		static_assert(!TLosesQualifiersFromTo<FromValueType, ToValueType>::Value, "Conversion loses qualifiers");

		// If we're casting to void, cast to UObject instead and let it implicitly cast to void
		return Cast<typename TChooseClass<TIsVoidType<ToValueType>::Value, UObject, ToValueType>::Result>(Arg);
	}

	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<!TAnd<TIsPointerType<To>, TAnd<TIsCastableToPointer<typename TRemovePointer<To>::Type>, TIsCastable<From>>>::Value, To>::Type DynamicCast(From* Arg)
	{
		return dynamic_cast<To>(Arg);
	}

	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<TAnd<TIsCastable<typename TRemoveReference<To>::Type>, TIsCastable<typename TRemoveReference<From>::Type>>::Value, To>::Type DynamicCast(From&& Arg)
	{
		typedef typename TRemoveReference<From>::Type FromValueType;
		typedef typename TRemoveReference<To  >::Type ToValueType;

		// Casting away const/volatile
		static_assert(!TLosesQualifiersFromTo<FromValueType, ToValueType>::Value, "Conversion loses qualifiers");

		// T&& can only be cast to U&&
		// http://en.cppreference.com/w/cpp/language/dynamic_cast
		static_assert(TOr<TIsLValueReferenceType<From>, TIsRValueReferenceType<To>>::Value, "Cannot dynamic_cast from an rvalue to a non-rvalue reference");

		return Forward<To>(*CastChecked<typename TRemoveReference<To>::Type>(&Arg));
	}

	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<!TAnd<TIsCastable<typename TRemoveReference<To>::Type>, TIsCastable<typename TRemoveReference<From>::Type>>::Value, To>::Type DynamicCast(From&& Arg)
	{
		// This may fail when dynamic_casting rvalue references due to patchy compiler support
		return dynamic_cast<To>(Arg);
	}
}

#define dynamic_cast UE4Casts_Private::DynamicCast
