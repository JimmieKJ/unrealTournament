
#pragma once

#include "GroupedKeyArea.h"

/** A layout element specifying the geometry required to render a key area */
struct FKeyAreaLayoutElement
{
	/** Whether this layout element pertains to one or multiple key areas */
	enum EType { Single, Group };

	/** Construct this element from a grouped key area */
	static FKeyAreaLayoutElement FromGroup(const TSharedRef<FGroupedKeyArea>& InKeyAreaGroup, float InOffset, TOptional<float> InHeight = TOptional<float>());

	/** Construct this element from a single Key area node */
	static FKeyAreaLayoutElement FromKeyAreaNode(const TSharedRef<FSequencerSectionKeyAreaNode>& InKeyAreaNode, int32 SectionIndex, float InOffset);

	/** Retrieve the type of this layout element */
	EType GetType() const;

	/** Retrieve vertical offset from the top of this element's parent */
	float GetOffset() const;

	/** Retrieve the desired height of this element based on the specified parent geometry */
	float GetHeight(const FGeometry& InParentGeometry) const;

	/** Access the key area that this layout element was generated for */
	TSharedRef<IKeyArea> GetKeyArea() const;

private:

	/** Pointer to the key area that we were generated from */
	TSharedPtr<IKeyArea> KeyArea;

	/** The type of this layout element */
	EType Type;

	/** The vertical offset from the top of the element's parent */
	float LocalOffset;

	/** Explicit height, or unset to fill the parent geometry */
	TOptional<float> Height;
};

/** Class used for generating, and caching the layout geometry for a given display node's key areas */
class FKeyAreaLayout
{
public:

	/** Constructor that takes a display node, and the index of the section to layout */
	FKeyAreaLayout(FSequencerDisplayNode& InNode, int32 InSectionIndex);

	/** Get all layout elements that we generated */
	const TArray<FKeyAreaLayoutElement>& GetElements() const;
	
	/** Get the desired total height of this layout */
	float GetTotalHeight() const;

private:
	/** Array of layout elements that we generated */
	TArray<FKeyAreaLayoutElement> Elements;

	/** The desired total height of this layout */
	float TotalHeight;
};