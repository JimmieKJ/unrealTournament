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
			LinearXPositionDrive,
			LinearYPositionDrive,
			LinearZPositionDrive,
			LinearPositionDrive,
			LinearXVelocityDrive,
			LinearYVelocityDrive,
			LinearZVelocityDrive,
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

	bool IsPropertyEnabled(EPropertyType::Type Type) const;
	
	bool IsLinearMotionLimited() const;
	
	bool GetBoolProperty(TSharedPtr<IPropertyHandle> Prop) const;
	bool IsAngularPropertyLimited(TSharedPtr<IPropertyHandle> Prop) const;

	FReply OnHingeClicked();
	FReply OnPrismaticClicked();
	FReply OnBallSocketClicked();

	ECheckBoxState IsLimitRadioChecked(TSharedPtr<IPropertyHandle> Property, uint8 Value) const ;
	void OnLimitRadioChanged(ECheckBoxState CheckType, TSharedPtr<IPropertyHandle> Property, uint8 Value);

	void AddConstraintPresets(IDetailLayoutBuilder& DetailBuilder, class APhysicsConstraintActor* OwningConstraintActor);
	void AddLinearLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance);
	void AddAngularLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance);
	void AddLinearDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance);
	void AddAngularDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance);

	TSharedRef<SWidget> JoinPropertyWidgets(TSharedPtr<IPropertyHandle> TargetProperty, FName TargetChildName, TSharedPtr<IPropertyHandle> ConstraintInstance, FName CheckPropertyName, TSharedPtr<IPropertyHandle>& StoreCheckProperty, EPropertyType::Type DriveType);
private:

	TSharedPtr<IPropertyHandle> LinearPositionDriveProperty;
	TSharedPtr<IPropertyHandle> LinearXPositionDriveProperty;
	TSharedPtr<IPropertyHandle> LinearYPositionDriveProperty;
	TSharedPtr<IPropertyHandle> LinearZPositionDriveProperty;

	TSharedPtr<IPropertyHandle> LinearVelocityDriveProperty;
	TSharedPtr<IPropertyHandle> LinearXVelocityDriveProperty;
	TSharedPtr<IPropertyHandle> LinearYVelocityDriveProperty;
	TSharedPtr<IPropertyHandle> LinearZVelocityDriveProperty;
	TSharedPtr<IPropertyHandle> AngularPositionDriveProperty;
	TSharedPtr<IPropertyHandle> AngularVelocityDriveProperty;

	TSharedPtr<IPropertyHandle> LinearXMotionProperty;
	TSharedPtr<IPropertyHandle> LinearYMotionProperty;
	TSharedPtr<IPropertyHandle> LinearZMotionProperty;
	
	TSharedPtr<IPropertyHandle> AngularSwing1MotionProperty;
	TSharedPtr<IPropertyHandle> AngularSwing2MotionProperty;
	TSharedPtr<IPropertyHandle> AngularTwistMotionProperty;

	TWeakObjectPtr<UObject> ConstraintComponent;
	
	bool bInPhat;
};
