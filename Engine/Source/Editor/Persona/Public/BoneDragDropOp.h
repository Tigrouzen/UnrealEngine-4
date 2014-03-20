// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

//////////////////////////////////////////////////////////////////////////
// FBoneDragDropOp
class FBoneDragDropOp : public FDragDropOperation, public TSharedFromThis<FBoneDragDropOp>
{
public:	

	static FString GetTypeId() 
	{
		static FString Type = TEXT("FBoneDragDropOp"); 
		return Type;
	}

	USkeleton * TargetSkeleton;
	FName BoneName;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const OVERRIDE
	{
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[		
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &FBoneDragDropOp::GetIcon)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock) 
					.Text( this, &FBoneDragDropOp::GetHoverText )
				]
			];
	}

	FString GetHoverText() const
	{
		FString HoverText = FString::Printf(TEXT("Bone %s"), *BoneName.GetPlainNameString());
		return HoverText;
	}

	const FSlateBrush* GetIcon( ) const
	{
		return CurrentIconBrush;
	}

	void SetIcon(const FSlateBrush* InIcon)
	{
		CurrentIconBrush = InIcon;
	}

	static TSharedRef<FBoneDragDropOp> New(USkeleton* Skeleton, const FName & InBoneName)
	{
		TSharedRef<FBoneDragDropOp> Operation = MakeShareable(new FBoneDragDropOp);
		Operation->BoneName = InBoneName;
		Operation->TargetSkeleton = Skeleton;
		Operation->SetIcon( FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")) );
		FSlateApplication::GetDragDropReflector().RegisterOperation<FBoneDragDropOp>(Operation);
		Operation->Construct();
		return Operation;
	}

private:
	const FSlateBrush* CurrentIconBrush;
};
