// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreviewScene.h"
#include "SEditorViewport.h"


/**
 * Material Editor Preview viewport widget
 */
class SNiagaraEffectEditorViewport : public SEditorViewport, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SNiagaraEffectEditorViewport ){}
	//SLATE_ARGUMENT(TWeakPtr<INiagaraEffectEditor>, EffectEditor)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	~SNiagaraEffectEditorViewport();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	void RefreshViewport();
	
	void SetPreviewEffect(FNiagaraEffectInstance* InEffect);
	
	void ToggleRealtime();
	
	/** @return The list of commands known by the material editor */
	TSharedRef<FUICommandList> GetNiagaraEffectEditorCommands() const;
	
	/** If true, render background object in the preview scene. */
	bool bShowBackground;
	
	/** If true, render grid the preview scene. */
	bool bShowGrid;
	
	FPreviewScene PreviewScene;
	
	/** The material editor has been added to a tab */
	void OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab );
	
	/** Event handlers */
	void TogglePreviewGrid();
	bool IsTogglePreviewGridChecked() const;
	void TogglePreviewBackground();
	bool IsTogglePreviewBackgroundChecked() const;
	class UNiagaraComponent *GetPreviewComponent()	{ return PreviewComponent;  }
protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
private:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;
	
	
	bool IsVisible() const override;
	
	/** Pointer back to the material editor tool that owns us */
	//TWeakPtr<INiagaraEffectEditor> EffectEditorPtr;
	
	class UNiagaraComponent *PreviewComponent;
	
	/** Level viewport client */
	TSharedPtr<class FNiagaraEffectEditorViewportClient> EditorViewportClient;
};
