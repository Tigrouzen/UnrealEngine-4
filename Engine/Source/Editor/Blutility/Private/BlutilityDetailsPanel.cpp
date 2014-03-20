// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlutilityPrivatePCH.h"
#include "BlutilityDetailsPanel.h"

#include "Editor/PropertyEditor/Public/PropertyEditing.h"

#include "BlutilityClasses.h"
#include "ScopedTransaction.h"

/////////////////////////////////////////////////////

struct FCompareClassNames
{
	bool operator()(const UClass& A, const UClass& B) const
	{
		return A.GetName() < B.GetName();
	}
};

/////////////////////////////////////////////////////
// FEditorUtilityInstanceDetails

TSharedRef<IDetailCustomization> FEditorUtilityInstanceDetails::MakeInstance()
{
	return MakeShareable(new FEditorUtilityInstanceDetails);
}

void FEditorUtilityInstanceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetDetailsView().GetSelectedObjects();

	// Hide some useless categories
	//@TODO: How to hide Actors, Layers, etc...?

	// Build a list of unique selected blutilities
	TArray<UClass*> UniqueBlutilityClasses;
	bool bFoundAnyCDOs = false;

	for (auto SelectedObjectIt = SelectedObjectsList.CreateConstIterator(); SelectedObjectIt; ++SelectedObjectIt)
	{
		UObject* Object = (*SelectedObjectIt).Get();

		if (!Object->HasAnyFlags(RF_ClassDefaultObject))
		{
			UClass* ObjectClass = Object->GetClass();

			if (UEditorUtilityBlueprint* Blutility = Cast<UEditorUtilityBlueprint>(ObjectClass->ClassGeneratedBy))
			{
				UniqueBlutilityClasses.Add(ObjectClass);
			}
		}
		else
		{
			bFoundAnyCDOs = true;
		}
	}

	// Run thru each one
	UniqueBlutilityClasses.Sort(FCompareClassNames());
	for (auto ClassIt = UniqueBlutilityClasses.CreateIterator(); ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;

		FString CategoryName = FString::Printf(TEXT("%sActions"), *Class->ClassGeneratedBy->GetName());
		IDetailCategoryBuilder& ActionsCategory = DetailLayoutBuilder.EditCategory(*CategoryName);

		const APlacedEditorUtilityBase* PlacedActorCDO = Cast<const APlacedEditorUtilityBase>(Class->GetDefaultObject());
		if (PlacedActorCDO)
		{
			ActionsCategory.AddCustomRow( PlacedActorCDO->HelpText )
			[
				SNew(STextBlock)
				.Text(PlacedActorCDO->HelpText)
			];
		}
		
		const UGlobalEditorUtilityBase* GlobalBlutilityCDO = Cast<const UGlobalEditorUtilityBase>(Class->GetDefaultObject());
		if (GlobalBlutilityCDO)
		{
			ActionsCategory.AddCustomRow( GlobalBlutilityCDO->HelpText )
			[
				SNew(STextBlock)
				.Text(GlobalBlutilityCDO->HelpText)
			];
		}

		TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox).UseAllottedWidth(true);
		int32 NumButtons = 0;

		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;

			const bool bCanExecute = (Function->NumParms == 0) && Function->HasAllFunctionFlags(FUNC_Exec);

			if (bCanExecute)
			{
				++NumButtons;

				const FString ButtonCaption = EngineUtils::SanitizeDisplayName(*Function->GetName(), false);

				//@TODO: Expose the code in UK2Node_CallFunction::GetUserFacingFunctionName / etc...
				FString Tooltip = Function->GetToolTipText().ToString();
				if (Tooltip.IsEmpty())
				{
					Tooltip = Function->GetName();
				}

				TWeakObjectPtr<UFunction> WeakFunctionPtr(Function);

				WrapBox->AddSlot()
				[
					SNew(SButton)
					.Text(ButtonCaption)
					.OnClicked(	FOnClicked::CreateSP(this, &FEditorUtilityInstanceDetails::OnExecuteAction, WeakFunctionPtr) )
					.ToolTipText(Tooltip)
				];

			}
		}

		if (NumButtons > 0)
		{
			ActionsCategory.AddCustomRow(TEXT(""))
			[
				WrapBox
			];
		}
	}

	// Hide the hint property
	if (!bFoundAnyCDOs)
	{
		DetailLayoutBuilder.HideProperty(TEXT("HelpText"));
	}
}

FReply FEditorUtilityInstanceDetails::OnExecuteAction(TWeakObjectPtr<UFunction> WeakFunctionPtr)
{
	if (UFunction* Function = WeakFunctionPtr.Get())
	{
		FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action") );
		FEditorScriptExecutionGuard ScriptGuard;

		UClass* MinRequiredClass = Function->GetOuterUClass();

		// Execute this function on any objects that support it
		for (auto SelectedObjectIt = SelectedObjectsList.CreateConstIterator(); SelectedObjectIt; ++SelectedObjectIt)
		{
			UObject* Object = (*SelectedObjectIt).Get();

			if ((Object != NULL) && (Object->IsA(MinRequiredClass)))
			{
				Object->ProcessEvent(Function, NULL);

				if (UGlobalEditorUtilityBase* BlutilityInstance = Cast<UGlobalEditorUtilityBase>(Object))
				{
					BlutilityInstance->PostExecutionCleanup();
				}
			}
		}
	}

	return FReply::Handled();
}