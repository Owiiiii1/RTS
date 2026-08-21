# GP — Production HUD Data Foundation

**Status:** `PRODUCTION_HUD_FOUNDATION_FINALIZED_READY_FOR_MERGE`
**Branch:** `feature/gp-production-hud-foundation`
**Base:** `origin/main` @ `ad2e5eb94afbef6922c332c0d35ff0f9337423c2`

## Delivered boundary

- `UGP_UserWidgetBase : UCommonUserWidget`
- `UGP_HUDRootWidget : UGP_UserWidgetBase`
- FieldNotify `UGP_ResourceViewModel` and `UGP_MatchViewModel`
- Push-only GAS/GameState adapters
- `UGP_HUDViewModelSubsystem : ULocalPlayerSubsystem` as per-local-player owner/access path
- Event-driven late PlayerState/GameState/opponent rebind with clean teardown
- `gp.UI.HUDDump`
- `gp.UI.RunProductionHUDFoundationContractTest`

The only gameplay-side additions are read-only lifecycle notifications:
`AGP_PlayerController::OnPlayerStatePresentationReady` for the owning PlayerState link and
`AGP_GameState::OnPlayerStateRosterChanged` for roster add/remove. Authority and replication semantics
are unchanged.

## Explicitly not delivered

- No authored `WBP_GP_HUD`
- No visible production resource/timer HUD
- No Selection UI / Command Bar
- No Order Menu
- No minimap
- No notifications
- No production end-of-match screen
- No TEMP HUD removal or migration
- No Content, Config, map, Blueprint, DataAsset, or Tools edits

## Operator validation

PIE `gp.UI.HUDDump`:

- Initial: Ready=Ready, LocalTeamId=1, OrbitalFerronite=0, FerroniteScore=0, FerroniteThreatValue=0
- After live gameplay: OrbitalFerronite=100, FerroniteScore=100, FerroniteThreatValue=250

Live push path accepted.

## Final validation

- `gp.UI.RunProductionHUDFoundationContractTest` — **PASS**, Failures=0
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, Failures=0
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **PASS**
- GP Win64 Shipping — **PASS**

This slice is **NOT MERGED**. Production HUD remains **PARTIAL**.
