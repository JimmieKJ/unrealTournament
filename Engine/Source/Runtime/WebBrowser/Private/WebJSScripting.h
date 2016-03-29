// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreUObject.h"
#include "WebJSFunction.h"

#if WITH_CEF3
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/cef_client.h"
#include "include/cef_values.h"
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

/**
 * Implements handling of bridging UObjects client side with JavaScript renderer side.
 */
class FWebJSScripting
	: public FGCObject
	, public TSharedFromThis<FWebJSScripting>
{
public:
	FWebJSScripting(CefRefPtr<CefBrowser> Browser)
		: BaseGuid(FGuid::NewGuid())
		, InternalCefBrowser(Browser)
	{}

	void UnbindCefBrowser();

	void BindUObject(const FString& Name, UObject* Object, bool bIsPermanent = true);
	void UnbindUObject(const FString& Name, UObject* Object = nullptr, bool bIsPermanent = true);

	/**
	 * Called when a message was received from the renderer process.
	 *
	 * @param Browser The CefBrowser for this window.
	 * @param SourceProcess The process id of the sender of the message. Currently always PID_RENDERER.
	 * @param Message The actual message.
	 * @return true if the message was handled, else false.
	 */
	bool OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser, CefProcessId SourceProcess, CefRefPtr<CefProcessMessage> Message);

	/**
	 * Sends a message to the renderer process. 
	 * See https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-inter-process-communication-ipc for more information.
	 *
	 * @param Message the message to send to the renderer process
	 */
	void SendProcessMessage(CefRefPtr<CefProcessMessage> Message);

	CefRefPtr<CefDictionaryValue> ConvertStruct(UStruct* TypeInfo, const void* StructPtr);
	CefRefPtr<CefDictionaryValue> ConvertObject(UObject* Object);
	CefRefPtr<CefDictionaryValue> ConvertObject(const FGuid& Key);

	// Works for CefListValue and CefDictionaryValues
	template<typename ContainerType, typename KeyType>
	bool SetConverted(CefRefPtr<ContainerType> Container, KeyType Key, FWebJSParam& Param)
	{
		switch (Param.Tag)
		{
			case FWebJSParam::PTYPE_NULL:
				return Container->SetNull(Key);
			case FWebJSParam::PTYPE_BOOL:
				return Container->SetBool(Key, Param.BoolValue);
			case FWebJSParam::PTYPE_DOUBLE:
				return Container->SetDouble(Key, Param.DoubleValue);
			case FWebJSParam::PTYPE_INT:
				return Container->SetInt(Key, Param.IntValue);
			case FWebJSParam::PTYPE_STRING:
			{
				CefString ConvertedString = **Param.StringValue;
				return Container->SetString(Key, ConvertedString);
			}
			case FWebJSParam::PTYPE_OBJECT:
			{
				if (Param.ObjectValue == nullptr)
				{
					return Container->SetNull(Key);
				}
				else
				{
					CefRefPtr<CefDictionaryValue> ConvertedObject = ConvertObject(Param.ObjectValue);
					return Container->SetDictionary(Key, ConvertedObject);
				}
			}
			case FWebJSParam::PTYPE_STRUCT:
			{
				CefRefPtr<CefDictionaryValue> ConvertedStruct = ConvertStruct(Param.StructValue->GetTypeInfo(), Param.StructValue->GetData());
				return Container->SetDictionary(Key, ConvertedStruct);
			}
			case FWebJSParam::PTYPE_ARRAY:
			{
				CefRefPtr<CefListValue> ConvertedArray = CefListValue::Create();
				for(int i=0; i < Param.ArrayValue->Num(); ++i)
				{
					SetConverted(ConvertedArray, i, (*Param.ArrayValue)[i]);
				}
				return Container->SetList(Key, ConvertedArray);
			}
			case FWebJSParam::PTYPE_MAP:
			{
				CefRefPtr<CefDictionaryValue> ConvertedMap = CefDictionaryValue::Create();
				for(auto& Pair : *Param.MapValue)
				{
					SetConverted(ConvertedMap, *Pair.Key, Pair.Value);
				}
				return Container->SetDictionary(Key, ConvertedMap);
			}
			default:
				return false;
		}
	}

	CefRefPtr<CefDictionaryValue> GetPermanentBindings();

	void InvokeJSFunction(FGuid FunctionId, int32 ArgCount, FWebJSParam Arguments[], bool bIsError=false);
	void InvokeJSFunction(FGuid FunctionId, const CefRefPtr<CefListValue>& FunctionArguments, bool bIsError=false);
	void InvokeJSErrorResult(FGuid FunctionId, const FString& Error);

public:

	// FGCObject API
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

private:
	bool ConvertStructArgImpl(uint8* Args, UProperty* Param, CefRefPtr<CefListValue> List, int32 Index);

	bool IsValid()
	{
		return InternalCefBrowser.get() != nullptr;
	}

	// Creates a reversible memory addres -> psuedo-guid mapping.
	// This is done by xoring the address with the first 64 bits of a base guid owned by the instance.
	// Used to identify UObjects from the render process withough exposing internal pointers.
	FGuid PtrToGuid(UObject* Ptr)
	{
		FGuid Guid = BaseGuid;
		if (Ptr == nullptr)
		{
			Guid.Invalidate();
		}
		else
		{
			UPTRINT IntPtr = reinterpret_cast<UPTRINT>(Ptr);
			if (sizeof(UPTRINT) > 4)
			{
				Guid[0] ^= (static_cast<uint64>(IntPtr) >> 32);
			}
			Guid[1] ^= IntPtr & 0xFFFFFFFF;
		}
		return Guid;
	}

	// In addition to reversing the mapping, it verifies that we are currently holding on to an instance of that UObject
	UObject* GuidToPtr(const FGuid& Guid)
	{
		UPTRINT IntPtr = 0;
		if (sizeof(UPTRINT) > 4)
		{
			IntPtr = static_cast<UPTRINT>(static_cast<uint64>(Guid[0] ^ BaseGuid[0]) << 32);
		}
		IntPtr |= (Guid[1] ^ BaseGuid[1]) & 0xFFFFFFFF;

		UObject* Result = reinterpret_cast<UObject*>(IntPtr);
		if (BoundObjects.Contains(Result))
		{
			return Result;
		}
		else
		{
			return nullptr;
		}
	}

	void RetainBinding(UObject* Object)
	{
		if (BoundObjects.Contains(Object))
		{
			if(!BoundObjects[Object].bIsPermanent)
			{
				BoundObjects[Object].Refcount++;
			}
		}
		else
		{
			BoundObjects.Add(Object, {false, 1});
		}
	}

	void ReleaseBinding(UObject* Object)
	{
		if (BoundObjects.Contains(Object))
		{
			auto& Binding = BoundObjects[Object];
			if(!Binding.bIsPermanent)
			{
				Binding.Refcount--;
				if (Binding.Refcount <= 0)
				{
					BoundObjects.Remove(Object);
				}
			}
		}
	}

	/** Message handling helpers */

	bool HandleExecuteUObjectMethodMessage(CefRefPtr<CefListValue> MessageArguments);
	bool HandleReleaseUObjectMessage(CefRefPtr<CefListValue> MessageArguments);

private:

	struct ObjectBinding
	{
		bool bIsPermanent;
		int32 Refcount;
	};

	/** Private data */
	FGuid BaseGuid;

	/** Pointer to the CEF Browser for this window. */
	CefRefPtr<CefBrowser> InternalCefBrowser;

	/** UObjects currently visible on the renderer side. */
	TMap<UObject*, ObjectBinding> BoundObjects;

	/** Reverse lookup for permanent bindings */
	TMap<FString, UObject*> PermanentUObjectsByName;

};

#endif