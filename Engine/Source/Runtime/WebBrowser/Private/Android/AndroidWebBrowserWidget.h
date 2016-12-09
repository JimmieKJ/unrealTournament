// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AndroidWebBrowserWindow.h"
#include "AndroidWebBrowserDialog.h"
#include "AndroidJava.h"
#include <jni.h>

class SAndroidWebBrowserWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SAndroidWebBrowserWidget)
		: _InitialURL("about:blank")
	{ }

		SLATE_ARGUMENT(FString, InitialURL);
		SLATE_ARGUMENT(TSharedPtr<FAndroidWebBrowserWindow>, WebBrowserWindow);
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& Args);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float) const override;

	void ExecuteJavascript(const FString& Script);
	void LoadURL(const FString& NewURL);
	void LoadString(const FString& Content, const FString& BaseUrl);

	void StopLoad();
	void Reload();
	void Close();
	void GoBack();
	void GoForward();
	bool CanGoBack();
	bool CanGoForward();

	// WebViewClient callbacks

	jbyteArray HandleShouldInterceptRequest(jstring JUrl);

	bool HandleShouldOverrideUrlLoading(jstring JUrl);
	bool HandleJsDialog(EWebBrowserDialogType Type, jstring JUrl, jstring MessageText, jobject ResultCallback)
	{
		TSharedPtr<IWebBrowserDialog> Dialog(new FWebBrowserDialog(Type, MessageText, ResultCallback));
		return HandleJsDialog(Dialog);
	}

	bool HandleJsPrompt(jstring JUrl, jstring MessageText, jstring DefaultPrompt, jobject ResultCallback)
	{
		TSharedPtr<IWebBrowserDialog> Dialog(new FWebBrowserDialog(MessageText, DefaultPrompt, ResultCallback));
		return HandleJsDialog(Dialog);
	}

	void HandleReceivedTitle(jstring JTitle);
	void HandlePageLoad(jstring JUrl, bool bIsLoading, int InHistorySize, int InHistoryPosition);
	void HandleReceivedError(jint ErrorCode, jstring JMessage, jstring JUrl);

	// Helper to get the native widget pointer from a native WebViewControl.
	// Jobj can either be a WebViewControl instance or a WebViewControl.Client instance
	static SAndroidWebBrowserWidget* GetWidgetPtr(JNIEnv* JEnv, jobject Jobj)
	{
		jclass Class = JEnv->GetObjectClass(Jobj);
		jfieldID Fid = JEnv->GetFieldID(Class, "this$0", "Lcom/epicgames/ue4/WebViewControl;");
		if (Fid != 0)
		{
			Jobj = JEnv->GetObjectField(Jobj, Fid);
			Class = JEnv->GetObjectClass(Jobj);
		}
		Fid = JEnv->GetFieldID(Class, "nativePtr", "J");
		check(Fid != 0);
		return reinterpret_cast<SAndroidWebBrowserWidget*>(JEnv->GetLongField(Jobj,Fid));
	}

protected:
	bool HandleJsDialog(TSharedPtr<IWebBrowserDialog>& Dialog);
	int HistorySize;
	int HistoryPosition;

	// mutable to allow calling JWebView_Update from inside OnPaint (which is const)
	mutable TOptional<FJavaClassObject> JWebView;
	TOptional<FJavaClassMethod> JWebView_Update;
	TOptional<FJavaClassMethod> JWebView_ExecuteJavascript;
	TOptional<FJavaClassMethod> JWebView_LoadURL;
	TOptional<FJavaClassMethod> JWebView_LoadString;
	TOptional<FJavaClassMethod> JWebView_StopLoad;
	TOptional<FJavaClassMethod> JWebView_Reload;
	TOptional<FJavaClassMethod> JWebView_Close;
	TOptional<FJavaClassMethod> JWebView_GoBackOrForward;

	TWeakPtr<FAndroidWebBrowserWindow> WebBrowserWindowPtr;
};
