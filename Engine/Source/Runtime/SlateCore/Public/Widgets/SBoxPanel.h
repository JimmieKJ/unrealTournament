// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A BoxPanel contains one child and describes how that child should be arranged on the screen.
 */
class SLATECORE_API SBoxPanel
	: public SPanel
{
public:

	/**
	 * A BoxPanel contains one BoxPanel child and describes how that
	 * child should be arranged on the screen.
	 */
	class FSlot : public TSlotBase<FSlot>
	{
	public:		
		/** Horizontal and Vertical Boxes inherit from FSlot */
		virtual ~FSlot(){}
		/**
		 * How much space this slot should occupy along panel's direction.
		 *   When SizeRule is SizeRule_Auto, the widget's DesiredSize will be used as the space required.
		 *   When SizeRule is SizeRule_Stretch, the available space will be distributed proportionately between
		 *   peer Widgets depending on the Value property. Available space is space remaining after all the
		 *   peers' SizeRule_Auto requirements have been satisfied.
		 */
		FSizeParam SizeParam;
		
		/** Horizontal positioning of child within the allocated slot */
		EHorizontalAlignment HAlignment;
		
		/** Vertical positioning of child within the allocated slot */
		EVerticalAlignment VAlignment;
		
		/** The padding to add around the child. */
		TAttribute<FMargin> SlotPadding;
		
		/** The max size that this slot can be (0 if no max) */
		TAttribute<float> MaxSize;
			
	protected:
		/** Default values for a slot. */
		FSlot()
			: TSlotBase<FSlot>()
			, SizeParam( FStretch(1) )
			, HAlignment( HAlign_Fill )
			, VAlignment( VAlign_Fill )
			, SlotPadding( FMargin(0) )
			, MaxSize( 0.0f )
		{ }
	};

public:

	/** Removes a slot from this box panel which contains the specified SWidget
	 *
	 * @param SlotWidget The widget to match when searching through the slots
	 * @returns The index in the children array where the slot was removed and -1 if no slot was found matching the widget
	 */
	int32 RemoveSlot( const TSharedRef<SWidget>& SlotWidget );

	/**
	 * Removes all children from the box.
	 */
	void ClearChildren();

public:

	// Begin SWidget overrides.

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const;
	
	virtual FVector2D ComputeDesiredSize(float) const;

	virtual FChildren* GetChildren();

	// End SWidget overrides.

protected:
	/**
	 * A Box Panel's orientation cannot be changed once it is constructed..
	 *
	 * @param InOrientation   The orientation of the Box Panel
	 */
	SBoxPanel( EOrientation InOrientation );

	/** The Box Panel's orientation; determined at construct time. */
	const EOrientation Orientation;
	/** The Box Panel's children. */
	TPanelChildren<FSlot> Children;
};


/** A Horizontal Box Panel. See SBoxPanel for more info. */
class SLATECORE_API SHorizontalBox : public SBoxPanel
{
public:
	class FSlot : public SBoxPanel::FSlot
	{
		public:
		FSlot()
		: SBoxPanel::FSlot()
		{
		}

		FSlot& AutoWidth()
		{
			SizeParam = FAuto();
			return *this;
		}

		FSlot& MaxWidth( const TAttribute< float >& InMaxWidth )
		{
			MaxSize = InMaxWidth;
			return *this;
		}

		FSlot& FillWidth( const TAttribute< float >& StretchCoefficient )
		{
			SizeParam = FStretch( StretchCoefficient );
			return *this;
		}

		DEPRECATED( 4.5, "AspectRatio() property is no longer supported; it did not behave correctly to begin with and was rarely used." )
		FSlot& AspectRatio()
		{
			return *this;
		}
		
		FSlot& Padding( float Uniform )
		{
			SlotPadding = FMargin(Uniform);
			return *this;
		}

		FSlot& Padding( float Horizontal, float Vertical )
		{
			SlotPadding = FMargin(Horizontal, Vertical);
			return *this;
		}

		FSlot& Padding( float Left, float Top, float Right, float Bottom )
		{
			SlotPadding = FMargin(Left, Top, Right, Bottom);
			return *this;
		}
		
		FSlot& Padding( TAttribute<FMargin> InPadding )
		{
			SlotPadding = InPadding;
			return *this;
		}

		FSlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FSlot& VAlign( EVerticalAlignment InVAlignment )
		{
			VAlignment = InVAlignment;
			return *this;
		}

		FSlot& operator[]( TSharedRef<SWidget> InWidget )
		{
			SBoxPanel::FSlot::operator[](InWidget);
			return *this;
		}

		FSlot& Expose( FSlot*& OutVarToInit )
		{
			OutVarToInit = this;
			return *this;
		}
	};

	static FSlot& Slot()
	{
		return *(new FSlot());
	}

	SLATE_BEGIN_ARGS( SHorizontalBox )
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}

		SLATE_SUPPORTS_SLOT(SHorizontalBox::FSlot)

	SLATE_END_ARGS()

	FSlot& AddSlot()
	{
		SHorizontalBox::FSlot& NewSlot = *new SHorizontalBox::FSlot();
		this->Children.Add( &NewSlot );
		return NewSlot;
	}

	FSlot& InsertSlot(int32 Index = INDEX_NONE)
	{
		if (Index == INDEX_NONE)
		{
			return AddSlot();
		}
		SHorizontalBox::FSlot& NewSlot = *new SHorizontalBox::FSlot();
		this->Children.Insert(&NewSlot, Index);
		return NewSlot;
	}

	int32 NumSlots() const
	{
		return this->Children.Num();
	}

	FORCENOINLINE SHorizontalBox()
	: SBoxPanel( Orient_Horizontal )
	{
	}

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
};

/** A Vertical Box Panel. See SBoxPanel for more info. */
class SLATECORE_API SVerticalBox : public SBoxPanel
{
public:
	class FSlot : public SBoxPanel::FSlot
	{
		public:

		FSlot()
		: SBoxPanel::FSlot()
		{
		}

		FSlot& AutoHeight()
		{
			SizeParam = FAuto();
			return *this;
		}

		FSlot& MaxHeight( const TAttribute< float >& InMaxHeight )
		{
			MaxSize = InMaxHeight;
			return *this;
		}

		FSlot& FillHeight( const TAttribute< float >& StretchCoefficient )
		{
			SizeParam = FStretch( StretchCoefficient );
			return *this;
		}

		DEPRECATED( 4.5, "AspectRatio() property is no longer supported; it did not behave correctly to begin with and was rarely used." )
		FSlot& AspectRatio()
		{
			return *this;
		}

		FSlot& Padding( float Uniform )
		{
			SlotPadding = FMargin(Uniform);
			return *this;
		}

		FSlot& Padding( float Horizontal, float Vertical )
		{
			SlotPadding = FMargin(Horizontal, Vertical);
			return *this;
		}

		FSlot& Padding( float Left, float Top, float Right, float Bottom )
		{
			SlotPadding = FMargin(Left, Top, Right, Bottom);
			return *this;
		}

		FSlot& Padding( const TAttribute<FMargin>::FGetter& InDelegate )
		{
			SlotPadding.Bind( InDelegate );
			return *this;
		}

		FSlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FSlot& VAlign( EVerticalAlignment InVAlignment )
		{
			VAlignment = InVAlignment;
			return *this;
		}

		FSlot& Padding( TAttribute<FMargin> InPadding )
		{
			SlotPadding = InPadding;
			return *this;
		}

		FSlot& operator[]( TSharedRef<SWidget> InWidget )
		{
			SBoxPanel::FSlot::operator[](InWidget);
			return *this;
		}

		FSlot& Expose( FSlot*& OutVarToInit )
		{
			OutVarToInit = this;
			return *this;
		}
	};

	static FSlot& Slot()
	{
		return *(new FSlot());
	}


	SLATE_BEGIN_ARGS( SVerticalBox )
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}

		SLATE_SUPPORTS_SLOT(SVerticalBox::FSlot)

	SLATE_END_ARGS()

	FSlot& AddSlot()
	{
		SVerticalBox::FSlot& NewSlot = *new SVerticalBox::FSlot();
		this->Children.Add( &NewSlot );
		return NewSlot;
	}

	FSlot& InsertSlot(int32 Index = INDEX_NONE)
	{
		if (Index == INDEX_NONE)
		{
			return AddSlot();
		}
		SVerticalBox::FSlot& NewSlot = *new SVerticalBox::FSlot();
		this->Children.Insert(&NewSlot, Index);
		return NewSlot;
	}

	int32 NumSlots() const
	{
		return this->Children.Num();
	}

	FORCENOINLINE SVerticalBox()
	: SBoxPanel( Orient_Vertical )
	{
	}

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
};
