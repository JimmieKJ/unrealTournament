// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __SGraphNodeK2Terminator_h__
#define __SGraphNodeK2Terminator_h__

class SGraphNodeK2Terminator : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Terminator){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, UK2Node* InNode );

	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;

protected:
	virtual void UpdateGraphNode() override;
};

#endif // __SGraphNodeK2Terminator_h__
