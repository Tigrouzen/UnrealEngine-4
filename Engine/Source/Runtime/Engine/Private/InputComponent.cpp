// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineLevelScriptClasses.h"		// For ALevelScriptActor

FInputChord::RelationshipType FInputChord::GetRelationship(const FInputChord& OtherChord) const
{
	RelationshipType Relationship = None;

	if (Key == OtherChord.Key)
	{
		if (    bAlt == OtherChord.bAlt
			 && bCtrl == OtherChord.bCtrl
			 && bShift == OtherChord.bShift)
		{
			Relationship = Same;
		}
		else if (    (bAlt || !OtherChord.bAlt)
				  && (bCtrl || !OtherChord.bCtrl)
				  && (bShift || !OtherChord.bShift))
		{
			Relationship = Masks;
		}
		else if (    (!bAlt || OtherChord.bAlt)
				  && (!bCtrl || OtherChord.bCtrl)
				  && (!bShift || OtherChord.bShift))
		{
			Relationship = Masked;
		}
	}

	return Relationship;
}

UInputComponent::UInputComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bBlockInput = false;
}

float UInputComponent::GetAxisValue(const FName AxisName) const
{
	for (int32 Index = 0; Index < AxisBindings.Num(); ++Index)
	{
		const FInputAxisBinding& AxisBinding = AxisBindings[Index];
		if (AxisBinding.AxisName == AxisName)
		{
			return AxisBinding.AxisValue;
		}
	}

	return 0.f;
}

float UInputComponent::GetAxisKeyValue(const FKey AxisKey) const
{
	for (int32 Index = 0; Index < AxisKeyBindings.Num(); ++Index)
	{
		const FInputAxisKeyBinding& AxisBinding = AxisKeyBindings[Index];
		if (AxisBinding.AxisKey == AxisKey)
		{
			return AxisBinding.AxisValue;
		}
	}

	return 0.f;
}

bool UInputComponent::HasBindings() const
{
	return ((ActionBindings.Num() > 0) || (AxisBindings.Num() > 0) || (AxisKeyBindings.Num() > 0) || (KeyBindings.Num() > 0) || (TouchBindings.Num() > 0));
}

FInputActionBinding& UInputComponent::AddActionBinding(const FInputActionBinding& Binding)
{
	ActionBindings.Add(Binding);
	if (Binding.KeyEvent == IE_Pressed || Binding.KeyEvent == IE_Released)
	{
		const EInputEvent PairedEvent = (Binding.KeyEvent == IE_Pressed ? IE_Released : IE_Pressed);
		for (int32 BindingIndex = ActionBindings.Num() - 2; BindingIndex >= 0; --BindingIndex )
		{
			FInputActionBinding& ActionBinding = ActionBindings[BindingIndex];
			if (ActionBinding.ActionName == Binding.ActionName)
			{
				// If we find a matching event that is already paired we know this is paired so mark it off and we're done
				if (ActionBinding.bPaired)
				{
					ActionBindings.Last().bPaired = true;
					break;
				}
				// Otherwise if this is a pair to the new one mark them both as paired
				// Don't break as there could be two bound paired events
				else if (ActionBinding.KeyEvent == PairedEvent)
				{
					ActionBinding.bPaired = true;
					ActionBindings.Last().bPaired = true;
				}
			}
		}
	}

	return ActionBindings.Last();
}

void UInputComponent::RemoveActionBinding(const int32 BindingIndex)
{
	if (BindingIndex >= 0 && BindingIndex < ActionBindings.Num())
	{
		const FInputActionBinding& BindingToRemove = ActionBindings[BindingIndex];

		// Potentially need to clear some pairings
		if (BindingToRemove.bPaired)
		{
			TArray<int32> IndicesToClear;
			const EInputEvent PairedEvent = (BindingToRemove.KeyEvent == IE_Pressed ? IE_Released : IE_Pressed);

			for (int32 ActionIndex = 0; ActionIndex < ActionBindings.Num(); ++ActionIndex)
			{
				if (ActionIndex != BindingIndex)
				{
					const FInputActionBinding& ActionBinding = ActionBindings[ActionIndex];
					if (ActionBinding.ActionName == BindingToRemove.ActionName)
					{
						// If we find another of the same key event then the pairing is intact so we're done
						if (ActionBinding.KeyEvent == BindingToRemove.KeyEvent)
						{
							IndicesToClear.Empty();
							break;
						}
						// Otherwise we may need to clear the pairing so track the index
						else if (ActionBinding.KeyEvent == PairedEvent)
						{
							IndicesToClear.Add(ActionIndex);
						}
					}
				}
			}
			for (int32 ClearIndex = 0; ClearIndex < IndicesToClear.Num(); ++ClearIndex)
			{
				ActionBindings[IndicesToClear[ClearIndex]].bPaired = false;
			}
		}

		ActionBindings.RemoveAt(BindingIndex);
	}
}

int32 UInputComponent::GetNumActionBindings() const
{
	return ActionBindings.Num();
}

FInputActionBinding& UInputComponent::GetActionBinding(const int32 BindingIndex)
{
	return ActionBindings[BindingIndex];
}


// Deprecated UFUNCTIONS needed for blueprint purposes
bool UInputComponent::IsControllerKeyDown(FKey Key) const { return false; }
bool UInputComponent::WasControllerKeyJustPressed(FKey Key) const { return false; }
bool UInputComponent::WasControllerKeyJustReleased(FKey Key) const { return false; }
float UInputComponent::GetControllerAnalogKeyState(FKey Key) const { return 0.f; }
FVector UInputComponent::GetControllerVectorKeyState(FKey Key) const { return FVector(); }
void UInputComponent::GetTouchState(int32 FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed) const { }
float UInputComponent::GetControllerKeyTimeDown(FKey Key) const { return 0.f; }
void UInputComponent::GetControllerMouseDelta(float& DeltaX, float& DeltaY) const { }
void UInputComponent::GetControllerAnalogStickState(EControllerAnalogStick::Type WhichStick, float& StickX, float& StickY) const { }
