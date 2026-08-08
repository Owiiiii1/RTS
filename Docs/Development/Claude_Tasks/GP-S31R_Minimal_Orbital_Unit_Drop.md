# GP-S31R — Minimal Orbital Unit Drop

## Status
**GP-S31R_LANDING_AND_DEPLOYMENT_PRESENTATION_READY_FOR_OPERATOR_RETEST**

## Slice Group
Slice 8 — Buildings + Orbital Drops

## Branch
`feature/gp-s31r-minimal-orbital-unit-drop`  
Prior: authored payload seams `731a704…` (operator PASS for BP classes)

## Operator validation
Authored BP classes + gameplay **PASS**. Remaining: half-buried capsule spawn + deploy presentation window.

## Landing Z
`GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass` — CDO capsule half-height; spawn Z = ground + offset.

## Deployment phase
Descending → Impact/Deploying → `UnitDropPayloadDeployDelaySeconds` (TEMP 1.25s) → PayloadDeployed → Cleanup.  
Multicast presentation events for all clients. Authority payload exactly once.

## Builds
- GPEditor Win64 Development + UHT — **PASS**
- GP Dev/Shipping — **NOT RUN**

## Stop Condition
Operator retest. Do **not** merge / finalize.

## Authored integration fix
- Soft class seams on `UGP_OrbitalDeliverySettings` (Worker / SalvageWalker / UnitDropPod)
- Resolve with base-class enforcement + native fallback
- Capacity/costs/descent remain in settings (no DropPodDefinition DA yet)
- `bUseNativePlaceholder` on `AGP_DropPod`
- Cursor does **not** create owner BP/Niagara assets

## Owner workflow
Project Settings → GP Orbital Delivery → assign BP_Worker, BP_SalvageWalker, BP_DropPod_MVP. On DropPod BP: `bUseNativePlaceholder=false` + mesh/Niagara.

## Builds
- GPEditor Win64 Development + UHT — **PASS**
- GP Dev/Shipping — **NOT RUN**

## Stop Condition
Operator retest authored BP integration. Do **not** merge / finalize.

---

## Factual source inventory (pre-implementation)

| Area | Fact on main @ 118660bb |
|---|---|
| `UGP_UnitDefinition` | **Absent** — no parallel catalog created |
| Spend GE | **Absent** → added `UGP_GE_SpendOrbital` |
| DropPod | **Absent** → added `AGP_DropPod` |
| Unit Drop Zone | **Absent** → `AGP_MainBase::UnitDropZone` SceneComponent |
| Launch path | PC `Server_RequestLaunchReadyContainer` → MainBase → Storage → AddOrbital/AddScore |
| CurrentUnits / MaxUnits | Attributes exist; **no production mutators**; MaxUnits defaults 0 → **soft-open** |
| TEMP HUD | `UGP_TEMP_S28P_PlanetaryFerroniteHUD` (programmatic) |
| Settings pattern | `UGP_ResourceGameplaySettings` Config=Game |

---

## Architecture chosen

1. **Catalog / costs / slots:** `UGP_OrbitalDeliverySettings` (DeveloperSettings, Config=Game). No duplicate UnitDefinition DA.
2. **Manifest:** `FGP_UnitDropManifest` — WorkerCount + SalvageWalkerCount only. Server resolves classes.
3. **Authority:** `GPUnitDropAuthority::AuthorityRequestUnitDrop` (no subsystem).
4. **Spend:** Instant `UGP_GE_SpendOrbital` Additive SetByCaller **negative** magnitude. Validate funds first; reject → no spend.
5. **Drop zone:** Inherited `UnitDropZone` under PresentationRoot (default relative offset; BP-repositionable). Not hardcoded world coords.
6. **DropPod:** One shared `AGP_DropPod` — unit payload now; building-ready seam later. Engine BasicShapes cylinder placeholder (no Content .uasset).
7. **UI:** Extend TEMP HUD with Unit Drop steppers + Confirm → `Server_RequestUnitDrop`.
8. **MaxUnits:** Soft-open when MaxUnits≤0. If MaxUnits>0, reject whole manifest on overflow; increment CurrentUnits on spawn. Death decrement **not** wired (attributes unused in production).

---

## TEMP tuning (not final balance)

| Key | Value |
|---|---|
| PodTransportSlotCapacity | 4 |
| WorkerTransportSlotCost | 1 |
| SalvageWalkerTransportSlotCost | 2 |
| WorkerOrbitalDropCost | 25 |
| SalvageWalkerOrbitalDropCost | 50 |
| UnitDropDescentDurationSeconds | 2.5 |
| UnitDropSpawnAltitudeCm | 2500 |
| UnitDropSpawnSpacingCm | 180 |
| UnitDropCleanupDelaySeconds | 0.35 |

One Ready container launch (~100 Orbital at 1:1) can buy e.g. 4 Workers or 2 SW or mixed.

---

## Operator acceptance (no console)

1. Mine Ferronite → Launch Container → Orbital↑
2. TEMP HUD Unit Drop panel: Worker / Salvage Walker steppers, Slots X/Cap, Cost, Confirm Drop
3. Confirm → Orbital decreases once → visible pod descends ~2–3s onto Unit Drop Zone
4. Units spawn with offsets; correct TeamId; Worker mines; SW Move/Attack

## Visual handoff (after candidate)

Create BP child of `AGP_DropPod`, replace PlaceholderMesh, add Niagara exhaust/impact via BP events `OnDescentStarted` / `OnImpact` / `OnPayloadDeployed`. No C++ rewrite required.

## Out of scope
Building READY/ghost, FoW, BuildGrid, Logistics Hub, Turrets, Walls, production Order Menu, GP Dev/Shipping until operator PASS.

## Contracts
- `gp.Resource.RunOrbitalUnitDropContractTest` (new)
- Rerun: S28 suite, ContainerLaunch(+HUD), DropOff, SalvageWalker, LOSFireGate — expect Failures=0 in PIE

## Builds
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development / Shipping — **NOT RUN**

## Stop Condition
Operator validation. Do **not** merge.
