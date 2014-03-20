// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
 
#pragma once

DECLARE_DELEGATE_OneParam( FOnTextChosen, const FString& );

/** Simple box containing a combobox of strings to pick from, usually used within a MenuStack */
class SLATE_API STextComboPopup : public SCompoundWidget
{
public:
	/** Array of shared pointers to strings so combo widget can work on them */
	TArray< TSharedPtr<FString> >		Strings;

	/** Delegate to call when OK button is pressed */
	FOnTextChosen						OnTextChosen;

	/** The combo box */
	TSharedPtr< SComboBox< TSharedPtr<FString> > >	StringCombo;
	/** The OK button */
	TSharedPtr< SButton > OkButton;

	SLATE_BEGIN_ARGS( STextComboPopup ) {}

		/** Label, placed before combo box */
		SLATE_TEXT_ARGUMENT( Label )

		/** Selection of strings to pick from */
		SLATE_ARGUMENT( TArray<FString>, TextOptions )

		/** Called when the text is chosen. */
		SLATE_EVENT( FOnTextChosen, OnTextChosen )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** SWidget interface */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE;

	/** Called to create a widget for each string */
	TSharedRef<SWidget> MakeItemWidget( TSharedPtr<FString> StringItem );
	/** Called when 'OK' button is pressed */
	FReply OnOK();

	/** focuses the control on the OK button */
	void FocusDefaultWidget();

private:
	TSharedPtr<FString> OnGetSelection() const {return SelectedItem;}

	/** Called when selection changes in the combo popup */
	void OnSelectionChanged (TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	/** Returns the item to select in the combo popup */
	FString GetSelectedItem () const;

private:
	/** The text item selected */
	TSharedPtr<FString> SelectedItem;
};
