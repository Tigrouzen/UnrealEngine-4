// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 PropertyTablePropertyNameColumn.cpp: Implements the FPropertyTablePropertyNameColumn class.
=============================================================================*/

#include "PropertyEditorPrivatePCH.h"
#include "PropertyTableCell.h"
#include "PropertyTablePropertyNameColumn.h"

#define LOCTEXT_NAMESPACE "PropertyNameColumnHeader"

FPropertyTablePropertyNameColumn::FPropertyTablePropertyNameColumn( const TSharedRef< IPropertyTable >& InTable )
	: Table( InTable )
	, Cells()
	, bIsHidden( false )
	, Width( 2.0f )
	, DataSource( MakeShareable( new NoDataSource() ) )
{

}


TSharedRef< class IPropertyTableCell > FPropertyTablePropertyNameColumn::GetCell( const TSharedRef< class IPropertyTableRow >& Row )
{
	TSharedRef< IPropertyTableCell >* CellPtr = Cells.Find( Row );

	if( CellPtr != NULL )
	{
		return *CellPtr;
	}

	TSharedRef< IPropertyTableCell > Cell = MakeShareable( new FPropertyTableCell( SharedThis( this ), Row ) );
	Cells.Add( Row, Cell );

	return Cell;
}


void FPropertyTablePropertyNameColumn::Sort( TArray< TSharedRef< class IPropertyTableRow > >& Rows, const EColumnSortMode::Type SortMode )
{
	struct FCompareRowByPropertyNameAscending
	{
	public:
		FCompareRowByPropertyNameAscending( const TSharedRef< FPropertyTablePropertyNameColumn >& Column )
			: NameColumn( Column )
		{ }

		FORCEINLINE bool operator()( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
		{
			return NameColumn->GetPropertyNameAsString( Lhs ) < NameColumn->GetPropertyNameAsString( Rhs );
		}

		TSharedRef< FPropertyTablePropertyNameColumn > NameColumn;
	};

	struct FCompareRowByPropertyNameDescending
	{
	public:
		FCompareRowByPropertyNameDescending( const TSharedRef< FPropertyTablePropertyNameColumn >& Column )
			: Comparer( Column )
		{ }

		FORCEINLINE bool operator()( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
		{
			return !Comparer( Lhs, Rhs ); 
		}

	private:

		const FCompareRowByPropertyNameAscending Comparer;
	};

	if ( SortMode == EColumnSortMode::None )
	{
		return;
	}

	if ( SortMode == EColumnSortMode::Ascending )
	{
		Rows.Sort( FCompareRowByPropertyNameAscending( SharedThis( this ) ) );
	}
	else
	{
		Rows.Sort( FCompareRowByPropertyNameDescending( SharedThis( this ) ) );
	}
}


FString FPropertyTablePropertyNameColumn::GetPropertyNameAsString( const TSharedRef< IPropertyTableRow >& Row )
{
	FString PropertyName;
	if( Row->GetDataSource()->AsPropertyPath().IsValid() )
	{
		const TWeakObjectPtr< UProperty > Property = Row->GetDataSource()->AsPropertyPath()->GetLeafMostProperty().Property;
		PropertyName = UEditorEngine::GetFriendlyName( Property.Get() );
	}
	return PropertyName;
}

#undef LOCTEXT_NAMESPACE