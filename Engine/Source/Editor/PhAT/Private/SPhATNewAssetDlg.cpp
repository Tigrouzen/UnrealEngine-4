// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "SPhATNewAssetDlg.h"

// Add in the constants from the static mesh editor as we need them here too
const int32 DefaultHullCount = 4;
const int32 DefaultVertsPerHull = 12;
const int32 MaxHullCount = 24;
const int32 MinHullCount = 1;
const int32 MaxVertsPerHullCount = 32;
const int32 MinVertsPerHullCount = 6;

void SPhATNewAssetDlg::Construct(const FArguments& InArgs)
{
	NewBodyData = InArgs._NewBodyData.Get();
	NewBodyResponse = InArgs._NewBodyResponse.Get();
	ParentWindow = InArgs._ParentWindow.Get();
	ParentWindow.Pin()->SetWidgetToFocusOnActivate(SharedThis(this));

	check(NewBodyData && NewBodyResponse);

	// Initialize combobox options
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Sphyl/Sphere"))));
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Box"))));

	// Add in the ability to create a convex hull from the source geometry for skeletal meshes
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Single Convex Hull"))));
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Multi Convex Hull"))));

	WeightOptions.Add(TSharedPtr<FString>(new FString(TEXT("Dominant Weight"))));
	WeightOptions.Add(TSharedPtr<FString>(new FString(TEXT("Any Weight"))));

	AngularConstraintModes.Add(TSharedPtr<FString>(new FString(TEXT("Limited"))));
	AngularConstraintModes.Add(TSharedPtr<FString>(new FString(TEXT("Locked"))));
	AngularConstraintModes.Add(TSharedPtr<FString>(new FString(TEXT("Free"))));

	// Initialize new body parameters
	NewBodyData->Initialize();

	NewBodyData->MaxHullCount = DefaultHullCount;
	NewBodyData->MaxHullVerts = DefaultVertsPerHull;

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(4.0f)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				// Option widgets
				SNew(SUniformGridPanel)
				+SUniformGridPanel::Slot(0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "MinimumBoneSizeLabel", "Minimum Bone Size:").ToString())
				]
				+SUniformGridPanel::Slot(1, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SPhATNewAssetDlg::GetMinBoneSize )
					.OnValueCommitted(this, &SPhATNewAssetDlg::OnMinimumBoneSizeCommitted)
				]
				+SUniformGridPanel::Slot(0, 1)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "OrientAlongBoneLabel", "Orient Along Bone:").ToString())
				]
				+SUniformGridPanel::Slot(1, 1)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bAlignDownBone)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleOrientAlongBone)
				]
				+SUniformGridPanel::Slot(0, 2)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "CollisionGeometryLabel", "Collision Geometry:").ToString())
				]
				+SUniformGridPanel::Slot(1, 2)
				.VAlign(VAlign_Center)
				[
					SNew(STextComboBox)
					.OptionsSource(&CollisionGeometryOptions)
					.InitiallySelectedItem(CollisionGeometryOptions[0])
					.OnSelectionChanged(this, &SPhATNewAssetDlg::OnCollisionGeometrySelectionChanged)
				]
				+SUniformGridPanel::Slot(0, 3)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "UseVertsWithLabel", "Use Verts With:").ToString())
				]
				+SUniformGridPanel::Slot(1, 3)
				.VAlign(VAlign_Center)
				[
					SNew(STextComboBox)
					.OptionsSource(&WeightOptions)
					.InitiallySelectedItem(WeightOptions[0])
					.OnSelectionChanged(this, &SPhATNewAssetDlg::OnWeightOptionSelectionChanged)
				]
				+SUniformGridPanel::Slot(0, 4)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "CreateJointsLabel", "Create Joints:").ToString())
				]
				+SUniformGridPanel::Slot(1, 4)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bCreateJoints)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleCreateJoints)
				]
				+SUniformGridPanel::Slot(0, 5)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "AngularConstraintMode", "Default Angular Constraint Mode:").ToString())
				]
				+SUniformGridPanel::Slot(1, 5)
				.VAlign(VAlign_Center)
				[
					SNew(STextComboBox)
					.OptionsSource(&AngularConstraintModes)
					.InitiallySelectedItem(AngularConstraintModes[0])
					.OnSelectionChanged(this, &SPhATNewAssetDlg::OnAngularConstraintModeSelectionChanged)
				]
				+SUniformGridPanel::Slot(0, 6)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "WalkPastSmallBonesLabel", "Walk Past Small Bones:").ToString())
				]
				+SUniformGridPanel::Slot(1, 6)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bWalkPastSmall)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleWalkPastSmallBones)
				]
				+SUniformGridPanel::Slot(0, 7)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "CreateBodyForAllBonesLabel", "Create Body For All Bones:").ToString())
				]
				+SUniformGridPanel::Slot(1, 7)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bBodyForAll)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleCreateBodyForAllBones)
				]
				// Add in the UI options for the max hulls and the max verts on the hulls
				+SUniformGridPanel::Slot(0, 7)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.Text( NSLOCTEXT("PhAT", "MaxNumHulls_ConvexDecomp", "Max Num Hulls").ToString() )
					]
				+SUniformGridPanel::Slot(1, 7)
					[
						SAssignNew(MaxHull, SSpinBox<int32>)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.MinValue(MinHullCount)
						.MaxValue(MaxHullCount)
						.Value( this, &SPhATNewAssetDlg::GetHullCount )
						.OnValueChanged( this, &SPhATNewAssetDlg::OnHullCountChanged )
					]
				+SUniformGridPanel::Slot(0, 8)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.Text( NSLOCTEXT("PhAT", "MaxHullVerts_ConvexDecomp", "Max Hull Verts").ToString() )
					]
				+SUniformGridPanel::Slot(1, 8)
					[
						SAssignNew(MaxVertsPerHull, SSpinBox<int32>)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.MinValue(MinVertsPerHullCount)
						.MaxValue(MaxVertsPerHullCount)
						.Value( this, &SPhATNewAssetDlg::GetVertsPerHullCount )
						.OnValueChanged( this, &SPhATNewAssetDlg::OnVertsPerHullCountChanged )
					]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("PhAT", "OkButtonText", "Ok").ToString())
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(FOnClicked::CreateSP<SPhATNewAssetDlg, EAppReturnType::Type>(this, &SPhATNewAssetDlg::OnClicked, EAppReturnType::Ok))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("PhAT", "CancelButtonText", "Cancel").ToString())
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(FOnClicked::CreateSP<SPhATNewAssetDlg, EAppReturnType::Type>(this, &SPhATNewAssetDlg::OnClicked, EAppReturnType::Cancel))
				]
			]
		]
	];
}

FReply SPhATNewAssetDlg::OnKeyDown(const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent)
{
	//see if we pressed the Enter or Spacebar keys
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		return OnClicked(EAppReturnType::Cancel);
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}

bool SPhATNewAssetDlg::SupportsKeyboardFocus() const
{
	return true;
}

FReply SPhATNewAssetDlg::OnClicked(EAppReturnType::Type InResponse)
{
	*NewBodyResponse = InResponse;
	ParentWindow.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

void SPhATNewAssetDlg::OnMinimumBoneSizeCommitted(float NewValue, ETextCommit::Type CommitInfo)
{
	NewBodyData->MinBoneSize = NewValue;
}

TOptional<float> SPhATNewAssetDlg::GetMinBoneSize() const
{
	TOptional<float> MinBoneSize;
	MinBoneSize = NewBodyData->MinBoneSize;
	return MinBoneSize;
}

void SPhATNewAssetDlg::OnCollisionGeometrySelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (*NewSelection == TEXT("Box"))
	{
		NewBodyData->GeomType = EFG_Box;
	}
	// Check to see if the user has picked that they want to create a convex hull
	else if ( *NewSelection == TEXT("Single Convex Hull") )
	{
		NewBodyData->GeomType = EFG_SingleConvexHull;		
	}
	else if ( *NewSelection == TEXT("Multi Convex Hull") )
	{
		NewBodyData->GeomType = EFG_MultiConvexHull;		
	}
	else
	{
		NewBodyData->GeomType = EFG_SphylSphere;
	}
}

void SPhATNewAssetDlg::OnWeightOptionSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (*NewSelection == TEXT("Dominant Weight"))
	{
		NewBodyData->VertWeight = EVW_DominantWeight;
	} 
	else
	{
		NewBodyData->VertWeight = EVW_AnyWeight;
	}
}

void SPhATNewAssetDlg::OnAngularConstraintModeSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (*NewSelection == TEXT("Limited"))
	{
		NewBodyData->AngularConstraintMode = ACM_Limited;
	} 
	else if (*NewSelection == TEXT("Locked"))
	{
		NewBodyData->AngularConstraintMode = ACM_Locked;
	}
	else if (*NewSelection == TEXT("Free"))
	{
		NewBodyData->AngularConstraintMode = ACM_Free;
	}
}

void SPhATNewAssetDlg::OnToggleOrientAlongBone(ESlateCheckBoxState::Type InCheckboxState)
{
	NewBodyData->bAlignDownBone = (InCheckboxState == ESlateCheckBoxState::Checked);
}

void SPhATNewAssetDlg::OnToggleCreateJoints(ESlateCheckBoxState::Type InCheckboxState)
{
	NewBodyData->bCreateJoints = (InCheckboxState == ESlateCheckBoxState::Checked);
}

void SPhATNewAssetDlg::OnToggleWalkPastSmallBones(ESlateCheckBoxState::Type InCheckboxState)
{
	NewBodyData->bWalkPastSmall = (InCheckboxState == ESlateCheckBoxState::Checked);
}

void SPhATNewAssetDlg::OnToggleCreateBodyForAllBones(ESlateCheckBoxState::Type InCheckboxState)
{
	NewBodyData->bBodyForAll = (InCheckboxState == ESlateCheckBoxState::Checked);
}

EVisibility SPhATNewAssetDlg::GetHullOptionsVisibility() const
{
	return NewBodyData->GeomType == EFG_MultiConvexHull ? EVisibility::Visible : EVisibility::Collapsed;
}

void SPhATNewAssetDlg::OnHullCountChanged(int32 InNewValue)
{
	NewBodyData->MaxHullCount = InNewValue;
}

void SPhATNewAssetDlg::OnVertsPerHullCountChanged(int32 InNewValue)
{
	NewBodyData->MaxHullVerts = InNewValue;
}

int32 SPhATNewAssetDlg::GetHullCount() const
{
	return NewBodyData->MaxHullCount;
}

int32 SPhATNewAssetDlg::GetVertsPerHullCount() const
{
	return NewBodyData->MaxHullVerts;
}
