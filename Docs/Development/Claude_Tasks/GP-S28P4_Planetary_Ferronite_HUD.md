# GP-S28P4 — Minimal Planetary Ferronite HUD

## Status
**GP-S28P4_READY_FOR_MERGE**

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
`feature/gp-s28p4-planetary-ferronite-hud` (merge only after tech-lead approval)

## Implementation summary
- **Client-safe MainBase resolve:** replicated `ReplicatedMainBases` on `AGP_GameState` (`FGP_ReplicatedMainBaseEntry`), synced from authority Register/Unregister; `FindMainBaseForTeamClientSafe(TeamId)`; `OnResolvedMainBaseChanged(TeamId, Previous, New)`
- **TeamId change signal:** `AGP_PlayerState::OnTeamIdChanged` for HUD rebind
- **TEMP HUD:** `UGP_TEMP_S28P_PlanetaryFerroniteHUD` (`TEMP_S28P_HUD`) — PC-owned paint widget (`Ferronite: <int>` / `Ferronite: --`)
- **Binding:** `AGP_PlayerController` ensures HUD, binds GameState resolve + PlayerState TeamId + Storage `OnStorageChanged`; initial sync on bind; rebind on MainBase / Team change
- **SoT:** `UGP_StorageComponent::GetTotalStored()` only — no duplicate replicated Ferronite counter
- **Contract:** `gp.Resource.RunPlanetaryFerroniteHUDContractTest` + S28 suite entry
- **TEMP debt:** presentation is temporary playable-pass HUD; future production HUD may replace presentation while keeping client-safe MainBase/data-source contract. Do not polish/rename to final HUD in P4.

## Operator validation (final)
| Scenario | Result |
| --- | --- |
| **A** — Initial HUD | **PASS** |
| **B** — Storage live update | **PASS** |
| **C** — MainBase unregister/destroy | **PASS** |
| **D** — MainBase replacement/rebind | **PASS** |

No separate remote dedicated-client manual test claimed. Multiplayer/team isolation covered by automated contract.

## Automated tests (finalization re-run)
| Command | Result |
| --- | --- |
| `gp.Resource.RunPlanetaryFerroniteHUDContractTest` | `Complete Failures=0 Cancelled=None` |
| `gp.Resource.RunS28RegressionSuite` | `Complete Failures=0` |

## Builds (finalization)
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Win64 Development | **PASSED** |
| GP Win64 Shipping | **PASSED** |

## Scope audit (vs `fb699db…`)
Allowed: GameState replicated MainBase handles + resolve API/delegate; PlayerState TeamId changed; TEMP HUD; PC bind/sync; P4 contract + suite; docs.
`GPWorker.h`: **only** `UGP_PlanetaryFerroniteHUDContractTestRunner` declaration (debug contract infrastructure; Shipping stubs in `.cpp`). No production Worker gameplay API/semantics changes.
Absent: mining quantities; haul / WaitingForDropOff; Threat; Storage transaction/overflow redesign; OrbitalFerronite/Score; launch/container; LogisticsHub drop-off; combat; construction; navigation; Blueprint/map/content.

## Invariants confirmed
- Storage `GetTotalStored()` sole SoT for Planetary stored Ferronite
- No duplicate replicated Ferronite counter
- Replicated MainBase handles are client-visible representation — authority Register/Unregister remains mutation SoT
- Team1 lookup does not return Team2 MainBase
- HUD binds only local PlayerState TeamId
- Unregister → safe `--`; replacement → rebind + live Storage updates
- Late/current-state initial sync on bind
- No permanent HUD Tick; no PC resource polling Tick; no `GetAllActorsOfClass` per frame
- No enemy resource value; no ThreatValue / OrbitalFerronite / FerroniteScore in P4 HUD

## Stop condition
**READY_FOR_MERGE.** Merge only after tech-lead approval. Do **not** start next Resource Playable Pass stage until explicitly assigned.
