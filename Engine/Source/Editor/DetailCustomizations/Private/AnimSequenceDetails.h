// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreviewScene.h"

class FAnimSequenceDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) OVERRIDE;

	/** Create an override for the supplide Property */
	void CreateOverridenProperty(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& AdditiveSettingsCategory, TSharedPtr<IPropertyHandle> PropertyHandle, TAttribute<EVisibility> VisibilityAttribute);

	/** Functions to control property visibility */
	EVisibility ShouldShowRefPoseType() const;
	EVisibility ShouldShowRefAnimInfo() const;
	EVisibility ShouldShowRefFrameIndex() const;

	virtual ~FAnimSequenceDetails();

private:
	TWeakObjectPtr<USkeleton> TargetSkeleton;

	TSharedPtr<IPropertyHandle> AdditiveAnimTypeHandle;
	TSharedPtr<IPropertyHandle> RefPoseTypeHandle;
	TSharedPtr<IPropertyHandle> RefPoseSeqHandle;
	TSharedPtr<IPropertyHandle> RefFrameIndexHandle;

	// retarget source handler
	TSharedPtr<IPropertyHandle> RetargetSourceNameHandler;

	TSharedPtr<class SComboBox< TSharedPtr<FString> > > RetargetSourceComboBox;
	TArray< TSharedPtr< FString > >						RetargetSourceComboList;

	TSharedRef<SWidget> MakeRetargetSourceComboWidget( TSharedPtr<FString> InItem );
	void OnRetargetSourceChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo  );
	FString GetRetargetSourceComboBoxContent() const;
	FString GetRetargetSourceComboBoxToolTip() const;
	void OnRetargetSourceComboOpening();
	TSharedPtr<FString> GetRetargetSourceString(FName RetargetSourceName) const;

	USkeleton::FOnRetargetSourceChanged OnDelegateRetargetSourceChanged;
	void RegisterRetargetSourceChanged();
	void DelegateRetargetSourceChanged();
};

///////////////////////////////////////////////////
class SAnimationRefPoseViewport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimationRefPoseViewport )
	{}

		SLATE_ARGUMENT( USkeleton*, Skeleton )
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, AnimRefPropertyHandle )
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, RefPoseTypeHandle )
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, RefFrameIndexPropertyHandle )

	SLATE_END_ARGS()

public:
	SAnimationRefPoseViewport();
	virtual ~SAnimationRefPoseViewport();

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) OVERRIDE;
	void RefreshViewport();

private:
	void InitSkeleton();

	TSharedPtr<FEditorViewportClient> LevelViewportClient;

	TSharedPtr<IPropertyHandle> AnimRefPropertyHandle;
	TSharedPtr<IPropertyHandle> RefPoseTypeHandle;
	TSharedPtr<IPropertyHandle> RefFrameIndexPropertyHandle;

	/** Slate viewport for rendering and I/O */
	//TSharedPtr<class FSceneViewport> Viewport;
	TSharedPtr<SViewport> ViewportWidget;

	TSharedPtr<class FSceneViewport> SceneViewport;

	/** Skeleton */
	USkeleton* TargetSkeleton;
	UAnimSequence* AnimRef;

	FPreviewScene PreviewScene;
	class FFXSystemInterface* FXSystem;

	TSharedPtr<STextBlock> Description;

	class UDebugSkelMeshComponent* PreviewComponent;

	void CleanupComponent(USceneComponent* Component);
	bool IsVisible() const;

public:
	/** Get Min/Max Input of value **/
	float GetViewMinInput() const;
	float GetViewMaxInput() const;

	class UAnimSingleNodeInstance*	GetPreviewInstance() const;

	/** Optional, additional values to draw on the timeline **/
	TArray<float> GetBars() const;
	void OnBarDrag(int32 index, float newPos);
};
