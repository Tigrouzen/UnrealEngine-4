// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UNavArea::UNavArea(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	DefaultCost = 1.f;
	FixedAreaEnteringCost = 0.f;
	DrawColor = FColor(255,0,255);
	SupportedAgentsBits = 0xffffffff;
	// NOTE! AreaFlags == 0 means UNWALKABLE!
	AreaFlags = 1;  
}

void UNavArea::PostInitProperties()
{
	Super::PostInitProperties();
	
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UNavigationSystem::RequestAreaRegistering(GetClass());
	}
}

void UNavArea::FinishDestroy()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UNavigationSystem::RequestAreaUnregistering(GetClass());
	}

	Super::FinishDestroy();
}

FColor UNavArea::GetColor(class UClass* AreaDefinitionClass)
{
	return AreaDefinitionClass ? AreaDefinitionClass->GetDefaultObject<UNavArea>()->DrawColor : FColor::Black;
}

void UNavArea::CopyFrom(TSubclassOf<UNavArea> AreaClass)
{
	if (AreaClass)
	{
		UNavArea* DefArea = (UNavArea*)AreaClass->GetDefaultObject();

		DefaultCost = DefArea->DefaultCost;
		FixedAreaEnteringCost = DefArea->FixedAreaEnteringCost;
		AreaFlags = DefArea->GetAreaFlags();
		DrawColor = DefArea->DrawColor;

		// don't copy supported agents bits
	}
}

#if WITH_EDITOR
void UNavArea::UpdateAgentConfig()
{
	const UNavigationSystem* DefNavSys = (UNavigationSystem*)(UNavigationSystem::StaticClass()->GetDefaultObject());
	check(DefNavSys);

	const int32 MaxAllowedAgents = 16;
	const int32 NumAgents = FMath::Min(DefNavSys->SupportedAgents.Num(), MaxAllowedAgents);
	if (DefNavSys->SupportedAgents.Num() > MaxAllowedAgents)
	{
		UE_LOG(LogNavigation, Error, TEXT("Navigation system supports %d agents, but only %d can be shown in %s properties!"),
			DefNavSys->SupportedAgents.Num(), MaxAllowedAgents, *GetClass()->GetName());
	}

	const FString CustomNameMeta = TEXT("DisplayName");
	for (int32 i = 0; i < MaxAllowedAgents; i++)
	{
		const FString PropName = FString::Printf(TEXT("bSupportsAgent%d"), i);
		UProperty* Prop = FindField<UProperty>(UNavArea::StaticClass(), *PropName);
		check(Prop);

		if (i < NumAgents && NumAgents > 1)
		{
			Prop->SetPropertyFlags(CPF_Edit);
			Prop->SetMetaData(*CustomNameMeta, *FString::Printf(TEXT("Supports Agent: %s"), *DefNavSys->SupportedAgents[i].Name.ToString()));
		}
		else
		{
			Prop->ClearPropertyFlags(CPF_Edit);
		}
	}
}
#endif