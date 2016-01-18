// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3
#	if PLATFORM_WINDOWS
#		include "AllowWindowsPlatformTypes.h"
#	endif
#	pragma push_macro("OVERRIDE")
#		undef OVERRIDE // cef headers provide their own OVERRIDE macro
#		include "include/cef_jsdialog_handler.h"
#	pragma pop_macro("OVERRIDE")
#	if PLATFORM_WINDOWS
#		include "HideWindowsPlatformTypes.h"
#	endif
#endif

#include "IWebBrowserDialog.h"

class FWebBrowserWindow;

class FWebBrowserDialog
	: public IWebBrowserDialog
{
public:
	virtual ~FWebBrowserDialog()
	{}

	// IWebBrowserDialog interface:

	virtual EWebBrowserDialogType GetType() override
	{
		return Type;
	}

	virtual const FText& GetMessageText() override
	{
		return MessageText;
	}

	virtual const FText& GetDefaultPrompt() override
	{
		return DefaultPrompt;
	}

	virtual bool IsReload() override
	{
		check(Type == EWebBrowserDialogType::Unload);
		return bIsReload;
	}

	virtual void Continue(bool Success = true, const FText& UserResponse = FText::GetEmpty()) override
	{
#if WITH_CEF3
		check(Type == EWebBrowserDialogType::Prompt || UserResponse.IsEmpty());
		Callback->Continue(Success, *UserResponse.ToString());
#endif
	}

private:


	EWebBrowserDialogType Type;
	FText MessageText;
	FText DefaultPrompt;
	bool bIsReload;
#if WITH_CEF3
	CefRefPtr<CefJSDialogCallback> Callback;

	// Create a dialog from OnJSDialog arguments
	FWebBrowserDialog(CefJSDialogHandler::JSDialogType InDialogType, const CefString& InMessageText, const CefString& InDefaultPrompt, CefRefPtr<CefJSDialogCallback> InCallback)
		: Type((EWebBrowserDialogType)InDialogType)
		, MessageText(FText::FromString(InMessageText.ToWString().c_str()))
		, DefaultPrompt(FText::FromString(InDefaultPrompt.ToWString().c_str()))
		, bIsReload(false)
		, Callback(InCallback)
	{}

	// Create a dialog from OnBeforeUnloadDialog arguments
	FWebBrowserDialog(const CefString& InMessageText, bool InIsReload, CefRefPtr<CefJSDialogCallback> InCallback)
		: Type(EWebBrowserDialogType::Unload)
		, MessageText(FText::FromString(InMessageText.ToWString().c_str()))
		, DefaultPrompt(FText::GetEmpty())
		, bIsReload(InIsReload)
		, Callback(InCallback)
	{};

	friend class FWebBrowserWindow;
#endif
};
