// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	DelegateInstancesImpl.inl: Inline implementation of delegate bindings.

	This file in re-included in DelegateCombinations.h for EVERY supported delegate signature.
	Every combination of parameter count, return value presence or other function modifier will
	include this file to generate a delegate interface type and implementation type for that signature.

	The types declared in this file are for internal use only. 
================================================================================*/

#if !defined(__Delegate_h__) || !defined(FUNC_INCLUDING_INLINE_IMPL)
	#error "This inline header must only be included by Delegate.h"
#endif


/* Class names for supported delegate bindings.
 *****************************************************************************/

#define DELEGATE_INSTANCE_INTERFACE_CLASS FUNC_COMBINE(IBaseDelegateInstance_, FUNC_SUFFIX)
#define SP_METHOD_DELEGATE_INSTANCE_CLASS FUNC_COMBINE(TBaseSPMethodDelegateInstance_, FUNC_COMBINE(FUNC_PAYLOAD_SUFFIX, FUNC_CONST_SUFFIX))
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS FUNC_COMBINE(TBaseRawMethodDelegateInstance_, FUNC_COMBINE(FUNC_PAYLOAD_SUFFIX, FUNC_CONST_SUFFIX))
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS FUNC_COMBINE(TBaseUObjectMethodDelegateInstance_, FUNC_COMBINE(FUNC_PAYLOAD_SUFFIX, FUNC_CONST_SUFFIX))
#define STATIC_DELEGATE_INSTANCE_CLASS FUNC_COMBINE(TBaseStaticDelegateInstance_, FUNC_COMBINE(FUNC_PAYLOAD_SUFFIX, FUNC_CONST_SUFFIX))
#define FUNCTOR_DELEGATE_INSTANCE_CLASS FUNC_COMBINE(TBaseFunctorDelegateInstance_, FUNC_COMBINE(FUNC_PAYLOAD_SUFFIX, FUNC_CONST_SUFFIX))
#define UFUNCTION_DELEGATE_INSTANCE_CLASS FUNC_COMBINE(TBaseUFunctionDelegateInstance_, FUNC_COMBINE(FUNC_PAYLOAD_SUFFIX, FUNC_CONST_SUFFIX))

#if FUNC_IS_CONST
	#define DELEGATE_QUALIFIER const
#else
	#define DELEGATE_QUALIFIER
#endif


/* Macros for function parameter and delegate payload lists
 *****************************************************************************/

#if FUNC_HAS_PAYLOAD
	#define DELEGATE_COMMA_PAYLOAD_LIST							, FUNC_PAYLOAD_LIST
	#define DELEGATE_COMMA_PAYLOAD_PASSTHRU						, FUNC_PAYLOAD_PASSTHRU
	#define DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST				, FUNC_PAYLOAD_INITIALIZER_LIST
	#define DELEGATE_COMMA_PAYLOAD_PASSIN						, FUNC_PAYLOAD_PASSIN
	#if FUNC_HAS_PARAMS
		#define DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN	FUNC_PARAM_PASSTHRU, FUNC_PAYLOAD_PASSIN
		#define DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST			FUNC_PARAM_LIST, FUNC_PAYLOAD_LIST
		#define DELEGATE_PARMS_COLON_INITIALIZER_LIST			: FUNC_PARAM_INITIALIZER_LIST, FUNC_PAYLOAD_INITIALIZER_LIST
	#else
		#define DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN	FUNC_PAYLOAD_PASSIN
		#define DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST			FUNC_PAYLOAD_LIST
		#define DELEGATE_PARMS_COLON_INITIALIZER_LIST			: FUNC_PAYLOAD_INITIALIZER_LIST
	#endif
#else
	#define DELEGATE_COMMA_PAYLOAD_LIST
	#define DELEGATE_COMMA_PAYLOAD_PASSTHRU
	#define DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
	#define DELEGATE_COMMA_PAYLOAD_PASSIN
	#define DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN		FUNC_PARAM_PASSTHRU
	#define DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST				FUNC_PARAM_LIST
	#if FUNC_HAS_PARAMS
		#define DELEGATE_PARMS_COLON_INITIALIZER_LIST			: FUNC_PARAM_INITIALIZER_LIST
	#else
		#define DELEGATE_PARMS_COLON_INITIALIZER_LIST
	#endif
#endif


/* Delegate binding types
 *****************************************************************************/

template<class UserClass, FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME> class UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS;
template<class UserClass, FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME> class RAW_METHOD_DELEGATE_INSTANCE_CLASS;


/**
 * Implements a delegate binding for shared pointer member functions.
 */
template<class UserClass, FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME, ESPMode SPMode>
class SP_METHOD_DELEGATE_INSTANCE_CLASS
	: public DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>
{
public:

	/** Type definition for member function pointers. */
	typedef RetValType (UserClass::*FMethodPtr)( DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST ) DELEGATE_QUALIFIER;

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InUserObject A shared reference to an arbitrary object (templated) that hosts the member function.
	 * @param InMethodPtr C++ member function pointer for the method to bind.
	 */
	SP_METHOD_DELEGATE_INSTANCE_CLASS( const TSharedPtr<UserClass, SPMode>& InUserObject, FMethodPtr InMethodPtr DELEGATE_COMMA_PAYLOAD_LIST )
		: UserObject(InUserObject)
		, MethodPtr(InMethodPtr)
		DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
		, Handle(FDelegateHandle::GenerateNewHandle)
	{
		// NOTE: Shared pointer delegates are allowed to have a null incoming object pointer.  Weak pointers can expire,
		//       an it is possible for a copy of a delegate instance to end up with a null pointer.
		checkSlow(MethodPtr != nullptr);
	}

public:

	// IDelegateInstance interface

	virtual FName GetFunctionName( ) const override
	{
		return NAME_None;
	}

	virtual const void* GetRawMethodPtr( ) const override
	{
		return GetRawMethodPtrInternal();
	}

	virtual const void* GetRawUserObject( ) const override
	{
		return GetRawUserObjectInternal();
	}

	virtual EDelegateInstanceType::Type GetType( ) const override
	{
		return SPMode == ESPMode::ThreadSafe ? EDelegateInstanceType::ThreadSafeSharedPointerMethod : EDelegateInstanceType::SharedPointerMethod;
	}

	virtual bool HasSameObject( const void* InUserObject ) const override
	{
		return UserObject.HasSameObject(InUserObject);
	}

	virtual bool IsSafeToExecute( ) const override
	{
		return UserObject.IsValid();
	}

public:

	// DELEGATE_INSTANCE_INTERFACE_CLASS interface

	virtual DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* CreateCopy() override
	{
		return new SP_METHOD_DELEGATE_INSTANCE_CLASS(*this);
	}

	virtual RetValType Execute( FUNC_PARAM_LIST ) const override
	{
		// Verify that the user object is still valid.  We only have a weak reference to it.
		TSharedPtr<UserClass, SPMode> SharedUserObject( UserObject.Pin());
		checkSlow(SharedUserObject.IsValid());

		// Safely remove const to work around a compiler issue with instantiating template permutations for 
		// overloaded functions that take a function pointer typedef as a member of a templated class.  In
		// all cases where this code is actually invoked, the UserClass will already be a const pointer.
		typedef typename TRemoveConst<UserClass>::Type MutableUserClass;
		MutableUserClass* MutableUserObject = const_cast<MutableUserClass*>(SharedUserObject.Get());

		// Call the member function on the user's object.  And yes, this is the correct C++ syntax for calling a
		// pointer-to-member function.
		checkSlow(MethodPtr != nullptr);

		return (MutableUserObject->*MethodPtr)(DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN);
	}

	virtual FDelegateHandle GetHandle() const override
	{
		return Handle;
	}

#if FUNC_IS_VOID
	virtual bool ExecuteIfSafe( FUNC_PARAM_LIST ) const override
	{
		// Verify that the user object is still valid.  We only have a weak reference to it.
		TSharedPtr<UserClass, SPMode> SharedUserObject(UserObject.Pin());
		if (SharedUserObject.IsValid())
		{
			Execute(FUNC_PARAM_PASSTHRU);

			return true;
		}

		return false;
	}
#endif

	virtual bool IsSameFunction( const DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>& InOtherDelegate ) const override
	{
		// NOTE: Payload data is not currently considered when comparing delegate instances.
		// See the comment in multi-cast delegate's Remove() method for more information.
		if ((InOtherDelegate.GetType() == EDelegateInstanceType::SharedPointerMethod) ||
			(InOtherDelegate.GetType() == EDelegateInstanceType::ThreadSafeSharedPointerMethod) ||
			(InOtherDelegate.GetType() == EDelegateInstanceType::RawMethod))
		{
			return (GetRawMethodPtrInternal() == InOtherDelegate.GetRawMethodPtr() && UserObject.HasSameObject(InOtherDelegate.GetRawUserObject()));
		}

		return false;
	}

public:

	/**
	 * Creates a new shared pointer delegate binding for the given user object and method pointer.
	 *
	 * @param InUserObjectRef Shared reference to the user's object that contains the class method.
	 * @param InFunc Member function pointer to your class method.
	 * @return The new delegate.
	 */
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create( const TSharedPtr<UserClass, SPMode>& InUserObjectRef, FMethodPtr InFunc DELEGATE_COMMA_PAYLOAD_LIST )
	{
		return new SP_METHOD_DELEGATE_INSTANCE_CLASS(InUserObjectRef, InFunc DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}

	/**
	 * Creates a new shared pointer delegate binding for the given user object and method pointer.
	 *
	 * This overload requires that the supplied object derives from TSharedFromThis.
	 *
	 * @param InUserObject  The user's object that contains the class method.  Must derive from TSharedFromThis.
	 * @param InFunc  Member function pointer to your class method.
	 * @return The new delegate.
	 */
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create( UserClass* InUserObject, FMethodPtr InFunc DELEGATE_COMMA_PAYLOAD_LIST )
	{
		// We expect the incoming InUserObject to derived from TSharedFromThis.
		TSharedRef<UserClass> UserObjectRef(StaticCastSharedRef<UserClass>(InUserObject->AsShared()));
		return Create(UserObjectRef, InFunc DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}

protected:

	/**
	 * Internal, inlined and non-virtual version of GetRawUserObject interface.
	 */
	FORCEINLINE const void* GetRawUserObjectInternal( ) const
	{
		return UserObject.Pin().Get();
	}

	/**
	 * Internal, inlined and non-virtual version of GetRawMethod interface.
	 */
	FORCEINLINE const void* GetRawMethodPtrInternal( ) const
	{
		return *(const void**)&MethodPtr;
	}

private:

	// Declare ourselves as a friend so we can access other template permutations in IsSameFunction().
	template<class UserClassNoShadow, FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW, ESPMode SPModeNoShadow> friend class SP_METHOD_DELEGATE_INSTANCE_CLASS;

	// Declare other pointer-based delegates as a friend so IsSameFunction() can compare members.
	template<class UserClassNoShadow, FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW> friend class UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS;
	template<class UserClassNoShadow, FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW> friend class RAW_METHOD_DELEGATE_INSTANCE_CLASS;

	// Weak reference to an instance of the user's class which contains a method we would like to call.
	TWeakPtr<UserClass, SPMode> UserObject;

	// C++ member function pointer.
	FMethodPtr MethodPtr;

	// Payload member variables, if any.
	FUNC_PAYLOAD_MEMBERS

	// The handle of this delegate
	FDelegateHandle Handle;
};


/**
 * Implements a delegate binding for C++ member functions.
 */
template<class UserClass, FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME>
class RAW_METHOD_DELEGATE_INSTANCE_CLASS
	: public DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>
{
	static_assert(!TPointerIsConvertibleFromTo<UserClass, const UObjectBase>::Value, "You cannot use raw method delegates with UObjects.");

public:

	/** Pointer-to-member typedef */
	typedef RetValType ( UserClass::*FMethodPtr )( DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST ) DELEGATE_QUALIFIER;

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InUserObject An arbitrary object (templated) that hosts the member function.
	 * @param InMethodPtr C++ member function pointer for the method to bind.
	 */
	RAW_METHOD_DELEGATE_INSTANCE_CLASS( UserClass* InUserObject, FMethodPtr InMethodPtr DELEGATE_COMMA_PAYLOAD_LIST )
		: UserObject(InUserObject)
		, MethodPtr(InMethodPtr)
		DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
		, Handle(FDelegateHandle::GenerateNewHandle)
	{
		// Non-expirable delegates must always have a non-null object pointer on creation (otherwise they could never execute.)
		check(InUserObject != nullptr && MethodPtr != nullptr);
	}

public:

	// IDelegateInstance interface

	virtual FName GetFunctionName( ) const override
	{
		return NAME_None;
	}

	virtual const void* GetRawMethodPtr( ) const override
	{
		return GetRawMethodPtrInternal();
	}

	virtual const void* GetRawUserObject( ) const override
	{
		return GetRawUserObjectInternal();
	}

	virtual EDelegateInstanceType::Type GetType( ) const override
	{
		return EDelegateInstanceType::RawMethod;
	}

	virtual bool HasSameObject( const void* InUserObject ) const override
	{
		return UserObject == InUserObject;
	}

	virtual bool IsSafeToExecute( ) const override
	{
		// We never know whether or not it is safe to deference a C++ pointer, but we have to
		// trust the user in this case.  Prefer using a shared-pointer based delegate type instead!
		return true;
	}

public:

	// DELEGATE_INSTANCE_INTERFACE_CLASS interface

	virtual DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* CreateCopy( ) override
	{
		return new RAW_METHOD_DELEGATE_INSTANCE_CLASS(*this);
	}

	virtual RetValType Execute( FUNC_PARAM_LIST ) const override
	{
		// Safely remove const to work around a compiler issue with instantiating template permutations for 
		// overloaded functions that take a function pointer typedef as a member of a templated class.  In
		// all cases where this code is actually invoked, the UserClass will already be a const pointer.
		typedef typename TRemoveConst<UserClass>::Type MutableUserClass;
		MutableUserClass* MutableUserObject = const_cast<MutableUserClass*>(UserObject);

		// Call the member function on the user's object.  And yes, this is the correct C++ syntax for calling a
		// pointer-to-member function.
		checkSlow(MethodPtr != nullptr);

		return (MutableUserObject->*MethodPtr)(DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN);
	}

	virtual FDelegateHandle GetHandle() const override
	{
		return Handle;
	}

#if FUNC_IS_VOID
	virtual bool ExecuteIfSafe( FUNC_PARAM_LIST ) const override
	{
		// We never know whether or not it is safe to deference a C++ pointer, but we have to
		// trust the user in this case.  Prefer using a shared-pointer based delegate type instead!
		Execute(FUNC_PARAM_PASSTHRU);

		return true;
	}
#endif

	virtual bool IsSameFunction( const DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>& InOtherDelegate ) const override
	{
		// NOTE: Payload data is not currently considered when comparing delegate instances.
		// See the comment in multi-cast delegate's Remove() method for more information.
		if ((InOtherDelegate.GetType() == EDelegateInstanceType::RawMethod) || 
			(InOtherDelegate.GetType() == EDelegateInstanceType::UObjectMethod) ||
			(InOtherDelegate.GetType() == EDelegateInstanceType::SharedPointerMethod) ||
			(InOtherDelegate.GetType() == EDelegateInstanceType::ThreadSafeSharedPointerMethod))
		{
			return (GetRawMethodPtrInternal() == InOtherDelegate.GetRawMethodPtr()) && (UserObject == InOtherDelegate.GetRawUserObject());
		}

		return false;
	}

public:

	/**
	 * Creates a new raw method delegate binding for the given user object and function pointer.
	 *
	 * @param InUserObject User's object that contains the class method.
	 * @param InFunc Member function pointer to your class method.
	 * @return The new delegate.
	 */
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create( UserClass* InUserObject, FMethodPtr InFunc DELEGATE_COMMA_PAYLOAD_LIST )
	{
		return new RAW_METHOD_DELEGATE_INSTANCE_CLASS(InUserObject, InFunc DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}

protected:

	/**
	 * Internal, inlined and non-virtual version of GetRawUserObject interface.
	 */
	FORCEINLINE const void* GetRawUserObjectInternal( ) const
	{
		return UserObject;
	}

	/**
	 * Internal, inlined and non-virtual version of GetRawMethodPtr interface.
	 */
	FORCEINLINE const void* GetRawMethodPtrInternal( ) const
	{
		return *(const void**)&MethodPtr;
	}

private:

	// Declare other pointer-based delegates as a friend so IsSameFunction() can compare members
	template<class UserClassNoShadow, FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW, ESPMode SPModeNoShadow> friend class SP_METHOD_DELEGATE_INSTANCE_CLASS;
	template<class UserClassNoShadow, FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW> friend class UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS;

	// Pointer to the user's class which contains a method we would like to call.
	UserClass* UserObject;

	// C++ member function pointer.
	FMethodPtr MethodPtr;

	// Payload member variables (if any).
	FUNC_PAYLOAD_MEMBERS

	// The handle of this delegate
	FDelegateHandle Handle;
};


/**
 * Implements a delegate binding for UObject methods.
 */
template<class UserClass, FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME>
class UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS
	: public DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>
{
	static_assert(TPointerIsConvertibleFromTo<UserClass, const UObjectBase>::Value, "You cannot use UObject method delegates with raw pointers.");

public:

	// Type definition for member function pointers. */
	typedef RetValType (UserClass::*FMethodPtr)( DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST ) DELEGATE_QUALIFIER;

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InUserObject An arbitrary object (templated) that hosts the member function.
	 * @param InMethodPtr C++ member function pointer for the method to bind.
	 */
	UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS( UserClass* InUserObject, FMethodPtr InMethodPtr DELEGATE_COMMA_PAYLOAD_LIST )
		: UserObject(InUserObject)
		, MethodPtr(InMethodPtr)
		DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
		, Handle(FDelegateHandle::GenerateNewHandle)
	{
		// NOTE: UObject delegates are allowed to have a null incoming object pointer.  UObject weak pointers can expire,
		//       an it is possible for a copy of a delegate instance to end up with a null pointer.
		checkSlow(MethodPtr != nullptr);
	}

public:

	// IDelegateInstance interface

	virtual FName GetFunctionName( ) const override
	{
		return NAME_None;
	}

	virtual const void* GetRawMethodPtr( ) const override
	{
		return GetRawMethodPtrInternal();
	}

	virtual const void* GetRawUserObject( ) const override
	{
		return GetRawUserObjectInternal();
	}

	virtual EDelegateInstanceType::Type GetType( ) const override
	{
		return EDelegateInstanceType::UObjectMethod;
	}

	virtual bool HasSameObject( const void* InUserObject ) const override
	{
		return (UserObject.Get() == InUserObject);
	}

	virtual bool IsCompactable( ) const override
	{
		return !UserObject.Get(true);
	}

	virtual bool IsSafeToExecute( ) const override
	{
		return !!UserObject.Get();
	}

public:

	// DELEGATE_INSTANCE_INTERFACE_CLASS interface

	virtual DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* CreateCopy( ) override
	{
		return new UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS(*this);
	}

	virtual RetValType Execute( FUNC_PARAM_LIST ) const override
	{
		// Verify that the user object is still valid.  We only have a weak reference to it.
		checkSlow(UserObject.IsValid());

		// Safely remove const to work around a compiler issue with instantiating template permutations for 
		// overloaded functions that take a function pointer typedef as a member of a templated class.  In
		// all cases where this code is actually invoked, the UserClass will already be a const pointer.
		typedef typename TRemoveConst<UserClass>::Type MutableUserClass;
		MutableUserClass* MutableUserObject = const_cast<MutableUserClass*>(UserObject.Get());

		// Call the member function on the user's object.  And yes, this is the correct C++ syntax for calling a
		// pointer-to-member function.
		checkSlow(MethodPtr != nullptr);

		return (MutableUserObject->*MethodPtr)(DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN);
	}

	virtual FDelegateHandle GetHandle() const override
	{
		return Handle;
	}

#if FUNC_IS_VOID
	virtual bool ExecuteIfSafe( FUNC_PARAM_LIST ) const override
	{
		// Verify that the user object is still valid.  We only have a weak reference to it.
		UserClass* ActualUserObject = UserObject.Get();
		if (ActualUserObject != nullptr)
		{
			Execute(FUNC_PARAM_PASSTHRU);

			return true;
		}
		return false;
	}
#endif

	virtual bool IsSameFunction( const DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>& InOtherDelegate ) const override
	{
		// NOTE: Payload data is not currently considered when comparing delegate instances.
		// See the comment in multi-cast delegate's Remove() method for more information.
		if ((InOtherDelegate.GetType() == EDelegateInstanceType::UObjectMethod) || 
			(InOtherDelegate.GetType() == EDelegateInstanceType::RawMethod))
		{
			return (GetRawMethodPtrInternal() == InOtherDelegate.GetRawMethodPtr()) && (UserObject.Get() == InOtherDelegate.GetRawUserObject());
		}

		return false;
	}

public:

	/**
	 * Creates a new UObject delegate binding for the given user object and method pointer.
	 *
	 * @param InUserObject User's object that contains the class method.
	 * @param InFunc Member function pointer to your class method.
	 * @return The new delegate.
	 */
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create( UserClass* InUserObject, FMethodPtr InFunc DELEGATE_COMMA_PAYLOAD_LIST )
	{
		return new UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS(InUserObject, InFunc DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}

protected:

	/**
	 * Internal, inlined and non-virtual version of GetRawUserObject interface.
	 */
	FORCEINLINE const void* GetRawUserObjectInternal( ) const
	{
		return UserObject.Get();
	}

	/**
	 * Internal, inlined and non-virtual version of GetRawMethodPtr interface.
	 */
	FORCEINLINE const void* GetRawMethodPtrInternal( ) const
	{
		return *(const void**)&MethodPtr;
	}

private:

	// Declare other pointer-based delegates as a friend so IsSameFunction() can compare members
	template<class UserClassNoShadow, FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW,ESPMode SPModeNoShadow> friend class SP_METHOD_DELEGATE_INSTANCE_CLASS;
	template<class UserClassNoShadow, FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW> friend class RAW_METHOD_DELEGATE_INSTANCE_CLASS;

	// Pointer to the user's class which contains a method we would like to call.
	TWeakObjectPtr<UserClass> UserObject;

	// C++ member function pointer.
	FMethodPtr MethodPtr;

	// Payload member variables (if any).
	FUNC_PAYLOAD_MEMBERS

	// The handle of this delegate
	FDelegateHandle Handle;
};

#if !FUNC_IS_CONST

/**
 * Implements a delegate binding for regular C++ functions.
 */
template<FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME>
class STATIC_DELEGATE_INSTANCE_CLASS
	: public DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>
{
public:

	/**
	 * Type definition for static function pointers.
	 *
	 * NOTE: Static C++ functions can never be const, so 'const' is always omitted here. That means there is
	 * usually an extra static delegate class created with a 'const' name, even though it doesn't use const.
	 */
	typedef RetValType (*FFuncPtr)( DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST );

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStaticFuncPtr C++ function pointer.
	 */
	STATIC_DELEGATE_INSTANCE_CLASS( FFuncPtr InStaticFuncPtr DELEGATE_COMMA_PAYLOAD_LIST )
		: StaticFuncPtr(InStaticFuncPtr)
		DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
		, Handle(FDelegateHandle::GenerateNewHandle)
	{
		check(StaticFuncPtr != nullptr);
	}

public:

	// IDelegateInstance interface

	virtual FName GetFunctionName( ) const override
	{
		return NAME_None;
	}

	virtual const void* GetRawMethodPtr( ) const override
	{
		return *(const void**)&StaticFuncPtr;
	}

	virtual const void* GetRawUserObject( ) const override
	{
		return nullptr;
	}

	virtual EDelegateInstanceType::Type GetType( ) const override
	{
		return EDelegateInstanceType::Raw;
	}

	virtual bool HasSameObject( const void* UserObject ) const override
	{
		// Raw Delegates aren't bound to an object so they can never match
		return false;
	}

	virtual bool IsSafeToExecute( ) const override
	{
		// Static functions are always safe to execute!
		return true;
	}

public:

	// DELEGATE_INSTANCE_INTERFACE_CLASS interface

	virtual DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* CreateCopy( ) override
	{
		return new STATIC_DELEGATE_INSTANCE_CLASS(*this);
	}

	virtual RetValType Execute( FUNC_PARAM_LIST ) const override
	{
		// Call the static function
		checkSlow(StaticFuncPtr != nullptr);

		return StaticFuncPtr(DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN);
	}

	virtual FDelegateHandle GetHandle() const override
	{
		return Handle;
	}

#if FUNC_IS_VOID
	virtual bool ExecuteIfSafe( FUNC_PARAM_LIST ) const override
	{
		Execute(FUNC_PARAM_PASSTHRU);

		return true;
	}
#endif

	virtual bool IsSameFunction( const DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>& InOtherDelegate ) const override
	{
		// NOTE: Payload data is not currently considered when comparing delegate instances.
		// See the comment in multi-cast delegate's Remove() method for more information.
		if (InOtherDelegate.GetType() == EDelegateInstanceType::Raw)
		{
			// Downcast to our delegate type and compare
			const STATIC_DELEGATE_INSTANCE_CLASS<FUNC_PAYLOAD_TEMPLATE_ARGS>& OtherStaticDelegate =
				static_cast<const STATIC_DELEGATE_INSTANCE_CLASS<FUNC_PAYLOAD_TEMPLATE_ARGS>&>(InOtherDelegate);

			return (StaticFuncPtr == OtherStaticDelegate.StaticFuncPtr);
		}

		return false;
	}

public:

	/**
	 * Creates a new static function delegate binding for the given function pointer.
	 *
	 * @param InFunc Static function pointer.
	 * @return The new delegate.
	 */
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create( FFuncPtr InFunc DELEGATE_COMMA_PAYLOAD_LIST )
	{
		return new STATIC_DELEGATE_INSTANCE_CLASS(InFunc DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}

private:

	// C++ function pointer.
	FFuncPtr StaticFuncPtr;

	// Payload member variables, if any.
	FUNC_PAYLOAD_MEMBERS

	// The handle of this delegate
	FDelegateHandle Handle;
};


/**
 * Implements a delegate binding for C++ functors, e.g. lambdas.
 */
template<typename FunctorType, FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME>
class FUNCTOR_DELEGATE_INSTANCE_CLASS
	: public DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>
{
public:
	static_assert(TAreTypesEqual<FunctorType, typename TRemoveReference<FunctorType>::Type>::Value, "FunctorType cannot be a reference");

	/**
	 * Creates and initializes a new instance
	 *
	 * @param InFunctor C++ functor
	 */
	FUNCTOR_DELEGATE_INSTANCE_CLASS(const FunctorType& InFunctor DELEGATE_COMMA_PAYLOAD_LIST)
		: Functor(InFunctor)
		DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
		, Handle(FDelegateHandle::GenerateNewHandle)
	{
	}

	FUNCTOR_DELEGATE_INSTANCE_CLASS(FunctorType&& InFunctor DELEGATE_COMMA_PAYLOAD_LIST)
		: Functor(MoveTemp(InFunctor))
		DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
		, Handle(FDelegateHandle::GenerateNewHandle)
	{
	}

public:
	// IDelegateInstance interface

	virtual FName GetFunctionName() const override
	{
		return NAME_None;
	}

	virtual const void* GetRawMethodPtr() const override
	{
		// casting operator() to void* is not legal C++ if it's a member function
		// and wouldn't necessarily be a useful thing to return anyway
		check(0);
		return nullptr;
	}

	virtual const void* GetRawUserObject() const override
	{
		// returning &Functor wouldn't be particularly useful to the comparison code
		// as it would always be unique because we store a copy of the functor
		check(0);
		return nullptr;
	}

	virtual EDelegateInstanceType::Type GetType() const override
	{
		return EDelegateInstanceType::Functor;
	}

	virtual bool HasSameObject(const void* UserObject) const override
	{
		// Functor Delegates aren't bound to a user object so they can never match
		return false;
	}

	virtual bool IsSafeToExecute() const override
	{
		// Functors are always considered safe to execute!
		return true;
	}

public:

	// DELEGATE_INSTANCE_INTERFACE_CLASS interface

	virtual DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* CreateCopy() override
	{
		return new FUNCTOR_DELEGATE_INSTANCE_CLASS(*this);
	}

	virtual RetValType Execute(FUNC_PARAM_LIST) const override
	{
		return Functor(DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN);
	}

	virtual FDelegateHandle GetHandle() const override
	{
		return Handle;
	}

#if FUNC_IS_VOID
	virtual bool ExecuteIfSafe(FUNC_PARAM_LIST) const override
	{
		Execute(FUNC_PARAM_PASSTHRU);

		return true;
	}
#endif

	virtual bool IsSameFunction(const DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>& InOtherDelegate) const override
	{
		// There's no nice way to implement this (we don't have the type info necessary to compare against OtherDelegate's Functor)
		return false;
	}

public:

	/**
	* Creates a new static function delegate binding for the given function pointer.
	*
	* @param InFunctor C++ functor
	* @return The new delegate.
	*/
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create(const FunctorType& InFunctor DELEGATE_COMMA_PAYLOAD_LIST)
	{
		return new FUNCTOR_DELEGATE_INSTANCE_CLASS<FunctorType, FUNC_PAYLOAD_TEMPLATE_ARGS>(InFunctor DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create(FunctorType&& InFunctor DELEGATE_COMMA_PAYLOAD_LIST)
	{
		return new FUNCTOR_DELEGATE_INSTANCE_CLASS<FunctorType, FUNC_PAYLOAD_TEMPLATE_ARGS>(MoveTemp(InFunctor) DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}

private:
	FunctorType Functor;

	// Payload member variables, if any.
	FUNC_PAYLOAD_MEMBERS

	// The handle of this delegate
	FDelegateHandle Handle;
};

/**
 * Implements a delegate binding for UFunctions.
 *
 * @params UserClass Must be an UObject derived class.
 */
template<class UserClass, FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME>
class UFUNCTION_DELEGATE_INSTANCE_CLASS
	: public DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>
{
	static_assert(TPointerIsConvertibleFromTo<UserClass, const UObjectBase>::Value, "You cannot use UFunction delegates with non UObject classes.");

	// Structure for UFunction call parameter marshaling.
	struct FParmsWithPayload
	{
		FUNC_PARAM_MEMBERS		// (Param1Type Param1; Param2Type Param2; ...)
		FUNC_PAYLOAD_MEMBERS	// (Var1Type Var1; Var2Type Var2; ...)

#if !FUNC_IS_VOID
		RetValType Result;		// UFunction return value.
#endif
	};

public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InUserObject The UObject to call the function on.
	 * @param InFunctionName The name of the function call.
	 */
	UFUNCTION_DELEGATE_INSTANCE_CLASS( UserClass* InUserObject, const FName& InFunctionName DELEGATE_COMMA_PAYLOAD_LIST )
		: FunctionName(InFunctionName)
		, UserObjectPtr(InUserObject)
		DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
		, Handle(FDelegateHandle::GenerateNewHandle)
	{
		check(InFunctionName != NAME_None);
		
		if (InUserObject != nullptr)
		{
			CachedFunction = UserObjectPtr->FindFunctionChecked(InFunctionName);
		}		
	}
	
public:

	// IDelegateInstance interface

	virtual FName GetFunctionName( ) const override
	{
		return FunctionName;
	}

	virtual const void* GetRawMethodPtr( ) const override
	{
		return nullptr;
	}

	virtual const void* GetRawUserObject( ) const override
	{
		return UserObjectPtr.Get();
	}

	virtual EDelegateInstanceType::Type GetType() const override
	{
		return EDelegateInstanceType::UFunction;
	}

	virtual bool HasSameObject( const void* InUserObject ) const override
	{
		return (UserObjectPtr.Get() == InUserObject);
	}

	virtual bool IsCompactable( ) const override
	{
		return !UserObjectPtr.Get(true);
	}

	virtual bool IsSafeToExecute( ) const override
	{
		return UserObjectPtr.IsValid();
	}

public:

	// DELEGATE_INSTANCE_INTERFACE_CLASS interface

	virtual DELEGATE_INSTANCE_INTERFACE_CLASS* CreateCopy( ) override
	{
		return new UFUNCTION_DELEGATE_INSTANCE_CLASS(*this);
	}

	virtual RetValType Execute( FUNC_PARAM_LIST ) const override
	{
		checkSlow(IsSafeToExecute());

		FParmsWithPayload Parms = { DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN };
		UserObjectPtr->ProcessEvent(CachedFunction, &Parms);

#if !FUNC_IS_VOID
		return Parms.Result;
#endif
	}

	virtual FDelegateHandle GetHandle() const override
	{
		return Handle;
	}

#if FUNC_IS_VOID
	virtual bool ExecuteIfSafe( FUNC_PARAM_LIST ) const override
	{
		if (IsSafeToExecute())
		{
			Execute(FUNC_PARAM_PASSTHRU);

			return true;
		}

		return false;
	}
#endif

	virtual bool IsSameFunction( const DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>& Other ) const override
	{
		// NOTE: Payload data is not currently considered when comparing delegate instances.
		// See the comment in multi-cast delegate's Remove() method for more information.
		return ((Other.GetType() == EDelegateInstanceType::UFunction) &&
				(Other.GetRawUserObject() == GetRawUserObject()) &&
				(Other.GetFunctionName() == GetFunctionName()));
	}

	// End DELEGATE_INSTANCE_INTERFACE_CLASS interface

public:

	/**
	 * Creates a new UFunction delegate binding for the given user object and function name.
	 *
	 * @param InObject The user object to call the function on.
	 * @param InFunctionName The name of the function call.
	 * @return The new delegate.
	 */
	FORCEINLINE static DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS>* Create( UserClass* InUserObject, const FName& InFunctionName DELEGATE_COMMA_PAYLOAD_LIST )
	{
		return new UFUNCTION_DELEGATE_INSTANCE_CLASS(InUserObject, InFunctionName DELEGATE_COMMA_PAYLOAD_PASSTHRU);
	}

public:

	// Holds the cached UFunction to call.
	UFunction* CachedFunction;

	// Holds the name of the function to call.
	FName FunctionName;

	// The user object to call the function on.
	TWeakObjectPtr<UserClass> UserObjectPtr;

	// Payload member variables, if any.
	FUNC_PAYLOAD_MEMBERS

	// The handle of this delegate
	FDelegateHandle Handle;
};

#endif // FUNC_IS_CONST


#undef SP_METHOD_DELEGATE_INSTANCE_CLASS
#undef RAW_METHOD_DELEGATE_INSTANCE_CLASS
#undef UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS
#undef STATIC_DELEGATE_INSTANCE_CLASS
#undef UFUNCTION_DELEGATE_INSTANCE_CLASS

#undef DELEGATE_INSTANCE_INTERFACE_CLASS
#undef DELEGATE_QUALIFIER
#undef DELEGATE_COMMA_PAYLOAD_LIST
#undef DELEGATE_COMMA_PAYLOAD_PASSTHRU
#undef DELEGATE_COMMA_PAYLOAD_INITIALIZER_LIST
#undef DELEGATE_COMMA_PAYLOAD_PASSIN
#undef DELEGATE_PARAM_PASSTHRU_COMMA_PAYLOAD_PASSIN
#undef DELEGATE_PARAM_LIST_COMMA_PAYLOAD_LIST
#undef DELEGATE_PARMS_COLON_INITIALIZER_LIST
