// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SSplitter;
class IGridRow; 

namespace EColumnSortMode
{
	enum Type
	{
		/** Unsorted */
		None = 0,

		/** Ascending */
		Ascending = 1,

		/** Descending */
		Descending = 2,
	};
};

namespace EColumnSizeMode
{
	enum Type
	{
		/** Column stretches to a fractional of the header row */
		Fill = 0,
		
		/**	Column is fixed width and cannot be resized */
		Fixed = 1,
	};
};

/** Callback when sort mode changes */
DECLARE_DELEGATE_TwoParams( FOnSortModeChanged, const FName&, EColumnSortMode::Type );

/**	Callback when the width of the column changes */
DECLARE_DELEGATE_OneParam( FOnWidthChanged, float );

/**
 * The header that appears above lists and trees when they are showing multiple columns.
 */
class SLATE_API SHeaderRow : public SBorder
{
public:
	/** Describes a single column header */
	class FColumn
	{
	public:

		SLATE_BEGIN_ARGS(FColumn)
			: _ColumnId()
			, _DefaultLabel()
			, _FillWidth( 1.0f )
			, _FixedWidth()
			, _OnWidthChanged()
			, _HeaderContent()
			, _HAlignHeader( HAlign_Fill )
			, _VAlignHeader( VAlign_Fill )
			, _HeaderContentPadding()
			, _MenuContent()
			, _HAlignCell( HAlign_Fill )
			, _VAlignCell( VAlign_Fill )
			, _SortMode( EColumnSortMode::None )
			, _OnSort()
			{}
			SLATE_ARGUMENT( FName, ColumnId )
			SLATE_TEXT_ATTRIBUTE( DefaultLabel )
			SLATE_ATTRIBUTE( float, FillWidth )
			SLATE_ARGUMENT( TOptional< float >, FixedWidth )
			SLATE_EVENT( FOnWidthChanged, OnWidthChanged )

			SLATE_DEFAULT_SLOT( FArguments, HeaderContent )
			SLATE_ARGUMENT( EHorizontalAlignment, HAlignHeader )
			SLATE_ARGUMENT( EVerticalAlignment, VAlignHeader )
			SLATE_ARGUMENT( TOptional< FMargin >, HeaderContentPadding )
			SLATE_NAMED_SLOT( FArguments, MenuContent )

			SLATE_ARGUMENT( EHorizontalAlignment, HAlignCell )
			SLATE_ARGUMENT( EVerticalAlignment, VAlignCell )

			SLATE_ATTRIBUTE( EColumnSortMode::Type, SortMode )
			SLATE_EVENT( FOnSortModeChanged, OnSort )
		SLATE_END_ARGS()

		FColumn( const FArguments& InArgs )
			: ColumnId( InArgs._ColumnId )
			, DefaultText( InArgs._DefaultLabel )
			, Width( 1.0f )
			, DefaultWidth( 1.0f )
			, OnWidthChanged( InArgs._OnWidthChanged)
			, SizeRule( EColumnSizeMode::Fill )
			, HeaderContent( InArgs._HeaderContent )
			, HeaderMenuContent( InArgs._MenuContent )
			, HeaderHAlignment( InArgs._HAlignHeader )
			, HeaderVAlignment( InArgs._VAlignHeader )
			, HeaderContentPadding( InArgs._HeaderContentPadding )
			, CellHAlignment( InArgs._HAlignCell )
			, CellVAlignment( InArgs._VAlignCell )
			, SortMode( InArgs._SortMode )
			, OnSortModeChanged( InArgs._OnSort )
		{
			if ( InArgs._FixedWidth.IsSet() )
			{
				Width = InArgs._FixedWidth.GetValue();
				SizeRule = EColumnSizeMode::Fixed;
			}
			else
			{
				Width = InArgs._FillWidth;
				SizeRule = EColumnSizeMode::Fill;
			}

			DefaultWidth = Width.Get();
		}


	public:

		void SetWidth( float NewWidth )
		{
			if ( OnWidthChanged.IsBound() )
			{
				OnWidthChanged.Execute( NewWidth );
			}
			else
			{
				Width = NewWidth;
			}
		}

		float GetWidth() const
		{
			return Width.Get();
		}

		/** A unique ID for this column, so that it can be saved and restored. */
		FName ColumnId;

		/** Default text to use if no widget is passed in. */
		TAttribute< FString > DefaultText;

		/** A column width in Slate Units */
		TAttribute< float > Width;

		/** A original column width in Slate Units */
		float DefaultWidth;

		FOnWidthChanged OnWidthChanged;

		EColumnSizeMode::Type SizeRule;

		TAlwaysValidWidget HeaderContent;	
		TAlwaysValidWidget HeaderMenuContent;

		EHorizontalAlignment HeaderHAlignment;
		EVerticalAlignment HeaderVAlignment;
		TOptional< FMargin > HeaderContentPadding;

		EHorizontalAlignment CellHAlignment;
		EVerticalAlignment CellVAlignment;

		TAttribute< EColumnSortMode::Type > SortMode;
		FOnSortModeChanged OnSortModeChanged;
	};

	/** Create a column with the specified ColumnId */
	static FColumn::FArguments Column( const FName& InColumnId )
	{
		FColumn::FArguments NewArgs;
		NewArgs.ColumnId( InColumnId ); 
		return NewArgs;
	}

	DECLARE_EVENT_OneParam( SHeaderRow, FColumnsChanged, const TSharedRef< SHeaderRow >& );
	FColumnsChanged* OnColumnsChanged() { return &ColumnsChanged; }

	SLATE_BEGIN_ARGS(SHeaderRow)
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FHeaderRowStyle>("TableView.Header") )
		{}
		SLATE_STYLE_ARGUMENT( FHeaderRowStyle, Style )
		SLATE_SUPPORTS_SLOT_WITH_ARGS( FColumn )	
		SLATE_EVENT( FColumnsChanged::FDelegate, OnColumnsChanged )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** Restore the columns to their original width */
	void ResetColumnWidths();

	/** @return the Columns driven by the column headers */
	const TIndirectArray<FColumn>& GetColumns() const;

	/** Adds a column to the header */
	void AddColumn( const FColumn::FArguments& NewColumnArgs );
	void AddColumn( FColumn& NewColumn );

	/** Inserts a column at the specified index in the header */
	void InsertColumn( const FColumn::FArguments& NewColumnArgs, int32 InsertIdx );
	void InsertColumn( FColumn& NewColumn, int32 InsertIdx );

	/** Removes a column from the header */
	void RemoveColumn( const FName& InColumnId );

	/** Removes all columns from the header */
	void ClearColumns();

	void SetAssociatedVerticalScrollBar( const TSharedRef< SScrollBar >& ScrollBar, const float ScrollBarSize );

	/** Sets the column, with the specified name, to the desired width */
	void SetColumnWidth( const FName& InColumnId, float InWidth );

private:

	/** Regenerates all widgets in the header */
	void RegenerateWidgets();

	/** Information about the various columns */
	TIndirectArray<FColumn> Columns;

	FVector2D ScrollBarThickness;
	TAttribute< EVisibility > ScrollBarVisibility;
	const FHeaderRowStyle* Style;
	FColumnsChanged ColumnsChanged;
};
