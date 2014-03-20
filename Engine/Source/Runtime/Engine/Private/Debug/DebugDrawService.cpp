// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

TArray<TArray<FDebugDrawDelegate> > UDebugDrawService::Delegates;
FEngineShowFlags UDebugDrawService::ObservedFlags(ESFIM_Editor);

UDebugDrawService::UDebugDrawService(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	Delegates.Reserve(sizeof(FEngineShowFlags)*8);
}

void UDebugDrawService::Register(const TCHAR* Name, const FDebugDrawDelegate& NewDelegate)
{
	check(IsInGameThread());

	int32 Index = FEngineShowFlags::FindIndexByName(Name);

	if (Index != INDEX_NONE)
	{
		if (Index >= Delegates.Num())
		{
			Delegates.AddZeroed(Index - Delegates.Num() + 1);
		}
		Delegates[Index].Add(NewDelegate);
		ObservedFlags.SetSingleFlag(Index, true);
	}
}

void UDebugDrawService::Unregister(const FDebugDrawDelegate& DelegateToRemove)
{
	check(IsInGameThread());

	TArray<FDebugDrawDelegate>* DelegatesArray = Delegates.GetTypedData();
	for (int32 Flag = 0; Flag < Delegates.Num(); ++Flag, ++DelegatesArray)
	{
		check(DelegatesArray); //it shouldn't happen, but to be sure
		const uint32 Index = DelegatesArray->Find(DelegateToRemove);
		if (Index != INDEX_NONE)
		{
			DelegatesArray->RemoveAtSwap(Index, 1, false);
			if (DelegatesArray->Num() == 0)
			{
				ObservedFlags.SetSingleFlag(Flag, false);
			}
		}
	}	
}

void UDebugDrawService::Draw(const FEngineShowFlags Flags, FViewport* Viewport, FSceneView* View, FCanvas* Canvas)
{
	UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(),TEXT("DebugCanvasObject"));
	if (CanvasObject == NULL)
	{
		CanvasObject = ConstructObject<UCanvas>(UCanvas::StaticClass(),GetTransientPackage(),TEXT("DebugCanvasObject"));
		CanvasObject->AddToRoot();
	}
	
	CanvasObject->Init(View->ViewRect.Width(), View->ViewRect.Height(), View);
	CanvasObject->Update();	
	CanvasObject->Canvas = Canvas;

	// PreRender the player's view.
	Draw(Flags, CanvasObject);	
}

void UDebugDrawService::Draw(const FEngineShowFlags Flags, UCanvas* Canvas)
{
	if (Canvas == NULL)
	{
		return;
	}
	
	for (int32 FlagIndex = 0; FlagIndex < Delegates.Num(); ++FlagIndex)
	{
		if (Flags.GetSingleFlag(FlagIndex) && ObservedFlags.GetSingleFlag(FlagIndex) && Delegates[FlagIndex].Num() > 0)
		{
			for (int32 i = Delegates[FlagIndex].Num() - 1; i >= 0; --i)
			{
				FDebugDrawDelegate& Delegate = Delegates[FlagIndex][i];

				if (Delegate.IsBound())
				{
					Delegate.Execute(Canvas, NULL);
				}
				else
				{
					Delegates[FlagIndex].RemoveAtSwap(i, 1, /*bAllowShrinking=*/false);
				}
			}
		}
	}
}
