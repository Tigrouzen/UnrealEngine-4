// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinEnum.h"
#include "SGraphPinComboBox.h"

//Construct combo box using combo button and combo list
void SPinComboBox::Construct( const FArguments& InArgs )
{
	ComboItemList = InArgs._ComboItemList.Get();
	OnSelectionChanged = InArgs._OnSelectionChanged;
	VisibleText = InArgs._VisibleText;
	OnGetDisplayName = InArgs._OnGetDisplayName;

	this->ChildSlot
	[
		SAssignNew( ComboButton, SComboButton )
		.MenuWidth(200.0f)
		.ContentPadding(3)
		.ButtonContent()
		[
			SNew( STextBlock ).ToolTipText(NSLOCTEXT("PinComboBox", "ToolTip", "Select enum values from the list"))
			.Text( this, &SPinComboBox::OnGetVisibleTextInternal )
			.Font( FEditorStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") ) )
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.MaxHeight(450.0f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				.Padding( 0 )
				[
					SAssignNew( ComboList, SComboList )
					.ListItemsSource( &ComboItemList )
					.ItemHeight( 22 )
					.OnGenerateRow( this, &SPinComboBox::OnGenerateComboWidget )
					.OnSelectionChanged( this, &SPinComboBox::OnSelectionChangedInternal )
				]
			]
		]
	];
}

//Function to set the newly selected item
void SPinComboBox::OnSelectionChangedInternal( TSharedPtr<int32> NewSelection, ESelectInfo::Type SelectInfo )
{
	if (CurrentSelection.Pin() != NewSelection)
	{
		CurrentSelection = NewSelection;
		// Close the popup as soon as the selection changes
		ComboButton->SetIsOpen( false );

		OnSelectionChanged.ExecuteIfBound( NewSelection, SelectInfo );
	}
}

// Function to create each row of the combo widget
TSharedRef<ITableRow> SPinComboBox::OnGenerateComboWidget( TSharedPtr<int32> InComboIndex, const TSharedRef<STableViewBase>& OwnerTable )
{
	int32 RowIndex = *InComboIndex;

	return
		SNew(STableRow< TSharedPtr<int32> >, OwnerTable)
		[
			SNew( STextBlock )
			.Text( this, &SPinComboBox::GetRowString, RowIndex )
			.Font( FEditorStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") ) )
		];
}

void SGraphPinEnum::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinEnum::GetDefaultValueWidget()
{
	//Get list of enum indexes
	TArray< TSharedPtr<int32> > ComboItems;
	GenerateComboBoxIndexes( ComboItems );

	//Create widget
	return SAssignNew(ComboBox, SPinComboBox)
		.ComboItemList( ComboItems )
		.VisibleText( this, &SGraphPinEnum::OnGetText )
		.OnSelectionChanged( this, &SGraphPinEnum::ComboBoxSelectionChanged )
		.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
		.OnGetDisplayName(this, &SGraphPinEnum::OnGetFriendlyName);
}

FString SGraphPinEnum::OnGetFriendlyName(int32 EnumIndex)
{
	UEnum* EnumPtr = Cast<UEnum>(GraphPinObj->PinType.PinSubCategoryObject.Get());

	check(EnumPtr);
	check(EnumIndex < EnumPtr->NumEnums());

	FString EnumValueName = EnumPtr->GetDisplayNameText(EnumIndex).ToString();
	if (EnumValueName.Len() == 0) 
	{
		EnumValueName = EnumPtr->GetEnumName(EnumIndex);
	}

	return EnumValueName;
}

void SGraphPinEnum::ComboBoxSelectionChanged( TSharedPtr<int32> NewSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	UEnum* EnumPtr = Cast<UEnum>(GraphPinObj->PinType.PinSubCategoryObject.Get());
	check(EnumPtr);
	check(*NewSelection < EnumPtr->NumEnums() - 1);
	//Set new selection
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, EnumPtr->GetEnumName(*NewSelection));
}

FString SGraphPinEnum::OnGetText() const 
{
	FString SelectedString = GraphPinObj->GetDefaultAsString();

	UEnum* EnumPtr = Cast<UEnum>(GraphPinObj->PinType.PinSubCategoryObject.Get());
	if (EnumPtr && EnumPtr->NumEnums())
	{
		const int32 MaxIndex = EnumPtr->NumEnums() - 1;
		for (int32 EnumIdx = 0; EnumIdx < MaxIndex; ++EnumIdx)
		{
			// Ignore hidden enum values
			if( !EnumPtr->HasMetaData(TEXT("Hidden"), EnumIdx ) )
			{
				if (SelectedString == EnumPtr->GetEnumName(EnumIdx))
				{
					FString EnumDisplayName = EnumPtr->GetDisplayNameText(EnumIdx).ToString();
					if (EnumDisplayName.Len() == 0)
					{
						return SelectedString;
					}
					else
					{
						return EnumDisplayName;
					}
				}
			}
		}

		if (SelectedString == EnumPtr->GetEnumName(MaxIndex))
		{
			return TEXT("(INVALID)");
		}

	}
	return SelectedString;
}

void SGraphPinEnum::GenerateComboBoxIndexes( TArray< TSharedPtr<int32> >& OutComboBoxIndexes )
{
	UEnum* EnumPtr = Cast<UEnum>( GraphPinObj->PinType.PinSubCategoryObject.Get() );
	if (EnumPtr)
	{

		//NumEnums() - 1, because the last item in an enum is the _MAX item
		for (int32 EnumIndex = 0; EnumIndex < EnumPtr->NumEnums() - 1; ++EnumIndex)
		{
			// Ignore hidden enum values
			if( !EnumPtr->HasMetaData(TEXT("Hidden"), EnumIndex ) )
			{
				TSharedPtr<int32> EnumIdxPtr(new int32(EnumIndex));
				OutComboBoxIndexes.Add(EnumIdxPtr);
			}
		}
	}
}