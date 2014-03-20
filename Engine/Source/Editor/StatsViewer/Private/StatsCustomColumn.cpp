// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "StatsViewerPrivatePCH.h"
#include "StatsCustomColumn.h"
#include "StatsCellPresenter.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/PropertyPath.h"
#include "Editor/PropertyEditor/Public/IPropertyTableCell.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"

class FNumericStatCellPresenter : public TSharedFromThis< FNumericStatCellPresenter >, public FStatsCellPresenter
{
public:

	FNumericStatCellPresenter( const TSharedPtr< IPropertyHandle > PropertyHandle )
	{
		Text = FStatsCustomColumn::GetPropertyAsText( PropertyHandle );
	}

	virtual TSharedRef< class SWidget > ConstructDisplayWidget() OVERRIDE
	{
		return 
			SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( STextBlock )
					.Text( Text )
				];
	}
};

bool FStatsCustomColumn::Supports( const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities ) const
{
	if( Column->GetDataSource()->IsValid() )
	{
		TSharedPtr< FPropertyPath > PropertyPath = Column->GetDataSource()->AsPropertyPath();
		if( PropertyPath->GetNumProperties() > 0 )
		{
			const FPropertyInfo& PropertyInfo = PropertyPath->GetRootProperty();
			return FStatsCustomColumn::SupportsProperty( PropertyInfo.Property.Get() );
		}
	}

	return false;
}

TSharedPtr< SWidget > FStatsCustomColumn::CreateColumnLabel( const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style ) const
{
	TSharedPtr< FPropertyPath > PropertyPath = Column->GetDataSource()->AsPropertyPath();
	const FPropertyInfo& PropertyInfo = PropertyPath->GetRootProperty();
	if( PropertyInfo.Property.Get()->HasMetaData( "ShowTotal" ) )
	{
		return 
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( STextBlock )
				.Font( FEditorStyle::GetFontStyle( Style ) )
				.Text( Column->GetDisplayName().ToString() )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( STextBlock )
				.Font( FEditorStyle::GetFontStyle(TEXT("BoldFont") ) )
				.Text( this, &FStatsCustomColumn::GetTotalText, Column )
			];
	}
	else
	{
		return 
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( STextBlock )
				.Font( FEditorStyle::GetFontStyle( Style ) )
				.Text( Column->GetDisplayName().ToString() )
			];
	}
}

TSharedPtr< IPropertyTableCellPresenter > FStatsCustomColumn::CreateCellPresenter( const TSharedRef< IPropertyTableCell >& Cell, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style ) const
{
	TSharedPtr< IPropertyHandle > PropertyHandle = Cell->GetPropertyHandle();
	if( PropertyHandle.IsValid() )
	{
		return MakeShareable( new FNumericStatCellPresenter( PropertyHandle ) );
	}

	return NULL;
}

FText FStatsCustomColumn::GetTotalText( TSharedRef< IPropertyTableColumn > Column ) const
{
	TSharedPtr< FPropertyPath > PropertyPath = Column->GetDataSource()->AsPropertyPath();
	const FPropertyInfo& PropertyInfo = PropertyPath->GetRootProperty();
	const FString PropertyName = PropertyInfo.Property->GetNameCPP();
	const FText* TotalText = TotalsMap.Find( PropertyName );

	if( TotalText != NULL )
	{
		FText OutText = *TotalText;
		if( PropertyInfo.Property.Get()->HasMetaData("Unit") )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Value"), OutText );
			Args.Add( TEXT("Unit"), FText::FromString( PropertyInfo.Property.Get()->GetMetaData("Unit") ) );
			OutText = FText::Format( NSLOCTEXT("Stats", "Value + Unit", "{Value} {Unit}"), Args );
		}

		return OutText;
	}

	return FText::GetEmpty();
}

bool FStatsCustomColumn::SupportsProperty( UProperty* Property )
{
	if( Property->IsA( UFloatProperty::StaticClass() ) || Property->IsA( UIntProperty::StaticClass() ) )
	{
		return true;
	}

	if( Property->IsA( UStructProperty::StaticClass() ) )
	{
		return ( Cast<const UStructProperty>(Property)->Struct->GetFName() == NAME_Vector2D );
	}

	return false;
}

FText FStatsCustomColumn::GetPropertyAsText( const TSharedPtr< IPropertyHandle > PropertyHandle )
{
	FText Text;

	if( PropertyHandle->GetProperty()->IsA( UIntProperty::StaticClass() ) )
	{
		int32 IntValue = INT_MAX;
		PropertyHandle->GetValue( IntValue );
		if( IntValue == INT_MAX )
		{
			Text = NSLOCTEXT("Stats", "UnknownIntegerValue", "?");
		}
		else
		{
			Text = FText::AsNumber( IntValue );
		}
	}
	else if( PropertyHandle->GetProperty()->IsA( UFloatProperty::StaticClass() ) )
	{
		float FloatValue = FLT_MAX;
		PropertyHandle->GetValue( FloatValue );
		if( FloatValue == FLT_MAX )
		{
			Text = NSLOCTEXT("Stats", "UnknownFloatValue", "?");
		}
		else
		{
			Text = FText::AsNumber( FloatValue );
		}
	}
	else if( PropertyHandle->GetProperty()->IsA( UStructProperty::StaticClass() ) )
	{
		if( Cast<const UStructProperty>(PropertyHandle->GetProperty())->Struct->GetFName() == NAME_Vector2D )
		{
			FVector2D VectorValue(0.0f, 0.0f);
			PropertyHandle->GetValue( VectorValue );
			if( VectorValue.X == FLT_MAX || VectorValue.Y == FLT_MAX )
			{
				Text = NSLOCTEXT("Stats", "UnknownVectorValue", "?");
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("VectorX"), VectorValue.X );
				Args.Add( TEXT("VectorY"), VectorValue.Y );
				Text = FText::Format( NSLOCTEXT("Stats", "VectorValue", "{VectorX}x{VectorY}"), Args );
			}
		}
	}

	if( PropertyHandle->GetProperty()->HasMetaData("Unit") )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("Value"), Text );
		Args.Add( TEXT("Unit"), FText::FromString( PropertyHandle->GetProperty()->GetMetaData("Unit") ) );
		Text = FText::Format( NSLOCTEXT("Stats", "Value + Unit", "{Value} {Unit}"), Args );
	}

	return Text;
}

