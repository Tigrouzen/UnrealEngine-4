// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BspModePrivatePCH.h"
#include "SBspPalette.h"
#include "BspModeModule.h"
#include "SScrollBorder.h"
#include "BspModeActions.h"
#include "Editor/UnrealEd/Public/DragAndDrop/BrushBuilderDragDropOp.h"

#define LOCTEXT_NAMESPACE "BspPalette"

/** The list view mode of the asset view */
class SBspBuilderListView : public SListView<TSharedPtr<FBspBuilderType>>
{
public:
	virtual bool SupportsKeyboardFocus() const OVERRIDE { return true; }
	virtual FReply OnKeyDown( const FGeometry& InGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE
	{
		return FReply::Unhandled();
	}
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBspPalette::Construct( const FArguments& InArgs )
{
	bIsAdditive = true;

	FBspModeModule& BspModeModule = FModuleManager::GetModuleChecked<FBspModeModule>("BspMode");

	TSharedRef<SBspBuilderListView> ListViewWidget = 
		SNew(SBspBuilderListView)
		.SelectionMode(ESelectionMode::Single)
		.ListItemsSource(&BspModeModule.GetBspBuilderTypes())
		.OnGenerateRow(this, &SBspPalette::MakeListViewWidget)
		.OnSelectionChanged(this, &SBspPalette::OnSelectionChanged)
		.ItemHeight(35);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			.Padding(0.0f)
			[
				ListViewWidget
			]
			+SOverlay::Slot()
			.HAlign( HAlign_Fill )
			.VAlign( VAlign_Fill )
			[
				SNew(SScrollBorder, ListViewWidget)
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 3.0f )
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.Style(FEditorStyle::Get(), "Toolbar.RadioButton")
				.ToolTipText(LOCTEXT("BspModeAdditiveTooltip", "Place brushes in additive mode."))
				.OnCheckStateChanged(this, &SBspPalette::OnAdditiveModeButtonClicked)
				.IsChecked(this, &SBspPalette::IsAdditiveModeChecked)
				.ForegroundColor(FCoreStyle::Get().GetSlateColor("Foreground"))
				.Content()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(this, &SBspPalette::GetAdditiveModeImage)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BspModeAdd", "Add"))
						.Font(FCoreStyle::Get().GetFontStyle("Toolbar.Label.Font"))
						.ShadowOffset(FVector2D::UnitVector)
					]
				]
			]
			+SHorizontalBox::Slot()
			.Padding( 3.0f )
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.Style(FEditorStyle::Get(), "Toolbar.RadioButton")
				.ToolTipText(LOCTEXT("BspModeAdditiveTooltip", "Place brushes in subtractive mode."))
				.OnCheckStateChanged(this, &SBspPalette::OnSubtractiveModeButtonClicked)
				.IsChecked(this, &SBspPalette::IsSubtractiveModeChecked)
				.ForegroundColor(FCoreStyle::Get().GetSlateColor("Foreground"))
				.Content()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(this, &SBspPalette::GetSubtractiveModeImage)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BspModeSubtract", "Subtract"))
						.Font(FCoreStyle::Get().GetFontStyle("Toolbar.Label.Font"))
						.ShadowOffset(FVector2D::UnitVector)
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<ITableRow> SBspPalette::MakeListViewWidget(TSharedPtr<FBspBuilderType> BspBuilder, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(BspBuilder.IsValid());
	check(BspBuilder->BuilderClass.IsValid());

	UClass* BuilderClass = BspBuilder->BuilderClass.Get();
	
	TSharedRef< STableRow<TSharedPtr<FBspBuilderType>> > TableRowWidget = 
		SNew( STableRow<TSharedPtr<FBspBuilderType>>, OwnerTable )
		.Style( FBspModeStyle::Get(), "BspMode.TableRow" )
		.OnDragDetected( this, &SBspPalette::OnDraggingListViewWidget );

	TSharedRef<SWidget> Content = 
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.Padding(0)
		.ToolTipText(BspBuilder->ToolTipText)
		.Cursor( EMouseCursor::GrabHand )
		[
			SNew(SHorizontalBox)

			// Icon
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(5.0f)
				.BorderImage(FBspModeStyle::Get().GetBrush("BspMode.ThumbnailShadow"))
				[
					SNew(SBox)
					.WidthOverride(35.0f)
					.HeightOverride(35.0f)
					[
						SNew(SBorder)
						.BorderImage(FBspModeStyle::Get().GetBrush("BspMode.ThumbnailBackground"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(BspBuilder->Icon)
						]
					]
				]
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.TextStyle(FBspModeStyle::Get(), "BspMode.ThumbnailText")
				.Text(BspBuilder->Text)
			]
		];

	TableRowWidget->SetContent(Content);

	return TableRowWidget;
}

void SBspPalette::OnSelectionChanged(TSharedPtr<FBspBuilderType> BspBuilder, ESelectInfo::Type SelectionType)
{
	if(BspBuilder.IsValid())
	{
		ActiveBrushBuilder = GEditor->FindBrushBuilder(BspBuilder->BuilderClass.Get());
	}
}

FReply SBspPalette::OnDraggingListViewWidget(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		if (ActiveBrushBuilder.IsValid())
		{
			FBspModeModule& BspModeModule = FModuleManager::GetModuleChecked<FBspModeModule>("BspMode");
			TSharedPtr<FBspBuilderType> BspBuilder = BspModeModule.FindBspBuilderType(ActiveBrushBuilder.Get()->GetClass());
			if(BspBuilder.IsValid())
			{
				// We have an active brush builder, start a drag-drop
				return FReply::Handled().BeginDragDrop(FBrushBuilderDragDropOp::New(ActiveBrushBuilder, BspBuilder->Icon, bIsAdditive));
			}
		}
	}

	return FReply::Unhandled();
}

void SBspPalette::OnAdditiveModeButtonClicked(ESlateCheckBoxState::Type CheckType)
{
	bIsAdditive = CheckType == ESlateCheckBoxState::Checked;
}

void SBspPalette::OnSubtractiveModeButtonClicked(ESlateCheckBoxState::Type CheckType)
{
	bIsAdditive = CheckType != ESlateCheckBoxState::Checked;
}

ESlateCheckBoxState::Type SBspPalette::IsAdditiveModeChecked() const
{
	return bIsAdditive ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

ESlateCheckBoxState::Type SBspPalette::IsSubtractiveModeChecked() const
{
	return !bIsAdditive ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

const FSlateBrush* SBspPalette::GetAdditiveModeImage() const
{
	return FBspModeStyle::Get().GetBrush("BspMode.CSGAdd.Small");
}

const FSlateBrush* SBspPalette::GetSubtractiveModeImage() const
{
	return FBspModeStyle::Get().GetBrush("BspMode.CSGSubtract.Small");
}

#undef LOCTEXT_NAMESPACE