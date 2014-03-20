// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SBlutilityShelf : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlutilityShelf) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SWidget> OnBlutilityGetContextMenu(const TArray<FAssetData>& SelectedAssets);

	static void OnBlutilityDoubleClicked(const class FAssetData& AssetData);
	
	static void ToggleFavoriteStatusOnSelection(TArray<FAssetData> AssetList, bool bIsNewFavorite);
	static bool GetFavoriteStatusOnSelection(TArray<FAssetData> AssetList);
	static void GetFavoriteStatus(const TArray<FAssetData>& AssetList, /*out*/ bool& bAreAnySelected, /*out*/ bool& bAreAllSelected);

	void BuildShelf();
	void ToggleShelfMode();
private:
	bool bInFavoritesMode;
};
