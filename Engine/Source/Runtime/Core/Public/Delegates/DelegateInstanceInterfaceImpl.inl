// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// Only designed to be included directly by Delegate.h
#if !defined( __Delegate_h__ ) || !defined( FUNC_INCLUDING_INLINE_IMPL )
	#error "This inline header must only be included by Delegate.h"
#endif


// NOTE: This file in re-included for EVERY delegate signature that we support.  That is, every combination
//		 of parameter count, return value presence or other function modifier will include this file to
//		 generate a delegate interface type and implementation type for that signature.

#define DELEGATE_INSTANCE_INTERFACE_CLASS FUNC_COMBINE( IBaseDelegateInstance_, FUNC_SUFFIX )

/**
 * Delegate instance base interface.  For internal use only.  This abstract class is used to interact with
 * a delegate instance for *any* type of object (e.g. raw, shared pointer, UObject-based, etc.)
 */
template< FUNC_TEMPLATE_DECL_TYPENAME >
class DELEGATE_INSTANCE_INTERFACE_CLASS
	: public IDelegateInstance
{
	// Structure for UFunction call parameter marshaling.
	struct FParms
	{
		FUNC_PARAM_MEMBERS

#if !FUNC_IS_VOID
		RetValType Result;
#endif
	};

public:

	/**
	 * Creates a copy of the delegate instance
	 *
	 * @return	The newly created copy
	 */
	virtual DELEGATE_INSTANCE_INTERFACE_CLASS* CreateCopy() = 0;

	/**
	 * Returns true if this delegate points to exactly the same object and method as the specified delegate,
	 * even if the delegate objects themselves are different.  Also, the delegate types *must* be compatible.
	 *
	 * @param  InOtherDelegate
	 * @return  True if delegates match
	 */
	virtual bool IsSameFunction( const DELEGATE_INSTANCE_INTERFACE_CLASS& InOtherDelegate ) const = 0;

	/**
	 * Execute the delegate.  If the function pointer is not valid, an error will occur.
	 */
	virtual RetValType Execute( FUNC_PARAM_LIST ) const = 0;

#if FUNC_IS_VOID

	/**
	 * Execute the delegate, but only if the function pointer is still valid
	 *
	 * @return  Returns true if the function was executed
	 */
	// NOTE: Currently only delegates with no return value support ExecuteIfSafe() 
	virtual bool ExecuteIfSafe( FUNC_PARAM_LIST ) const = 0;

#endif // FUNC_IS_VOID

public:

	/**
	 * Execute the delegate with parameters from the virtual machine.
	 *
	 * This non-virtual interface (NVI) method marshals a set of call parameters
	 * from the UObject virtual machine to the bound delegate handler function.
	 *
	 * @param InParms An UFunction parameter structure.
	 */
	RetValType ExecuteParms( void* InParms ) const
	{
		FParms& Parms = *((FParms*)InParms);
		return Execute(FUNC_PARAM_PARMS_PASSIN);
	}
};


#undef DELEGATE_INSTANCE_INTERFACE_CLASS
