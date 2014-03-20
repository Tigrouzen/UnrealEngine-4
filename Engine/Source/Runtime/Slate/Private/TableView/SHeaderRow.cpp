// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
 
#include "Slate.h"

class STableColumnHeader : public SCompoundWidget
{
public:
	 
	SLATE_BEGIN_ARGS(STableColumnHeader)
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FTableColumnHeaderStyle>("TableView.Header.Column") )
		{}
		SLATE_STYLE_ARGUMENT( FTableColumnHeaderStyle, Style )

	SLATE_END_ARGS()

	STableColumnHeader()
		: SortMode( EColumnSortMode::None )
		, OnSortModeChanged()
		, ContextMenuContent( SNullWidget::NullWidget )
		, ColumnId( NAME_None )
		, Style( nullptr )
	{

	}

	/**
	 * Construct the widget
	 * 
	 * @param InDeclaration   A declaration from which to construct the widget
	 */
	void Construct( const STableColumnHeader::FArguments& InArgs, const SHeaderRow::FColumn& Column, const FMargin DefaultHeaderContentPadding )
	{
		check(InArgs._Style);

		SWidget::Construct( InArgs._ToolTipText, InArgs._ToolTip, InArgs._Cursor, InArgs._IsEnabled, InArgs._Visibility, InArgs._Tag );

		Style = InArgs._Style;
		ColumnId = Column.ColumnId;
		SortMode = Column.SortMode;

		SortMode = Column.SortMode;
		OnSortModeChanged = Column.OnSortModeChanged;
		ContextMenuContent = Column.HeaderMenuContent.Widget;

		FMargin AdjustedDefaultHeaderContentPadding = DefaultHeaderContentPadding;

		TAttribute< FString > LabelText = Column.DefaultText;

		if (Column.HeaderContent.Widget == SNullWidget::NullWidget && !Column.DefaultText.IsBound() && Column.DefaultText.Get().IsEmpty())
		{
			LabelText = Column.ColumnId.ToString() + TEXT("[LabelMissing]");
		}

		TSharedPtr< SHorizontalBox > Box;
		TSharedRef< SOverlay > Overlay = SNew( SOverlay );

		Overlay->AddSlot( 0 )
			[
				SAssignNew( Box, SHorizontalBox )
			];

		TSharedRef< SWidget > PrimaryContent = Column.HeaderContent.Widget;
		if ( PrimaryContent == SNullWidget::NullWidget )
		{
			PrimaryContent = 
				SNew( SBox )
				.Padding( OnSortModeChanged.IsBound() ? FMargin( 0, 2, 0, 2 ) : FMargin( 0, 4, 0, 4 ) )
				.VAlign( VAlign_Center )
				[
					SNew(STextBlock)
					.Text( LabelText )
					.ToolTipText( LabelText )
				];
		}

		if ( OnSortModeChanged.IsBound() )
		{
			//optional main button with the column's title. Used to toggle sorting modes.
			PrimaryContent = SNew(SButton)
			.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
			.ForegroundColor( FSlateColor::UseForeground() )
			.ContentPadding( FMargin( 0, 2, 0, 2 ) )
			.OnClicked(this, &STableColumnHeader::OnTitleClicked)
			[
				PrimaryContent
			];
		}
		
		Box->AddSlot()
		.FillWidth(1.0f)
		[
			PrimaryContent
		];

		if( Column.HeaderMenuContent.Widget != SNullWidget::NullWidget )
		{
			// Add Drop down menu button (only if menu content has been specified)
			Box->AddSlot()
			.AutoWidth()
			[
				SAssignNew( MenuOverlay, SOverlay )
				.Visibility( this, &STableColumnHeader::GetMenuOverlayVisibility )
				+SOverlay::Slot()
				[
					SNew( SBorder )
					.Padding( FMargin( 0, 0, AdjustedDefaultHeaderContentPadding.Right, 0 ) )
					.BorderImage( this, &STableColumnHeader::GetComboButtonBorderBrush )
					[
						SAssignNew( ComboButton, SComboButton)
						.HasDownArrow(false)
						.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
						.ContentPadding( FMargin(4, 0, 4, 0) )
						.ButtonContent()
						[
							SNew( SSpacer )
							.Size( FVector2D( 12.0f, 0 ) )
						]
						.MenuContent()
						[
							ContextMenuContent
						]
					]
				]

				+SOverlay::Slot()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				[
					SNew(SImage)
					.Image( &Style->MenuDropdownImage )
					.Visibility( EVisibility::HitTestInvisible )
				]
			];		

			AdjustedDefaultHeaderContentPadding.Right = 0;
		}

		Overlay->AddSlot( 1 )
			.HAlign(HAlign_Center)
			.VAlign( VAlign_Top )
			.Padding( FMargin( 0, 2, 0, 0 ) )
			[
				SNew(SImage)
				.Image( this, &STableColumnHeader::GetSortingBrush )
				.Visibility( this, &STableColumnHeader::GetSortModeVisibility )
			];

		this->ChildSlot
		[
			SNew( SBorder )
			.BorderImage( this, &STableColumnHeader::GetHeaderBackgroundBrush )
			.HAlign( Column.HeaderHAlignment )
			.VAlign( Column.HeaderVAlignment )
			.Padding( Column.HeaderContentPadding.Get( AdjustedDefaultHeaderContentPadding ) )
			[
				Overlay
			]
		];
	}

	/** Gets sorting mode */
	EColumnSortMode::Type GetSortMode() const
	{
		return SortMode.Get();
	}

	/** Sets sorting mode */
	void SetSortMode( EColumnSortMode::Type NewMode )
	{
		SortMode = NewMode;
	}

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ContextMenuContent != SNullWidget::NullWidget )
		{
			OpenContextMenu( MouseEvent.GetScreenSpacePosition() );
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}


private:


	const FSlateBrush* GetHeaderBackgroundBrush() const
	{
		if ( IsHovered() && SortMode.IsBound() )
		{
			return &Style->HoveredBrush;
		}

		return &Style->NormalBrush;
	}

	EVisibility GetMenuOverlayVisibility() const
	{
		if ( IsHovered() || ( ComboButton.IsValid() && ComboButton->IsOpen() ) )
		{
			return EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	}

	const FSlateBrush* GetComboButtonBorderBrush() const
	{
		if ( ComboButton.IsValid() && ( ComboButton->IsHovered() || ComboButton->IsOpen() ) )
		{
			return &Style->MenuDropdownHoveredBorderBrush;
		}

		if ( IsHovered() )
		{
			return &Style->MenuDropdownNormalBorderBrush;
		}

		return FStyleDefaults::GetNoBrush();
	}

	/** Gets the icon associated with the current sorting mode */
	const FSlateBrush* GetSortingBrush() const
	{
		return (SortMode == EColumnSortMode::Ascending ? &Style->SortAscendingImage : &Style->SortDescendingImage);
	}

	/** Checks if sorting mode has been selected */
	EVisibility GetSortModeVisibility() const
	{
		return (SortMode.Get() != EColumnSortMode::None) ? EVisibility::HitTestInvisible : EVisibility::Hidden;
	}
	
	/** Called when the column title has been clicked to change sorting mode */
	FReply OnTitleClicked()
	{
		if ( OnSortModeChanged.IsBound() )
		{
			EColumnSortMode::Type ColumnSortMode = SortMode.Get();
			if ( ColumnSortMode == EColumnSortMode::None || ColumnSortMode == EColumnSortMode::Descending )
			{
				ColumnSortMode = EColumnSortMode::Ascending;
			}
			else
			{
				ColumnSortMode = EColumnSortMode::Descending;
			}

			OnSortModeChanged.Execute( ColumnId, ColumnSortMode );
		}

		return FReply::Handled();
	}

	void OpenContextMenu(const FVector2D& SummonLocation)
	{
		if ( ContextMenuContent != SNullWidget::NullWidget )
		{
			FSlateApplication::Get().PushMenu( AsShared(), ContextMenuContent, SummonLocation, FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu ) );
		}
	}


private:

	/** Current sorting mode */
	TAttribute< EColumnSortMode::Type > SortMode;

	/** Callback triggered when sorting mode changes */
	FOnSortModeChanged OnSortModeChanged;

	TSharedRef< SWidget > ContextMenuContent;

	TSharedPtr< SComboButton > ComboButton;

	TSharedPtr< SOverlay > MenuOverlay;

	FName ColumnId;

	const FTableColumnHeaderStyle* Style;
};

void SHeaderRow::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	SWidget::Construct( InArgs._ToolTipText, InArgs._ToolTip, InArgs._Cursor, InArgs._IsEnabled, InArgs._Visibility, InArgs._Tag );

	ScrollBarThickness = FVector2D::ZeroVector;
	ScrollBarVisibility = EVisibility::Collapsed;
	Style = InArgs._Style;

	if ( InArgs._OnColumnsChanged.IsBound() )
	{
		ColumnsChanged.Add( InArgs._OnColumnsChanged );
	}

	SBorder::Construct( SBorder::FArguments()
		.Padding( 0 )
		.BorderImage( &Style->BackgroundBrush )
		.ForegroundColor( Style->ForegroundColor )
	);

	// Copy all the column info from the declaration
	bool bHaveFillerColumn = false;
	for ( int32 SlotIndex=0; SlotIndex < InArgs.Slots.Num(); ++SlotIndex )
	{
		FColumn* const Column = InArgs.Slots[SlotIndex];
		Columns.Add( Column );
	}

	// Generate widgets for all columns
	RegenerateWidgets();
}

void SHeaderRow::ResetColumnWidths()
{
	for ( int32 iColumn = 0; iColumn < Columns.Num(); iColumn++ )
	{
		FColumn& Column = Columns[iColumn];
		Column.SetWidth( Column.DefaultWidth );
	}
}

const TIndirectArray<SHeaderRow::FColumn>& SHeaderRow::GetColumns() const
{
	return Columns;
}

void SHeaderRow::AddColumn( const FColumn::FArguments& NewColumnArgs )
{
	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn( NewColumnArgs );
	AddColumn( *NewColumn );
}

void SHeaderRow::AddColumn( SHeaderRow::FColumn& NewColumn )
{
	int32 InsertIdx = Columns.Num();
	InsertColumn( NewColumn, InsertIdx );
}

void SHeaderRow::InsertColumn( const FColumn::FArguments& NewColumnArgs, int32 InsertIdx )
{
	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn( NewColumnArgs );
	InsertColumn( *NewColumn, InsertIdx );
}

void SHeaderRow::InsertColumn( FColumn& NewColumn, int32 InsertIdx )
{
	check(NewColumn.ColumnId != NAME_None);

	if ( Columns.Num() > 0 && Columns[Columns.Num() - 1].ColumnId == NAME_None )
	{
		// Insert before the filler column, or where the filler column used to be if we replaced it.
		InsertIdx--;
	}

	Columns.Insert( &NewColumn, InsertIdx );
	ColumnsChanged.Broadcast( SharedThis( this ) );

	RegenerateWidgets();
}

void SHeaderRow::RemoveColumn( const FName& InColumnId )
{
	check(InColumnId != NAME_None);

	bool bHaveFillerColumn = false;
	for ( int32 SlotIndex=Columns.Num() - 1; SlotIndex >= 0; --SlotIndex )
	{
		FColumn& Column = Columns[SlotIndex];
		if ( Column.ColumnId == InColumnId )
		{
			Columns.RemoveAt(SlotIndex);
		}
	}

	ColumnsChanged.Broadcast( SharedThis( this ) );
	RegenerateWidgets();
}

void SHeaderRow::ClearColumns()
{
	Columns.Empty();
	ColumnsChanged.Broadcast( SharedThis( this ) );

	RegenerateWidgets();
}

void SHeaderRow::SetAssociatedVerticalScrollBar( const TSharedRef< SScrollBar >& ScrollBar, const float ScrollBarSize )
{
	ScrollBarThickness.X = ScrollBarSize;
	ScrollBarVisibility.Bind( TAttribute< EVisibility >::FGetter::CreateSP( ScrollBar, &SScrollBar::ShouldBeVisible ) );
	RegenerateWidgets();
}

void SHeaderRow::SetColumnWidth( const FName& InColumnId, float InWidth )
{
	check(InColumnId != NAME_None);

	for ( int32 SlotIndex=Columns.Num() - 1; SlotIndex >= 0; --SlotIndex )
	{
		FColumn& Column = Columns[SlotIndex];
		if ( Column.ColumnId == InColumnId )
		{
			Column.SetWidth( InWidth );
		}
	}
}

void SHeaderRow::RegenerateWidgets()
{
	const float SplitterHandleDetectionSize = 5.0f;
	TSharedPtr<SSplitter> Splitter;

	TSharedRef< SHorizontalBox > Box = 
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		[
			SAssignNew(Splitter, SSplitter)
			.Style( &Style->ColumnSplitterStyle )
			.ResizeMode( ESplitterResizeMode::Fill )
			.PhysicalSplitterHandleSize( 0.0f )
			.HitDetectionSplitterHandleSize( SplitterHandleDetectionSize )
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 0 )
		[
			SNew( SSpacer )
			.Size( ScrollBarThickness )
			.Visibility( ScrollBarVisibility )
		];

	// Construct widgets for all columns
	{
		const float HalfSplitterDetectionSize = ( SplitterHandleDetectionSize + 2 ) / 2;

		// Populate the slot with widgets that represent the columns.
		TSharedPtr<STableColumnHeader> NewlyMadeHeader;
		for ( int32 SlotIndex=0; SlotIndex < Columns.Num(); ++SlotIndex )
		{
			FColumn& SomeColumn = Columns[SlotIndex];

			// Keep track of the last header we created.
			TSharedPtr<STableColumnHeader> PrecedingHeader = NewlyMadeHeader;
			NewlyMadeHeader.Reset();

			FMargin DefaultPadding = FMargin( HalfSplitterDetectionSize, 0, HalfSplitterDetectionSize, 0 );
			TSharedRef<STableColumnHeader> NewHeader =
				SAssignNew(NewlyMadeHeader, STableColumnHeader, SomeColumn, DefaultPadding)
				.Style( (SlotIndex + 1 == Columns.Num()) ? &Style->LastColumnStyle : &Style->ColumnStyle );

			if ( SomeColumn.SizeRule == EColumnSizeMode::Fixed )
			{
				// Add resizable cell
				Splitter->AddSlot()
				.SizeRule( SSplitter::SizeToContent )
				[
					SNew( SBox )
					.WidthOverride( SomeColumn.Width.Get() )
					[
						NewHeader
					]
				];
			}
			else 
			{
				TAttribute<float> WidthBinding;
				WidthBinding.BindRaw( &SomeColumn, &FColumn::GetWidth );

				// Add resizable cell
				Splitter->AddSlot()
				.Value( WidthBinding )
				.SizeRule( SSplitter::FractionOfParent )
				.OnSlotResized( SSplitter::FOnSlotResized::CreateRaw( &SomeColumn, &FColumn::SetWidth ) )
				[
					NewHeader
				];
			}
		}
	}

	// Create a box to contain widgets for each column
	SetContent( Box );
}
