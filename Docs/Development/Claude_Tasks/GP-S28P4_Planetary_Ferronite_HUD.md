# GP-S28P4 — Minimal Planetary Ferronite HUD

## Status
**GP-S28P4_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Slice Group
Slice 6 follow-on — Resource Playable Pass (P4)

## Code Allowed
**YES**

## Depends On
- GP-S28 merged (haul / Storage / Threat)
- GP-S28P1…P3 merged on `main`
- Base main: `fb699db32d1bc79a62809274e39b8a588633cf3c` (Merge GP-S28P3 drop-off resilience)
- Canonical: `Resource_Playable_Pass_Audit.md` §10 Minimal resource HUD

## Branch
`feature/gp-s28p4-planetary-ferronite-hud` (no merge until operator validation + tech-lead approval)

## Goal
Show the local player **Planetary Ferronite** = `UGP_StorageComponent::GetTotalStored()` on the local-team MainBase. TEMP prototype HUD only (`TEMP_S28P_HUD`). No Tick / no per-frame actor polling / no duplicate resource SoT.

## Implementation summary
- **Client-safe MainBase resolve:** replicated `ReplicatedMainBases` on `AGP_GameState` (`FGP_ReplicatedMainBaseEntry`), synced from authority Register/Unregister; `FindMainBaseForTeamClientSafe(TeamId)`; `OnResolvedMainBaseChanged(TeamId, Previous, New)`
- **TeamId change signal:** `AGP_PlayerState::OnTeamIdChanged` for HUD rebind
- **TEMP HUD:** `UGP_TEMP_S28P_PlanetaryFerroniteHUD` — PC-owned paint widget (`Ferronite: <int>` / `Ferronite: --`)
- **Binding:** `AGP_PlayerController` ensures HUD, binds GameState resolve + PlayerState TeamId + Storage `OnStorageChanged`; initial sync on bind; rebind on MainBase / Team change
- **Contract:** `gp.Resource.RunPlanetaryFerroniteHUDContractTest` + S28 suite entry

## Operator validation target (manual, pending)
1. Map: Worker + ResourceNode + MainBase Team1
2. PIE — HUD shows current stored amount immediately
3. Haul deposit +50 → HUD updates without Tick/polling
4. Next haul +50 again
5. Destroy/replace MainBase → rebind
6. ListenServer/client if available: local-team value only

## Automated tests
| Command | Result |
| --- | --- |
| `gp.Resource.RunPlanetaryFerroniteHUDContractTest` | `Complete Failures=0 Cancelled=None` |
| `gp.Resource.RunS28RegressionSuite` | `Complete Failures=0` |

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Win64 Development / Shipping | Not required at this candidate stage |

## Scope audit
Allowed: client-safe MainBase resolve replication; TEMP Planetary Ferronite HUD; PC event binding; P4 contract + suite; docs.
Absent: combat; P3 haul/drop-off semantics changes; orbital launch; OrbitalFerronite; FerroniteScore; LogisticsHub; storage overflow; construction; navigation; projectiles; Blueprint/map/content; Threat/quota/timer UI; full RTS HUD.

## Invariants
- Storage `GetTotalStored()` remains SoT — no duplicate replicated resource counter
- Authority remains SoT for MainBase registration mutation
- No Tick / no `GetAllActorsOfClass` per frame for HUD
- Local team only; foreign MainBase never bound to this widget
- Late join / OnRep path gets current total without waiting for next deposit

## Stop condition
**CODE_READY_OPERATOR_VALIDATION_PENDING.** Operator validates in Editor. Do **not** merge. Do **not** start P5 / next stage until explicitly assigned.
