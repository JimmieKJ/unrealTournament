// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/Visual.h"
#include "SlateWrapperTypes.h"
#include "WidgetTransform.h"
#include "DynamicPropertyPath.h"
#include "UObject/UObjectThreadContext.h"

#include "Widget.generated.h"

class UPanelSlot;

/**
 * Helper macro for binding to a delegate or using the constant value when constructing the underlying SWidget
 */
#define OPTIONAL_BINDING(ReturnType, MemberName)				\
	( MemberName ## Delegate.IsBound() && !IsDesignTime() )		\
	?															\
		TAttribute< ReturnType >::Create(MemberName ## Delegate.GetUObject(), MemberName ## Delegate.GetFunctionName()) \
	:															\
		TAttribute< ReturnType >(MemberName)

#if WITH_EDITOR

#define GAME_SAFE_OPTIONAL_BINDING(ReturnType, MemberName)			\
	( MemberName ## Delegate.IsBound() && !IsDesignTime() )			\
	?																\
		BIND_UOBJECT_ATTRIBUTE(ReturnType, K2_Gate_ ## MemberName)	\
	:																\
		TAttribute< ReturnType >(MemberName)

#define GAME_SAFE_BINDING_IMPLEMENTATION(ReturnType, MemberName)		\
	ReturnType K2_Cache_ ## MemberName;									\
	ReturnType K2_Gate_ ## MemberName()									\
	{																	\
		if (CanSafelyRouteEvent())										\
		{																\
			K2_Cache_ ## MemberName = TAttribute< ReturnType >::Create(MemberName ## Delegate.GetUObject(), MemberName ## Delegate.GetFunctionName()).Get(); \
		}																\
																		\
		return K2_Cache_ ## MemberName;									\
	}

#else

#define GAME_SAFE_OPTIONAL_BINDING(ReturnType, MemberName)		\
	( MemberName ## Delegate.IsBound() && !IsDesignTime() )		\
	?															\
		TAttribute< ReturnType >::Create(MemberName ## Delegate.GetUObject(), MemberName ## Delegate.GetFunctionName()) \
	:															\
		TAttribute< ReturnType >(MemberName)

#define GAME_SAFE_BINDING_IMPLEMENTATION(Type, MemberName)

#endif

/**
 * Helper macro for binding to a delegate or using the constant value when constructing the underlying SWidget,
 * also allows a conversion function to be provided to convert between the SWidget value and the value exposed to UMG.
 */
#define OPTIONAL_BINDING_CONVERT(ReturnType, MemberName, ConvertedType, ConversionFunction) \
		( MemberName ## Delegate.IsBound() && !IsDesignTime() )								\
		?																					\
			TAttribute< ConvertedType >::Create(TAttribute< ConvertedType >::FGetter::CreateUObject(this, &ThisClass::ConversionFunction, TAttribute< ReturnType >::Create(MemberName ## Delegate.GetUObject(), MemberName ## Delegate.GetFunctionName()))) \
		:																					\
			ConversionFunction(TAttribute< ReturnType >(MemberName))



/**
 * This is the base class for all wrapped Slate controls that are exposed to UObjects.
 */
UCLASS(Abstract, BlueprintType)
class UMG_API UWidget : public UVisual
{
	GENERATED_UCLASS_BODY()

public:

	// Common Bindings - If you add any new common binding, you must provide a UPropertyBinder for it.
	//                   all primitive binding in UMG goes through native binding evaluators to prevent
	//                   thunking through the VM.
	DECLARE_DYNAMIC_DELEGATE_RetVal(bool, FGetBool);
	DECLARE_DYNAMIC_DELEGATE_RetVal(float, FGetFloat);
	DECLARE_DYNAMIC_DELEGATE_RetVal(int32, FGetInt32);
	DECLARE_DYNAMIC_DELEGATE_RetVal(FText, FGetText);
	DECLARE_DYNAMIC_DELEGATE_RetVal(FSlateColor, FGetSlateColor);
	DECLARE_DYNAMIC_DELEGATE_RetVal(FLinearColor, FGetLinearColor);
	DECLARE_DYNAMIC_DELEGATE_RetVal(FSlateBrush, FGetSlateBrush);
	DECLARE_DYNAMIC_DELEGATE_RetVal(ESlateVisibility, FGetSlateVisibility);
	DECLARE_DYNAMIC_DELEGATE_RetVal(EMouseCursor::Type, FGetMouseCursor);
	DECLARE_DYNAMIC_DELEGATE_RetVal(ECheckBoxState, FGetCheckBoxState);
	DECLARE_DYNAMIC_DELEGATE_RetVal(UWidget*, FGetWidget);

	// Events
	DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(UWidget*, FGenerateWidgetForString, FString, Item);
	DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(UWidget*, FGenerateWidgetForObject, UObject*, Item);

	// Events
	DECLARE_DYNAMIC_DELEGATE_RetVal(FEventReply, FOnReply);
	DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(FEventReply, FOnPointerEvent, FGeometry, MyGeometry, const FPointerEvent&, MouseEvent);

	/**
	 * Allows controls to be exposed as variables in a blueprint.  Not all controls need to be exposed
	 * as variables, so this allows only the most useful ones to end up being exposed.
	 */
	UPROPERTY()
	bool bIsVariable;

	/** Flag if the Widget was created from a blueprint */
	UPROPERTY(Transient)
	bool bCreatedByConstructionScript;

	/**
	 * The parent slot of the UWidget.  Allows us to easily inline edit the layout controlling this widget.
	 */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category=Layout, meta=(ShowOnlyInnerProperties))
	UPanelSlot* Slot;

	/** Sets whether this widget can be modified interactively by the user */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior")
	bool bIsEnabled;

	/** A bindable delegate for bIsEnabled */
	UPROPERTY()
	FGetBool bIsEnabledDelegate;

	/** Tooltip text to show when the user hovers over the widget with the mouse */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", meta=(MultiLine=true))
	FText ToolTipText;

	/** A bindable delegate for ToolTipText */
	UPROPERTY()
	FGetText ToolTipTextDelegate;

	/** Tooltip widget to show when the user hovers over the widget with the mouse */
	UPROPERTY(EditAnywhere, Category="Behavior", AdvancedDisplay)
	UWidget* ToolTipWidget;

	/** A bindable delegate for ToolTipWidget */
	UPROPERTY()
	FGetWidget ToolTipWidgetDelegate;

	/** The visibility of the widget */
	UPROPERTY()
	TEnumAsByte<ESlateVisibility> Visiblity_DEPRECATED;

	/** The visibility of the widget */
	UPROPERTY(EditAnywhere, Category="Behavior")
	ESlateVisibility Visibility;

	/** A bindable delegate for Visibility */
	UPROPERTY()
	FGetSlateVisibility VisibilityDelegate;

	/** The cursor to show when the mouse is over the widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", AdvancedDisplay)
	TEnumAsByte<EMouseCursor::Type> Cursor;

	/** A bindable delegate for Cursor */
	UPROPERTY()
	FGetMouseCursor CursorDelegate;

	/** The render transform of the widget allows for arbitrary 2D transforms to be applied to the widget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render Transform", meta=( DisplayName="Transform" ))
	FWidgetTransform RenderTransform;

	/**
	 * The render transform pivot controls the location about which transforms are applied.  
	 * This value is a normalized coordinate about which things like rotations will occur.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render Transform", meta=( DisplayName="Pivot" ))
	FVector2D RenderTransformPivot;

	/**
	 * The navigation object for this widget is optionally created if the user has configured custom
	 * navigation rules for this widget in the widget designer.  Those rules determine how navigation transitions
	 * can occur between widgets.
	 */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category="Navigation")
	class UWidgetNavigation* Navigation;

#if WITH_EDITORONLY_DATA

	/** Stores the design time flag setting if the widget is hidden inside the designer */
	UPROPERTY()
	bool bHiddenInDesigner;

	/** Stores the design time flag setting if the widget is expanded inside the designer */
	UPROPERTY()
	bool bExpandedInDesigner;

	/** Stores a reference to the asset responsible for this widgets construction. */
	UPROPERTY(Transient)
	UObject* WidgetGeneratedBy;

#endif

public:

	/** */
	UFUNCTION(BlueprintCallable, Category="Widget|Transform")
	void SetRenderTransform(FWidgetTransform InTransform);

	/** */
	UFUNCTION(BlueprintCallable, Category="Widget|Transform")
	void SetRenderScale(FVector2D Scale);

	/** */
	UFUNCTION(BlueprintCallable, Category="Widget|Transform")
	void SetRenderShear(FVector2D Shear);

	/** */
	UFUNCTION(BlueprintCallable, Category="Widget|Transform")
	void SetRenderAngle(float Angle);
	
	/** */
	UFUNCTION(BlueprintCallable, Category="Widget|Transform")
	void SetRenderTranslation(FVector2D Translation);

	/** */
	UFUNCTION(BlueprintCallable, Category="Widget|Transform")
	void SetRenderTransformPivot(FVector2D Pivot);

	/** Gets the current enabled status of the widget */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool GetIsEnabled() const;

	/** Sets the current enabled status of the widget */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetIsEnabled(bool bInIsEnabled);

	/** Sets the tooltip text for the widget. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetToolTipText(const FText& InToolTipText);

	/** Sets a custom widget as the tooltip of the widget. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetToolTip(UWidget* Widget);

	/** @return true if the widget is Visible, HitTestInvisible or SelfHitTestInvisible. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsVisible() const;

	/** Gets the current visibility of the widget. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	ESlateVisibility GetVisibility() const;

	/** Sets the visibility of the widget. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetVisibility(ESlateVisibility InVisibility);

	/** @return true if the widget is currently being hovered by a pointer device */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsHovered() const;

	/**
	 * Checks to see if this widget currently has the keyboard focus
	 *
	 * @return  True if this widget has keyboard focus
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool HasKeyboardFocus() const;

	/**
	 * @return Whether this widget has any descendants with keyboard focus
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool HasFocusedDescendants() const;

	/**
	 * Checks to see if this widget is the current mouse captor
	 * @return  True if this widget has captured the mouse
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool HasMouseCapture() const;

	/** Sets the focus to this widget. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetKeyboardFocus();

	/** Gets the focus to this widget. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool HasUserFocus(APlayerController* PlayerController) const;

	/** Gets the focus to this widget. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool HasAnyUserFocus() const;
	
	/** Sets the focus to this widget for a specific user */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetUserFocus(APlayerController* PlayerController);

	/**
	 * Forces a pre-pass.  A pre-pass caches the desired size of the widget hierarchy owned by this widget.  
	 * One pre-pass is already happens for every widget before Tick occurs.  You only need to perform another 
	 * pre-pass if you are adding child widgets this frame and want them to immediately be visible this frame.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void ForceLayoutPrepass();

	/**
	 * Gets the widgets desired size.
	 * NOTE: The underlying Slate widget must exist and be valid, also at least one pre-pass must
	 *       have occurred before this value will be of any use.
	 * 
	 * @return The widget's desired size
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	FVector2D GetDesiredSize() const;

	/** Gets the parent widget */
	UFUNCTION(BlueprintCallable, Category="Widget")
	class UPanelWidget* GetParent() const;

	/**
	 * Removes the widget from its parent widget.  If this widget was added to the player's screen or the viewport
	 * it will also be removed from those containers.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	virtual void RemoveFromParent();

	/**
	 * Gets the underlying slate widget or constructs it if it doesn't exist.  This function is
	 * virtual however, you should not inherit this function unless you're very aware of what you're
	 * doing.  Normal derived versions should only ever override RebuildWidget.
	 */
	TSharedRef<SWidget> TakeWidget();

	/** Gets the last created widget does not recreate the gc container for the widget if one is needed. */
	TSharedPtr<SWidget> GetCachedWidget() const;

	/**
	 * Applies all properties to the native widget if possible.  This is called after a widget is constructed.
	 * It can also be called by the editor to update modified state, so ensure all initialization to a widgets
	 * properties are performed here, or the property and visual state may become unsynced.
	 */
	virtual void SynchronizeProperties();

	/**
	 * Called by the owning user widget after the slate widget has been created.  After the entire widget tree
	 * has been initialized, any widget reference that was needed to support navigating to another widget will
	 * now be initialized and ready for usage.
	 */
	void BuildNavigation();

#if WITH_EDITOR
	/** Returns if the widget is currently being displayed in the designer, it may want to display different data. */
	bool IsDesignTime() const;
#else
	FORCEINLINE bool IsDesignTime() const { return false; }
#endif
	
	/** Sets that this widget is being designed */
	virtual void SetIsDesignTime(bool bInDesignTime);

	/** Mark this object as modified, also mark the slot as modified. */
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;

	/**
	 * Recurses up the list of parents and returns true if this widget is a descendant of the PossibleParent
	 * @return true if this widget is a child of the PossibleParent
	 */
	bool IsChildOf(UWidget* PossibleParent);

	/**  */
	bool AddBinding(UDelegateProperty* DelegateProperty, UObject* SourceObject, const FDynamicPropertyPath& BindingPath);

	static TSubclassOf<class UPropertyBinding> FindBinderClassForDestination(UProperty* Property);

	// Begin UObject
	virtual UWorld* GetWorld() const override;
	virtual void PostLoad() override;
	// End UObject

#if WITH_EDITOR
	FORCEINLINE bool CanSafelyRouteEvent()
	{
		return !(IsDesignTime() || GIntraFrameDebuggingGameThread || HasAnyFlags(RF_Unreachable) || FUObjectThreadContext::Get().IsRoutingPostLoad);
	}
#else
	FORCEINLINE bool CanSafelyRouteEvent() { return !(HasAnyFlags(RF_Unreachable) || FUObjectThreadContext::Get().IsRoutingPostLoad); }
#endif

#if WITH_EDITOR

	/** Is the label generated or provided by the user? */
	bool IsGeneratedName() const;

	/** Get Label Metadata, which may be as simple as a bit of string data to help identify an anonymous text block. */
	virtual FString GetLabelMetadata() const;

	/** Gets the label to display to the user for this widget. */
	DEPRECATED(4.8, "Use GetLabelText(), which will return the label as FText.")
	FString GetLabel() const;

	/** Gets the label to display to the user for this widget. */
	FText GetLabelText() const;

	/** Gets the palette category of the widget */
	virtual const FText GetPaletteCategory();

	/**
	 * Called by the palette after constructing a new widget, allows the widget to perform interesting 
	 * default setup that we don't want to be UObject Defaults.
	 */
	virtual void OnCreationFromPalette() { }

	/** Gets the editor icon */
	virtual const FSlateBrush* GetEditorIcon();
	
	/** Allows general fixups and connections only used at editor time. */
	virtual void ConnectEditorData() { }

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	/** Gets the visibility of the widget inside the designer. */
	EVisibility GetVisibilityInDesigner() const;

	// Begin Designer contextual events
	void Select();
	void Deselect();

	virtual void OnSelected() { }
	virtual void OnDeselected() { }

	virtual void OnDescendantSelected(UWidget* DescendantWidget) { }
	virtual void OnDescendantDeselected(UWidget* DescendantWidget) { }

	virtual void OnBeginEdit() { }
	virtual void OnEndEdit() { }
	// End Designer contextual events
#endif

	// Utility methods
	//@TODO UMG: Should move elsewhere
	static EVisibility ConvertSerializedVisibilityToRuntime(ESlateVisibility Input);
	static ESlateVisibility ConvertRuntimeToSerializedVisibility(const EVisibility& Input);

	static FSizeParam ConvertSerializedSizeParamToRuntime(const FSlateChildSize& Input);

	static UWidget* FindChildContainingDescendant(UWidget* Root, UWidget* Descendant);

protected:
	virtual void OnBindingChanged(const FName& Property);

protected:
	/** Function implemented by all subclasses of UWidget is called when the underlying SWidget needs to be constructed. */
	virtual TSharedRef<SWidget> RebuildWidget();

	/** Function called after the underlying SWidget is constructed. */
	virtual void OnWidgetRebuilt();
	
	TSharedRef<SWidget> BuildDesignTimeWidget(TSharedRef<SWidget> WrapWidget);

	void UpdateRenderTransform();

	/** Gets the base name used to generate the display label/name of this widget. */
	FText GetDisplayNameBase() const;

protected:
	//TODO UMG Consider moving conversion functions into another class.
	// Conversion functions
	EVisibility ConvertVisibility(TAttribute<ESlateVisibility> SerializedType) const
	{
		ESlateVisibility SlateVisibility = SerializedType.Get();
		return ConvertSerializedVisibilityToRuntime(SlateVisibility);
	}

	TOptional<float> ConvertFloatToOptionalFloat(TAttribute<float> InFloat) const
	{
		return InFloat.Get();
	}

	FSlateColor ConvertLinearColorToSlateColor(TAttribute<FLinearColor> InLinearColor) const
	{
		return FSlateColor(InLinearColor.Get());
	}

protected:
	/** The underlying SWidget. */
	TWeakPtr<SWidget> MyWidget;

	/** The underlying SWidget contained in a SObjectWidget */
	TWeakPtr<class SObjectWidget> MyGCWidget;
	
	/** Is this widget being displayed on a designer surface */
	UPROPERTY(Transient)
	bool bDesignTime;

	/** Native property bindings. */
	UPROPERTY(Transient)
	TArray<class UPropertyBinding*> NativeBindings;

	static TArray<TSubclassOf<UPropertyBinding>> BinderClasses;

private:
	GAME_SAFE_BINDING_IMPLEMENTATION(FText, ToolTipText)
	GAME_SAFE_BINDING_IMPLEMENTATION(bool, bIsEnabled)
};
