// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// Only designed to be included directly by Delegate.h
#if !defined( __Delegate_h__ ) || !defined( FUNC_INCLUDING_INLINE_IMPL )
	#error "This inline header must only be included by Delegate.h"
#endif


// NOTE: This file is re-included for EVERY delegate signature that we support.  That is, every combination
//		 of parameter count, return value presence or other function modifier will include this file to
//		 generate a delegate interface type and implementation type for that signature.

#define DELEGATE_INSTANCE_INTERFACE_CLASS FUNC_COMBINE( IBaseDelegateInstance_, FUNC_SUFFIX )

#define SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars FUNC_COMBINE( TBaseSPMethodDelegateInstance_, FUNC_SUFFIX )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _Const )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _OneVar )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar, _Const )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _TwoVars )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars, _Const )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _ThreeVars )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars, _Const )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _FourVars )
#define SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const FUNC_COMBINE( SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars, _Const )

#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars FUNC_COMBINE( TBaseRawMethodDelegateInstance_, FUNC_SUFFIX )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _Const )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _OneVar )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar, _Const )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _TwoVars )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars, _Const )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _ThreeVars )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars, _Const )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _FourVars )
#define RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const FUNC_COMBINE( RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars, _Const )

#define UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars FUNC_COMBINE( TBaseUFunctionDelegateInstance_, FUNC_SUFFIX )
#define UFUNCTION_DELEGATE_INSTANCE_CLASS_OneVar FUNC_COMBINE( UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars, _OneVar )
#define UFUNCTION_DELEGATE_INSTANCE_CLASS_TwoVars FUNC_COMBINE( UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars, _TwoVars )
#define UFUNCTION_DELEGATE_INSTANCE_CLASS_ThreeVars FUNC_COMBINE( UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars, _ThreeVars )
#define UFUNCTION_DELEGATE_INSTANCE_CLASS_FourVars FUNC_COMBINE( UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars, _FourVars )

#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars FUNC_COMBINE( TBaseUObjectMethodDelegateInstance_, FUNC_SUFFIX )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _Const )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _OneVar )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar, _Const )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _TwoVars )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars, _Const )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _ThreeVars )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars, _Const )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars, _FourVars )
#define UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const FUNC_COMBINE( UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars, _Const )

#define STATIC_DELEGATE_INSTANCE_CLASS_NoVars FUNC_COMBINE( TBaseStaticDelegateInstance_, FUNC_SUFFIX )
#define STATIC_DELEGATE_INSTANCE_CLASS_OneVar FUNC_COMBINE( STATIC_DELEGATE_INSTANCE_CLASS_NoVars, _OneVar )
#define STATIC_DELEGATE_INSTANCE_CLASS_TwoVars FUNC_COMBINE( STATIC_DELEGATE_INSTANCE_CLASS_NoVars, _TwoVars )
#define STATIC_DELEGATE_INSTANCE_CLASS_ThreeVars FUNC_COMBINE( STATIC_DELEGATE_INSTANCE_CLASS_NoVars, _ThreeVars )
#define STATIC_DELEGATE_INSTANCE_CLASS_FourVars FUNC_COMBINE( STATIC_DELEGATE_INSTANCE_CLASS_NoVars, _FourVars )

#define FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars FUNC_COMBINE( TBaseFunctorDelegateInstance_, FUNC_SUFFIX )
#define FUNCTOR_DELEGATE_INSTANCE_CLASS_OneVar FUNC_COMBINE( FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars, _OneVar )
#define FUNCTOR_DELEGATE_INSTANCE_CLASS_TwoVars FUNC_COMBINE( FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars, _TwoVars )
#define FUNCTOR_DELEGATE_INSTANCE_CLASS_ThreeVars FUNC_COMBINE( FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars, _ThreeVars )
#define FUNCTOR_DELEGATE_INSTANCE_CLASS_FourVars FUNC_COMBINE( FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars, _FourVars )

#define DELEGATE_CLASS FUNC_COMBINE( TBaseDelegate_, FUNC_SUFFIX )
#define BASE_MULTICAST_DELEGATE_CLASS FUNC_COMBINE( TBaseMulticastDelegate_, FUNC_SUFFIX )
#define MULTICAST_DELEGATE_CLASS FUNC_COMBINE( TMulticastDelegate_, FUNC_SUFFIX )
#define EVENT_CLASS FUNC_COMBINE( TEvent_, FUNC_SUFFIX )
#define DYNAMIC_DELEGATE_CLASS FUNC_COMBINE( TBaseDynamicDelegate_, FUNC_SUFFIX )
#define DYNAMIC_MULTICAST_DELEGATE_CLASS FUNC_COMBINE( TBaseDynamicMulticastDelegate_, FUNC_SUFFIX )

#define PAYLOAD_TEMPLATE_DECL_OneVar typename Var1Type
#define PAYLOAD_TEMPLATE_LIST_OneVar Var1Type
#define PAYLOAD_TEMPLATE_ARGS_OneVar Var1Type Var1
#define PAYLOAD_TEMPLATE_PASSIN_OneVar Var1

#define PAYLOAD_TEMPLATE_DECL_TwoVars PAYLOAD_TEMPLATE_DECL_OneVar, typename Var2Type
#define PAYLOAD_TEMPLATE_LIST_TwoVars PAYLOAD_TEMPLATE_LIST_OneVar, Var2Type
#define PAYLOAD_TEMPLATE_ARGS_TwoVars PAYLOAD_TEMPLATE_ARGS_OneVar, Var2Type Var2
#define PAYLOAD_TEMPLATE_PASSIN_TwoVars PAYLOAD_TEMPLATE_PASSIN_OneVar, Var2

#define PAYLOAD_TEMPLATE_DECL_ThreeVars PAYLOAD_TEMPLATE_DECL_TwoVars, typename Var3Type
#define PAYLOAD_TEMPLATE_LIST_ThreeVars PAYLOAD_TEMPLATE_LIST_TwoVars, Var3Type
#define PAYLOAD_TEMPLATE_ARGS_ThreeVars PAYLOAD_TEMPLATE_ARGS_TwoVars, Var3Type Var3
#define PAYLOAD_TEMPLATE_PASSIN_ThreeVars PAYLOAD_TEMPLATE_PASSIN_TwoVars, Var3

#define PAYLOAD_TEMPLATE_DECL_FourVars PAYLOAD_TEMPLATE_DECL_ThreeVars, typename Var4Type
#define PAYLOAD_TEMPLATE_LIST_FourVars PAYLOAD_TEMPLATE_LIST_ThreeVars, Var4Type
#define PAYLOAD_TEMPLATE_ARGS_FourVars PAYLOAD_TEMPLATE_ARGS_ThreeVars, Var4Type Var4
#define PAYLOAD_TEMPLATE_PASSIN_FourVars PAYLOAD_TEMPLATE_PASSIN_ThreeVars, Var4


/**
 * Unicast delegate base object.
 *
 * Use the various DECLARE_DELEGATE macros to create the actual delegate type, templated to
 * the function signature the delegate is compatible with. Then, you can create an instance
 * of that class when you want to bind a function to the delegate.
 */
template< FUNC_TEMPLATE_DECL_TYPENAME >
class DELEGATE_CLASS
	: public FDelegateBase<>
{
public:

	/** Type definition for return value type. */
	FUNC_RETVAL_TYPEDEF
		
	/** Type definition for the shared interface of delegate instance types compatible with this delegate class. */
	typedef DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS> TDelegateInstanceInterface;

	/** Declare the user's "fast" shared pointer-based delegate instance types. */
	template< class UserClass                                  > class TSPMethodDelegate                 : public SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::Fast > { public: TSPMethodDelegate                ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::Fast >::FMethodPtr InMethodPtr                                  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::Fast >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass                                  > class TSPMethodDelegate_Const           : public SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::Fast > { public: TSPMethodDelegate_Const          ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::Fast >::FMethodPtr InMethodPtr                                  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::Fast >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TSPMethodDelegate_OneVar          : public SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::Fast > { public: TSPMethodDelegate_OneVar         ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TSPMethodDelegate_OneVar_Const    : public SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::Fast > { public: TSPMethodDelegate_OneVar_Const   ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TSPMethodDelegate_TwoVars         : public SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::Fast > { public: TSPMethodDelegate_TwoVars        ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TSPMethodDelegate_TwoVars_Const   : public SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::Fast > { public: TSPMethodDelegate_TwoVars_Const  ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TSPMethodDelegate_ThreeVars       : public SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::Fast > { public: TSPMethodDelegate_ThreeVars      ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TSPMethodDelegate_ThreeVars_Const : public SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::Fast > { public: TSPMethodDelegate_ThreeVars_Const( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TSPMethodDelegate_FourVars        : public SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::Fast > { public: TSPMethodDelegate_FourVars       ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TSPMethodDelegate_FourVars_Const  : public SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::Fast > { public: TSPMethodDelegate_FourVars_Const ( const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::Fast >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::Fast >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };

	/** Declare the user's "thread-safe" shared pointer-based delegate instance types. */
	template< class UserClass                                  > class TThreadSafeSPMethodDelegate                 : public SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate                ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::ThreadSafe >::FMethodPtr InMethodPtr                                  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::ThreadSafe >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass                                  > class TThreadSafeSPMethodDelegate_Const           : public SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_Const          ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::ThreadSafe >::FMethodPtr InMethodPtr                                  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS,                                  ESPMode::ThreadSafe >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TThreadSafeSPMethodDelegate_OneVar          : public SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_OneVar         ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TThreadSafeSPMethodDelegate_OneVar_Const    : public SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_OneVar_Const   ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar,    ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TThreadSafeSPMethodDelegate_TwoVars         : public SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_TwoVars        ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TThreadSafeSPMethodDelegate_TwoVars_Const   : public SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_TwoVars_Const  ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars,   ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TThreadSafeSPMethodDelegate_ThreeVars       : public SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_ThreeVars      ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TThreadSafeSPMethodDelegate_ThreeVars_Const : public SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_ThreeVars_Const( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars, ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TThreadSafeSPMethodDelegate_FourVars        : public SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_FourVars       ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TThreadSafeSPMethodDelegate_FourVars_Const  : public SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::ThreadSafe > { public: TThreadSafeSPMethodDelegate_FourVars_Const ( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::ThreadSafe >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars,  ESPMode::ThreadSafe >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };

	/** Declare the user's C++ pointer-based delegate instance types. */
	template< class UserClass                                  > class TRawMethodDelegate                 : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS                                  > { public: TRawMethodDelegate                ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS                                  >::FMethodPtr InMethodPtr                                  ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS                                  >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass                                  > class TRawMethodDelegate_Const           : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS                                  > { public: TRawMethodDelegate_Const          ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS                                  >::FMethodPtr InMethodPtr                                  ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS                                  >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TRawMethodDelegate_OneVar          : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    > { public: TRawMethodDelegate_OneVar         ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TRawMethodDelegate_OneVar_Const    : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    > { public: TRawMethodDelegate_OneVar_Const   ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TRawMethodDelegate_TwoVars         : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   > { public: TRawMethodDelegate_TwoVars        ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TRawMethodDelegate_TwoVars_Const   : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   > { public: TRawMethodDelegate_TwoVars_Const  ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TRawMethodDelegate_ThreeVars       : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars > { public: TRawMethodDelegate_ThreeVars      ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TRawMethodDelegate_ThreeVars_Const : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars > { public: TRawMethodDelegate_ThreeVars_Const( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TRawMethodDelegate_FourVars        : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  > { public: TRawMethodDelegate_FourVars       ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TRawMethodDelegate_FourVars_Const  : public RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  > { public: TRawMethodDelegate_FourVars_Const ( UserClass* InUserObject, typename RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };
	
	/** Declare the user's UFunction-based delegate instance types. */
	template< class UObjectTemplate                                  > class TUFunctionDelegateBinding                     : public UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars   < UObjectTemplate, FUNC_TEMPLATE_ARGS                                  > { public: TUFunctionDelegateBinding          ( UObjectTemplate* InUserObject, const FName& InFunctionName                                  ) : UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars   < UObjectTemplate, FUNC_TEMPLATE_ARGS                                  >( InUserObject, InFunctionName                                    ) {} };
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_OneVar    > class TUFunctionDelegateBinding_OneVar              : public UFUNCTION_DELEGATE_INSTANCE_CLASS_OneVar   < UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    > { public: TUFunctionDelegateBinding_OneVar   ( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : UFUNCTION_DELEGATE_INSTANCE_CLASS_OneVar   < UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TUFunctionDelegateBinding_TwoVars             : public UFUNCTION_DELEGATE_INSTANCE_CLASS_TwoVars  < UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   > { public: TUFunctionDelegateBinding_TwoVars  ( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : UFUNCTION_DELEGATE_INSTANCE_CLASS_TwoVars  < UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TUFunctionDelegateBinding_ThreeVars           : public UFUNCTION_DELEGATE_INSTANCE_CLASS_ThreeVars< UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars > { public: TUFunctionDelegateBinding_ThreeVars( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : UFUNCTION_DELEGATE_INSTANCE_CLASS_ThreeVars< UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_FourVars  > class TUFunctionDelegateBinding_FourVars            : public UFUNCTION_DELEGATE_INSTANCE_CLASS_FourVars < UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  > { public: TUFunctionDelegateBinding_FourVars ( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : UFUNCTION_DELEGATE_INSTANCE_CLASS_FourVars < UObjectTemplate, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };

	/** Declare the user's UObject-based delegate instance types. */
	template< class UserClass                                  > class TUObjectMethodDelegate                 : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS                                  > { public: TUObjectMethodDelegate                ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS                                  >::FMethodPtr InMethodPtr                                  ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars         < UserClass, FUNC_TEMPLATE_ARGS                                  >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass                                  > class TUObjectMethodDelegate_Const           : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS                                  > { public: TUObjectMethodDelegate_Const          ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS                                  >::FMethodPtr InMethodPtr                                  ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars_Const   < UserClass, FUNC_TEMPLATE_ARGS                                  >( InUserObject, InMethodPtr                                    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TUObjectMethodDelegate_OneVar          : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    > { public: TUObjectMethodDelegate_OneVar         ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar         < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar    > class TUObjectMethodDelegate_OneVar_Const    : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    > { public: TUObjectMethodDelegate_OneVar_Const   ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar_Const   < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TUObjectMethodDelegate_TwoVars         : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   > { public: TUObjectMethodDelegate_TwoVars        ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars        < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars   > class TUObjectMethodDelegate_TwoVars_Const   : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   > { public: TUObjectMethodDelegate_TwoVars_Const  ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars_Const  < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TUObjectMethodDelegate_ThreeVars       : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars > { public: TUObjectMethodDelegate_ThreeVars      ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars      < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars > class TUObjectMethodDelegate_ThreeVars_Const : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars > { public: TUObjectMethodDelegate_ThreeVars_Const( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars_Const< UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TUObjectMethodDelegate_FourVars        : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  > { public: TUObjectMethodDelegate_FourVars       ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars       < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars  > class TUObjectMethodDelegate_FourVars_Const  : public UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  > { public: TUObjectMethodDelegate_FourVars_Const ( UserClass* InUserObject, typename UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >::FMethodPtr InMethodPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars_Const < UserClass, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >( InUserObject, InMethodPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };
	
	/** Declare the user's static function pointer delegate instance types. */
	typedef STATIC_DELEGATE_INSTANCE_CLASS_NoVars< FUNC_TEMPLATE_ARGS > FStaticDelegate;
	template< PAYLOAD_TEMPLATE_DECL_OneVar    > class TStaticDelegate_OneVar    : public STATIC_DELEGATE_INSTANCE_CLASS_OneVar   < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    > { public: TStaticDelegate_OneVar   ( typename STATIC_DELEGATE_INSTANCE_CLASS_OneVar   < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >::FFuncPtr InFuncPtr, PAYLOAD_TEMPLATE_ARGS_OneVar    ) : STATIC_DELEGATE_INSTANCE_CLASS_OneVar   < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >( InFuncPtr, PAYLOAD_TEMPLATE_PASSIN_OneVar    ) {} };
	template< PAYLOAD_TEMPLATE_DECL_TwoVars   > class TStaticDelegate_TwoVars   : public STATIC_DELEGATE_INSTANCE_CLASS_TwoVars  < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   > { public: TStaticDelegate_TwoVars  ( typename STATIC_DELEGATE_INSTANCE_CLASS_TwoVars  < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >::FFuncPtr InFuncPtr, PAYLOAD_TEMPLATE_ARGS_TwoVars   ) : STATIC_DELEGATE_INSTANCE_CLASS_TwoVars  < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >( InFuncPtr, PAYLOAD_TEMPLATE_PASSIN_TwoVars   ) {} };
	template< PAYLOAD_TEMPLATE_DECL_ThreeVars > class TStaticDelegate_ThreeVars : public STATIC_DELEGATE_INSTANCE_CLASS_ThreeVars< FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars > { public: TStaticDelegate_ThreeVars( typename STATIC_DELEGATE_INSTANCE_CLASS_ThreeVars< FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FFuncPtr InFuncPtr, PAYLOAD_TEMPLATE_ARGS_ThreeVars ) : STATIC_DELEGATE_INSTANCE_CLASS_ThreeVars< FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >( InFuncPtr, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) {} };
	template< PAYLOAD_TEMPLATE_DECL_FourVars  > class TStaticDelegate_FourVars  : public STATIC_DELEGATE_INSTANCE_CLASS_FourVars < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  > { public: TStaticDelegate_FourVars ( typename STATIC_DELEGATE_INSTANCE_CLASS_FourVars < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >::FFuncPtr InFuncPtr, PAYLOAD_TEMPLATE_ARGS_FourVars  ) : STATIC_DELEGATE_INSTANCE_CLASS_FourVars < FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >( InFuncPtr, PAYLOAD_TEMPLATE_PASSIN_FourVars  ) {} };

	/** Declare the user's functor delegate instance types. */
	template<typename FunctorType                                 > class TFunctorDelegate           : public FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars   <FunctorType, FUNC_TEMPLATE_ARGS                                 > { public: TFunctorDelegate          (const FunctorType& InFunctor                                 ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars   <FunctorType, FUNC_TEMPLATE_ARGS                                  >(InFunctor                                   ) {} TFunctorDelegate          (FunctorType&& InFunctor                                 ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_NoVars   <FunctorType, FUNC_TEMPLATE_ARGS                                  >(MoveTemp(InFunctor)                                   ) {} };
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_OneVar   > class TFunctorDelegate_OneVar    : public FUNCTOR_DELEGATE_INSTANCE_CLASS_OneVar   <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar   > { public: TFunctorDelegate_OneVar   (const FunctorType& InFunctor, PAYLOAD_TEMPLATE_ARGS_OneVar   ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_OneVar   <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >(InFunctor, PAYLOAD_TEMPLATE_PASSIN_OneVar   ) {} TFunctorDelegate_OneVar   (FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_OneVar   ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_OneVar   <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_OneVar    >(MoveTemp(InFunctor), PAYLOAD_TEMPLATE_PASSIN_OneVar   ) {} };
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_TwoVars  > class TFunctorDelegate_TwoVars   : public FUNCTOR_DELEGATE_INSTANCE_CLASS_TwoVars  <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars  > { public: TFunctorDelegate_TwoVars  (const FunctorType& InFunctor, PAYLOAD_TEMPLATE_ARGS_TwoVars  ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_TwoVars  <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >(InFunctor, PAYLOAD_TEMPLATE_PASSIN_TwoVars  ) {} TFunctorDelegate_TwoVars  (FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_TwoVars  ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_TwoVars  <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_TwoVars   >(MoveTemp(InFunctor), PAYLOAD_TEMPLATE_PASSIN_TwoVars  ) {} };
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_ThreeVars> class TFunctorDelegate_ThreeVars : public FUNCTOR_DELEGATE_INSTANCE_CLASS_ThreeVars<FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars> { public: TFunctorDelegate_ThreeVars(const FunctorType& InFunctor, PAYLOAD_TEMPLATE_ARGS_ThreeVars) : FUNCTOR_DELEGATE_INSTANCE_CLASS_ThreeVars<FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >(InFunctor, PAYLOAD_TEMPLATE_PASSIN_ThreeVars) {} TFunctorDelegate_ThreeVars(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_ThreeVars) : FUNCTOR_DELEGATE_INSTANCE_CLASS_ThreeVars<FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_ThreeVars >(MoveTemp(InFunctor), PAYLOAD_TEMPLATE_PASSIN_ThreeVars) {} };
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_FourVars > class TFunctorDelegate_FourVars  : public FUNCTOR_DELEGATE_INSTANCE_CLASS_FourVars <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars > { public: TFunctorDelegate_FourVars (const FunctorType& InFunctor, PAYLOAD_TEMPLATE_ARGS_FourVars ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_FourVars <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >(InFunctor, PAYLOAD_TEMPLATE_PASSIN_FourVars ) {} TFunctorDelegate_FourVars (FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_FourVars ) : FUNCTOR_DELEGATE_INSTANCE_CLASS_FourVars <FunctorType, FUNC_TEMPLATE_ARGS, PAYLOAD_TEMPLATE_LIST_FourVars  >(MoveTemp(InFunctor), PAYLOAD_TEMPLATE_PASSIN_FourVars ) {} };

public:

	/**
	 * Static: Creates a raw C++ pointer global function delegate
	 */
	inline static DELEGATE_CLASS CreateStatic( typename FStaticDelegate::FFuncPtr InFunc )
	{
		return DELEGATE_CLASS( FStaticDelegate::Create( InFunc ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateStatic( typename TStaticDelegate_OneVar< PAYLOAD_TEMPLATE_LIST_OneVar >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TStaticDelegate_OneVar< PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateStatic( typename TStaticDelegate_TwoVars< PAYLOAD_TEMPLATE_LIST_TwoVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TStaticDelegate_TwoVars< PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateStatic( typename TStaticDelegate_ThreeVars< PAYLOAD_TEMPLATE_LIST_ThreeVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TStaticDelegate_ThreeVars< PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateStatic( typename TStaticDelegate_FourVars< PAYLOAD_TEMPLATE_LIST_FourVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TStaticDelegate_FourVars< PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Static: Creates a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename FunctorType>
	inline static DELEGATE_CLASS<FUNC_TEMPLATE_ARGS> CreateLambda(FunctorType&& InFunctor)
	{
		return DELEGATE_CLASS<FUNC_TEMPLATE_ARGS>(TFunctorDelegate<typename TRemoveReference<FunctorType>::Type>::Create(Forward<FunctorType>(InFunctor)));
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_OneVar>
	inline static DELEGATE_CLASS<FUNC_TEMPLATE_ARGS> CreateLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_OneVar)
	{
		return DELEGATE_CLASS<FUNC_TEMPLATE_ARGS>(TFunctorDelegate_OneVar<typename TRemoveReference<FunctorType>::Type, PAYLOAD_TEMPLATE_LIST_OneVar>::Create(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_OneVar));
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_TwoVars>
	inline static DELEGATE_CLASS<FUNC_TEMPLATE_ARGS> CreateLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_TwoVars)
	{
		return DELEGATE_CLASS<FUNC_TEMPLATE_ARGS>(TFunctorDelegate_TwoVars<typename TRemoveReference<FunctorType>::Type, PAYLOAD_TEMPLATE_LIST_TwoVars>::Create(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_TwoVars));
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_ThreeVars>
	inline static DELEGATE_CLASS<FUNC_TEMPLATE_ARGS> CreateLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_ThreeVars)
	{
		return DELEGATE_CLASS<FUNC_TEMPLATE_ARGS>(TFunctorDelegate_ThreeVars<typename TRemoveReference<FunctorType>::Type, PAYLOAD_TEMPLATE_LIST_ThreeVars>::Create(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_ThreeVars));
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_FourVars>
	inline static DELEGATE_CLASS<FUNC_TEMPLATE_ARGS> CreateLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_FourVars)
	{
		return DELEGATE_CLASS<FUNC_TEMPLATE_ARGS>(TFunctorDelegate_FourVars<typename TRemoveReference<FunctorType>::Type, PAYLOAD_TEMPLATE_LIST_FourVars>::Create(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_FourVars));
	}

	/**
	 * Static: Creates a raw C++ pointer member function delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template< class UserClass >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TRawMethodDelegate< UserClass >::Create( InUserObject, InFunc ) );
	}
	template< class UserClass >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_Const< UserClass >::Create( InUserObject, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateRaw( UserClass* InUserObject, typename TRawMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TRawMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	
	/**
	 * Static: Creates a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TSPMethodDelegate< UserClass >::Create( InUserObjectRef, InFunc ) );
	}
	template< class UserClass >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_Const< UserClass >::Create( InUserObjectRef, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Static: Creates a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc );
	}
	template< class UserClass >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateSP( UserClass* InUserObject, typename TSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return CreateSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	
	/**
	 * Static: Creates a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate< UserClass >::Create( InUserObjectRef, InFunc ) );
	}
	template< class UserClass >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_Const< UserClass >::Create( InUserObjectRef, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TThreadSafeSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Static: Creates a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc );
	}
	template< class UserClass >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return CreateThreadSafeSP( StaticCastSharedRef< UserClass >( InUserObject->AsShared() ), InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Static: Creates a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UObjectTemplate >
	inline static DELEGATE_CLASS CreateUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName )
	{
		return DELEGATE_CLASS( TUFunctionDelegateBinding< UObjectTemplate >::Create( InUserObject, InFunctionName ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TUFunctionDelegateBinding_OneVar< UObjectTemplate, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TUFunctionDelegateBinding_TwoVars< UObjectTemplate, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TUFunctionDelegateBinding_ThreeVars< UObjectTemplate, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TUFunctionDelegateBinding_FourVars< UObjectTemplate, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Static: Creates a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate< UserClass >::Create( InUserObject, InFunc ) );
	}
	template< class UserClass >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_Const< UserClass >::Create( InUserObject, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline static DELEGATE_CLASS CreateUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return DELEGATE_CLASS( TUObjectMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::Create( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

public:

	/**
	 * Default constructor
	 */
	inline DELEGATE_CLASS()
		: FDelegateBase(nullptr)
	{ }

	/**
	 * Destructor.
	 */
	inline ~DELEGATE_CLASS()
	{
		Unbind();
	}

	/**
	 * Creates and initializes a new instance with the given delegate instance.
	 *
	 * The delegate will assume ownership of the incoming delegate instance!
	 * IMPORTANT: This is a system-internal function and you should never be using this in regular C++ code
	 *
	 * @param InDelegateInstance The delegate instance to assign.
	 */
	inline DELEGATE_CLASS( TDelegateInstanceInterface* InDelegateInstance )
		: FDelegateBase(InDelegateInstance)
	{ }

	/**
	 * Creates and initializes a new instance from an existing delegate object.
	 *
	 * @param Other The delegate object to copy from.
	 */
	inline DELEGATE_CLASS( const DELEGATE_CLASS& Other )
		: FDelegateBase(nullptr)
	{
		*this = Other;
	}

	/**
	 * Assignment operator.
	 *
	 * @param	OtherDelegate	Delegate object to copy from
	 */
	inline DELEGATE_CLASS& operator=( const DELEGATE_CLASS& Other )
	{
		if (&Other != this)
		{
			// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
			TDelegateInstanceInterface* OtherInstance = (TDelegateInstanceInterface*)Other.GetDelegateInstance();

			if (OtherInstance != nullptr)
			{
				SetDelegateInstance(OtherInstance->CreateCopy());
			}
			else
			{
				SetDelegateInstance(nullptr);
			}
		}

		return *this;
	}

public:

	/**
	 * Binds a raw C++ pointer global function delegate
	 */
	inline void BindStatic( typename FStaticDelegate::FFuncPtr InFunc )
	{
		*this = CreateStatic( InFunc );
	}
	template< PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindStatic( typename TStaticDelegate_OneVar< PAYLOAD_TEMPLATE_LIST_OneVar >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindStatic( typename TStaticDelegate_TwoVars< PAYLOAD_TEMPLATE_LIST_TwoVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindStatic( typename TStaticDelegate_ThreeVars< PAYLOAD_TEMPLATE_LIST_ThreeVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindStatic( typename TStaticDelegate_FourVars< PAYLOAD_TEMPLATE_LIST_FourVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Static: Binds a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	*/
	template<typename FunctorType>
	inline void BindLambda(FunctorType&& InFunctor)
	{
		*this = CreateLambda(Forward<FunctorType>(InFunctor));
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_OneVar>
	inline void BindLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_OneVar)
	{
		*this = CreateLambda(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_OneVar);
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_TwoVars>
	inline void BindLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_TwoVars)
	{
		*this = CreateLambda(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_TwoVars);
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_ThreeVars>
	inline void BindLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_ThreeVars)
	{
		*this = CreateLambda(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_ThreeVars);
	}
	template<typename FunctorType, PAYLOAD_TEMPLATE_DECL_FourVars>
	inline void BindLambda(FunctorType&& InFunctor, PAYLOAD_TEMPLATE_ARGS_FourVars)
	{
		*this = CreateLambda(Forward<FunctorType>(InFunctor), PAYLOAD_TEMPLATE_PASSIN_FourVars);
	}

	/**
	 * Binds a raw C++ pointer delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template< class UserClass >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateRaw( InUserObject, InFunc );
	}
	template< class UserClass >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateRaw( InUserObject, InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindRaw( UserClass* InUserObject, typename TRawMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Binds a shared pointer-based (fast, not thread-safe) member function delegate.  Shared pointer delegates keep a weak reference to your object.  You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateSP( InUserObjectRef, InFunc );
	}
	template< class UserClass >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateSP( InUserObjectRef, InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename TSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Binds a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateSP( InUserObject, InFunc );
	}
	template< class UserClass >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateSP( InUserObject, InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindSP( UserClass* InUserObject, typename TSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Binds a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc );
	}
	template< class UserClass >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename TThreadSafeSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Binds a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc );
	}
	template< class UserClass >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindThreadSafeSP( UserClass* InUserObject, typename TThreadSafeSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Binds a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UObjectTemplate >
	inline void BindUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName )
	{
		*this = CreateUFunction( InUserObject, InFunctionName );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}

	/**
	 * Creates a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template< class UserClass >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateUObject( InUserObject, InFunc );
	}
	template< class UserClass >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		*this = CreateUObject( InUserObject, InFunc );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline void BindUObject( UserClass* InUserObject, typename TUObjectMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		*this = CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars );
	}
public:

	/**
	 * Execute the delegate.
	 *
	 * If the function pointer is not valid, an error will occur. Check IsBound() before
	 * calling this method or use ExecuteIfBound() instead.
	 *
	 * @see ExecuteIfBound
	 */
	inline RetValType Execute( FUNC_PARAM_LIST ) const
	{
		TDelegateInstanceInterface* DelegateInstance = (TDelegateInstanceInterface*)GetDelegateInstance();

		// If this assert goes off, Execute() was called before a function was bound to the delegate.
		// Consider using ExecuteIfSafe() instead.
		checkSlow(DelegateInstance != nullptr);

		return DelegateInstance->Execute(FUNC_PARAM_PASSTHRU);
	}

#if FUNC_IS_VOID
	/**
	 * Execute the delegate, but only if the function pointer is still valid
	 *
	 * @return  Returns true if the function was executed
	 */
	// NOTE: Currently only delegates with no return value support ExecuteIfBound() 
	inline bool ExecuteIfBound( FUNC_PARAM_LIST ) const
	{
		if( IsBound() )
		{
			return ((TDelegateInstanceInterface*)GetDelegateInstance())->ExecuteIfSafe( FUNC_PARAM_PASSTHRU );
		}

		return false;
	}
#endif

	/**
	 * Equality operator.
	 *
	 * @return true if this delegate equals the other, false otherwise.
	 */
	DELEGATE_DEPRECATED("Delegate comparison is deprecated - please replace any usage with comparison of FDelegateHandles.")
	bool operator==( const DELEGATE_CLASS& Other ) const
	{
		TDelegateInstanceInterface* DelegateInstance = (TDelegateInstanceInterface*)GetDelegateInstance();
		TDelegateInstanceInterface* OtherInstance = (TDelegateInstanceInterface*)Other.GetDelegateInstance();
	
		// The function these delegates point to must be the same
		if ((DelegateInstance != nullptr) && (OtherInstance != nullptr))
		{
			return DelegateInstance->IsSameFunction(*OtherInstance);
		}
	
		// If neither delegate is initialized to anything yet, then we treat them as equal
		if ((DelegateInstance == nullptr) && (OtherInstance == nullptr))
		{
			return true;
		}
	
		// No match!
		return false;
	}

	/**
	 * Delegate comparison operator.
	 *
	 * @return true if this delegate equals the other, false otherwise.
	 */
	bool DEPRECATED_Compare( const DELEGATE_CLASS& Other ) const
	{
		TDelegateInstanceInterface* DelegateInstance = (TDelegateInstanceInterface*)GetDelegateInstance();
		TDelegateInstanceInterface* OtherInstance = (TDelegateInstanceInterface*)Other.GetDelegateInstance();
	
		// The function these delegates point to must be the same
		if ((DelegateInstance != nullptr) && (OtherInstance != nullptr))
		{
			return DelegateInstance->IsSameFunction(*OtherInstance);
		}
	
		// If neither delegate is initialized to anything yet, then we treat them as equal
		if ((DelegateInstance == nullptr) && (OtherInstance == nullptr))
		{
			return true;
		}
	
		// No match!
		return false;
	}

private:

	// Declare ourselves as a friend to our multi-cast counterpart, so that it can access our interface directly
	template< FUNC_TEMPLATE_DECL_NO_SHADOW > friend class BASE_MULTICAST_DELEGATE_CLASS;
};


#if FUNC_IS_VOID

/**
 * Multicast delegate base class.
 *
 * This class implements the functionality of multicast delegates. It is templated to the function signature
 * that it is compatible with. Use the various DECLARE_MULTICAST_DELEGATE and DECLARE_EVENT macros to create
 * actual delegate types.
 *
 * Multicast delegates offer no guarantees for the calling order of bound functions. As bindings get added
 * and removed over time, the calling order may change. Only bindings without return values are supported.
 */
template< FUNC_TEMPLATE_DECL_TYPENAME >
class BASE_MULTICAST_DELEGATE_CLASS
	: public FMulticastDelegateBase<>
{
public:

	/** DEPRECATED: Type definition for unicast delegate classes whose delegate instances are compatible with this delegate. */
	typedef DELEGATE_CLASS< FUNC_TEMPLATE_ARGS > FDelegate;

	/** Type definition for the shared interface of delegate instance types compatible with this delegate class. */
	typedef DELEGATE_INSTANCE_INTERFACE_CLASS<FUNC_TEMPLATE_ARGS> TDelegateInstanceInterface;

public:

	/**
	 * Adds a delegate instance to this multicast delegate's invocation list.
	 *
	 * This delegate will take over ownership of the given delegate instance.
	 *
	 * @param DelegateInstance The delegate instance to add.
	 */
	FDelegateHandle Add( TDelegateInstanceInterface* DelegateInstance )
	{
		FDelegateHandle Result;

		if (DelegateInstance != nullptr)
		{
			Result = AddDelegateInstance(DelegateInstance);
		}

		return Result;
	}

	/**
	 * DEPRECATED: Adds a unicast delegate to this multi-cast delegate's invocation list.
	 *
	 * This method is retained for backwards compatibility.
	 *
	 * @param Delegate The delegate to add.
	 */
	FDelegateHandle Add( const FDelegate& Delegate )
	{
		FDelegateHandle             Result;
		TDelegateInstanceInterface* DelegateInstance = (TDelegateInstanceInterface*)Delegate.GetDelegateInstance();

		if (DelegateInstance != nullptr)
		{
			Result = AddDelegateInstance(DelegateInstance->CreateCopy());
		}

		return Result;
	}

	/**
	 * Adds a raw C++ pointer global function delegate
	 *
	 * @param	InFunc	Function pointer
	 */
	inline FDelegateHandle AddStatic( typename FDelegate::FStaticDelegate::FFuncPtr InFunc )
	{
		return Add( FDelegate::CreateStatic( InFunc ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddStatic( typename FDelegate::template TStaticDelegate_OneVar< PAYLOAD_TEMPLATE_LIST_OneVar >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddStatic( typename FDelegate::template TStaticDelegate_TwoVars< PAYLOAD_TEMPLATE_LIST_TwoVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddStatic( typename FDelegate::template TStaticDelegate_ThreeVars< PAYLOAD_TEMPLATE_LIST_ThreeVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddStatic( typename FDelegate::template TStaticDelegate_FourVars< PAYLOAD_TEMPLATE_LIST_FourVars >::FFuncPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateStatic( InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Adds a raw C++ pointer delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc ) );
	}
	template< class UserClass >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateRaw( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Adds a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObjectRef	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc ) );
	}
	template< class UserClass >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Adds a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc ) );
	}
	template< class UserClass >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Adds a shared pointer-based (slower, conditionally thread-safe) member function delegate.  Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObjectRef	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc ) );
	}
	template< class UserClass >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Adds a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc ) );
	}
	template< class UserClass >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateThreadSafeSP( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Adds a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunctionName			Class method function address
	 */
	template< class UObjectTemplate >
	inline FDelegateHandle AddUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName )
	{
		return Add( FDelegate::CreateUFunction( InUserObject, InFunctionName ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UObjectTemplate, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateUFunction( InUserObject, InFunctionName, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

	/**
	 * Adds a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc ) );
	}
	template< class UserClass >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_OneVar< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_OneVar >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_OneVar_Const< UserClass, PAYLOAD_TEMPLATE_LIST_OneVar >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_OneVar )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_OneVar ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_TwoVars< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_TwoVars >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_TwoVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_TwoVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_TwoVars )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_TwoVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_ThreeVars< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_ThreeVars >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_ThreeVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_ThreeVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_ThreeVars )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_ThreeVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_FourVars< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}
	template< class UserClass, PAYLOAD_TEMPLATE_DECL_FourVars >
	inline FDelegateHandle AddUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_FourVars_Const< UserClass, PAYLOAD_TEMPLATE_LIST_FourVars >::FMethodPtr InFunc, PAYLOAD_TEMPLATE_ARGS_FourVars )
	{
		return Add( FDelegate::CreateUObject( InUserObject, InFunc, PAYLOAD_TEMPLATE_PASSIN_FourVars ) );
	}

public:

	/**
	 * Removes a delegate instance from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * Note that the order of the delegate instances may not be preserved!
	 *
	 * @param Handle The handle of the delegate instance to remove.
	 */
	void Remove( FDelegateHandle Handle )
	{
		RemoveDelegateInstance(Handle);
	}

	/**
	 * Removes a delegate instance from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * Note that the order of the delegate instances may not be preserved!
	 *
	 * @param DelegateInstance The delegate instance to remove.
	 */
	DELEGATE_DEPRECATED("This Remove overload is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	void Remove( const TDelegateInstanceInterface& DelegateInstance )
	{
		DEPRECATED_RemoveDelegateInstance(&DelegateInstance);
	}

	void DEPRECATED_Remove( const TDelegateInstanceInterface& DelegateInstance )
	{
		// Provided to help implement other deprecated functions without giving multiple warnings.
		// Should not be called directly.

		DEPRECATED_RemoveDelegateInstance(&DelegateInstance);
	}

	/**
	 * DEPRECATED: Removes a unicast delegate from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * The order of the delegates may not be preserved!
	 * This function is retained for backwards compatibility.
	 *
	 * @param Delegate The delegate to remove.
	 */
	DELEGATE_DEPRECATED("This Remove overload is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	void Remove( const FDelegate& Delegate )
	{
		DEPRECATED_Remove(Delegate);
	}

	void DEPRECATED_Remove( const FDelegate& Delegate )
	{
		// Provided to help implement other deprecated functions without giving multiple warnings.
		// Should not be called directly.

		TDelegateInstanceInterface* DelegateInstance = (TDelegateInstanceInterface*)Delegate.GetDelegateInstance();

		if (DelegateInstance != nullptr)
		{
			DEPRECATED_RemoveDelegateInstance(DelegateInstance);
		}		
	}

	// NOTE: These direct Remove methods are only supported for multi-cast delegates with no payload attached.
	// See the comment in the multi-cast delegate Remove() method above for more details.

	/**
	 * Removes a raw C++ pointer global function delegate (performance is O(N)).  Note that the order of the
	 * delegates may not be preserved!
	 *
	 * @param	InFunc	Function pointer
	 */
	DELEGATE_DEPRECATED("RemoveStatic is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveStatic( typename FDelegate::FStaticDelegate::FFuncPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateStatic( InFunc ).GetDelegateInstance() );
	}

	/**
	 * Removes a raw C++ pointer delegate (performance is O(N)).  Note that the order of the delegates may not
	 * be preserved!
	 *
	 * @param	InUserObject	User object to unbind from
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveRaw is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateRaw( InUserObject, InFunc ).GetDelegateInstance() );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveRaw is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveRaw( UserClass* InUserObject, typename FDelegate::template TRawMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateRaw( InUserObject, InFunc ).GetDelegateInstance() );
	}

	/**
	 * Removes a shared pointer-based member function delegate (performance is O(N)).  Note that the order of
	 * the delegates may not be preserved!
	 *
	 * @param	InUserObjectRef	User object to unbind from
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateSP( InUserObjectRef, InFunc ).GetDelegateInstance() );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveSP( const TSharedRef< UserClass, ESPMode::Fast >& InUserObjectRef, typename FDelegate::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateSP( InUserObjectRef, InFunc ).GetDelegateInstance() );
	}

	/**
	 * Removes a shared pointer-based member function delegate (performance is O(N)).  Note that the order of
	 * the delegates may not be preserved!
	 *
	 * @param	InUserObject	User object to unbind from
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateSP( InUserObject, InFunc ).GetDelegateInstance() );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveSP( UserClass* InUserObject, typename FDelegate::template TSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateSP( InUserObject, InFunc ).GetDelegateInstance() );
	}

	/**
	 * Removes a shared pointer-based member function delegate (performance is O(N)).  Note that the order of
	 * the delegates may not be preserved!
	 *
	 * @param	InUserObjectRef	User object to unbind from
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveThreadSafeSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc ).GetDelegateInstance() );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveThreadSafeSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveThreadSafeSP( const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObjectRef, typename FDelegate::template TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateThreadSafeSP( InUserObjectRef, InFunc ).GetDelegateInstance() );
	}

	/**
	 * Removes a shared pointer-based member function delegate (performance is O(N)).  Note that the order of
	 * the delegates may not be preserved!
	 *
	 * @param	InUserObject	User object to unbind from
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveThreadSafeSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateThreadSafeSP( InUserObject, InFunc ).GetDelegateInstance() );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveThreadSafeSP is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveThreadSafeSP( UserClass* InUserObject, typename FDelegate::template TThreadSafeSPMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateThreadSafeSP( InUserObject, InFunc ).GetDelegateInstance() );
	}

	/**
	 * Removes a UFunction-based member function delegate (performance is O(N)).  Note that the order of the
	 * delegates may not be preserved!
	 *
	 * @param	InUserObject	User object to unbind from
	 * @param	InFunc			Class method function address
	 */
	template< class UObjectTemplate >
	DELEGATE_DEPRECATED("RemoveUFunction is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveUFunction( UObjectTemplate* InUserObject, const FName& InFunctionName )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateUFunction( InUserObject, InFunctionName ).GetDelegateInstance() );
	}

	/**
	 * Removes a UObject-based member function delegate (performance is O(N)).  Note that the order of the
	 * delegates may not be preserved!
	 *
	 * @param	InUserObject	User object to unbind from
	 * @param	InFunc			Class method function address
	 */
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveUObject is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateUObject( InUserObject, InFunc ).GetDelegateInstance() );
	}
	template< class UserClass >
	DELEGATE_DEPRECATED("RemoveUObject is deprecated - please remove delegates using the FDelegateHandle returned by the Add function.")
	inline void RemoveUObject( UserClass* InUserObject, typename FDelegate::template TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc )
	{
		DEPRECATED_RemoveDelegateInstance( (TDelegateInstanceInterface*)FDelegate::CreateUObject( InUserObject, InFunc ).GetDelegateInstance() );
	}

protected:

	/** 
	 * Hidden default constructor.
	 */
	inline BASE_MULTICAST_DELEGATE_CLASS( ) { }

	/**
	 * Hidden copy constructor (for proper deep copies).
	 *
	 * @param Other The multicast delegate to copy from.
	 */
	BASE_MULTICAST_DELEGATE_CLASS( const BASE_MULTICAST_DELEGATE_CLASS& Other )
	{
		*this = Other;
	}

	/**
	 * Hidden assignment operator (for proper deep copies).
	 *
	 * @param Other The delegate to assign from.
	 * @return This instance.
	 */
	BASE_MULTICAST_DELEGATE_CLASS& operator=( const BASE_MULTICAST_DELEGATE_CLASS& Other )
	{
		if (&Other != this)
		{
			Clear();

			for (IDelegateInstance* DelegateInstance : Other.GetInvocationList())
			{
				// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
				TDelegateInstanceInterface* OtherInstance = (TDelegateInstanceInterface*)DelegateInstance;

				if (OtherInstance != nullptr)
				{
					AddInternal(OtherInstance->CreateCopy());
				}			
			}
		}

		return *this;
	}

protected:

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list.
	 *
	 * This method will assert if the same function has already been bound.
	 *
	 * @param	InDelegate	Delegate to add
	 */
	FDelegateHandle AddDelegateInstance( TDelegateInstanceInterface* InDelegateInstance )
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		// verify that the same function isn't already bound
		for (IDelegateInstance* DelegateInstance : GetInvocationList())
		{
			if (DelegateInstance != nullptr)
			{
				// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
				TDelegateInstanceInterface* DelegateInstanceInterface = (TDelegateInstanceInterface*)DelegateInstance;
				check(!DelegateInstanceInterface->IsSameFunction(*InDelegateInstance));
			}
		}
#endif

		return AddInternal(InDelegateInstance);
	}

	/**
	 * Broadcasts this delegate to all bound objects, except to those that may have expired.
	 *
	 * The constness of this method is a lie, but it allows for broadcasting from const functions.
	 */
	void Broadcast( FUNC_PARAM_LIST ) const
	{
		bool NeedsCompaction = false;

		LockInvocationList();
		{
			const TArray<IDelegateInstance*>& InvocationList = GetInvocationList();

			// call bound functions in reverse order, so we ignore any instances that may be added by callees
			for (int32 InvocationListIndex = InvocationList.Num() - 1; InvocationListIndex >= 0; --InvocationListIndex)
			{
				// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
				TDelegateInstanceInterface* DelegateInstanceInterface = (TDelegateInstanceInterface*)InvocationList[InvocationListIndex];

				if ((DelegateInstanceInterface == nullptr) || !DelegateInstanceInterface->ExecuteIfSafe(FUNC_PARAM_PASSTHRU))
				{
					NeedsCompaction = true;
				}
			}
		}
		UnlockInvocationList();

		if (NeedsCompaction)
		{
			const_cast<BASE_MULTICAST_DELEGATE_CLASS*>(this)->CompactInvocationList();
		}
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * The function is not actually removed, but deleted and marked as removed.
	 * It will be removed next time the invocation list is compacted within Broadcast().
	 *
	 * @param Handle The handle of the delegate instance to remove.
	 */
	void RemoveDelegateInstance( FDelegateHandle Handle )
	{
		const TArray<IDelegateInstance*>& InvocationList = GetInvocationList();

		// NOTE: We assume that this method is never called with a nullptr object, in which case the
		//       the following algorithm would break down (it would remove the first found instance
		//       of a matching function binding, which is not necessarily the instance we wish to remove).

		for (int32 InvocationListIndex = 0; InvocationListIndex < InvocationList.Num(); ++InvocationListIndex)
		{
			// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
			TDelegateInstanceInterface*& DelegateInstanceRef = (TDelegateInstanceInterface*&)InvocationList[InvocationListIndex];

			// NOTE: We must do a deep compare here, not just compare delegate pointers, because multiple
			//       delegate pointers can refer to the exact same object and method
			if ((DelegateInstanceRef != nullptr) && DelegateInstanceRef->GetHandle() == Handle)
			{
				delete DelegateInstanceRef;
				DelegateInstanceRef = nullptr;

				break;	// no need to continue, as we never allow the same delegate to be bound twice
			}
		}

		const_cast<BASE_MULTICAST_DELEGATE_CLASS*>(this)->CompactInvocationList();
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * The function is not actually removed, but deleted and marked as removed.
	 * It will be removed next time the invocation list is compacted within Broadcast().
	 *
	 * @param InDelegateInstance The delegate instance to remove.
	 */
	void DEPRECATED_RemoveDelegateInstance( const TDelegateInstanceInterface* InDelegateInstance )
	{
		const TArray<IDelegateInstance*>& InvocationList = GetInvocationList();

		// NOTE: We assume that this method is never called with a nullptr object, in which case the
		//       the following algorithm would break down (it would remove the first found instance
		//       of a matching function binding, which is not necessarily the instance we wish to remove).

		for (int32 InvocationListIndex = 0; InvocationListIndex < InvocationList.Num(); ++InvocationListIndex)
		{
			// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
			TDelegateInstanceInterface*& DelegateInstanceRef = (TDelegateInstanceInterface*&)InvocationList[InvocationListIndex];

			// NOTE: We must do a deep compare here, not just compare delegate pointers, because multiple
			//       delegate pointers can refer to the exact same object and method
			if ((DelegateInstanceRef != nullptr) && DelegateInstanceRef->IsSameFunction(*InDelegateInstance))
			{
				delete DelegateInstanceRef;
				DelegateInstanceRef = nullptr;
				
				break;	// no need to continue, as we never allow the same delegate to be bound twice
			}
		}

		const_cast<BASE_MULTICAST_DELEGATE_CLASS*>(this)->CompactInvocationList();
	}
};


/**
 * Implements a multicast delegate.
 *
 * This class should not be instantiated directly. Use the DECLARE_MULTICAST_DELEGATE macros instead.
 */
template< FUNC_TEMPLATE_DECL_TYPENAME >
class MULTICAST_DELEGATE_CLASS : public BASE_MULTICAST_DELEGATE_CLASS< FUNC_TEMPLATE_DECL >
{
public:

	/**
	 * Default constructor.
	 */
	MULTICAST_DELEGATE_CLASS( ) { }

	/**
	 * Copy constructor (for proper deep copies).
	 *
	 * @param Other The multicast delegate to copy from.
	 */
	MULTICAST_DELEGATE_CLASS( const MULTICAST_DELEGATE_CLASS& Other )
		: Super(Other)
	{ }

	/**
	 * Assignment operator (for proper deep copies).
	 *
	 * @param Other The delegate to assign from.
	 * @return This instance.
	 */
	MULTICAST_DELEGATE_CLASS& operator=( const MULTICAST_DELEGATE_CLASS& Other )
	{
		Super::operator=(Other);
		return *this;
	}

public:

	/**
	 * Checks to see if any functions are bound to this multi-cast delegate
	 *
	 * @return	True if any functions are bound
	 */
	inline bool IsBound() const
	{
		return Super::IsBound();
	}

	/**
	 * Removes all functions from this delegate's invocation list
	 */
	inline void Clear()
	{
		return Super::Clear();
	}

	/**
	 * Broadcasts this delegate to all bound objects, except to those that may have expired
	 */
	inline void Broadcast( FUNC_PARAM_LIST ) const
	{
		return Super::Broadcast( FUNC_PARAM_PASSTHRU );
	}

private: 
	
	// Prevents erroneous use by other classes.
	typedef BASE_MULTICAST_DELEGATE_CLASS< FUNC_TEMPLATE_DECL > Super;
};

#endif		// FUNC_IS_VOID


/**
 * Dynamic delegate base object (UObject-based, serializable).  You'll use the various DECLARE_DYNAMIC_DELEGATE
 * macros to create the actual delegate type, templated to the function signature the delegate is compatible with.
 * Then, you can create an instance of that class when you want to assign functions to the delegate.
 */
template< FUNC_TEMPLATE_DECL_TYPENAME, typename TWeakPtr = FWeakObjectPtr >
class DYNAMIC_DELEGATE_CLASS
	: public TScriptDelegate<TWeakPtr>
{
public:

	/**
	 * Default constructor
	 */
	DYNAMIC_DELEGATE_CLASS() { }

	/**
	 * Construction from an FScriptDelegate must be explicit.  This is really only used by UObject system internals.
	 *
	 * @param	InScriptDelegate	The delegate to construct from by copying
	 */
	explicit DYNAMIC_DELEGATE_CLASS( const TScriptDelegate<TWeakPtr>& InScriptDelegate )
		: TScriptDelegate<TWeakPtr>( InScriptDelegate )
	{ }

	/**
	 * Templated helper class to define a typedef for user's method pointer, then used below
	 */
	template< class UserClass >
	class TMethodPtrResolver
	{
	public:

		typedef RetValType ( UserClass::*FMethodPtr )( FUNC_PARAM_LIST );
	};

	/**
	 * Binds a UObject instance and a UObject method address to this delegate.
	 *
	 * @param	InUserObject			UObject instance
	 * @param	InMethodPtr				Member function address pointer
	 * @param	InMacroFunctionName		Name of member function, including class name (generated by a macro)
	 *
	 * NOTE:  Do not call this function directly.  Instead, call BindDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_BindDynamic( UserClass* InUserObject, typename TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, const TCHAR* InMacroFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr && InMacroFunctionName[0] );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		// NOTE: If you hit a compile error on the following line, it means you're trying to use a non-UObject type
		//       with this delegate, which is not supported
		this->Object = InUserObject;

		// Store the function name.  The incoming function name was generated by a macro and includes the method's class name.
		this->FunctionName = UE4Delegates_Private::GetTrimmedMemberFunctionName( InMacroFunctionName );

		ensureMsgf(this->IsBound(), TEXT("Unable to bind delegate to '%s' (function might not be marked as a UFUNCTION)"), InMacroFunctionName);
	}

	friend uint32 GetTypeHash(const DYNAMIC_DELEGATE_CLASS& Key)
	{
		return FCrc::MemCrc_DEPRECATED(&Key,sizeof(Key));
	}

	// NOTE:  Execute() method must be defined in derived classes

	// NOTE:  ExecuteIfBound() method must be defined in derived classes
};


/**
 * Dynamic multi-cast delegate base object (UObject-based, serializable).  You'll use the various
 * DECLARE_DYNAMIC_MULTICAST_DELEGATE macros to create the actual delegate type, templated to the function
 * signature the delegate is compatible with.   Then, you can create an instance of that class when you
 * want to assign functions to the delegate.
 */
template< FUNC_TEMPLATE_DECL_TYPENAME, typename TWeakPtr = FWeakObjectPtr >
class DYNAMIC_MULTICAST_DELEGATE_CLASS
	: public TMulticastScriptDelegate<TWeakPtr>
{
public:

	/** The actual single-cast delegate class for this multi-cast delegate */
	typedef DYNAMIC_DELEGATE_CLASS< FUNC_TEMPLATE_ARGS > FDelegate;

	/**
	 * Default constructor
	 */
	DYNAMIC_MULTICAST_DELEGATE_CLASS() { }

	/**
	 * Construction from an FMulticastScriptDelegate must be explicit.  This is really only used by UObject system internals.
	 *
	 * @param	InScriptDelegate	The delegate to construct from by copying
	 */
	explicit DYNAMIC_MULTICAST_DELEGATE_CLASS( const TMulticastScriptDelegate<TWeakPtr>& InMulticastScriptDelegate )
		: TMulticastScriptDelegate<TWeakPtr>( InMulticastScriptDelegate )
	{ }

	/**
	 * Tests if a UObject instance and a UObject method address pair are already bound to this multi-cast delegate.
	 *
	 * @param	InUserObject			UObject instance
	 * @param	InMethodPtr				Member function address pointer
	 * @param	InMacroFunctionName		Name of member function, including class name (generated by a macro)
	 * @return	True if the instance/method is already bound.
	 *
	 * NOTE:  Do not call this function directly.  Instead, call IsAlreadyBound() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	bool __Internal_IsAlreadyBound( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, const TCHAR* InMacroFunctionName ) const
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr && InMacroFunctionName[0] );

		// NOTE: We're not actually using the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FName TrimmedName = UE4Delegates_Private::GetTrimmedMemberFunctionName( InMacroFunctionName );
		return this->Contains( InUserObject, TrimmedName );
	}

	/**
	 * Binds a UObject instance and a UObject method address to this multi-cast delegate.
	 *
	 * @param	InUserObject			UObject instance
	 * @param	InMethodPtr				Member function address pointer
	 * @param	InMacroFunctionName		Name of member function, including class name (generated by a macro)
	 *
	 * NOTE:  Do not call this function directly.  Instead, call AddDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_AddDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, const TCHAR* InMacroFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr && InMacroFunctionName[0] );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FDelegate NewDelegate;
		NewDelegate.__Internal_BindDynamic( InUserObject, InMethodPtr, InMacroFunctionName );

		this->Add( NewDelegate );
	}

	/**
	 * Binds a UObject instance and a UObject method address to this multi-cast delegate, but only if it hasn't been bound before.
	 *
	 * @param	InUserObject			UObject instance
	 * @param	InMethodPtr				Member function address pointer
	 * @param	InMacroFunctionName		Name of member function, including class name (generated by a macro)
	 *
	 * NOTE:  Do not call this function directly.  Instead, call AddUniqueDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_AddUniqueDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, const TCHAR* InMacroFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr && InMacroFunctionName[0] );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FDelegate NewDelegate;
		NewDelegate.__Internal_BindDynamic( InUserObject, InMethodPtr, InMacroFunctionName );

		this->AddUnique( NewDelegate );
	}

	/**
	 * Unbinds a UObject instance and a UObject method address from this multi-cast delegate.
	 *
	 * @param	InUserObject			UObject instance
	 * @param	InMethodPtr				Member function address pointer
	 * @param	InMacroFunctionName		Name of member function, including class name (generated by a macro)
	 *
	 * NOTE:  Do not call this function directly.  Instead, call RemoveDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_RemoveDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, const TCHAR* InMacroFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr && InMacroFunctionName[0] );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FName TrimmedName = UE4Delegates_Private::GetTrimmedMemberFunctionName( InMacroFunctionName );
		this->Remove( InUserObject, TrimmedName );
	}

	// NOTE:  Broadcast() method must be defined in derived classes
};


#undef PAYLOAD_TEMPLATE_DECL_OneVar
#undef PAYLOAD_TEMPLATE_LIST_OneVar
#undef PAYLOAD_TEMPLATE_ARGS_OneVar
#undef PAYLOAD_TEMPLATE_PASSIN_OneVar

#undef PAYLOAD_TEMPLATE_DECL_TwoVars
#undef PAYLOAD_TEMPLATE_LIST_TwoVars
#undef PAYLOAD_TEMPLATE_ARGS_TwoVars
#undef PAYLOAD_TEMPLATE_PASSIN_TwoVars

#undef PAYLOAD_TEMPLATE_DECL_ThreeVars
#undef PAYLOAD_TEMPLATE_LIST_ThreeVars
#undef PAYLOAD_TEMPLATE_ARGS_ThreeVars
#undef PAYLOAD_TEMPLATE_PASSIN_ThreeVars

#undef PAYLOAD_TEMPLATE_DECL_FourVars
#undef PAYLOAD_TEMPLATE_LIST_FourVars
#undef PAYLOAD_TEMPLATE_ARGS_FourVars
#undef PAYLOAD_TEMPLATE_PASSIN_FourVars

#undef DYNAMIC_MULTICAST_DELEGATE_CLASS
#undef DYNAMIC_DELEGATE_CLASS
#undef EVENT_CLASS
#undef MULTICAST_DELEGATE_CLASS
#undef DELEGATE_CLASS

#undef SP_METHOD_DELEGATE_INSTANCE_CLASS_NoVars
#undef SP_METHOD_DELEGATE_INSTANCE_CLASS_OneVar
#undef SP_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars
#undef SP_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars
#undef SP_METHOD_DELEGATE_INSTANCE_CLASS_FourVars

#undef RAW_METHOD_DELEGATE_INSTANCE_CLASS_NoVars
#undef RAW_METHOD_DELEGATE_INSTANCE_CLASS_OneVar
#undef RAW_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars
#undef RAW_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars
#undef RAW_METHOD_DELEGATE_INSTANCE_CLASS_FourVars

#undef UFUNCTION_DELEGATE_INSTANCE_CLASS_NoVars
#undef UFUNCTION_DELEGATE_INSTANCE_CLASS_OneVar
#undef UFUNCTION_DELEGATE_INSTANCE_CLASS_TwoVars
#undef UFUNCTION_DELEGATE_INSTANCE_CLASS_ThreeVars
#undef UFUNCTION_DELEGATE_INSTANCE_CLASS_FourVars

#undef UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_NoVars
#undef UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_OneVar
#undef UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_TwoVars
#undef UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_ThreeVars
#undef UOBJECT_METHOD_DELEGATE_INSTANCE_CLASS_FourVars

#undef STATIC_DELEGATE_INSTANCE_CLASS_NoVars
#undef STATIC_DELEGATE_INSTANCE_CLASS_OneVar
#undef STATIC_DELEGATE_INSTANCE_CLASS_TwoVars
#undef STATIC_DELEGATE_INSTANCE_CLASS_ThreeVars
#undef STATIC_DELEGATE_INSTANCE_CLASS_FourVars

#undef DELEGATE_INSTANCE_INTERFACE_CLASS
