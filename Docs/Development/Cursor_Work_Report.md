# Cursor Work Report

## Status

**MINIMAP_SURFACE_PALETTE_FIX_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Point fix only: native `UGP_MinimapWidget` UMG Palette exposure. Runtime/rendering/FoW unchanged. `WBP_GP_HUD` untouched.

## Branch / SHAs

- Branch: `ui/gp-minimap`
- Checkpoint base `origin/main` (as specified earlier): `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Current `origin/main`: `cfd3d3858993b372ea69bd55865b831584297a83` (not rebased)
- Merge-base with `origin/main`: `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Previous surface tip: `52454dcfb9b58bb3b0eab378b468e383fc0458f1`
- Head (implementation): `7968c4a7ca84a4bb910d948c8fae73e79c146901`
- This report commit is the following commit on the same branch
- Fast-forward merge to `main`: **not requested / not merge-ready**

## Exact cause

UMG Palette does **not** use `UCLASS` `DisplayName` as the catalog key.

`SPaletteViewModel::BuildClassWidgetList` gathers `GetDerivedClasses(UWidget)` then `UWidget::GetPaletteCategory()` on the CDO (`FWidgetBlueprintEditorUtils::GetPaletteCategory`). Engine native widgets (`UImage`, `UButton`, …) override `GetPaletteCategory()` under `WITH_EDITOR` and return a real category (`"Common"`, etc.). Default `UWidget::GetPaletteCategory()` is `"Uncategorized"`.

`UGP_MinimapWidget` only had `meta = (DisplayName = "GP Minimap", ShortTooltip = ...)`. That names the entry if it is listed; it does **not** register a palette category. After a full editor restart the widget was not findable as **GP Minimap**.

Project audit of the named examples:

| Class | Super | Palette-relevant flags | GetPaletteCategory |
| --- | --- | --- | --- |
| `UGP_FoWWorldOverlayWidget` | `UUserWidget` | `UCLASS(NotBlueprintable)` — hidden from palette | none (UserWidget default `"User Created"` unused because NotBlueprintable) |
| `UGP_HealthBarWidget` | `UUserWidget` | default Blueprintable native user widget | none → `"User Created"` |
| `UGP_MarqueeSelectionWidget` | `UUserWidget` | same | none → `"User Created"` |
| `UGP_MinimapWidget` | `UWidget` (native leaf, like `UImage`) | not Abstract/Hidden/HideDropDown | **was missing** → `"Uncategorized"` |

Those three are not the same UMG-leaf pattern. The production match for a native `UWidget` is the engine `UImage` override.

No existing project `GetPaletteCategory` string. New category: **GP**.

## Exact fix

`UGP_MinimapWidget`:

```cpp
#if WITH_EDITOR
virtual const FText GetPaletteCategory() override;
#endif
```

```cpp
#if WITH_EDITOR
const FText UGP_MinimapWidget::GetPaletteCategory()
{
	return NSLOCTEXT("GPMinimapWidget", "PaletteCategory", "GP");
}
#endif
```

DisplayName **GP Minimap** kept. No runtime/FoW/background/bind changes. No Content, no Config, no WBP.

## Tests / build

Headless `-game -unattended -nop4 -NullRHI` `L_PrototypeArena`. No quit. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapSurfaceContractTest` | Complete Failures=0 (`T_WidgetIsPlaceableInUMGDesigner`, `T2_UMGPaletteCategoryIsGP`) |
| `GPEditor Win64 Development` (UHT included) | **Passed** |
| `GP Win64 Development` / Shipping | **not run** |

New contract: non-empty palette category `"GP"`, DisplayName `"GP Minimap"`, class not Abstract/Deprecated/Hidden/HideDropDown.

## Changed files

- `GP/Source/GPUIRuntime/Public/Widgets/GPMinimapWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPMinimapWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapSurfaceContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md` (this report)

## Protected-file audit

**Not committed:** `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `WBP_GP_SelectionGroupRow`, `WBP_GP_LaunchContainerRow`, `Content/`, `Config/`, authored maps, Materials, VFX, `GP.uproject`, `Tools/`.

## Operator step

1. Restart Unreal Editor (full restart, not live coding only).
2. Open `WBP_GP_HUD` (local, uncommitted).
3. Palette search: **GP Minimap**.
4. Expect category **GP**, widget **GP Minimap**.
5. Drag into the bottom-left minimap container (Anchors/Fill). Do not commit the WBP.
