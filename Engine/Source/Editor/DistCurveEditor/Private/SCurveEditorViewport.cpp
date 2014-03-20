// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DistCurveEditorModule.h"
#include "SDistributionCurveEditor.h"
#include "CurveEditorViewportClient.h"
#include "SCurveEditorViewport.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"


SCurveEditorViewport::~SCurveEditorViewport()
{
}

void SCurveEditorViewport::Construct(const FArguments& InArgs)
{
	CurveEditorPtr = InArgs._CurveEditor;
	
	bool bAlwaysShowScrollbar = InArgs._CurveEdOptions.bAlwaysShowScrollbar;

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				SAssignNew(ViewportWidget, SViewport)
				.EnableGammaCorrection(false)
				.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
				.ShowEffectWhenDisabled(false)
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			bAlwaysShowScrollbar ? 
			( 
				SAssignNew(ViewportVerticalScrollBar, SScrollBar)
				.AlwaysShowScrollbar(true)
				.OnUserScrolled(this, &SCurveEditorViewport::OnViewportVerticalScrollBarScrolled)
			) :
			(
				SAssignNew(ViewportVerticalScrollBar, SScrollBar)
				.Visibility(this, &SCurveEditorViewport::GetViewportVerticalScrollBarVisibility)
				.OnUserScrolled(this, &SCurveEditorViewport::OnViewportVerticalScrollBarScrolled)
			)
		]
	];
	
	ViewportClient = MakeShareable(new FCurveEditorViewportClient(CurveEditorPtr, SharedThis(this)));

	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface( Viewport.ToSharedRef() );

}

void SCurveEditorViewport::RefreshViewport()
{
	Viewport->Invalidate();
	Viewport->InvalidateDisplay();
}

void SCurveEditorViewport::SetVerticalScrollBarPosition(float Position)
{
	float Ratio = ViewportClient->GetViewportVerticalScrollBarRatio();
	float MaxOffset = (Ratio < 1.0f)? 1.0f - Ratio: 0.0f;
	OnViewportVerticalScrollBarScrolled(MaxOffset * Position);
}

TSharedPtr<FSceneViewport> SCurveEditorViewport::GetViewport() const
{
	return Viewport;
}

TSharedPtr<FCurveEditorViewportClient> SCurveEditorViewport::GetViewportClient() const
{
	return ViewportClient;
}

TSharedPtr<SViewport> SCurveEditorViewport::GetViewportWidget() const
{
	return ViewportWidget;
}

TSharedPtr<SScrollBar> SCurveEditorViewport::GetVerticalScrollBar() const
{
	return ViewportVerticalScrollBar;
}

EVisibility SCurveEditorViewport::GetViewportVerticalScrollBarVisibility() const
{
	return (ViewportClient->GetViewportVerticalScrollBarRatio() < 1.0f)? EVisibility::Visible: EVisibility::Collapsed;
}

void SCurveEditorViewport::OnViewportVerticalScrollBarScrolled(float InScrollOffsetFraction)
{
	float Ratio = ViewportClient->GetViewportVerticalScrollBarRatio();
	float MaxOffset = (Ratio < 1.0f)? 1.0f - Ratio: 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);
	ViewportVerticalScrollBar->SetState(InScrollOffsetFraction, Ratio);
	RefreshViewport();
}

