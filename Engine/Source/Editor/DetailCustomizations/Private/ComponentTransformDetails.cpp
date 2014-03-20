// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ComponentTransformDetails.h"
#include "SVectorInputBox.h"
#include "SRotatorInputBox.h"
#include "PropertyCustomizationHelpers.h"
#include "ActorEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/ComponentEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FComponentTransformDetails"

class FScopedSwitchWorldForObject
{
public:
	FScopedSwitchWorldForObject( UObject* Object )
		: PrevWorld( NULL )
	{
		bool bRequiresPlayWorld = false;
		if( GUnrealEd->PlayWorld && !GIsPlayInEditorWorld )
		{
			UPackage* ObjectPackage = Object->GetOutermost();
			bRequiresPlayWorld = !!(ObjectPackage->PackageFlags & PKG_PlayInEditor);
		}

		if( bRequiresPlayWorld )
		{
			PrevWorld = SetPlayInEditorWorld( GUnrealEd->PlayWorld );
		}
	}

	~FScopedSwitchWorldForObject()
	{
		if( PrevWorld )
		{
			RestoreEditorWorld( PrevWorld );
		}
	}

private:
	UWorld* PrevWorld;
};

template<typename T>
static void PropagateTransformPropertyChange(UObject* InObject, UProperty* InProperty, const T& OldValue, const T& NewValue)
{
	check(InObject != NULL);
	check(InProperty != NULL);

	TArray<UObject*> ArchetypeInstances;
	FComponentEditorUtils::GetArchetypeInstances(InObject, ArchetypeInstances);
	for(int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstances.Num(); ++InstanceIndex)
	{
		USceneComponent* InstancedSceneComponent = FComponentEditorUtils::GetSceneComponent(ArchetypeInstances[InstanceIndex], InObject);
		if(InstancedSceneComponent != NULL)
		{
			// Propagate the change only if the current instanced value matches the previous default value
			T* CurValue = InProperty->ContainerPtrToValuePtr<T>(InstancedSceneComponent);
			if(CurValue != NULL && *CurValue == OldValue)
			{
				// Ensure that this instance will be included in any undo/redo operations, and record it into the transaction buffer.
				// Note: We don't do this for components that originate from script, because they will be re-instanced from the template after an undo, so there is no need to record them.
				if(!InstancedSceneComponent->bCreatedByConstructionScript)
				{
					InstancedSceneComponent->SetFlags(RF_Transactional);
					InstancedSceneComponent->Modify();
				}

				// We must also modify the owner, because we'll need script components to be reconstructed as part of an undo operation.
				AActor* Owner = InstancedSceneComponent->GetOwner();
				if(Owner != NULL)
				{
					Owner->Modify();
				}

				// Change the property value
				*CurValue = NewValue;

				// Re-register the component with the scene so that transforms are updated for display
				InstancedSceneComponent->ReregisterComponent();
			}
		}
	}
}


FComponentTransformDetails::FComponentTransformDetails( const TArray< TWeakObjectPtr<UObject> >& InSelectedObjects, const FSelectedActorInfo& InSelectedActorInfo, FNotifyHook* InNotifyHook )
	: SelectedActorInfo( InSelectedActorInfo )
	, SelectedObjects( InSelectedObjects )
	, bPreserveScaleRatio( false )
	, NotifyHook( InNotifyHook )
	, bEditingRotationInUI( false )
{
	GConfig->GetBool(TEXT("SelectionDetails"), TEXT("PreserveScaleRatio"), bPreserveScaleRatio, GEditorUserSettingsIni);

	// Capture selected actor rotations so that we can adjust them without worrying about the Quat conversions affecting the raw values
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = InSelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );

			if( RootComponent )
			{
				FRotator& RelativeRotation = ObjectToRelativeRotationMap.FindOrAdd(Object);
				RelativeRotation = RootComponent->RelativeRotation;
			}
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FComponentTransformDetails::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	UClass* SceneComponentClass = USceneComponent::StaticClass();
		

	FSlateFontInfo FontInfo = IDetailLayoutBuilder::GetDetailFont();
		
	// Location
	ChildrenBuilder.AddChildContent( LOCTEXT("Location", "Location").ToString() )
		.NameContent()
		[
			SNew( SBox )
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding( FMargin( 2.0f, 0.0f ) )
			[
				SNew( SHyperlink )
				.Text( this, &FComponentTransformDetails::GetLocationLabel )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnNavigate( this, &FComponentTransformDetails::OnLocationLabelClicked )
				.TextStyle(FEditorStyle::Get(), "DetailsView.HyperlinkStyle")
			]
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign( VAlign_Center )
			[
				SNew( SVectorInputBox )
				.X( this, &FComponentTransformDetails::GetLocationX )
				.Y( this, &FComponentTransformDetails::GetLocationY )
				.Z( this, &FComponentTransformDetails::GetLocationZ )
				.bColorAxisLabels( true )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXCommitted( this, &FComponentTransformDetails::OnSetLocation, 0 )
				.OnYCommitted( this, &FComponentTransformDetails::OnSetLocation, 1 )
				.OnZCommitted( this, &FComponentTransformDetails::OnSetLocation, 2 )
				.Font( FontInfo )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				// Just take up space for alignment
				SNew( SBox )
				.WidthOverride( 18.0f )
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FComponentTransformDetails::OnLocationResetClicked)
				.Visibility(this, &FComponentTransformDetails::GetLocationResetVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.Content()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
				]
			]
		];

	
	// Rotation
	ChildrenBuilder.AddChildContent( LOCTEXT("RotationFilter", "Rotation").ToString() )
		.NameContent()
		[
			SNew( SBox )
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding( FMargin( 2.0f, 0.0f ) )
			[
				SNew( SHyperlink )
				.Text( this, &FComponentTransformDetails::GetRotationLabel )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnNavigate( this, &FComponentTransformDetails::OnRotationLabelClicked )
				.TextStyle(FEditorStyle::Get(), "DetailsView.HyperlinkStyle")
			]
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign( VAlign_Center )
			[
				SNew( SRotatorInputBox )
				.AllowSpin( SelectedObjects.Num() == 1 ) 
				.Roll( this, &FComponentTransformDetails::GetRotationX )
				.Pitch( this, &FComponentTransformDetails::GetRotationY )
				.Yaw( this, &FComponentTransformDetails::GetRotationZ )
				.bColorAxisLabels( true )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginRotatonSlider )
				.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndRotationSlider )
				.OnRollChanged( this, &FComponentTransformDetails::OnSetRotation, false, 0 )
				.OnPitchChanged( this, &FComponentTransformDetails::OnSetRotation, false, 1 )
				.OnYawChanged( this, &FComponentTransformDetails::OnSetRotation, false, 2 )
				.OnRollCommitted( this, &FComponentTransformDetails::OnRotationCommitted, 0 )
				.OnPitchCommitted( this, &FComponentTransformDetails::OnRotationCommitted, 1 )
				.OnYawCommitted( this, &FComponentTransformDetails::OnRotationCommitted, 2 )
				.Font( FontInfo )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				// Just take up space for alignment
				SNew( SBox )
				.WidthOverride( 18.0f )
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FComponentTransformDetails::OnRotationResetClicked)
				.Visibility(this, &FComponentTransformDetails::GetRotationResetVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.Content()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
				]
			]
		];

			
	ChildrenBuilder.AddChildContent( LOCTEXT("ScaleFilter", "Scale").ToString() )
		.NameContent()
		[
			SNew( SBox )
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding( FMargin( 2.0f, 0.0f ) )
			[
				SNew( SHyperlink )
				.Text( this, &FComponentTransformDetails::GetScaleLabel )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnNavigate( this, &FComponentTransformDetails::OnScaleLabelClicked )
				.TextStyle(FEditorStyle::Get(), "DetailsView.HyperlinkStyle")
			]
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.VAlign( VAlign_Center )
			.FillWidth(1.0f)
			[
				SNew( SVectorInputBox )
				.X( this, &FComponentTransformDetails::GetScaleX )
				.Y( this, &FComponentTransformDetails::GetScaleY )
				.Z( this, &FComponentTransformDetails::GetScaleZ )
				.bColorAxisLabels( true )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXCommitted( this, &FComponentTransformDetails::OnSetScale, 0 )
				.OnYCommitted( this, &FComponentTransformDetails::OnSetScale, 1 )
				.OnZCommitted( this, &FComponentTransformDetails::OnSetScale, 2 )
				.ContextMenuExtenderX( this, &FComponentTransformDetails::ExtendXScaleContextMenu )
				.ContextMenuExtenderY( this, &FComponentTransformDetails::ExtendYScaleContextMenu )
				.ContextMenuExtenderZ( this, &FComponentTransformDetails::ExtendZScaleContextMenu )
				.Font( FontInfo )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth( 18.0f )
			[
				// Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
				SNew( SCheckBox )
				.IsChecked( this, &FComponentTransformDetails::IsPreserveScaleRatioChecked )
				.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnCheckStateChanged( this, &FComponentTransformDetails::OnPreserveScaleRatioToggled )
				.Style( FEditorStyle::Get(), "TransparentCheckBox" )
				.ToolTipText( LOCTEXT("PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled" ) )
				[
					SNew( SImage )
					.Image( this, &FComponentTransformDetails::GetPreserveScaleRatioImage )
					.ColorAndOpacity( FSlateColor::UseForeground() )
				]
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FComponentTransformDetails::OnScaleResetClicked)
				.Visibility(this, &FComponentTransformDetails::GetScaleResetVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.Content()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
				]
			]
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FComponentTransformDetails::Tick( float DeltaTime ) 
{
	CacheTransform();
}

bool FComponentTransformDetails::GetIsEnabled( ) const
{
	return !GEditor->HasLockedActors() || SelectedActorInfo.NumSelected == 0;
}

const FSlateBrush* FComponentTransformDetails::GetPreserveScaleRatioImage() const
{
	return bPreserveScaleRatio ? FEditorStyle::GetBrush( TEXT("GenericLock") ) : FEditorStyle::GetBrush( TEXT("GenericUnlock") ) ;
}

ESlateCheckBoxState::Type FComponentTransformDetails::IsPreserveScaleRatioChecked() const
{
	return bPreserveScaleRatio ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

void FComponentTransformDetails::OnPreserveScaleRatioToggled( ESlateCheckBoxState::Type NewState )
{
	bPreserveScaleRatio = (NewState == ESlateCheckBoxState::Checked) ? true : false;
	GConfig->SetBool(TEXT("SelectionDetails"), TEXT("PreserveScaleRatio"), bPreserveScaleRatio, GEditorUserSettingsIni);
}

FString FComponentTransformDetails::GetLocationLabel() const
{
	return bAbsoluteLocation ? LOCTEXT( "AbsoluteLocation", "Absolute Location" ).ToString() : LOCTEXT( "Location", "Location" ).ToString();
}

FString FComponentTransformDetails::GetRotationLabel() const
{
	return bAbsoluteRotation ? LOCTEXT( "AbsoluteRotation", "Absolute Rotation" ).ToString() : LOCTEXT( "Rotation", "Rotation" ).ToString();
}

FString FComponentTransformDetails::GetScaleLabel() const
{
	return bAbsoluteScale ? LOCTEXT( "AbsoluteScale", "Absolute Scale" ).ToString() : LOCTEXT( "Scale", "Scale" ).ToString();
}

void FComponentTransformDetails::OnLocationLabelClicked( )
{
	UProperty* AbsoluteLocationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "bAbsoluteLocation" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
			if( RootComponent )
			{
				if( !bBeganTransaction )
				{
					// NOTE: One transaction per change, not per actor
					GEditor->BeginTransaction( LOCTEXT("ToggleAbsoluteLocation", "Toggle Absolute Location" ) );
					bBeganTransaction = true;
				}

				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				Object->PreEditChange( AbsoluteLocationProperty );

				RootComponent->bAbsoluteLocation = !RootComponent->bAbsoluteLocation;

				FPropertyChangedEvent PropertyChangedEvent( AbsoluteLocationProperty );
				Object->PostEditChangeProperty( PropertyChangedEvent );

				// If this is a default object or subobject, propagate the change out to any current instances of this object
				if(Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
				{
					uint32 NewValue = RootComponent->bAbsoluteLocation;
					uint32 OldValue = !NewValue;
					PropagateTransformPropertyChange(Object, AbsoluteLocationProperty, OldValue, NewValue);
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();

		GUnrealEd->RedrawLevelEditingViewports();
	}

}

void FComponentTransformDetails::OnRotationLabelClicked( )
{
	UProperty* AbsoluteRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "bAbsoluteRotation" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
			if( RootComponent )
			{
				if( !bBeganTransaction )
				{
					// NOTE: One transaction per change, not per actor
					GEditor->BeginTransaction( LOCTEXT("ToggleAbsoluteRotation", "Toggle Absolute Rotation" ) );
					bBeganTransaction = true;
				}

				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				Object->PreEditChange( AbsoluteRotationProperty );

				RootComponent->bAbsoluteRotation = !RootComponent->bAbsoluteRotation;

				FPropertyChangedEvent PropertyChangedEvent( AbsoluteRotationProperty );
				Object->PostEditChangeProperty( PropertyChangedEvent );

				// If this is a default object or subobject, propagate the change out to any current instances of this object
				if(Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
				{
					uint32 NewValue = RootComponent->bAbsoluteRotation;
					uint32 OldValue = !NewValue;
					PropagateTransformPropertyChange(Object, AbsoluteRotationProperty, OldValue, NewValue);
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();

		GUnrealEd->RedrawLevelEditingViewports();
	}
}

void FComponentTransformDetails::OnScaleLabelClicked( )
{
	UProperty* AbsoluteScaleProperty = FindField<UProperty>( USceneComponent::StaticClass(), "bAbsoluteScale" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
			if( RootComponent )
			{
				if( !bBeganTransaction )
				{
					// NOTE: One transaction per change, not per actor
					GEditor->BeginTransaction( LOCTEXT("ToggleAbsoluteScale", "Toggle Absolute Scale" ) );
					bBeganTransaction = true;
				}

				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				Object->PreEditChange( AbsoluteScaleProperty );

				RootComponent->bAbsoluteScale = !RootComponent->bAbsoluteScale;

				FPropertyChangedEvent PropertyChangedEvent( AbsoluteScaleProperty );
				Object->PostEditChangeProperty( PropertyChangedEvent );

				// If this is a default object or subobject, propagate the change out to any current instances of this object
				if(Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
				{
					uint32 NewValue = RootComponent->bAbsoluteScale;
					uint32 OldValue = !NewValue;
					PropagateTransformPropertyChange(Object, AbsoluteScaleProperty, OldValue, NewValue);
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();
		GUnrealEd->RedrawLevelEditingViewports();
	}

}

FReply FComponentTransformDetails::OnLocationResetClicked()
{
	const FText TransactionName = LOCTEXT("ResetLocation", "Reset Location");
	FScopedTransaction Transaction(TransactionName);

	OnSetLocation(0.f, ETextCommit::Default, 0);
	OnSetLocation(0.f, ETextCommit::Default, 1);
	OnSetLocation(0.f, ETextCommit::Default, 2);

	return FReply::Handled();
}

FReply FComponentTransformDetails::OnRotationResetClicked()
{
	const FText TransactionName = LOCTEXT("ResetRotation", "Reset Rotation");
	FScopedTransaction Transaction(TransactionName);

	OnSetRotation(0.f, true, 0);
	OnSetRotation(0.f, true, 1);
	OnSetRotation(0.f, true, 2);

	return FReply::Handled();
}

FReply FComponentTransformDetails::OnScaleResetClicked()
{
	const FText TransactionName = LOCTEXT("ResetScale", "Reset Scale");
	FScopedTransaction Transaction(TransactionName);

	ScaleObject(1.f, 0, false, TransactionName);
	ScaleObject(1.f, 1, false, TransactionName);
	ScaleObject(1.f, 2, false, TransactionName);

	return FReply::Handled();
}

void FComponentTransformDetails::ExtendXScaleContextMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "ScaleOperations", LOCTEXT( "ScaleOperations", "Scale Operations" ) );
	MenuBuilder.AddMenuEntry( 
		LOCTEXT( "MirrorValueX", "Mirror X" ),  
		LOCTEXT( "MirrorValueX_Tooltip", "Mirror scale value on the X axis" ), 
		FSlateIcon(), 		
		FUIAction( FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnXScaleMirrored ), FCanExecuteAction() )
	);
	MenuBuilder.EndSection();
}

void FComponentTransformDetails::ExtendYScaleContextMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "ScaleOperations", LOCTEXT( "ScaleOperations", "Scale Operations" ) );
	MenuBuilder.AddMenuEntry( 
		LOCTEXT( "MirrorValueY", "Mirror Y" ),  
		LOCTEXT( "MirrorValueY_Tooltip", "Mirror scale value on the Y axis" ), 
		FSlateIcon(), 		
		FUIAction( FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnYScaleMirrored ), FCanExecuteAction() )
	);
	MenuBuilder.EndSection();
}

void FComponentTransformDetails::ExtendZScaleContextMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "ScaleOperations", LOCTEXT( "ScaleOperations", "Scale Operations" ) );
	MenuBuilder.AddMenuEntry( 
		LOCTEXT( "MirrorValueZ", "Mirror Z" ),  
		LOCTEXT( "MirrorValueZ_Tooltip", "Mirror scale value on the Z axis" ), 
		FSlateIcon(), 		
		FUIAction( FExecuteAction::CreateSP( this, &FComponentTransformDetails::OnZScaleMirrored ), FCanExecuteAction() )
	);
	MenuBuilder.EndSection();
}

void FComponentTransformDetails::OnXScaleMirrored()
{
	ScaleObject( 1.0f, 0, true, LOCTEXT( "MirrorActorScaleX", "Mirror actor scale X" ) );
}

void FComponentTransformDetails::OnYScaleMirrored()
{
	ScaleObject( 1.0f, 1, true, LOCTEXT( "MirrorActorScaleY", "Mirror actor scale Y" ) );
}

void FComponentTransformDetails::OnZScaleMirrored()
{
	ScaleObject( 1.0f, 2, true, LOCTEXT( "MirrorActorScaleZ", "Mirror actor scale Z" ) );
}

/**
 * Cache the entire transform at it is seen by the input boxes so we dont have to iterate over the selected actors multiple times                   
 */
void FComponentTransformDetails::CacheTransform()
{
	FVector CurLoc;
	FRotator CurRot;
	FVector CurScale;

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );

			FVector Loc;
			FRotator Rot;
			FVector Scale;
			if( RootComponent )
			{
				Loc = RootComponent->RelativeLocation;
				Rot = bEditingRotationInUI ? ObjectToRelativeRotationMap.FindOrAdd(Object) : RootComponent->RelativeRotation;
				Scale = RootComponent->RelativeScale3D;

				if( ObjectIndex == 0 )
				{
					// Cache the current values from the first actor to see if any values differ among other actors
					CurLoc = Loc;
					CurRot = Rot;
					CurScale = Scale;

					CachedLocation.Set( Loc );
					CachedRotation.Set( Rot );
					CachedScale.Set( Scale );

					bAbsoluteLocation = RootComponent->bAbsoluteLocation;
					bAbsoluteScale = RootComponent->bAbsoluteScale;
					bAbsoluteRotation = RootComponent->bAbsoluteRotation;
				}
				else if( CurLoc != Loc || CurRot != Rot || CurScale != Scale )
				{
					// Check which values differ and unset the different values
					CachedLocation.X = Loc.X == CurLoc.X && CachedLocation.X.IsSet() ? Loc.X : TOptional<float>();
					CachedLocation.Y = Loc.Y == CurLoc.Y && CachedLocation.Y.IsSet() ? Loc.Y : TOptional<float>();
					CachedLocation.Z = Loc.Z == CurLoc.Z && CachedLocation.Z.IsSet() ? Loc.Z : TOptional<float>();

					CachedRotation.X = Rot.Roll == CurRot.Roll && CachedRotation.X.IsSet() ? Rot.Roll : TOptional<float>();
					CachedRotation.Y = Rot.Pitch == CurRot.Pitch && CachedRotation.Y.IsSet() ? Rot.Pitch : TOptional<float>();
					CachedRotation.Z = Rot.Yaw == CurRot.Yaw && CachedRotation.Z.IsSet() ? Rot.Yaw : TOptional<float>();

					CachedScale.X = Scale.X == CurScale.X && CachedScale.X.IsSet() ? Scale.X : TOptional<float>();
					CachedScale.Y = Scale.Y == CurScale.Y && CachedScale.Y.IsSet() ? Scale.Y : TOptional<float>();
					CachedScale.Z = Scale.Z == CurScale.Z && CachedScale.Z.IsSet() ? Scale.Z : TOptional<float>();

					// If all values are unset all values are different and we can stop looking
					const bool bAllValuesDiffer = !CachedLocation.IsSet() && !CachedRotation.IsSet() && !CachedScale.IsSet();
					if( bAllValuesDiffer )
					{
						break;
					}
				}
			}
		}
	}
}


void FComponentTransformDetails::OnSetLocation( float NewValue, ETextCommit::Type CommitInfo, int32 Axis )
{
	bool bBeganTransaction = false;

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
			if( RootComponent )
			{
				FVector OldRelativeLocation = RootComponent->RelativeLocation;
				FVector RelativeLocation = OldRelativeLocation;
				float OldValue = RelativeLocation[Axis];
				if( OldValue != NewValue )
				{
					if( !bBeganTransaction )
					{
						// Begin a transaction the first time an actors location is about to change.
						// NOTE: One transaction per change, not per actor
						if(Object->IsA<AActor>())
						{
							GEditor->BeginTransaction( LOCTEXT( "OnSetLocation", "Set actor location" ) );
						}
						else
						{
							GEditor->BeginTransaction( LOCTEXT( "OnSetLocation_ComponentDirect", "Modify Component(s)") );
						}
						
						bBeganTransaction = true;
					}

					if (Object->HasAnyFlags(RF_DefaultSubObject))
					{
						// Default subobjects must be included in any undo/redo operations
						Object->SetFlags(RF_Transactional);
					}

					// Begin a new movement event which will broadcast delegates before and after the actor moves
					FScopedObjectMovement ActorMoveEvent( Object );

					FScopedSwitchWorldForObject WorldSwitcher( Object );

					UProperty* RelativeLocationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeLocation" );
					Object->PreEditChange( RelativeLocationProperty );

					if( NotifyHook )
					{
						NotifyHook->NotifyPreChange( RelativeLocationProperty );
					}

					RelativeLocation[Axis] = NewValue;
					
					if( SelectedActorInfo.NumSelected == 0 )
					{
						// HACK: Set directly if no actors are selected since this causes Rot->Quat->Rot conversion
						// (recalculates relative rotation even though this is location)
						RootComponent->RelativeLocation = RelativeLocation;
					}
					else
					{
						RootComponent->SetRelativeLocation( RelativeLocation );
					}

					FPropertyChangedEvent PropertyChangedEvent( RelativeLocationProperty );
					Object->PostEditChangeProperty( PropertyChangedEvent );

					// If this is a default object or subobject, propagate the change out to any current instances of this object
					if(Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
					{
						PropagateTransformPropertyChange(Object, RelativeLocationProperty, OldRelativeLocation, RelativeLocation);
					}

					if( NotifyHook )
					{
						NotifyHook->NotifyPostChange( PropertyChangedEvent, RelativeLocationProperty );
					}
				}

			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();
	}

	CacheTransform();

	GUnrealEd->RedrawLevelEditingViewports();
}

void FComponentTransformDetails::OnSetRotation( float NewValue, bool bCommitted, int32 Axis )
{
	// OnSetRotation is sent from the slider or when the value changes and we dont have slider and the value is being typed.
	// We should only change the value on commit when it is being typed
	const bool bAllowSpin = SelectedObjects.Num() == 1;

	if( bAllowSpin || bCommitted )
	{
		bool bBeganTransaction = false;
		for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
		{
			TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
			if( ObjectPtr.IsValid() )
			{
				UObject* Object = ObjectPtr.Get();
				USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
				if( RootComponent )
				{
					FRotator& RelativeRotation = ObjectToRelativeRotationMap.FindOrAdd(Object);
					FRotator OldRelativeRotation = RelativeRotation;

					float& ValueToChange = Axis == 0 ? RelativeRotation.Roll : Axis == 1 ? RelativeRotation.Pitch : RelativeRotation.Yaw;

					if( bCommitted || ValueToChange != NewValue )
					{
						if( !bBeganTransaction && bCommitted )
						{
							// Begin a transaction the first time an actors rotation is about to change.
							// NOTE: One transaction per change, not per actor
							if(Object->IsA<AActor>())
							{
								GEditor->BeginTransaction( LOCTEXT( "OnSetRotation", "Set actor rotation" ) );
							}
							else
							{
								GEditor->BeginTransaction( LOCTEXT( "OnSetRotation_ComponentDirect", "Modify Component(s)") );
							}

							if (!Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
							{
								// Broadcast the first time an actor is about to move
								GEditor->BroadcastBeginObjectMovement( *Object );
							}

							bBeganTransaction = true;
						}

						FScopedSwitchWorldForObject WorldSwitcher( Object );

						UProperty* RelativeRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeRotation" );
						if( bCommitted && !bEditingRotationInUI )
						{
							if (Object->HasAnyFlags(RF_DefaultSubObject))
							{
								// Default subobjects must be included in any undo/redo operations
								Object->SetFlags(RF_Transactional);
							}

							Object->PreEditChange( RelativeRotationProperty );
						}

						if( NotifyHook )
						{
							NotifyHook->NotifyPreChange( RelativeRotationProperty );
						}

						ValueToChange = NewValue;

						if( SelectedActorInfo.NumSelected == 0 )
						{
							// HACK: Set directly if no actors are selected since this causes Rot->Quat->Rot conversion issues
							// (recalculates relative rotation from quat which can give an equivalent but different value than the user typed)
							RootComponent->RelativeRotation = RelativeRotation;
						}
						else
						{
							RootComponent->SetRelativeRotation( RelativeRotation );
						}

						AActor* ObjectAsActor = Cast<AActor>( Object );
						if( ObjectAsActor && !ObjectAsActor->HasAnyFlags(RF_ClassDefaultObject) )
						{
							ObjectAsActor->ReregisterAllComponents();
						}

						// If this is a default object or subobject, propagate the change out to any current instances of this object
						if(Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
						{
							PropagateTransformPropertyChange(Object, RelativeRotationProperty, OldRelativeRotation, RelativeRotation);
						}

						FPropertyChangedEvent PropertyChangedEvent( RelativeRotationProperty, false, !bCommitted && bEditingRotationInUI ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet );

						if( NotifyHook )
						{
							NotifyHook->NotifyPostChange( PropertyChangedEvent, RelativeRotationProperty );
						}

						if( bCommitted )
						{
							if( !bEditingRotationInUI )
							{
								Object->PostEditChangeProperty( PropertyChangedEvent );	
							}
					
							if (!Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
							{
								// The actor is done moving
								GEditor->BroadcastEndObjectMovement( *Object );
							}
						}
					}
				}
			}
		}

		if( bCommitted && bBeganTransaction )
		{
			GEditor->EndTransaction();
		}

		// Redraw
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

void FComponentTransformDetails::OnRotationCommitted(float NewValue, ETextCommit::Type CommitInfo, int32 Axis)
{
	OnSetRotation(NewValue, true, Axis);

	CacheTransform();
}

void FComponentTransformDetails::OnBeginRotatonSlider()
{
	bEditingRotationInUI = true;

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();

			// Start a new transation when a rotator slider begins to change
			// We'll end it when the slider is release
			// NOTE: One transaction per change, not per actor
			if(!bBeganTransaction)
			{
				if(Object->IsA<AActor>())
				{
					GEditor->BeginTransaction( LOCTEXT( "OnSetRotation", "Set actor rotation" ) );
				}
				else
				{
					GEditor->BeginTransaction( LOCTEXT( "OnSetRotation_ComponentDirect", "Modify Component(s)") );
				}

				bBeganTransaction = true;
			}

			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
			if( RootComponent )
			{
				FScopedSwitchWorldForObject WorldSwitcher( Object );
				
				if (Object->HasAnyFlags(RF_DefaultSubObject))
				{
					// Default subobjects must be included in any undo/redo operations
					Object->SetFlags(RF_Transactional);
				}

				UProperty* RelativeRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeRotation" );
				Object->PreEditChange( RelativeRotationProperty );
			}
		}
	}

	// Just in case we couldn't start a new transaction for some reason
	if(!bBeganTransaction)
	{
		GEditor->BeginTransaction( LOCTEXT( "OnSetRotation", "Set actor rotation" ) );
	}	
}

void FComponentTransformDetails::OnEndRotationSlider(float NewValue)
{
	bEditingRotationInUI = false;

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
			if( RootComponent )
			{
				FScopedSwitchWorldForObject WorldSwitcher( Object );

				UProperty* RelativeRotationProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeRotation" );
				FPropertyChangedEvent PropertyChangedEvent( RelativeRotationProperty );
				Object->PostEditChangeProperty( PropertyChangedEvent );
			}
		}
	}

	GEditor->EndTransaction();

	// Redraw
	GUnrealEd->RedrawLevelEditingViewports();
}

void FComponentTransformDetails::OnSetScale( const float NewValue, ETextCommit::Type CommitInfo, int32 Axis )
{
	ScaleObject( NewValue, Axis, false, LOCTEXT( "OnSetScale", "Set actor scale" ) );
}

void FComponentTransformDetails::ScaleObject( float NewValue, int32 Axis, bool bMirror, const FText& TransactionSessionName )
{
	UProperty* RelativeScale3DProperty = FindField<UProperty>( USceneComponent::StaticClass(), "RelativeScale3D" );

	bool bBeganTransaction = false;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectPtr = SelectedObjects[ObjectIndex];
		if( ObjectPtr.IsValid() )
		{
			UObject* Object = ObjectPtr.Get();
			USceneComponent* RootComponent = FComponentEditorUtils::GetSceneComponent( Object );
			if( RootComponent )
			{
				FVector OldRelativeScale = RootComponent->RelativeScale3D;
				FVector RelativeScale = OldRelativeScale;
				if( bMirror )
				{
					NewValue = -RelativeScale[Axis];
				}
				float OldValue = RelativeScale[Axis];
				if( OldValue != NewValue )
				{
					if( !bBeganTransaction )
					{
						// Begin a transaction the first time an actors scale is about to change.
						// NOTE: One transaction per change, not per actor
						GEditor->BeginTransaction( TransactionSessionName );
						bBeganTransaction = true;
					}

					FScopedSwitchWorldForObject WorldSwitcher( Object );
					
					if (Object->HasAnyFlags(RF_DefaultSubObject))
					{
						// Default subobjects must be included in any undo/redo operations
						Object->SetFlags(RF_Transactional);
					}

					// Begin a new movement event which will broadcast delegates before and after the actor moves
					FScopedObjectMovement ActorMoveEvent( Object );

					Object->PreEditChange( RelativeScale3DProperty );

					if( NotifyHook )
					{
						NotifyHook->NotifyPreChange( RelativeScale3DProperty );
					}

					// Set the new value for the corresponding axis
					RelativeScale[Axis] = NewValue;

					if( bPreserveScaleRatio )
					{
						// Account for the previous scale being zero.  Just set to the new value in that case?
						float Ratio = OldValue == 0.0f ? NewValue : NewValue/OldValue;

						// Change values on axes besides the one being directly changed
						switch( Axis )
						{
						case 0:
							RelativeScale.Y *= Ratio;
							RelativeScale.Z *= Ratio;
							break;
						case 1:
							RelativeScale.X *= Ratio;
							RelativeScale.Z *= Ratio;
							break;
						case 2:
							RelativeScale.X *= Ratio;
							RelativeScale.Y *= Ratio;
						}
					}

					RootComponent->SetRelativeScale3D( RelativeScale );

					// Build property chain so the actor knows whether we changed the X, Y or Z
					FEditPropertyChain PropertyChain;

					if (!bPreserveScaleRatio)
					{
						UStruct* VectorStruct = FindObjectChecked<UStruct>(UObject::StaticClass(), TEXT("Vector"), false);

						UProperty* VectorValueProperty = NULL;
						switch( Axis )
						{
						case 0:
							VectorValueProperty = FindField<UFloatProperty>(VectorStruct, TEXT("X"));
							break;
						case 1:
							VectorValueProperty = FindField<UFloatProperty>(VectorStruct, TEXT("Y"));
							break;
						case 2:
							VectorValueProperty = FindField<UFloatProperty>(VectorStruct, TEXT("Z"));
						}

						PropertyChain.AddHead(VectorValueProperty);
					}
					PropertyChain.AddHead(RelativeScale3DProperty);

					FPropertyChangedEvent PropertyChangedEvent(RelativeScale3DProperty, false, EPropertyChangeType::ValueSet);
					FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);
					Object->PostEditChangeChainProperty( PropertyChangedChainEvent );

					// For backwards compatibility, as for some reason PostEditChangeChainProperty calls PostEditChangeProperty with the property set to "X" not "RelativeScale3D"
					// (it does that for all vector properties, and I don't want to be the one to change that)
					if (!bPreserveScaleRatio)
					{
						Object->PostEditChangeProperty( PropertyChangedEvent );
					}
					else
					{
						// If the other scale values have been updated, make sure we update the transform now (as the tick will be too late)
						// so they appear correct when their EditedText is fetched from the delegate.
						CacheTransform();
					}

					// If this is a default object or subobject, propagate the change out to any current instances of this object
					if(Object->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject))
					{
						PropagateTransformPropertyChange(Object, RelativeScale3DProperty, OldRelativeScale, RelativeScale);
					}

					if( NotifyHook )
					{
						NotifyHook->NotifyPostChange( PropertyChangedEvent, RelativeScale3DProperty );
					}
				}
			}
		}
	}

	if( bBeganTransaction )
	{
		GEditor->EndTransaction();
	}

	CacheTransform();

	// Redraw
	GUnrealEd->RedrawLevelEditingViewports();
}

#undef LOCTEXT_NAMESPACE
