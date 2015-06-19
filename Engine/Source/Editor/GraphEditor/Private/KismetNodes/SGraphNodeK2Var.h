// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __SGraphNodeK2Var_h__
#define __SGraphNodeK2Var_h__

class GRAPHEDITOR_API SGraphNodeK2Var : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Var){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, UK2Node* InNode );

	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	// End of SGraphNode interface

protected:
	FSlateColor GetVariableColor() const;
};

#endif // __SGraphNodeK2Var_h__
