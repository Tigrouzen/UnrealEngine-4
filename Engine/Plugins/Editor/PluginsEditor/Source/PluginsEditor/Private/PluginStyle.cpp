// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PluginsEditorPrivatePCH.h"
#include "PluginStyle.h"
#include "SlateStyle.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPluginStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPluginStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( FPluginStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( FPluginStyle::InContent( RelativePath, ".ttf" ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( FPluginStyle::InContent( RelativePath, ".otf" ), __VA_ARGS__ )

FString FPluginStyle::InContent( const FString& RelativePath, const ANSICHAR* Extension )
{
	static FString ContentDir = FPaths::EnginePluginsDir() / TEXT("Editor/PluginsEditor/Content");
	return ( ContentDir / RelativePath ) + Extension;
}

TSharedPtr< FSlateStyleSet > FPluginStyle::StyleSet = NULL;
TSharedPtr< class ISlateStyle > FPluginStyle::Get() { return StyleSet; }

void FPluginStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon9x19(9.0f, 19.0f);
	const FVector2D Icon10x10(10.0f, 10.0f);
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon22x22(22.0f, 22.0f);
	const FVector2D Icon24x24(24.0f, 24.0f);
	const FVector2D Icon27x31(27.0f, 31.0f);
	const FVector2D Icon26x26(26.0f, 26.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon75x82(75.0f, 82.0f);
	const FVector2D Icon360x32(360.0f, 32.0f);
	const FVector2D Icon171x39(171.0f, 39.0f);
	const FVector2D Icon170x50(170.0f, 50.0f);
	const FVector2D Icon267x140(170.0f, 50.0f);

	// Only register once
	if( StyleSet.IsValid() )
	{
		return;
	}

	StyleSet = MakeShareable( new FSlateStyleSet("PluginStyle") );

	// Plugins Manager
	{
		const FTextBlockStyle NormalText = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");

		StyleSet->Set( "Plugins.TabIcon", new IMAGE_BRUSH( "icon_tab_Plugins_16x", Icon16x16 ) );
		StyleSet->Set( "Plugins.BreadcrumbArrow", new IMAGE_BRUSH( "SmallArrowRight", Icon10x10 ) );
		StyleSet->Set( "Plugins.Warning", new IMAGE_BRUSH( "alert", Icon20x20) );

		//Category Tree Item
		{
			const float IconSize = 16.0f;
			const float PaddingAmount = 2.0f;

			StyleSet->Set( "CategoryTreeItem.IconSize", IconSize );
			StyleSet->Set( "CategoryTreeItem.PaddingAmount", PaddingAmount );

			StyleSet->Set( "CategoryTreeItem.BuiltIn", new IMAGE_BRUSH( "icon_plugins_builtin_20x", Icon20x20 ) );
			StyleSet->Set( "CategoryTreeItem.Installed", new IMAGE_BRUSH( "icon_plugins_installed_20x", Icon20x20 ) );
			StyleSet->Set( "CategoryTreeItem.LeafItemWithPlugin", new IMAGE_BRUSH( "hiererchy_16x", Icon12x12 ) );
			StyleSet->Set( "CategoryTreeItem.ExpandedCategory", new IMAGE_BRUSH( "FolderOpen", FVector2D(18, 16) ) );
			StyleSet->Set( "CategoryTreeItem.Category", new IMAGE_BRUSH( "FolderClosed", FVector2D(18, 16) ) );

			//Root Category Tree Item
			{
				const float ExtraVerticalPadding = 3.0f;
				const int32 FontSize = 14;

				{
					StyleSet->Set( "CategoryTreeItem.Root.BackgroundBrush", new FSlateNoResource );
					StyleSet->Set( "CategoryTreeItem.Root.BackgroundPadding", FMargin( PaddingAmount, PaddingAmount + ExtraVerticalPadding, PaddingAmount, PaddingAmount + ExtraVerticalPadding ) );
				}

				FTextBlockStyle Text = FTextBlockStyle( NormalText );
				{
					Text.Font.Size = FontSize;
					StyleSet->Set( "CategoryTreeItem.Root.Text", Text );
				}

				FTextBlockStyle PluginCountText = FTextBlockStyle( NormalText )
					.SetColorAndOpacity( FSlateColor::UseSubduedForeground() );
				{
					PluginCountText.Font.Size = FontSize - 3;
					StyleSet->Set( "CategoryTreeItem.Root.PluginCountText", PluginCountText );
				}

			}

			//Subcategory Tree Item
			{
				const int32 FontSize = 11;

				{
					StyleSet->Set( "CategoryTreeItem.BackgroundBrush", new FSlateNoResource );
					StyleSet->Set( "CategoryTreeItem.BackgroundPadding", FMargin( PaddingAmount ) );
				}

				FTextBlockStyle Text = FTextBlockStyle( NormalText );
				{
					Text.Font.Size = FontSize;
					StyleSet->Set( "CategoryTreeItem.Text", Text );
				}

				FTextBlockStyle PluginCountText = FTextBlockStyle( NormalText )
					.SetColorAndOpacity( FSlateColor::UseSubduedForeground() );
				{
					PluginCountText.Font.Size = FontSize - 3;
					StyleSet->Set( "CategoryTreeItem.PluginCountText", PluginCountText );
				}

			}
		}

		//Plugin Tile
		{
			const float PaddingAmount = 2.0f;
			StyleSet->Set( "PluginTile.Padding", PaddingAmount );

			const float ThumbnailImageSize = 128.0f;
			StyleSet->Set( "PluginTile.ThumbnailImageSize", ThumbnailImageSize );

			{
				StyleSet->Set( "PluginTile.BackgroundBrush", new FSlateNoResource );
				StyleSet->Set( "PluginTile.BackgroundPadding", FMargin( PaddingAmount ) );
			}

			FTextBlockStyle NameText = FTextBlockStyle( NormalText )
				.SetColorAndOpacity( FLinearColor( 0.9f, 0.9f, 0.9f ) );
			{
				NameText.Font.Size = 14;
				StyleSet->Set( "PluginTile.NameText", NameText );
			}

			FTextBlockStyle BetaText = FTextBlockStyle( NormalText )
				.SetColorAndOpacity( FLinearColor( 0.9f, 0.9f, 0.9f ) );
			{
				BetaText.Font.Size = 14;
				StyleSet->Set( "PluginTile.BetaText", BetaText );
			}

			FTextBlockStyle VersionNumberText = FTextBlockStyle( NormalText )
				.SetColorAndOpacity( FLinearColor( 0.9f, 0.9f, 0.9f ) );
			{
				VersionNumberText.Font.Size = 12;
				StyleSet->Set( "PluginTile.VersionNumberText", VersionNumberText );
			}

			FTextBlockStyle CreatedByText = FTextBlockStyle( NormalText )
				.SetColorAndOpacity( FLinearColor( 0.45f, 0.45f, 0.45f ) );
			{
				CreatedByText.Font.Size = 8;
				StyleSet->Set( "PluginTile.CreatedByText", CreatedByText );
			}

			StyleSet->Set( "PluginTile.BetaWarning", new IMAGE_BRUSH( "icon_plugins_betawarn_14px", FVector2D(14, 14) ) );
		}
	}

	FSlateStyleRegistry::RegisterSlateStyle( *StyleSet.Get() );
};

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FPluginStyle::Shutdown()
{
	if( StyleSet.IsValid() )
	{
		FSlateStyleRegistry::UnRegisterSlateStyle( *StyleSet.Get() );
		ensure( StyleSet.IsUnique() );
		StyleSet.Reset();
	}
}