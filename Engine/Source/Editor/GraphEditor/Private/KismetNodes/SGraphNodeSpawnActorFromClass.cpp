// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SGraphNode.h"
#include "SGraphNodeK2Base.h"
#include "SGraphNodeK2Default.h"
#include "SGraphNodeSpawnActorFromClass.h"
#include "KismetPins/SGraphPinObject.h"
#include "NodeFactory.h"
#include "Editor/ClassViewer/Public/ClassViewerModule.h"
#include "Editor/ClassViewer/Public/ClassViewerFilter.h"

#define LOCTEXT_NAMESPACE "SGraphPinActorBasedClass"

//////////////////////////////////////////////////////////////////////////
// SGraphPinActorBasedClass

/** 
 * GraphPin can select only actor classes.
 * Instead of asset picker, a class viewer is used.
 */
class SGraphPinActorBasedClass : public SGraphPinObject
{
	void OnClassPicked(UClass* InChosenClass)
	{
		AssetPickerAnchor->SetIsOpen(false);

		if(InChosenClass && GraphPinObj)
		{
			check(InChosenClass);
			check(InChosenClass->IsChildOf(AActor::StaticClass()));
			if(const UEdGraphSchema* Schema = GraphPinObj->GetSchema())
			{
				Schema->TrySetDefaultObject(*GraphPinObj, InChosenClass);
			}
		}
	}

	class FActorBasedClassFilter : public IClassViewerFilter
	{
	public:

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) OVERRIDE
		{
			if(NULL != InClass)
			{
				const bool bActorBased = InClass->IsChildOf(AActor::StaticClass());
				const bool bBlueprintType = InClass->GetBoolMetaDataHierarchical(FBlueprintMetadata::MD_AllowableBlueprintVariableType);
				const bool bNotAbstract = !InClass->HasAnyClassFlags(CLASS_Abstract);
				return bActorBased && bBlueprintType && bNotAbstract;
			}
			return false;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) OVERRIDE
		{
			const bool bActorBased = InUnloadedClassData->IsChildOf(AActor::StaticClass());
			const bool bNotAbstract = !InUnloadedClassData->HasAnyClassFlags(CLASS_Abstract);
			return bActorBased && bNotAbstract;
		}
	};

protected:

	virtual FReply OnClickUse() OVERRIDE
	{
		if(GraphPinObj && GraphPinObj->GetSchema())
		{
			const UClass* PinRequiredParentClass = Cast<const UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
			ensure(PinRequiredParentClass);

			const UClass* SelectedClass = GEditor->GetFirstSelectedClass(PinRequiredParentClass);
			if(SelectedClass)
			{
				GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, const_cast<UClass*>(SelectedClass));
			}
		}
		return FReply::Handled();
	}

	virtual FOnClicked GetOnUseButtonDelegate() OVERRIDE
	{
		return FOnClicked::CreateSP( this, &SGraphPinActorBasedClass::OnClickUse );
	}

	virtual FText GetDefaultComboText() const OVERRIDE { return LOCTEXT( "DefaultComboText", "Select Class" ); }

	virtual TSharedRef<SWidget> GenerateAssetPicker() OVERRIDE
	{
		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;
		Options.bIsActorsOnly = true;
		Options.DisplayMode = EClassViewerDisplayMode::DefaultView;
		Options.bShowUnloadedBlueprints = true;
		Options.bShowNoneOption = true;
		Options.bShowObjectRootClass = true;
		TSharedPtr< FActorBasedClassFilter > Filter = MakeShareable(new FActorBasedClassFilter);
		Options.ClassFilter = Filter;

		return 
			SNew(SBox)
			.WidthOverride(280)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(500)
				[
					SNew(SBorder)
					.Padding(4)
					.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
					[
						ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SGraphPinActorBasedClass::OnClassPicked))
					]
				]			
			];
	}
};

//////////////////////////////////////////////////////////////////////////
// SGraphNodeSpawnActorFromClass

void SGraphNodeSpawnActorFromClass::CreatePinWidgets()
{
	UK2Node_SpawnActorFromClass* SpawnActorNode = CastChecked<UK2Node_SpawnActorFromClass>(GraphNode);
	UEdGraphPin* ClassPin = SpawnActorNode->GetClassPin();

	for (auto PinIt = GraphNode->Pins.CreateConstIterator(); PinIt; ++PinIt)
	{
		UEdGraphPin* CurrentPin = *PinIt;
		if ((!CurrentPin->bHidden) && (CurrentPin != ClassPin))
		{
			TSharedPtr<SGraphPin> NewPin = FNodeFactory::CreatePinWidget(CurrentPin);
			check(NewPin.IsValid());
			NewPin->SetIsEditable(IsEditable);
			this->AddPin(NewPin.ToSharedRef());
		}
		else if ((ClassPin == CurrentPin) && (!ClassPin->bHidden || (ClassPin->LinkedTo.Num() > 0)))
		{
			TSharedPtr<SGraphPinActorBasedClass> NewPin = SNew(SGraphPinActorBasedClass, ClassPin);
			check(NewPin.IsValid());
			NewPin->SetIsEditable(IsEditable);
			this->AddPin(NewPin.ToSharedRef());
		}
	}
}

#undef LOCTEXT_NAMESPACE