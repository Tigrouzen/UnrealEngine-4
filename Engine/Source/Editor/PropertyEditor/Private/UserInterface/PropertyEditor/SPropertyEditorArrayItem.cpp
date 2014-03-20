// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyEditorArrayItem.h"
#include "PropertyNode.h"
#include "PropertyEditor.h"

void SPropertyEditorArrayItem::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor>& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	ChildSlot
	.Padding( 0.0f, 0.0f, 5.0f, 0.0f )
	[
		SNew( STextBlock )
		.Text( this, &SPropertyEditorArrayItem::GetValueAsString )
		.Font( InArgs._Font )
	];

	SetEnabled( TAttribute<bool>( this, &SPropertyEditorArrayItem::CanEdit ) );
}

void SPropertyEditorArrayItem::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 130.0f;
	OutMaxDesiredWidth = 130.0f;
}

bool SPropertyEditorArrayItem::Supports( const TSharedRef< class FPropertyEditor >& PropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
	const UProperty* Property = PropertyEditor->GetProperty();

	return !Cast<const UClassProperty>( Property ) &&
		Cast<const UArrayProperty>( Property->GetOuter() ) &&
		PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly) &&
		!(Cast<const UArrayProperty>(Property->GetOuter())->PropertyFlags & CPF_EditConst);
}

FString SPropertyEditorArrayItem::GetValueAsString() const
{
	if( PropertyEditor->GetProperty() && PropertyEditor->PropertyIsA( UStructProperty::StaticClass() ) )
	{
		return FText::Format( NSLOCTEXT("PropertyEditor", "NumStructItems", "{0} members"), FText::AsNumber( PropertyEditor->GetPropertyNode()->GetNumChildNodes() ) ).ToString();
	}
	else
	{
		return PropertyEditor->GetValueAsString();
	}
}

bool SPropertyEditorArrayItem::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}