// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
 
#include "Slate.h"
#include "SListPanel.h"


FNoChildren SListPanel::NoChildren = FNoChildren();

/**
 * Construct the widget
 *
 * @param InArgs   A declaration from which to construct the widget
 */
void SListPanel::Construct( const FArguments& InArgs )
{
	PreferredRowNum = 0;
	SmoothScrollOffsetInItems = 0;
	ItemWidth = InArgs._ItemWidth;
	ItemHeight = InArgs._ItemHeight;
	NumDesiredItems = InArgs._NumDesiredItems;
	bIsRefreshPending = false;
}

/** Make a new ListPanel::Slot  */
SListPanel::FSlot& SListPanel::Slot()
{
	return *(new FSlot());
}
	
/** Add a slot to the ListPanel */
SListPanel::FSlot& SListPanel::AddSlot(int32 InsertAtIndex)
{
	FSlot& NewSlot = SListPanel::Slot();
	if (InsertAtIndex == INDEX_NONE)
	{
		this->Children.Add( &NewSlot );
	}
	else
	{
		this->Children.Insert( &NewSlot, InsertAtIndex );
	}
	
	return NewSlot;
}
	
/**
 * Arrange the children top-to-bottom with not additional layout info.
 */
void SListPanel::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if ( ShouldArrangeHorizontally() )
	{
		// This is a tile view list, arrange items horizontally until there is no more room then create a new row.
		const float AllottedWidth = AllottedGeometry.Size.X;
		const float ItemPadding = GetItemPadding(AllottedGeometry);
		const float HalfItemPadding = ItemPadding * 0.5;
		float WidthSoFar = 0;
		float LocalItemWidth = ItemWidth.Get();
		float LocalItemHeight = ItemHeight.Get();
		float HeightSoFar = -FMath::Floor(SmoothScrollOffsetInItems * LocalItemHeight);

		for( int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex )
		{
			ArrangedChildren.AddWidget(
				AllottedGeometry.MakeChild( Children[ItemIndex].Widget, FVector2D(WidthSoFar + HalfItemPadding, HeightSoFar), FVector2D(LocalItemWidth, LocalItemHeight) )
				);

			WidthSoFar += LocalItemWidth + ItemPadding;

			if ( WidthSoFar + LocalItemWidth + ItemPadding > AllottedWidth )
			{
				WidthSoFar = 0;
				HeightSoFar += LocalItemHeight;
			}
		}
	}
	else
	{
		if (Children.Num() > 0)
		{
			// This is a normal list, arrange items vertically
			float HeightSoFar = -FMath::Floor(SmoothScrollOffsetInItems * Children[0].Widget->GetDesiredSize().Y);
			for( int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex )
			{
					const FVector2D ItemDesiredSize = Children[ItemIndex].Widget->GetDesiredSize();
					const float LocalItemHeight = ItemDesiredSize.Y;

				// Note that ListPanel does not respect child Visibility.
				// It is simply not useful for ListPanels.
				ArrangedChildren.AddWidget(
					AllottedGeometry.MakeChild( Children[ItemIndex].Widget, FVector2D(0, HeightSoFar), FVector2D(AllottedGeometry.Size.X, LocalItemHeight) )
					);

				HeightSoFar += LocalItemHeight;
			}
		}
	}
}
	
void SListPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (ShouldArrangeHorizontally())
	{
		const float AllottedWidth = AllottedGeometry.Size.X;
		const float ItemPadding = GetItemPadding(AllottedGeometry);
		const float LocalItemWidth = ItemWidth.Get();
		const float TotalItemSize = LocalItemWidth + ItemPadding;
		const int32 NumChildren = Children.Num();

		if (TotalItemSize > 0.0f && NumChildren > 0)
		{
			int32 NumColumns = FMath::Clamp(FMath::Ceil(AllottedWidth / TotalItemSize) - 1, 1, NumChildren);
			PreferredRowNum = FMath::Ceil(NumChildren / (float)NumColumns);
		}
		else
		{
			PreferredRowNum = 1;
		}
	}
	else
	{
		PreferredRowNum = NumDesiredItems.Get();
	}

}
	
/**
 * Simply the sum of all the children (vertically), and the largest width (horizontally).
 */
FVector2D SListPanel::ComputeDesiredSize() const
{
	float MaxWidth = 0;
	float TotalHeight = 0;
	for( int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex )
	{
		// Notice that we do not respect child Visibility.
		// It is simply not useful for ListPanels.
		FVector2D ChildDesiredSize = Children[ItemIndex].Widget->GetDesiredSize();
		MaxWidth = FMath::Max( ChildDesiredSize.X, MaxWidth );
		TotalHeight += ChildDesiredSize.Y;
	}

	if (ShouldArrangeHorizontally())
	{
		return FVector2D( MaxWidth, ItemHeight.Get() * PreferredRowNum );
	}
	else
	{
		return (Children.Num() > 0)
			? FVector2D( MaxWidth, TotalHeight/Children.Num()*PreferredRowNum )
			: FVector2D::ZeroVector;
	}
	
}

/**
 * @return A slot-agnostic representation of this panel's children
 */
FChildren* SListPanel::GetChildren()
{
	if (bIsRefreshPending)
	{
		// When a refresh is pending it is unsafe to cache the desired sizes of our children because
		// they may be representing unsound data structures. Any delegates/attributes accessing unsound
		// data will cause a crash.
		return &NoChildren;
	}
	else
	{
		return &Children;
	}
	
}

/** Set the offset of the view area from the top of the list. */
void SListPanel::SmoothScrollOffset(float InOffsetInItems)
{
	SmoothScrollOffsetInItems = InOffsetInItems;
}

/** Remove all the children from this panel */
void SListPanel::ClearItems()
{
	Children.Empty();
}

float SListPanel::GetItemWidth() const
{
	return ItemWidth.Get();
}

float SListPanel::GetItemPadding(const FGeometry& AllottedGeometry) const
{
	float LocalItemWidth = ItemWidth.Get();
	const int32 NumItemsWide = LocalItemWidth > 0 ? FMath::Floor(AllottedGeometry.Size.X / LocalItemWidth) : 0;
	
	// Only add padding between items if we have more total items that we can fit on a single row.  Otherwise,
	// the padding around items would continue to change proportionately with the (ample) free horizontal space
	float Padding = 0.0f;
	if( NumItemsWide > 0 && Children.Num() > NumItemsWide )
	{
		// Subtract a tiny amount from the available width to avoid floating point precision problems when arranging children
		static const float FloatingPointPrecisionOffset = 0.001f;

		Padding = (AllottedGeometry.Size.X - FloatingPointPrecisionOffset - NumItemsWide * LocalItemWidth) / NumItemsWide;
	}
	return Padding;
}

/** @return the uniform item height used when arranging children. */
float SListPanel::GetItemHeight() const
{
	return ItemHeight.Get();
}

void SListPanel::SetRefreshPending( bool IsPendingRefresh )
{
	bIsRefreshPending = IsPendingRefresh;
}

bool SListPanel::IsRefreshPending() const
{
	return bIsRefreshPending;
}

bool SListPanel::ShouldArrangeHorizontally() const
{
	return ItemWidth.Get() > 0;
}

