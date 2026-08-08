# Cursor Work Report — GP-S31R Minimal Orbital Unit Drop

## Status
**GP-S31R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED. Stop for operator validation.

---

## 1. Branch / base SHA

- Branch: `feature/gp-s31r-minimal-orbital-unit-drop`
- Base: `main` @ `118660bb24bda51c7d5e5c1b97cbc1b9d5cb0d4c`
- Repo: `Owiiiii1/RTS`

## 2. Factual source inventory

- No `UGP_UnitDefinition` on main — catalog via `UGP_OrbitalDeliverySettings`
- No Spend GE → created `UGP_GE_SpendOrbital`
- No DropPod → `AGP_DropPod`
- Launch mirror: PC → MainBase → Storage / GAS
- CurrentUnits/MaxUnits attributes present, production tracking unwired (MaxUnits=0 soft-open)
- TEMP HUD `UGP_TEMP_S28P_PlanetaryFerroniteHUD` extended

## 3. Architecture chosen

Settings-driven slots/costs → manifest counts → `GPUnitDropAuthority` validate/spend/spawn one `AGP_DropPod` → landing → spawn Worker/SW via existing classes + `SetTeamId`. No subsystem. No building payload.

## 4. Spend GE path

`UGP_GE_SpendOrbital`: Instant Additive OrbitalFerronite, SetByCaller key `GP.Drop.OrbitalSpendMagnitude`, apply **negative** magnitude after funds check. Rejects never spend.

## 5. Unit catalog / data fields

`UGP_OrbitalDeliverySettings`: PodTransportSlotCapacity, Worker/SW TransportSlotCost, Worker/SW OrbitalDropCost, descent/altitude/spacing/cleanup. Server resolves `AGP_Worker` / `AGP_SalvageWalker` classes — client cannot pick class.

## 6. TEMP costs / slot tuning

Capacity 4; Worker 1 slot / 25 Orbital; SW 2 slots / 50 Orbital. Marked TEMP in settings + DefaultGame.ini. Not final balance.

## 7. MainBase Unit Drop Zone

`UnitDropZone` SceneComponent under PresentationRoot; default relative `(350,0,0)`; getter `GetUnitDropZone()`; missing/invalid → reject, no spend. Building pods will not use this anchor.

## 8. Manifest representation

`FGP_UnitDropManifest { WorkerCount, SalvageWalkerCount }` + `EGP_UnitDropRejectReason`.

## 9. MaxUnits / CurrentUnits

Soft-open when MaxUnits≤0. Cap reject when MaxUnits>0 and overflow. Increment CurrentUnits on spawn only when MaxUnits active. Death decrement not wired (production cap inactive).

## 10. DropPod lifecycle

AuthorityInit → tick lerp descent → OnImpact → spawn payload once → cleanup timer → Destroy. `bReplicates=true`.

## 11–12. Presentation / placeholder

BP events OnDescentStarted/OnImpact/OnPayloadDeployed. Native Engine `/Engine/BasicShapes/Cylinder` placeholder (no committed Content asset).

## 13. Spawn offsets

Deterministic ring around Drop Zone using local Forward/Right × spacing. No RNG.

## 14. Worker/SW init

`SpawnActor` + `SetTeamId` (same pattern as diagnostics/contracts).

## 15. TEMP UI

Unit Drop panel (top-right): steppers, Slots X/Cap, Cost, Confirm Drop. Keeps Base/containers/Orbital/Launch.

## 16. RPC

`Server_RequestUnitDrop(FGP_UnitDropManifest)` + `AuthorityTryRequestUnitDrop`. TeamId from owning PlayerState.

## 17. Contracts

Implemented `gp.Resource.RunOrbitalUnitDropContractTest` (A–N). Operator PIE: also rerun S28 / ContainerLaunch(+HUD) / DropOff / SalvageWalker / LOSFireGate — Failures=0 required.

## 18. GPEditor + UHT

**PASS**

## 19. GP Dev / Shipping

**NOT RUN** (await operator PASS)

## 20. Files changed (intended commit)

- `GP/Source/GPGASRuntime/.../GPGE_SpendOrbital.*`
- `GP/Source/GPRuntime/.../Orbital/*` (DropPod, Authority, Manifest, Contract)
- `GP/Source/GPRuntime/.../Settings/GPOrbitalDeliverySettings.*`
- MainBase, PlayerController, TEMP HUD
- `GP/Config/DefaultGame.ini` (Orbital Delivery section only)
- Docs: task, AI log, DOCUMENTATION_INDEX, Claude_Tasks README, this report

## 21. Operator assets untouched

Not committed: DefaultEngine.ini, L_PrototypeArena.umap, Blueprint/, Materials/, authored ResourceNode, Niagara, BP_SalvageWalker, Tools/, other local .uasset/.umap

## 22. Commit SHA

`dd1a62ae40b4db6b4ae40ff37f134c42a1c143d0`
