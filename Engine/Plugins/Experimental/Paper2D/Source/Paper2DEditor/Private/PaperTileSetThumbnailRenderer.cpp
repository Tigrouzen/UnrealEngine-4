// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// UPaperTileSetThumbnailRenderer

UPaperTileSetThumbnailRenderer::UPaperTileSetThumbnailRenderer(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UPaperTileSetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas)
{
	UPaperTileSet* TileSet = Cast<UPaperTileSet>(Object);
	if ((TileSet != NULL) && (TileSet->TileSheet != NULL))
	{
		const bool bUseTranslucentBlend = TileSet->TileSheet->HasAlphaChannel();

		// Draw the grid behind the sprite
		if (bUseTranslucentBlend)
		{
			static UTexture2D* GridTexture = NULL;
			if (GridTexture == NULL)
			{
				GridTexture = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EngineMaterials/DefaultWhiteGrid.DefaultWhiteGrid"), NULL, LOAD_None, NULL);
			}

			const bool bAlphaBlend = false;

			Canvas->DrawTile(
				(float)X,
				(float)Y,
				(float)Width,
				(float)Height,
				0.0f,
				0.0f,
				4.0f,
				4.0f,
				FLinearColor::White,
				GridTexture->Resource,
				bAlphaBlend);
		}

		// Draw the sprite itself
		const float TextureWidth = TileSet->TileSheet->GetSurfaceWidth();
		const float TextureHeight = TileSet->TileSheet->GetSurfaceHeight();

		float FinalX = (float)X;
		float FinalY = (float)Y;
		float FinalWidth = (float)Width;
		float FinalHeight = (float)Height;
		const float DesiredWidth = TextureWidth;
		const float DesiredHeight = TextureHeight;

		const FLinearColor BlackBarColor(0.0f, 0.0f, 0.0f, 0.5f);

		if (DesiredWidth > DesiredHeight)
		{
			const float ScaleFactor = Width / DesiredWidth;
			FinalHeight = ScaleFactor * DesiredHeight;
			FinalY += (Height - FinalHeight) * 0.5f;

			// Draw black bars (on top and bottom)
			const bool bAlphaBlend = true;
			Canvas->DrawTile(X, Y, Width, FinalY-Y, 0, 0, 1, 1, BlackBarColor, GWhiteTexture, bAlphaBlend);
			Canvas->DrawTile(X, FinalY+FinalHeight, Width, Height-FinalHeight, 0, 0, 1, 1, BlackBarColor, GWhiteTexture, bAlphaBlend);
		}
		else
		{
			const float ScaleFactor = Height / DesiredHeight;
			FinalWidth = ScaleFactor * DesiredWidth;
			FinalX += (Width - FinalWidth) * 0.5f;

			// Draw black bars (on either side)
			const bool bAlphaBlend = true;
			Canvas->DrawTile(X, Y, FinalX-X, Height, 0, 0, 1, 1, BlackBarColor, GWhiteTexture, bAlphaBlend);
			Canvas->DrawTile(FinalX+FinalWidth, Y, Width-FinalWidth, Height, 0, 0, 1, 1, BlackBarColor, GWhiteTexture, bAlphaBlend);
		}


		// Draw the tile sheet 
		Canvas->DrawTile(
			FinalX,
			FinalY,
			FinalWidth,
			FinalHeight,
			0.0f,
			0.0f,
			1.0f,
			1.0f,
			FLinearColor::White,
			TileSet->TileSheet->Resource,
			bUseTranslucentBlend);

		// Draw a label overlay
		//@TODO: Looks very ugly: DrawShadowedStringZ(Canvas, X, Y + Height * 0.8f, 1.0f, TEXT("Tile\nSet"), GEngine->GetSmallFont(), FLinearColor::White);
	}
}