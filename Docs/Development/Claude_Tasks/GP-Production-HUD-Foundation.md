# GP — Production HUD Data Foundation

**Status:** `PRODUCTION_HUD_FOUNDATION_READY_FOR_OPERATOR_VALIDATION`
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
- No visual production HUD panels
- No Order Menu
- No selection panel
- No minimap
- No notifications
- No TEMP HUD removal or migration
- No Content, Config, map, Blueprint, DataAsset, or Tools edits

## Validation

- `gp.UI.RunProductionHUDFoundationContractTest` — **PASS**, Failures=0
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, Failures=0
- GPEditor Win64 Development + UHT — **PASS**

## Operator check

Run a listen-server and remote-client session, then execute `gp.UI.HUDDump` for each local player.
Verify Ready, correct local TeamId, own resource/cap values, isolated opponent score, local-team threat,
timer, and match/result summary. This slice is **NOT MERGED** and **NOT FINALIZED**.
