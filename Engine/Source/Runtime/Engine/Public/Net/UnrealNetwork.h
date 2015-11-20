// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealNetwork.h: Unreal networking.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

#pragma once

class	UChannel;
class	UControlChannel;
class	UActorChannel;
class	UVoiceChannel;
class	UFileChannel;
class	FInBunch;
class	FOutBunch;
class	UChannelIterator;

/*class	UNetDriver;*/
class	UNetConnection;
class	UPendingNetGame;

class	FNetworkReplayVersion;

/*-----------------------------------------------------------------------------
	Types.
-----------------------------------------------------------------------------*/

// Return the value of Max/2 <= Value-Reference+some_integer*Max < Max/2.
inline int32 BestSignedDifference( int32 Value, int32 Reference, int32 Max )
{
	return ((Value-Reference+Max/2) & (Max-1)) - Max/2;
}
inline int32 MakeRelative( int32 Value, int32 Reference, int32 Max )
{
	return Reference + BestSignedDifference(Value,Reference,Max);
}

struct ENGINE_API FNetworkVersion
{
	/** Called in GetLocalNetworkVersion if bound */
	DECLARE_DELEGATE_RetVal( uint32, FGetLocalNetworkVersionOverride );
	static FGetLocalNetworkVersionOverride GetLocalNetworkVersionOverride;

	/** Called in IsNetworkCompatible if bound */
	DECLARE_DELEGATE_RetVal_TwoParams( bool, FIsNetworkCompatibleOverride, uint32, uint32 );
	static FIsNetworkCompatibleOverride IsNetworkCompatibleOverride;

	/**
	 * Generates a version number, that by default, is based on a checksum of the engine version + project name + project version string
	 * Game/project code can completely override what this value returns through the GetLocalNetworkVersionOverride delegate
	 * If called with AllowOverrideDelegate=false, we will not call the game project override. (This allows projects to call base implementation in their project implementation)
	 */
	static uint32 GetLocalNetworkVersion(bool AllowOverrideDelegate=true);

	/**
	 * Determine if a connection is compatible with this instance
	 *
	 * @param bRequireEngineVersionMatch should the engine versions match exactly
	 * @param LocalNetworkVersion current version of the local machine
	 * @param RemoteNetworkVersion current version of the remote machine
	 *
	 * @return true if the two instances can communicate, false otherwise
	 */
	static bool IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion );

	/**
	 * Generates a spexial struct that contains information to send to replay server
	 */
	static FNetworkReplayVersion GetReplayVersion();

	/**
	 * Internal network protocol version.
	 * By default, this value is incorporated into the result of GetLocalNetworkVersion().
	 * This value should be incremented manually along with any changes to the
	 * internal network protocol, such as the bunch header format or
	 * the RPC header format.
	 */
	static const uint32 InternalProtocolVersion;
};

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/

/** wrapper to find replicated properties that also makes sure they're valid */
static UProperty* GetReplicatedProperty(UClass* CallingClass, UClass* PropClass, const FName& PropName)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!CallingClass->IsChildOf(PropClass))
	{
		UE_LOG(LogNet, Fatal,TEXT("Attempt to replicate property '%s.%s' in C++ but class '%s' is not a child of '%s'"), *PropClass->GetName(), *PropName.ToString(), *CallingClass->GetName(), *PropClass->GetName());
	}
#endif
	UProperty* TheProperty = FindFieldChecked<UProperty>(PropClass, PropName);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!(TheProperty->PropertyFlags & CPF_Net))
	{
		UE_LOG(LogNet, Fatal,TEXT("Attempt to replicate property '%s' that was not tagged to replicate! Please use 'Replicated' or 'ReplicatedUsing' keyword in the UPROPERTY() declaration."), *TheProperty->GetFullName());
	}
#endif
	return TheProperty;
}
	
#define DOREPLIFETIME(c,v) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )							\
	{																		\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i ) );	\
	}																		\
}

#define DOREPLIFETIME_CONDITION(c,v,cond) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )										\
	{																					\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i, cond ) );	\
	}																					\
}

/** Allows gamecode to specify RepNotify condition: REPNOTIFY_OnChanged (default) or REPNOTIFY_Always for when repnotify function is called  */
#define DOREPLIFETIME_CONDITION_NOTIFY(c,v,cond, rncond) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )										\
	{																					\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i, cond, rncond) );	\
	}																					\
}

#define DOREPLIFETIME_ACTIVE_OVERRIDE(c,v,active)	\
{													\
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )											\
	{																						\
		ChangedPropertyTracker.SetCustomIsActiveOverride( sp##v->RepIndex + i, active );	\
	}																						\
}

/*-----------------------------------------------------------------------------
	RPC Parameter Validation Helpers
-----------------------------------------------------------------------------*/

// This macro is for RPC parameter validation.
// It handles the details of what should happen if a validation expression fails
#define RPC_VALIDATE( expression )						\
	if ( !( expression ) )								\
	{													\
		UE_LOG( LogNet, Warning,						\
		TEXT("RPC_VALIDATE Failed: ")					\
		TEXT( PREPROCESSOR_TO_STRING( expression ) )	\
		TEXT(" File: ")									\
		TEXT( PREPROCESSOR_TO_STRING( __FILE__ ) )		\
		TEXT(" Line: ")									\
		TEXT( PREPROCESSOR_TO_STRING( __LINE__ ) ) );	\
		return false;									\
	}
