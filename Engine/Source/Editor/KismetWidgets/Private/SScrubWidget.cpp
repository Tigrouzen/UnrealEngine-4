// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "KismetWidgetsPrivatePCH.h"
#include "SScrubWidget.h"


#define LOCTEXT_NAMESPACE "ScrubWidget"

static const float MinStepLengh=15.f;

/** This function is used by a few random widgets and is mostly arbitrary. It could be moved anywhere. */
int32 SScrubWidget::GetDivider( float InputMinX, float InputMaxX, FVector2D WidgetSize, float SequenceLength, int32 NumFrames )
{
	check (NumFrames!=0);

	FTrackScaleInfo TimeScaleInfo(InputMinX, InputMaxX, 0.f, 0.f, WidgetSize);

	float TimePerKey = (NumFrames>1)? SequenceLength/(float)(NumFrames-1) : 0;
	float TotalWidgetWidth = TimeScaleInfo.WidgetSize.X;
	float NumKeys =  TimeScaleInfo.ViewInputRange/TimePerKey;
	float KeyWidgetWidth = TotalWidgetWidth/ NumKeys;
	int32 Divider = 1; 
	if ( KeyWidgetWidth > 0 )
	{
		Divider = FMath::Max<int32>(50/KeyWidgetWidth, 1);
	}
	return Divider;
}

void SScrubWidget::Construct( const SScrubWidget::FArguments& InArgs )
{
	ValueAttribute = InArgs._Value;
	OnValueChanged = InArgs._OnValueChanged;
	OnBeginSliderMovement = InArgs._OnBeginSliderMovement;
	OnEndSliderMovement = InArgs._OnEndSliderMovement;

	DistanceDragged = 0.0f;
	NumOfKeys = InArgs._NumOfKeys;
	SequenceLength = InArgs._SequenceLength;
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;
	OnCropAnimSequence = InArgs._OnCropAnimSequence;
	OnReZeroAnimSequence = InArgs._OnReZeroAnimSequence;

	DraggableBars = InArgs._DraggableBars;
	OnBarDrag = InArgs._OnBarDrag;

	bMouseMovedDuringPanning = false;
	bDragging = false;
	bPanning = false;
	DraggableBarIndex = INDEX_NONE;
	DraggingBar = false;

	bAllowZoom = InArgs._bAllowZoom;
}

int32 SScrubWidget::OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const bool bActiveFeedback = IsHovered() || bDragging;
	const FSlateBrush* BackgroundImage = bActiveFeedback ?
		FEditorStyle::GetBrush("SpinBox.Background.Hovered") :
		FEditorStyle::GetBrush("SpinBox.Background");

	const FSlateBrush* FillImage = bActiveFeedback ?
		FEditorStyle::GetBrush("SpinBox.Fill.Hovered") :
		FEditorStyle::GetBrush("SpinBox.Fill");

	const int32 BackgroundLayer = LayerId;

	FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

	const bool bEnabled = ShouldBeEnabled( bParentEnabled );
	const ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const int32 TextLayer = BackgroundLayer + 1;

	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush( TEXT( "ProgressBar.Background" ) );
	FSlateRect GeomRect = AllottedGeometry.GetRect();

	if ( NumOfKeys.Get() > 0 && SequenceLength.Get() > 0)
	{
		FTrackScaleInfo TimeScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, AllottedGeometry.Size);
		int32 Divider = SScrubWidget::GetDivider( ViewInputMin.Get(), ViewInputMax.Get(), AllottedGeometry.Size, SequenceLength.Get(), NumOfKeys.Get() );
		float HalfDivider = Divider/2.f;
		
		int32 TotalNumKeys = NumOfKeys.Get();
		float TimePerKey = (TotalNumKeys > 1)? SequenceLength.Get()/(float)(TotalNumKeys-1) : 0;
		for ( float KeyVal=0; KeyVal<TotalNumKeys; KeyVal += HalfDivider )
		{
			float CurValue = KeyVal*TimePerKey;
			float XPos = TimeScaleInfo.InputToLocalX(CurValue);

			if ( FGenericPlatformMath::Fmod(KeyVal, Divider) == 0.f )
			{
				FVector2D Offset(XPos, 0.f);
				FVector2D Size(1, GeomRect.Bottom-GeomRect.Top);
				// draw each box with key frame
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					BackgroundLayer,
					AllottedGeometry.ToPaintGeometry(Offset, Size),
					StyleInfo,
					MyClippingRect,
					DrawEffects,
					InWidgetStyle.GetColorAndOpacityTint()
					);

				int32 FrameNumber = KeyVal;
				FString FrameString = FString::Printf(TEXT("%d"), (FrameNumber));
				FVector2D TextOffset(XPos+2.f, 0.f);

				const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

				FSlateDrawElement::MakeText(
					OutDrawElements,
					TextLayer, 
					AllottedGeometry.ToPaintGeometry(TextOffset, TextSize), 
					FrameString, 
					SmallLayoutFont, 
					MyClippingRect, 
					DrawEffects);
			}
 			else if (HalfDivider > 1.f)
 			{
 				float Height = GeomRect.Bottom-GeomRect.Top;
 				FVector2D Offset(XPos, Height*0.25f);
 				FVector2D Size(1, Height*0.5f);
 				// draw each box with key frame
 				FSlateDrawElement::MakeBox(
 					OutDrawElements,
 					BackgroundLayer,
					AllottedGeometry.ToPaintGeometry(Offset, Size),
 					StyleInfo,
 					MyClippingRect,
 					DrawEffects,
 					InWidgetStyle.GetColorAndOpacityTint()
 					);
 
 			}
		}

		const float XPos = TimeScaleInfo.InputToLocalX(ValueAttribute.Get());
		const float Height = AllottedGeometry.Size.Y;
		const FVector2D Offset( XPos - Height*0.25f, 0.f );

		const int32 ArrowLayer = TextLayer + 1;
		FPaintGeometry MyGeometry =	AllottedGeometry.ToPaintGeometry( Offset, FVector2D(Height*0.5f, Height) );
		FLinearColor ScrubColor = InWidgetStyle.GetColorAndOpacityTint();
		ScrubColor.A = ScrubColor.A*0.5f;
		ScrubColor.B *= 0.1f;
		ScrubColor.G *= 0.1f;
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			ArrowLayer, 
			MyGeometry, 
			StyleInfo, 
			MyClippingRect, 
			DrawEffects, 
			ScrubColor
			);


		// Draggable Bars
		for ( int32 BarIdx=0; DraggableBars.IsBound() && BarIdx < DraggableBars.Get().Num(); BarIdx++ )
		{
			float BarXPos = TimeScaleInfo.InputToLocalX(DraggableBars.Get()[BarIdx]);
			FVector2D BarOffset(BarXPos-2.f, 0.f);
			FVector2D Size(4.f, GeomRect.Bottom-GeomRect.Top);

			FLinearColor BarColor = InWidgetStyle.GetColorAndOpacityTint();	
			BarColor.R *= 0.1f;
			BarColor.G *= 0.1f;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				ArrowLayer+1,
				AllottedGeometry.ToPaintGeometry(Offset, Size),
				StyleInfo,
				MyClippingRect,
				DrawEffects,
				BarColor
				);
		}

		return FMath::Max( ArrowLayer, SCompoundWidget::OnPaint(AllottedGeometry, MyClippingRect, OutDrawElements, ArrowLayer, InWidgetStyle, bEnabled ) );
	}

	return SCompoundWidget::OnPaint(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );
}

FReply SScrubWidget::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bAllowZoom;

	bMouseMovedDuringPanning = false;
	if ( bHandleLeftMouseButton )
	{
		if(DraggableBarIndex != INDEX_NONE)
		{
			DraggingBar = true;
		} 
		else
		{
			DistanceDragged = 0;
		}

		// This has prevent throttling on so that viewports continue to run whilst dragging the slider
		return FReply::Handled().CaptureMouse( SharedThis(this) ).PreventThrottling();
	}
	else if ( bHandleRightMouseButton )
	{
		bPanning = true;

		// Always capture mouse if we left or right click on the widget
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}
	
FReply SScrubWidget::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && this->HasMouseCapture();
	bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && this->HasMouseCapture() && bAllowZoom;

	if ( bHandleRightMouseButton )
	{
		bPanning = false;

		FTrackScaleInfo TimeScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, MyGeometry.Size);
		FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetLastScreenSpacePosition());
		float NewValue = TimeScaleInfo.LocalXToInput(CursorPos.X);

		if( !bMouseMovedDuringPanning )
		{
			CreateContextMenu( NewValue );
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	else if ( bHandleLeftMouseButton )
	{
		if(DraggingBar)
		{
			DraggingBar = false;
		}
		else if( bDragging )
		{
			OnEndSliderMovement.ExecuteIfBound( ValueAttribute.Get() );
		}
		else
		{
			FTrackScaleInfo TimeScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, MyGeometry.Size);
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetLastScreenSpacePosition());
			float NewValue = TimeScaleInfo.LocalXToInput(CursorPos.X);

			CommitValue( NewValue, true, false );
		}

		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();

	}

	return FReply::Unhandled();
}
	
FReply SScrubWidget::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Bar Dragging
	if(DraggingBar)
	{
		// Update bar if we are dragging
		FVector2D CursorPos = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
		FTrackScaleInfo ScaleInfo(ViewInputMin.Get(),  ViewInputMax.Get(), 0.f, 0.f, MyGeometry.Size);
		float NewDataPos = FMath::Clamp( ScaleInfo.LocalXToInput(CursorPos.X), ViewInputMin.Get(), ViewInputMax.Get() );
		OnBarDrag.ExecuteIfBound(DraggableBarIndex, NewDataPos);
	}
	else
	{
		// Update what bar we are hovering over
		FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		FTrackScaleInfo ScaleInfo(ViewInputMin.Get(),  ViewInputMax.Get(), 0.f, 0.f, MyGeometry.Size);
		DraggableBarIndex = INDEX_NONE;
		for ( int32 I=0; DraggableBars.IsBound() && I < DraggableBars.Get().Num(); I++ )
		{
			if( FMath::Abs( ScaleInfo.InputToLocalX(DraggableBars.Get()[I]) - CursorPos.X ) < 10 )
			{
				DraggableBarIndex = I;
				break;
			}
		}
	}

	if ( this->HasMouseCapture() )
	{
		if (MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) && bPanning)
		{
			FTrackScaleInfo ScaleInfo(ViewInputMin.Get(),  ViewInputMax.Get(), 0.f, 0.f, MyGeometry.Size);
			FVector2D ScreenDelta = MouseEvent.GetCursorDelta();
			float InputDeltaX = ScreenDelta.X/ScaleInfo.PixelsPerInput;

			bMouseMovedDuringPanning |= !ScreenDelta.IsNearlyZero(0.001f);

			float NewViewInputMin = ViewInputMin.Get() - InputDeltaX;
			float NewViewInputMax = ViewInputMax.Get() - InputDeltaX;
			// we'd like to keep  the range if outside when panning
			if ( NewViewInputMin < 0.f )
			{
				NewViewInputMin = 0.f;
				NewViewInputMax = ScaleInfo.ViewInputRange;
			}
			else if ( NewViewInputMax > SequenceLength.Get() )
			{
				NewViewInputMax = SequenceLength.Get();
				NewViewInputMin = NewViewInputMax - ScaleInfo.ViewInputRange;
			}

			OnSetInputViewRange.ExecuteIfBound(NewViewInputMin, NewViewInputMax);
		}
		else if (!bDragging)
		{
			DistanceDragged += FMath::Abs(MouseEvent.GetCursorDelta().X);
			if ( DistanceDragged > SlateDragStartDistance )
			{
				bDragging = true;
			}
			if( bDragging )
			{
				OnBeginSliderMovement.ExecuteIfBound();
			}
		}
		else if (bDragging)
		{
			FTrackScaleInfo TimeScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, MyGeometry.Size);
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetLastScreenSpacePosition());
			float NewValue = TimeScaleInfo.LocalXToInput(CursorPos.X);

			CommitValue( NewValue, true, false );
		}
		return FReply::Handled();
	}

	

	return FReply::Unhandled();
}

// FCursorReply SScrubWidget::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
// {
// 	return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight );
// }


void SScrubWidget::CommitValue( float NewValue, bool bSliderClamp, bool bCommittedFromText )
{
	if ( !ValueAttribute.IsBound() )
	{
		ValueAttribute.Set( NewValue );
	}

	OnValueChanged.ExecuteIfBound( NewValue );
}

FVector2D SScrubWidget::ComputeDesiredSize() const 
{
	return FVector2D(100, 30);
}

FReply SScrubWidget::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( bAllowZoom && OnSetInputViewRange.IsBound() )
	{
		const float ZoomDelta = -0.1f * MouseEvent.GetWheelDelta();

		{
			const float InputViewSize = ViewInputMax.Get() - ViewInputMin.Get();
			const float InputChange = InputViewSize * ZoomDelta;

			float ViewMinInput = ViewInputMin.Get() - (InputChange * 0.5f);
			float ViewMaxInput = ViewInputMax.Get() + (InputChange * 0.5f);

			OnSetInputViewRange.Execute(ViewMinInput, ViewMaxInput);
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FCursorReply SScrubWidget::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if (DraggableBarIndex != INDEX_NONE)
	{
		return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight );
	}

	return FCursorReply::Unhandled();
}

void SScrubWidget::CreateContextMenu(float CurrentFrameTime)
{
	if( SequenceLength.Get() >=  MINIMUM_ANIMATION_LENGTH )
	{
		const bool CloseAfterSelection = true;
		FMenuBuilder MenuBuilder( CloseAfterSelection, NULL );

		MenuBuilder.BeginSection("SequenceEditingContext", LOCTEXT("SequenceEditing", "Sequence Editing") );
		{
			float CurrentFrameFraction = CurrentFrameTime / SequenceLength.Get();
			uint32 CurrentFrameNumber = CurrentFrameFraction * NumOfKeys.Get();

			FUIAction Action;
			FText Label;

			//Menu - "Remove Before"
			//Only show this option if the selected frame is greater than frame 1 (first frame)
			if( CurrentFrameNumber > 0 )
			{
				CurrentFrameFraction = float(CurrentFrameNumber) / (float)NumOfKeys.Get();

				//Corrected frame time based on selected frame number
				float CorrectedFrameTime = CurrentFrameFraction * SequenceLength.Get();

				Action = FUIAction( FExecuteAction::CreateSP( this, &SScrubWidget::OnSequenceCropped, true, CorrectedFrameTime ) );
				Label = FText::Format( LOCTEXT("RemoveTillFrame", "Remove till frame {0}"), FText::AsNumber(CurrentFrameNumber) );
				MenuBuilder.AddMenuEntry( Label, LOCTEXT("RemoveBefore_ToolTip", "Remove sequence before current position"), FSlateIcon(), Action);
			}

			uint32 NextFrameNumber = CurrentFrameNumber + 1;

			//Menu - "Remove After"
			//Only show this option if next frame (CurrentFrameNumber + 1) is valid
			if( NextFrameNumber < NumOfKeys.Get() )
			{
				float NextFrameFraction = float(NextFrameNumber) / (float)NumOfKeys.Get();
				float NextFrameTime = NextFrameFraction * SequenceLength.Get();
				Action = FUIAction( FExecuteAction::CreateSP( this, &SScrubWidget::OnSequenceCropped, false, NextFrameTime ) );
				Label = FText::Format( LOCTEXT("RemoveFromFrame", "Remove from frame {0}"), FText::AsNumber(NextFrameNumber) );
				MenuBuilder.AddMenuEntry( Label, LOCTEXT("RemoveAfter_ToolTip", "Remove sequence after current position"), FSlateIcon(), Action);
			}

			//Menu - "ReZero"
			Action = FUIAction( FExecuteAction::CreateSP( this, &SScrubWidget::OnReZero ) );
			Label = FText::Format( LOCTEXT("ReZeroAtFrame", "ReZero at frame {0}"), FText::AsNumber(CurrentFrameNumber) );
			MenuBuilder.AddMenuEntry( Label, LOCTEXT("ReZeroAtFrame_ToolTip", "ReZero sequence at the current frame"), FSlateIcon(), Action);
		}
		MenuBuilder.EndSection();

		FSlateApplication::Get().PushMenu( SharedThis( this ), MenuBuilder.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu ) );
	}
}

void SScrubWidget::OnSequenceCropped( bool bFromStart, float CurrentFrameTime )
{
	OnCropAnimSequence.ExecuteIfBound( bFromStart, CurrentFrameTime );

	//Update scrub widget's min and max view output.
	OnSetInputViewRange.ExecuteIfBound( ViewInputMin.Get(), ViewInputMax.Get() );
}

void SScrubWidget::OnReZero()
{
	OnReZeroAnimSequence.ExecuteIfBound();
}


#undef LOCTEXT_NAMESPACE
