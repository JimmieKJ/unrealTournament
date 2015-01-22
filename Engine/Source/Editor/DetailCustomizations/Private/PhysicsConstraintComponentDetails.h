// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Detail customizer for PhysicsConstraintComponent and PhysicsConstraintTemplate
 */
class FPhysicsConstraintComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	struct EPropertyType
	{
		enum Type
		{
			LinearPositionDrive,
			LinearVelocityDrive,
			LinearAnyDrive,
			LinearLimit,
			
			AngularPositionDrive,
			AngularVelocityDrive,
			AngularAnyDrive,
			AngularSwingLimit,
			AngularSwing1Limit,
			AngularSwing2Limit,
			AngularTwistLimit,
			AngularAnyLimit,
		};
	};

	EVisibility IsPropertyVisible(EPropertyType::Type Type) const;
	
	bool IsLinearMotionLimited() const;
	
	bool GetBoolProperty(TSharedPtr<IPropertyHandle> Prop) const;
	bool IsAngularPropertyLimited(TSharedPtr<IPropertyHandle> Prop) const;

	FReply OnHingeClicked();
	FReply OnPrismaticClicked();
	FReply OnBallSocketClicked();

	ECheckBoxState IsLimitRadioChecked(TSharedPtr<IPropertyHandle> Property, uint8 Value) const ;
	void OnLimitRadioChanged(ECheckBoxState CheckType, TSharedPtr<IPropertyHandle> Property, uint8 Value);


	TSharedPtr<IPropertyHandle> LinearPositionDriveProperty;
	TSharedPtr<IPropertyHandle> LinearVelocityDriveProperty;
	TSharedPtr<IPropertyHandle> AngularPositionDriveProperty;
	TSharedPtr<IPropertyHandle> AngularVelocityDriveProperty;

	TSharedPtr<IPropertyHandle> LinearXMotionProperty;
	TSharedPtr<IPropertyHandle> LinearYMotionProperty;
	TSharedPtr<IPropertyHandle> LinearZMotionProperty;
	
	TSharedPtr<IPropertyHandle> AngularSwing1MotionProperty;
	TSharedPtr<IPropertyHandle> AngularSwing2MotionProperty;
	TSharedPtr<IPropertyHandle> AngularTwistMotionProperty;

	TWeakObjectPtr<UObject> ConstraintComponent;
};
