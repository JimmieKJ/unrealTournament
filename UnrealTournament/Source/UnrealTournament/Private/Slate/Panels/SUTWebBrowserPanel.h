// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

#include "SWebBrowser.h"

class UNREALTOURNAMENT_API SUTWebBrowserPanel : public SUWPanel
{
	SLATE_BEGIN_ARGS(SUTWebBrowserPanel)
	: _ShowControls(true)
	, _ViewportSize(FVector2D(1920, 1080))
	, _AllowScaling(false)
	{}

	SLATE_ARGUMENT(bool, ShowControls)
	SLATE_ARGUMENT(FVector2D, ViewportSize)
	SLATE_ARGUMENT(bool, AllowScaling)

	/** Called when a custom Javascript message is received from the browser process. */
	SLATE_EVENT(FOnJSQueryReceivedDelegate, OnJSQueryReceived)

	/** Called when a pending Javascript message has been canceled, either explicitly or by navigating away from the page containing the script. */
	SLATE_EVENT(FOnJSQueryCanceledDelegate, OnJSQueryCanceled)

	SLATE_EVENT(FOnBeforeBrowseDelegate, OnBeforeBrowse)

	SLATE_EVENT(FOnBeforePopupDelegate, OnBeforePopup)

	SLATE_END_ARGS()
	
public:
	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner);

	virtual void ConstructPanel(FVector2D ViewportSize);
	virtual void Browse(FString URL);

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);
	virtual void OnHidePanel();

	virtual void ExecuteJavascript(const FString& JS) { WebBrowserPanel->ExecuteJavascript(JS); }
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:
	
	TSharedPtr<SOverlay> Overlay;

	FVector2D DesiredViewportSize;
	bool bAllowScaling;

	TSharedPtr<SVerticalBox> WebBrowserContainer;
	// The Actual Web browser panel.
	TSharedPtr<SWebBrowser> WebBrowserPanel;

	virtual bool QueryReceived( int64 QueryId, FString QueryString, bool Persistent, FJSQueryResultDelegate Delegate );
	virtual void QueryCancelled(int64 QueryId);
	virtual bool BeforeBrowse(FString TargetURL, bool bRedirect);
	virtual bool BeforePopup(FString URL, FString Target);

	/** A delegate that is invoked when render process Javascript code sends a query message to the client. */
	FOnJSQueryReceivedDelegate OnJSQueryReceived;

	/** A delegate that is invoked when render process cancels an ongoing query. Handler must clean up corresponding result delegate. */
	FOnJSQueryCanceledDelegate OnJSQueryCanceled;

	FOnBeforeBrowseDelegate OnBeforeBrowse;
	FOnBeforePopupDelegate OnBeforePopup;

	float GetReverseScale() const;
	bool ShowControls;

	FString DesiredURL;
	bool bShowWarning;

	void WarningResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
};

#endif