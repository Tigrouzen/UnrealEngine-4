// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyEditorText.h"
#include "PropertyNode.h"
#include "ObjectPropertyNode.h"
#include "PropertyEditor.h"
#include "PropertyEditorHelpers.h"


void SPropertyEditorText::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	ChildSlot
	[
		SAssignNew( PrimaryWidget, SEditableTextBox )
		.Text( InPropertyEditor, &FPropertyEditor::GetValueAsText )
		.Font( InArgs._Font )
		.SelectAllTextWhenFocused( true )
		.ClearKeyboardFocusOnCommit(false)
		.OnTextCommitted( this, &SPropertyEditorText::OnTextCommitted )
		.SelectAllTextOnCommit( true )
	];

	if( InPropertyEditor->PropertyIsA( UObjectPropertyBase::StaticClass() ) )
	{
		// Object properties should display their entire text in a tooltip
		PrimaryWidget->SetToolTipText( TAttribute<FString>( InPropertyEditor, &FPropertyEditor::GetValueAsString ) );
	}

	SetEnabled( TAttribute<bool>( this, &SPropertyEditorText::CanEdit ) );
}

void SPropertyEditorText::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 125.0f;
	OutMaxDesiredWidth = 600.0f;
}

bool SPropertyEditorText::Supports( const TSharedRef< FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();

	if(	!PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInline)
		&&	( (Property->IsA(UNameProperty::StaticClass()) && Property->GetFName() != NAME_InitialState)
		||	Property->IsA(UStrProperty::StaticClass())
		||	Property->IsA(UTextProperty::StaticClass())
		||	(Property->IsA(UObjectPropertyBase::StaticClass()) && !Property->HasAnyPropertyFlags(CPF_InstancedReference))
		||	Property->IsA(UInterfaceProperty::StaticClass())
		) )
	{
		return true;
	}

	return false;
}

void SPropertyEditorText::OnTextCommitted( const FText& NewText, ETextCommit::Type /*CommitInfo*/ )
{
	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
	const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();

	PropertyHandle->SetValueFromFormattedString( NewText.ToString() );
}

bool SPropertyEditorText::SupportsKeyboardFocus() const
{
	return PrimaryWidget.IsValid() && PrimaryWidget->SupportsKeyboardFocus();
}

FReply SPropertyEditorText::OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	// Forward keyboard focus to our editable text widget
	return FReply::Handled().SetKeyboardFocus( PrimaryWidget.ToSharedRef(), InKeyboardFocusEvent.GetCause() );
}

bool SPropertyEditorText::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}
