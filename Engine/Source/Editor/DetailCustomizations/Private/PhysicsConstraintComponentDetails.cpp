// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "PhysicsConstraintComponentDetails.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"

#define LOCTEXT_NAMESPACE "PhysicsConstraintComponentDetails"

TSharedRef<IDetailCustomization> FPhysicsConstraintComponentDetails::MakeInstance()
{
	return MakeShareable(new FPhysicsConstraintComponentDetails());
}

void FPhysicsConstraintComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	TSharedPtr<IPropertyHandle> ConstraintInstance;
	UPhysicsConstraintComponent* ConstraintComp = NULL;
	APhysicsConstraintActor* OwningConstraintActor = NULL;

	for (int32 i=0; i < Objects.Num(); ++i)
	{
		if (!Objects[i].IsValid()) { continue; }

		if (Objects[i]->IsA(UPhysicsConstraintTemplate::StaticClass()))
		{
			ConstraintInstance = DetailBuilder.GetProperty("DefaultInstance");
			break;
		}
		else if (Objects[i]->IsA(UPhysicsConstraintComponent::StaticClass()))
		{
			ConstraintInstance = DetailBuilder.GetProperty("ConstraintInstance");
			ConstraintComp = (UPhysicsConstraintComponent*)Objects[i].Get();
			OwningConstraintActor = Cast<APhysicsConstraintActor>(ConstraintComp->GetOwner());
			break;
		}
	}

	if (OwningConstraintActor != NULL)
	{
		ConstraintComponent = TWeakObjectPtr<UObject>(ConstraintComp);

		DetailBuilder.EditCategory( "Joint Presets" )
			.AddCustomRow( TEXT("") )
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding( 0, 2.0f, 0, 0 )
				.FillHeight(1.0f)
				.VAlign( VAlign_Center )
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( 2.0f, 0.0f )
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.OnClicked( this, &FPhysicsConstraintComponentDetails::OnHingeClicked )
						.Text( LOCTEXT("HingePreset", "Hinge").ToString() )
						.ToolTipText( LOCTEXT("HingePresetTooltip", "Setup joint as hinge. A hinge joint allows motion only in one plane.").ToString() )
					]
					+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding( 2.0f, 0.0f )
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SButton)
							.VAlign(VAlign_Center)
							.OnClicked( this, &FPhysicsConstraintComponentDetails::OnPrismaticClicked )
							.Text( LOCTEXT("PrismaticPreset", "Prismatic").ToString() )
							.ToolTipText( LOCTEXT("PrismaticPresetTooltip", "Setup joint as prismatic. A prismatic joint allows only linear sliding movement.").ToString() )
						]
					+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding( 2.0f, 0.0f )
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SButton)
							.VAlign(VAlign_Center)
							.OnClicked( this, &FPhysicsConstraintComponentDetails::OnBallSocketClicked )
							.Text( LOCTEXT("BSPreset", "Ball and Socket").ToString() )
							.ToolTipText( LOCTEXT("BSPresetTooltip", "Setup joint as ball and socket. A Ball and Socket joint allows motion around an indefinite number of axes, which have one common center").ToString() )
						]
				]
			];
	}

	// Linear Limits
	{
		IDetailCategoryBuilder& LinearLimitCat = DetailBuilder.EditCategory("Linear Limits", TEXT(""));

		LinearXMotionProperty = ConstraintInstance->GetChildHandle("LinearXMotion");
		LinearYMotionProperty = ConstraintInstance->GetChildHandle("LinearYMotion");
		LinearZMotionProperty = ConstraintInstance->GetChildHandle("LinearZMotion");
		
		TArray<TSharedPtr<FString>> LinearLimitOptionNames;
		TArray<TSharedPtr<FString>> LinearLimitOptionTooltips;
		TArray<bool> LinearLimitOptionRestrictItems;

		const int32 ExpectedLinearLimitOptionCount = 3;
		LinearXMotionProperty->GeneratePossibleValues(LinearLimitOptionNames, LinearLimitOptionTooltips, LinearLimitOptionRestrictItems);
		checkf(LinearLimitOptionNames.Num() == ExpectedLinearLimitOptionCount && 
			LinearLimitOptionTooltips.Num() == ExpectedLinearLimitOptionCount && 
			LinearLimitOptionRestrictItems.Num() == ExpectedLinearLimitOptionCount,
			TEXT("It seems the number of enum entries in ELinearConstraintMotion has changed. This must be handled here as well. "));

		
		uint8 LinearLimitEnum[LCM_MAX] = { LCM_Free, LCM_Limited, LCM_Locked };
		TSharedPtr<IPropertyHandle> LinearLimitProperties[] = { LinearXMotionProperty, LinearYMotionProperty, LinearZMotionProperty };


		for (int32 PropertyIdx=0; PropertyIdx < 3; ++PropertyIdx)
		{
			TSharedPtr<IPropertyHandle> CurProperty = LinearLimitProperties[PropertyIdx];

			LinearLimitCat.AddProperty(CurProperty).CustomWidget()
				.NameContent()
				[
					SNew(STextBlock)
					.Font(DetailBuilder.GetDetailFont())
					.Text(CurProperty->GetPropertyDisplayName())
					.ToolTipText(CurProperty->GetToolTipText())
				]
			.ValueContent()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked( this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[0])
						.OnCheckStateChanged( this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[0] )
						.ToolTipText(*LinearLimitOptionTooltips[0].Get())
						[
							SNew(STextBlock).Text(*LinearLimitOptionNames[0].Get())
						]						
					]
					+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						[
							SNew(SCheckBox)
							.Style(FEditorStyle::Get(), "RadioButton")
							.IsChecked( this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[1])
							.OnCheckStateChanged( this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[1] )
							.ToolTipText(*LinearLimitOptionTooltips[1].Get())
							[
								SNew(STextBlock).Text(*LinearLimitOptionNames[1].Get())
							]
						]
					+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						[
							SNew(SCheckBox)
							.Style(FEditorStyle::Get(), "RadioButton")
							.IsChecked( this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[2])
							.OnCheckStateChanged( this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[2] )
							.ToolTipText(*LinearLimitOptionTooltips[2].Get())
							[
								SNew(STextBlock).Text(*LinearLimitOptionNames[2].Get())
							]
						]
				];
		}
		

		LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle("LinearLimitSize").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearLimit)));
		LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle("bLinearLimitSoft").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearLimit)));
		LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle("LinearLimitStiffness").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearLimit)));
		LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle("LinearLimitDamping").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearLimit)));
		//LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle("bLinearBreakable").ToSharedRef());
		//LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle("LinearBreakThreshold").ToSharedRef());
	}

	// Angular Limits
	{
		IDetailCategoryBuilder& AngularLimitCat = DetailBuilder.EditCategory("Angular Limits", TEXT(""));

		AngularSwing1MotionProperty = ConstraintInstance->GetChildHandle("AngularSwing1Motion");
		AngularSwing2MotionProperty = ConstraintInstance->GetChildHandle("AngularSwing2Motion");
		AngularTwistMotionProperty = ConstraintInstance->GetChildHandle("AngularTwistMotion");

		TArray<TSharedPtr<FString>> AngularLimitOptionNames;
		TArray<TSharedPtr<FString>> AngularLimitOptionTooltips;
		TArray<bool> AngularLimitOptionRestrictItems;

		const int32 ExpectedAngularLimitOptionCount = 3;
		AngularSwing1MotionProperty->GeneratePossibleValues(AngularLimitOptionNames, AngularLimitOptionTooltips, AngularLimitOptionRestrictItems);
		checkf(AngularLimitOptionNames.Num() == ExpectedAngularLimitOptionCount && 
			AngularLimitOptionTooltips.Num() == ExpectedAngularLimitOptionCount && 
			AngularLimitOptionRestrictItems.Num() == ExpectedAngularLimitOptionCount,
			TEXT("It seems the number of enum entries in EAngularConstraintMotion has changed. This must be handled here as well. "));


		uint8 AngularLimitEnum[LCM_MAX] = { ACM_Free, LCM_Limited, LCM_Locked };
		TSharedPtr<IPropertyHandle> AngularLimitProperties[] = { AngularSwing1MotionProperty, AngularSwing2MotionProperty, AngularTwistMotionProperty };

		for (int32 PropertyIdx=0; PropertyIdx < 3; ++PropertyIdx)
		{
			TSharedPtr<IPropertyHandle> CurProperty = AngularLimitProperties[PropertyIdx];

			AngularLimitCat.AddProperty(CurProperty).CustomWidget()
				.NameContent()
				[
					SNew(STextBlock)
					.Font(DetailBuilder.GetDetailFont())
					.Text(CurProperty->GetPropertyDisplayName())
					.ToolTipText(CurProperty->GetToolTipText())
				]
			.ValueContent()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked( this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[0])
						.OnCheckStateChanged( this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[0] )
						.ToolTipText(*AngularLimitOptionTooltips[0].Get())
						[
							SNew(STextBlock).Text(*AngularLimitOptionNames[0].Get())
						]						
					]
					+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						[
							SNew(SCheckBox)
							.Style(FEditorStyle::Get(), "RadioButton")
							.IsChecked( this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[1])
							.OnCheckStateChanged( this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[1] )
							.ToolTipText(*AngularLimitOptionTooltips[1].Get())
							[
								SNew(STextBlock).Text(*AngularLimitOptionNames[1].Get())
							]
						]
					+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						[
							SNew(SCheckBox)
							.Style(FEditorStyle::Get(), "RadioButton")
							.IsChecked( this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[2])
							.OnCheckStateChanged( this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[2] )
							.ToolTipText(*AngularLimitOptionTooltips[2].Get())
							[
								SNew(STextBlock).Text(*AngularLimitOptionNames[2].Get())
							]
						]
				];
		}

		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("Swing1LimitAngle").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularSwing1Limit)));
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("Swing2LimitAngle").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularSwing2Limit)));
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("bSwingLimitSoft").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularSwingLimit)));
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("SwingLimitStiffness").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularSwingLimit)));
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("SwingLimitDamping").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularSwingLimit)));

		
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("TwistLimitAngle").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularTwistLimit)));
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("bTwistLimitSoft").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularTwistLimit)));
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("TwistLimitStiffness").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularTwistLimit)));
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("TwistLimitDamping").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularTwistLimit)));
		
		//AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("bAngularBreakable").ToSharedRef());
		//AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle("AngularBreakThreshold").ToSharedRef());
	}

	// Linear Drive
	{
		IDetailCategoryBuilder& LinearMotorCat = DetailBuilder.EditCategory("LinearMotor", TEXT(""));

		LinearPositionDriveProperty = ConstraintInstance->GetChildHandle("bLinearPositionDrive");
		LinearVelocityDriveProperty = ConstraintInstance->GetChildHandle("bLinearVelocityDrive");

		IDetailGroup& PositionGroup = LinearMotorCat.AddGroup("Linear Position Drive", TEXT(""));
		PositionGroup.HeaderProperty(LinearPositionDriveProperty.ToSharedRef());
		PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("bLinearXPositionDrive").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearPositionDrive)));
		PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("bLinearYPositionDrive").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearPositionDrive)));
		PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("bLinearZPositionDrive").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearPositionDrive)));
		PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("LinearPositionTarget").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearPositionDrive)));

		IDetailGroup& VelocityGroup = LinearMotorCat.AddGroup("Linear Velocity Drive", TEXT(""));
		VelocityGroup.HeaderProperty(LinearVelocityDriveProperty.ToSharedRef());
		VelocityGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("LinearVelocityTarget").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearVelocityDrive)));

		LinearMotorCat.AddProperty(ConstraintInstance->GetChildHandle("LinearDriveSpring").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearAnyDrive)));
		LinearMotorCat.AddProperty(ConstraintInstance->GetChildHandle("LinearDriveDamping").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearAnyDrive)));
		LinearMotorCat.AddProperty(ConstraintInstance->GetChildHandle("LinearDriveForceLimit").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::LinearAnyDrive)));
	}

	// Angular Drive
	{
		AngularPositionDriveProperty = ConstraintInstance->GetChildHandle("bAngularOrientationDrive");
		AngularVelocityDriveProperty = ConstraintInstance->GetChildHandle("bAngularVelocityDrive");

		IDetailCategoryBuilder& AngularMotorCat = DetailBuilder.EditCategory("AngularMotor", TEXT(""));

		IDetailGroup& PositionGroup = AngularMotorCat.AddGroup("Angular Orientation Drive", TEXT(""));
		PositionGroup.HeaderProperty(AngularPositionDriveProperty.ToSharedRef());
		
		PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("AngularOrientationTarget").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularPositionDrive)));

		IDetailGroup& VelocityGroup = AngularMotorCat.AddGroup("Angular Velocity Drive", TEXT(""));
		VelocityGroup.HeaderProperty(AngularVelocityDriveProperty.ToSharedRef());
		VelocityGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("AngularVelocityTarget").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularVelocityDrive)));
		VelocityGroup.AddPropertyRow(ConstraintInstance->GetChildHandle("AngularDriveForceLimit").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularVelocityDrive)));

		AngularMotorCat.AddProperty(ConstraintInstance->GetChildHandle("AngularDriveSpring").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularAnyDrive)));
		AngularMotorCat.AddProperty(ConstraintInstance->GetChildHandle("AngularDriveDamping").ToSharedRef())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyVisible, EPropertyType::AngularAnyDrive)));
	}
}

EVisibility FPhysicsConstraintComponentDetails::IsPropertyVisible( EPropertyType::Type Type ) const
{
	bool bIsVisible = false;
	switch (Type)
	{
		case EPropertyType::LinearPositionDrive:	bIsVisible = GetBoolProperty(LinearPositionDriveProperty); break;
		case EPropertyType::LinearVelocityDrive:	bIsVisible = GetBoolProperty(LinearVelocityDriveProperty); break;
		case EPropertyType::LinearAnyDrive:			bIsVisible = GetBoolProperty(LinearPositionDriveProperty) || GetBoolProperty(LinearVelocityDriveProperty); break;
		case EPropertyType::AngularPositionDrive:	bIsVisible = GetBoolProperty(AngularPositionDriveProperty); break;
		case EPropertyType::AngularVelocityDrive:	bIsVisible = GetBoolProperty(AngularVelocityDriveProperty); break;
		case EPropertyType::AngularAnyDrive:		bIsVisible = GetBoolProperty(AngularPositionDriveProperty) || GetBoolProperty(AngularVelocityDriveProperty); break;
		case EPropertyType::LinearLimit:			bIsVisible = IsLinearMotionLimited(); break;
		case EPropertyType::AngularSwing1Limit:		bIsVisible = IsAngularPropertyLimited(AngularSwing1MotionProperty); break;
		case EPropertyType::AngularSwing2Limit:		bIsVisible = IsAngularPropertyLimited(AngularSwing2MotionProperty); break;
		case EPropertyType::AngularSwingLimit:		bIsVisible = IsAngularPropertyLimited(AngularSwing1MotionProperty) || IsAngularPropertyLimited(AngularSwing2MotionProperty); break;
		case EPropertyType::AngularTwistLimit:		bIsVisible = IsAngularPropertyLimited(AngularTwistMotionProperty); break;
		case EPropertyType::AngularAnyLimit:		bIsVisible = IsAngularPropertyLimited(AngularSwing1MotionProperty) || IsAngularPropertyLimited(AngularSwing2MotionProperty) || 
																 IsAngularPropertyLimited(AngularTwistMotionProperty); break;
	}	

	return bIsVisible ? EVisibility::Visible : EVisibility::Hidden;
}

bool FPhysicsConstraintComponentDetails::GetBoolProperty(TSharedPtr<IPropertyHandle> Prop) const
{
	bool bIsEnabled = false;

	if (Prop->GetValue(bIsEnabled))
	{
		return bIsEnabled;
	}
	return false;
}

bool FPhysicsConstraintComponentDetails::IsAngularPropertyLimited(TSharedPtr<IPropertyHandle> Prop) const
{
	uint8 Val;
	if (Prop->GetValue(Val))
	{
		return Val == ACM_Limited;
	}
	return false;
}

bool FPhysicsConstraintComponentDetails::IsLinearMotionLimited() const
{
	uint8 XMotion, YMotion, ZMotion;
	if (LinearXMotionProperty->GetValue(XMotion) && 
		LinearYMotionProperty->GetValue(YMotion) && 
		LinearZMotionProperty->GetValue(ZMotion))
	{
		return XMotion == LCM_Limited || YMotion == LCM_Limited || ZMotion == LCM_Limited;
	}
	return false;
}

FReply FPhysicsConstraintComponentDetails::OnHingeClicked()
{
	if (ConstraintComponent.IsValid())
	{
		UPhysicsConstraintComponent* Comp = Cast<UPhysicsConstraintComponent>(ConstraintComponent.Get());
		Comp->ConstraintInstance.ConfigureAsHinge();
		Comp->UpdateSpriteTexture();
	}
	return FReply::Handled();
}

FReply FPhysicsConstraintComponentDetails::OnPrismaticClicked()
{
	if (ConstraintComponent.IsValid())
	{
		UPhysicsConstraintComponent* Comp = Cast<UPhysicsConstraintComponent>(ConstraintComponent.Get());
		Comp->ConstraintInstance.ConfigureAsPrismatic();
		Comp->UpdateSpriteTexture();
	}
	return FReply::Handled();
}

FReply FPhysicsConstraintComponentDetails::OnBallSocketClicked()
{
	if (ConstraintComponent.IsValid())
	{
		UPhysicsConstraintComponent* Comp = Cast<UPhysicsConstraintComponent>(ConstraintComponent.Get());
		Comp->ConstraintInstance.ConfigureAsBS();
		Comp->UpdateSpriteTexture();
	}
	return FReply::Handled();
}

ESlateCheckBoxState::Type FPhysicsConstraintComponentDetails::IsLimitRadioChecked( TSharedPtr<IPropertyHandle> Property, uint8 Value ) const
{
	uint8 PropertyEnumValue = 0;
	if (Property.IsValid() && Property->GetValue(PropertyEnumValue))
	{
		return PropertyEnumValue == Value ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}
	return ESlateCheckBoxState::Unchecked;
}

void FPhysicsConstraintComponentDetails::OnLimitRadioChanged( ESlateCheckBoxState::Type CheckType, TSharedPtr<IPropertyHandle> Property, uint8 Value )
{
	if (Property.IsValid() && CheckType == ESlateCheckBoxState::Checked)
	{
		Property->SetValue(Value);
	}
}

#undef LOCTEXT_NAMESPACE
