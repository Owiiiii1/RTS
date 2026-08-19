# Configuration & Data Ownership Audit

Status: **CONFIGURATION_DATA_OWNERSHIP_AUDIT_READY_FOR_REVIEW**

Branch: `docs/gp-configuration-data-ownership-audit`
Base: `origin/main` @ `d9e89605aae76446a5b41281df1c8f8773d67e4e`

This is a factual, read-only architecture audit. It does not authorize field removal, migration, config edits, asset edits, or runtime changes.

## 1. Executive Summary

The audit traced **48 exposed Project Settings fields** (29 orbital, 8 resource, 11 presentation including the two fields nested in `TeamStyles`) and **58 fields across six definition classes**, for **106 declared designer-facing settings/definition fields**, plus actor/CDO geometry and fallback chains.

The largest ownership problems are:

1. **Build footprint has five active representations.** Payload/live `PlacementFootprintBounds` normally wins; `BuildingDefinition.FootprintCells`, two non-identical class fallback tables, and replicated grid AABBs remain active.
2. **Orbital acquisition still has eight deprecated-but-active Project Settings bridges.** Unit costs, slots, payload classes, Logistics Hub cost, and Logistics Hub payload remain runtime fallbacks.
3. **Building payload class precedence contradicts the nominal DataAsset SoT.** A configured Defensive Turret settings class outranks `BuildingDefinition.SpawnedClass`; Hub settings remain the fallback when its spawned class is empty.
4. **Building vitals are not fully connected.** `BuildingDefinition.UnitDefinition` is the documented owner, but spawned buildings receive the BuildingDefinition only; its nested UnitDefinition is not automatically copied to `AGP_UnitBase::UnitDefinitionAsset`. BP/CDO `Default*` values can therefore remain effective.
5. **Actor-owned geometry/tuning remains a second database.** MainBase drop-off range, placement bounds, navigation obstacle, capsule dimensions, and drop-zone transforms are actor/BP/CDO authored and can disagree with DataAssets.
6. **One Project Settings field is proven dead:** `BuildingPlacementOverlapMarginCm` is editor-visible and config-backed but has no runtime or test reader.
7. **Some presentation metadata is unused:** `UGP_ResourceDefinition::Tint` and several Icon/Description fields have no current production reader.

Areas already comparatively clean:

- Resource search/retry/approach and container launch timing are global and consistently owned by `UGP_ResourceGameplaySettings`.
- Team colors and health-bar presentation are consistently owned by `UGP_GameplayPresentationSettings`.
- Unit intrinsic stats are read from `UGP_UnitDefinition` when loaded, with explicit actor defaults as fallback.
- Resource mining/conversion/deposit defaults are read from `UGP_ResourceDefinition`, with explicit node/component fallbacks and intentional authored instance overrides.
- Wall Package is correctly separate from Building READY and owns its own product cost, segment count, descent, and deploy delay.
- Authored building drop readiness now waits for its nested `BuildingDefinition`; cold load blocks purchase with `DefinitionNotReady` instead of changing value precedence.

## 2. Ownership Matrix

Status vocabulary: `CANONICAL`, `DUPLICATED`, `COMPATIBILITY`, `DEPRECATED_ACTIVE`, `DEAD`, `PRESENTATION_ONLY`, `AMBIGUOUS`.

| System | Parameter | Current Locations | Runtime Readers | Current Precedence | Effective SoT | Status | Recommended SoT | Migration Risk |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Unit | Vitals/combat/movement/cargo/retaliation | `UGP_UnitDefinition`; `AGP_UnitBase::Default*`; component defaults; native catalog | `GPUnitBase.cpp:491-646` | loaded UnitDefinition → actor/component fallback | UnitDefinition when loaded | DUPLICATED | `UGP_UnitDefinition` | MEDIUM: migrate BP defaults and retain bootstrap |
| Building | MaxHealth | `UnitDefinition.MaxHealth`; `BuildingDefinition.MaxHealth`; actor `DefaultMaxHealth`; native catalogs | `GPUnitBase.cpp:623-638`; `ResolveCanonicalMaxHealth()` is currently test-only | actor UnitDefinitionAsset → actor default; BuildingDefinition resolver is not in spawn initialization | BP/CDO or explicitly authored actor UnitDefinitionAsset | AMBIGUOUS | linked `UGP_UnitDefinition.MaxHealth` | HIGH: wire nested definition, migrate BPs, update combat contracts |
| Building | Storage capacity/count | `BuildingDefinition`; `UGP_StorageComponent` 100×5; MainBase fallback 100×5; native catalog | `GPMainBase.cpp:92-106` | loaded BuildingDefinition → MainBase/component fallback | BuildingDefinition when loaded | DUPLICATED | `UGP_BuildingDefinition` | MEDIUM |
| Building | Logistics Hub UnitCapBonus | `BuildingDefinition.UnitCapBonus`; class fallback 5; native catalog 5 | `GPLogisticsHub.cpp:85-137` | loaded BuildingDefinition → 5 | BuildingDefinition when loaded | DUPLICATED | `UGP_BuildingDefinition` | LOW |
| Building | Spawned payload class | `BuildingDefinition.SpawnedClass`; Hub/Turret settings classes; native class | `GPBuildingDropCatalog.cpp:956-1008` | Turret settings → BuildingDefinition → native; Hub BuildingDefinition → settings → native | depends on slot | AMBIGUOUS | `UGP_BuildingDefinition.SpawnedClass` | HIGH: authored BP migration |
| BuildGrid | Footprint size/offset | live/CDO `PlacementFootprintBounds`; `FootprintCells`; class fallbacks | `GPBuildGridSubsystem.cpp:447-532` | usable payload/live bounds → valid DA cells → class fallback | Placement bounds | DUPLICATED | one authored geometry source, with generated/validated mirror | HIGH |
| BuildGrid | Occupied cells | live oriented bounds; configured rectangular footprint; replicated AABB | `GPBuildingBase.cpp:451-500`; `GPDropPod.cpp:608` | pre-placed oriented cells; orbital configured rectangle | path-dependent | AMBIGUOUS | same bounds-derived occupied-cell algorithm | HIGH |
| Navigation | Building exclusion | `NavigationObstacle` BP/CDO box | Recast; `GPUnitCommandComponent.cpp:1556-1570` | independent of BuildGrid bounds | actor/BP component | CANONICAL | explicit nav geometry, validated against footprint | MEDIUM |
| MainBase | Drop-off range | `AGP_MainBase::DropOffRangeCm`; command cache defaults; diagnostics/test literals | `GPMainBase.cpp:70-77`; haul command path | MainBase actor value copied to command cache | MainBase actor/BP CDO | DUPLICATED | BuildingDefinition or one global system owner | MEDIUM |
| MainBase | Drop-off marker | `DropOffVisualAnchor` | no gameplay range reader | presentation only | actor/BP component | PRESENTATION_ONLY | authored presentation component | LOW |
| MainBase | Unit/Wall landing point | `UnitDropZone` | unit and Wall Package authorities | component world transform | actor/BP component | CANONICAL | authored MainBase zone component | LOW |
| Building placement | Deploy radius | orbital settings; literal 5000 fallback | `GPBuildingDropAuthority.cpp:169-177,364` | clamped settings → literal | orbital settings | CANONICAL | `UGP_OrbitalDeliverySettings` | LOW |
| Building placement | Overlap margin | orbital settings + INI | none | never read | none | DEAD | remove later or implement explicitly after decision | LOW |
| Unit acquisition | Cost | Unit drop DA; deprecated settings; native drop; hardcoded literal | catalog getters, authority, TEMP HUD | canonical drop → settings only if no drop → literal | unit drop DA/native catalog | DEPRECATED_ACTIVE | `UGP_OrbitalUnitDropDefinition.Cost` | MEDIUM |
| Unit acquisition | Transport slot cost | Unit drop DA; deprecated settings; native drop; literals | catalog getters, authority, TEMP HUD | valid DA/native value → settings only if no/invalid drop → literal | unit drop DA/native catalog | DEPRECATED_ACTIVE | `UGP_OrbitalUnitDropDefinition.TransportSlotCost` | MEDIUM |
| Unit acquisition | Payload class | Unit drop DA; deprecated settings class; native class | catalog resolver; DropPod | loaded DA class → sync-loaded settings class → native | path-dependent | DEPRECATED_ACTIVE | `UGP_OrbitalUnitDropDefinition.PayloadClass` | HIGH: migrate BP references |
| Unit transport | Pod capacity | orbital settings; literal 4 | authority, TEMP HUD | settings → literal | orbital settings | CANONICAL | `UGP_OrbitalDeliverySettings` | LOW |
| Unit transport | Descent/deploy delay | unit drop DA; settings defaults; native drop | catalog timing resolver | settings seed → per-product DA; max for mixed manifest | unit drop DA | DUPLICATED | `UGP_OrbitalUnitDropDefinition` | LOW after config cleanup |
| Unit transport | Altitude/spacing/cleanup | orbital settings; DropPod member defaults | authority initializes pod | settings passed at order time | orbital settings | CANONICAL | `UGP_OrbitalDeliverySettings` | LOW |
| Building acquisition | Cost | building drop DA; native drops; deprecated Hub setting | building catalog | authored/native canonical; native Hub cost mutated by settings bridge | drop DA, except native Hub bridge | DEPRECATED_ACTIVE | `UGP_OrbitalDropDefinition.Cost` | MEDIUM |
| Building transport | Descent/deploy delay | building drop DA; settings defaults; native drop | building catalog/authority | settings seed → canonical drop | building drop DA | DUPLICATED | `UGP_OrbitalDropDefinition` | LOW |
| Building transport | Altitude/cleanup | orbital settings; DropPod member defaults | building authority | settings passed at deploy | orbital settings | CANONICAL | `UGP_OrbitalDeliverySettings` | LOW |
| Wall Package | Cost/segments/timing | Wall Package DA; native package constants | package catalog/authority | authored ready → native; pending blocks | Wall Package definition | CANONICAL | `UGP_WallPackageDefinition` | LOW |
| Wall Package | Altitude/cleanup/pod class | building settings altitude/cleanup; shared pod setting | wall authority | global settings | orbital settings | CANONICAL | `UGP_OrbitalDeliverySettings` | LOW |
| Resource | Mining cycle/range | ResourceDefinition; diagnostic literals | mining component | resolved definition → diagnostic/component fallback | ResourceDefinition | DUPLICATED | `UGP_ResourceDefinition` | MEDIUM |
| Resource | Deposit max/miner slots | ResourceDefinition; ResourceNode CDO; authored instance | `GPResourceNode.cpp:200-227` | authored instance override → definition → node CDO | instance when intentionally overridden; otherwise definition | COMPATIBILITY | ResourceDefinition + explicit instance override policy | MEDIUM |
| Resource | Conversion/threat | ResourceDefinition; Storage literals 1/1/0.5 | storage launch and threat paths | resolved definition → literals | ResourceDefinition | DUPLICATED | `UGP_ResourceDefinition` | LOW |
| Resource | Search/retry/approach/depletion/launch | Resource settings; matching literals | Worker, command, node, storage | settings → defensive literal | resource settings | CANONICAL | `UGP_ResourceGameplaySettings` | LOW |
| Resource presentation | Tint/Icon/Description | ResourceDefinition | no current production reader for Tint/Icon/Description | unused | none | DEAD | presentation asset/UI when implemented | LOW |
| Team presentation | Team map/neutral/parameter | presentation settings; material fallback names | presentation/visual components | configured map/name → explicit fallback names | presentation settings | CANONICAL | `UGP_GameplayPresentationSettings` | LOW |
| Health-bar presentation | size/offset/colors | presentation settings; constructor literals | health-bar component | settings → literals | presentation settings | CANONICAL | `UGP_GameplayPresentationSettings` | LOW |
| Health state | Health/MaxHealth | GAS attributes; initial definitions/defaults | health bar/combat | runtime GAS after initialization | GAS runtime state | CANONICAL | GAS state, initialized from UnitDefinition | MEDIUM |
| UI | Orbital cost/slot display | TEMP HUD repeats catalog calculations | TEMP HUD and authorities | both call catalogs; capacity direct from settings | catalog/settings, duplicated consumer logic | COMPATIBILITY | ViewModel over authority/catalog APIs | MEDIUM |
| Catalogs | Native bootstrap definitions | transient native objects; authored DataAssets | all catalogs | authored ready → native; pending blocks; failed → native | authored when configured, native otherwise | COMPATIBILITY | explicit bootstrap only | MEDIUM |

## 3. Project Settings Audit

### 3.1 `UGP_OrbitalDeliverySettings`

Declaration: `GP/Source/GPRuntime/Public/Settings/GPOrbitalDeliverySettings.h:25-213`. Config section: `DefaultGame.ini` `[/Script/GPRuntime.GP_OrbitalDeliverySettings]`.

| Exposed setting | Current use / precedence | Status | Recommendation |
| --- | --- | --- | --- |
| `WorkerDropDefinition` | Async catalog binding; authored ready → native fallback; pending blocks | CANONICAL | Keep as catalog reference |
| `SalvageWalkerDropDefinition` | Same as Worker | CANONICAL | Keep |
| `LogisticsHubDropDefinition` | Async top-level + nested BuildingDefinition; authored ready → native | CANONICAL | Keep |
| `DefensiveTurretDropDefinition` | Same building slot chain | CANONICAL | Keep |
| `WallDropDefinition` | Building catalog compatibility slot; native if empty | COMPATIBILITY | Keep until old Wall READY path policy is decided |
| `WallTurretDropDefinition` | Building catalog slot; native if empty | CANONICAL | Keep |
| `WallPackageDefinition` | Separate package catalog; authored ready → native | CANONICAL | Keep |
| `PodTransportSlotCapacity` | Direct authority/HUD read; global pod capacity | CANONICAL | Keep |
| `WorkerTransportSlotCost` | Removed from C++ settings. Native bootstrap Cost/Slots live on unit-drop catalog products. Stale INI key only. | REMOVED | Config hygiene later |
| `SalvageWalkerTransportSlotCost` | Same | REMOVED | Config hygiene later |
| `WorkerOrbitalDropCost` | Same | REMOVED | Config hygiene later |
| `SalvageWalkerOrbitalDropCost` | Same | REMOVED | Config hygiene later |
| `WorkerPayloadClass` | Removed from C++ settings. Canonical payload is `UGP_OrbitalUnitDropDefinition.PayloadClass`. Native bootstrap is `AGP_Worker`. Stale INI key only. | REMOVED | Config hygiene later |
| `SalvageWalkerPayloadClass` | Same with native `AGP_SalvageWalker` | REMOVED | Config hygiene later |
| `UnitDropPodClass` | Shared pod presentation class for unit/building/package | CANONICAL | Keep global |
| `UnitDropDescentDurationSeconds` | Seed overwritten by any resolved unit drop definition | DUPLICATED | Hide or relabel “fallback default”; later remove if all products required |
| `UnitDropSpawnAltitudeCm` | Direct global transport read | CANONICAL | Keep |
| `UnitDropSpawnSpacingCm` | Direct global multi-unit spacing read | CANONICAL | Keep |
| `UnitDropPayloadDeployDelaySeconds` | Seed overwritten by resolved unit drop definition | DUPLICATED | Relabel fallback; later remove if safe |
| `UnitDropCleanupDelaySeconds` | Direct global cleanup read | CANONICAL | Keep |
| `BuildingOrbitalPurchaseCost` | Mutates native Logistics Hub drop cost | DEPRECATED_ACTIVE | Migrate Hub product/config, then remove sync bridge |
| `BuildingPayloadClass` | Hub fallback after BuildingDefinition.SpawnedClass | DEPRECATED_ACTIVE | Move Hub BP to BuildingDefinition |
| `DefensiveTurretPayloadClass` | **Outranks** BuildingDefinition.SpawnedClass when configured | AMBIGUOUS | Move to BuildingDefinition; deprecate only after asset migration |
| `BuildingDropDescentDurationSeconds` | Seed overwritten by canonical building drop | DUPLICATED | Relabel fallback; later remove if safe |
| `BuildingDropSpawnAltitudeCm` | Direct building and Wall Package global altitude | CANONICAL | Keep |
| `BuildingDropPayloadDeployDelaySeconds` | Building seed overwritten by drop; Wall Package ignores it | DUPLICATED | Relabel fallback |
| `BuildingDropCleanupDelaySeconds` | Direct building and Wall Package cleanup | CANONICAL | Keep |
| `BuildingMaxDeployRadiusFromMainBaseCm` | Server validation and preview | CANONICAL | Keep |
| `BuildingPlacementOverlapMarginCm` | No runtime/test reader | DEAD | Hide/remove later, or deliberately wire in a separate approved slice |

Important cold-load split:

- Definition soft references are loaded asynchronously by catalogs. `Pending` prevents purchase.
- Settings soft **class** references call `LoadSynchronous()` in `GPOrbitalDeliverySettings.cpp:29-49`, so first payload/pod resolution can hitch.
- Config values replace header defaults at CDO load. Current INI timing can disagree with header defaults even when later overwritten by product DAs.

### 3.2 `UGP_ResourceGameplaySettings`

Declaration: `GPResourceGameplaySettings.h:13-59`.

| Setting | Runtime use | Status | Recommendation |
| --- | --- | --- | --- |
| `ResourceSearchRadiusCm` | Worker alternative-node search | CANONICAL | Keep |
| `MaxResourcePathLengthCm` | Candidate path-length rejection | CANONICAL | Keep |
| `WaitingForResourceRetrySeconds` | Safety retry timer | CANONICAL | Keep |
| `DropOffRetrySeconds` | Drop-off safety retry | CANONICAL | Keep |
| `ContainerLaunchDurationSeconds` | Storage launch telegraph | CANONICAL | Keep |
| `DepletionDestroyDelaySeconds` | Resource node destruction delay | CANONICAL | Keep |
| `ResourceApproachSafetyMarginCm` | Mine/haul approach calculation | CANONICAL | Keep |
| `ResourceApproachDirectionCount` | Candidate approach sampling | CANONICAL | Keep |

All eight are active. No dead or deprecated setting was found in this class.

### 3.3 `UGP_GameplayPresentationSettings`

Declaration: `GPGameplayPresentationSettings.h:9-70`.

| Setting | Runtime use | Status | Recommendation |
| --- | --- | --- | --- |
| `TeamStyles` | `GetTeamColor` lookup | PRESENTATION_ONLY | Keep |
| `TeamStyles[].TeamId` | lookup key | PRESENTATION_ONLY | Keep |
| `TeamStyles[].TeamColor` | lookup value | PRESENTATION_ONLY | Keep |
| `NeutralTeamColor` | unassigned/unknown fallback | PRESENTATION_ONLY | Keep |
| `TeamColorParameterName` | preferred MID parameter; code has fallback names | PRESENTATION_ONLY | Keep |
| `HealthBarDrawSizeX` | widget component size | PRESENTATION_ONLY | Keep |
| `HealthBarDrawSizeY` | widget component size | PRESENTATION_ONLY | Keep |
| `HealthBarWorldOffset` | component attachment offset | PRESENTATION_ONLY | Keep |
| `HealthBarFillColor` | health widget style | PRESENTATION_ONLY | Keep |
| `HealthBarFrameColor` | health widget style | PRESENTATION_ONLY | Keep |
| `HealthBarBackgroundColor` | health widget style | PRESENTATION_ONLY | Keep |

All eleven exposed values are active presentation data. Constructor team colors and health-bar literals remain fallback copies but do not compete when config is present.

## 4. DataAsset Audit

### `UGP_UnitDefinition` — intrinsic unit/building-unit gameplay

Fields: identity plus MaxHealth, InitialHealth, Armor, DamageResistance, Damage, range, cooldown, sight, scan interval, facing rotation, move speed, retaliation pursuit, and cargo capacity (`GPUnitDefinition.h:22-72`).

- Desired and documented owner for intrinsic gameplay.
- Runtime `AGP_UnitBase` reads it when `UnitDefinitionAsset` resolves, then initializes GAS and component tuning.
- Every gameplay value has actor/component/native fallback copies. Those are compatibility/bootstrap values, not independent intended balance.
- Blueprint/CDO `UnitDefinitionAsset` can select a different definition from the orbital product’s `UnitDefinition`; DropPod only fills an empty actor reference.

### `UGP_BuildingDefinition` — identity, logistics, payload, footprint fallback

Fields: DisplayName, Icon, tags, SpawnedClass, UnitDefinition, compatibility MaxHealth, FootprintCells, storage capacity/count, UnitCapBonus.

- Storage and UnitCapBonus are actively consumed.
- SpawnedClass is active but settings can override it.
- FootprintCells is active only after usable payload/live placement bounds fail.
- MaxHealth is a compatibility field; `ResolveCanonicalMaxHealth()` prefers linked UnitDefinition but is not in the live building GAS initialization path.
- The linked UnitDefinition is not automatically propagated to the spawned actor’s `UnitDefinitionAsset`; BP defaults can silently remain effective.

### `UGP_ResourceDefinition` — resource identity and gameplay

Fields: identity, mining cycle/range, conversion rates, threat multiplier, deposit size/miners, presentation tint.

- Mining, conversion, threat, and deposit fields are active.
- Authored ResourceNode instance values intentionally override `DepositMaxAmount` / `MaxConcurrentMiners` when different from the native CDO baseline.
- `ResourceType=Ore` while display/tag identity is Ferronite remains an acknowledged naming compatibility issue.
- Tint, Icon, and Description have no current production reader; they are data schema without implemented presentation consumption.

### `UGP_OrbitalUnitDropDefinition` — unit product

Owns DisplayName/Icon, UnitDefinition link, payload class, Cost, TransportSlotCost, descent, and deploy delay.

- Cost/slots/timing are actively canonical through the unit drop catalog.
- PayloadClass is canonical only when loaded and valid; deprecated settings BP classes remain fallback.
- Mixed manifests use the maximum descent and deploy delay among included products.
- Catalog async loading covers the top-level product DA; nested UnitDefinition/payload class are already-loaded resolvers rather than a combined readiness contract.

### `UGP_OrbitalDropDefinition` — building product

Owns tags, Cost, BuildingDefinition link, descent, and deploy delay.

- Authored slot readiness now requires the nested BuildingDefinition to load.
- Cost and timing are product-owned.
- Building payload class and footprint live on/through BuildingDefinition and payload CDO, not this product.
- Native Hub Cost is still mutated by the deprecated settings bridge.

### `UGP_WallPackageDefinition` — Wall Package product

Owns DisplayName/Icon, Cost, SegmentCount, descent, deploy delay, tags.

- Separate catalog and authority; it never becomes Building READY.
- Altitude, cleanup, and shared DropPod class remain global settings.
- Native constants 150/5 are bootstrap only.

### Duplicated fields across definition layers

- DisplayName/Icon appear on several identity/product definitions. This is acceptable only if product display can intentionally differ from intrinsic identity; current readers are inconsistent.
- Payload class exists on unit product DA and building intrinsic DA, while settings contain fallback/override classes.
- MaxHealth exists on UnitDefinition and BuildingDefinition, plus actor defaults.
- Delivery descent/deploy exists on unit/building/package product DAs and settings fallback fields.

## 5. Actor / Blueprint / CDO Ownership Audit

### Build footprint

Actual footprint resolution:

```text
Payload/live PlacementFootprintBounds usable
→ BoxExtent × authored RelativeScale3D
→ actor/root scale ignored
→ footprint cells + local center offset
else BuildingDefinition exists
→ valid FootprintCells
→ invalid cells cause InvalidFootprint (no class fallback)
else
→ class fallback: MainBase 5×5, Hub 4×4, other 1×1
```

Relevant readers: `GPBuildGridSubsystem.cpp:447-532`.

`PlacementFootprintBounds` is Blueprint-child design data. For net-startup actors, the class CDO’s extent/location/rotation/relative scale is copied to the live component (`GPBuildingBase.cpp:178-206`) to prevent stale level snapshots. A BP child can therefore silently outrank its BuildingDefinition.

Pre-placed occupancy:

```text
live bounds → oriented occupied cells → RegisterCells
```

Orbital occupancy:

```text
payload CDO bounds → yaw-0 rectangular reservation
→ DropPod ConfigureGridPlacement
→ rectangular RegisterFootprint
```

The orbital configured rectangle bypasses the live oriented-bounds registration path. If the spawned BP bounds differ from the reserved CDO-derived rectangle, reservation and visible/live geometry can diverge.

### Navigation and exclusion geometry

`NavigationObstacle` is a separate Blueprint-authorable `UBoxComponent`:

- dynamic `NavArea_Null`
- independent extent from placement bounds
- **inherits actor/root scale**, unlike placement bounds
- also influences haul approach clearance

Capsule roots are a third geometry source and determine ground-spawn Z. Placement/environment checks use grid-sized axis-aligned boxes, not the nav box or capsule. These systems can disagree without validation.

Notable native mismatch:

- Defensive Turret bounds/catalog say 2×2.
- one class fallback table returns 1×1 for all non-MainBase/non-Hub classes.
- another fallback helper contains Turret 2×2 but is effectively bypassed when the first table returns valid 1×1.

### MainBase zones

- `UnitDropZone`: canonical landing transform for units and Wall Package.
- Building READY deploy: snapped build-grid footprint center, not UnitDropZone.
- `DropOffVisualAnchor`: presentation-only.
- `DropOffRangeCm`: gameplay Dist2D circle around actor origin; independent of visual anchor, nav box, placement footprint, and UnitDropZone.
- `BuildingMaxDeployRadiusFromMainBaseCm`: global radius measured from MainBase actor origin to snapped footprint center.

### DropPod presentation

- `UnitDropPodClass` is global settings data and shared by unit, building, and package orders.
- `AGP_DropPod::bUseNativePlaceholder` is BP/CDO presentation data.
- lifecycle timings are copied into pod instance members at authority initialization; pod member defaults are defensive fallback, not the normal order-time SoT.

### Other actor/component fallback databases

- `AGP_UnitBase::Default*` combat fields remain active whenever actor UnitDefinitionAsset is empty/unresolved.
- `UGP_CargoComponent::CargoCapacity=50` remains active without UnitDefinition application.
- `UGP_StorageComponent` and MainBase both retain 100×5 fallback values.
- `AGP_ResourceNode` instance MaxAmount/MaxConcurrentMiners can intentionally override ResourceDefinition.
- `DropOffRangeCm=400` is copied across MainBase, command cache, diagnostics, and tests.

## 6. Runtime Precedence Map

### Authored unit product

```text
Settings Worker/Walker DropDefinition configured
→ async top-level load
→ Pending blocks purchase
→ authored UGP_OrbitalUnitDropDefinition
else load failure/null
→ native bootstrap product

Cost / slots / timing
→ canonical product value
→ deprecated settings only if no usable product
→ hardcoded literal

Payload class
→ loaded product PayloadClass
→ sync-loaded deprecated settings BP class
→ native actor class
```

### Authored building product

```text
Settings building DropDefinition configured
→ async top-level load
→ validate non-null nested BuildingDefinition
→ async nested load
→ Pending blocks purchase/deploy
→ authored product Ready only when nested definition resolves
else null/failure
→ native slot bootstrap

Cost/timing
→ canonical product
→ native product
→ native Hub Cost alone is synchronized from deprecated settings bridge
```

### Building payload class

```text
Defensive Turret:
configured settings Turret BP class
→ BuildingDefinition.SpawnedClass
→ native AGP_DefensiveTurret

Logistics Hub:
BuildingDefinition.SpawnedClass
→ configured deprecated settings Hub BP class
→ native AGP_LogisticsHub
```

### Wall Package

```text
Settings WallPackageDefinition configured
→ async load; Pending blocks purchase
→ authored Cost / SegmentCount / descent / deploy
else
→ native package 150 / 5 / 2.5 / 2.0

Pod class / altitude / cleanup
→ global orbital settings
Landing
→ MainBase.UnitDropZone
```

### Unit/building intrinsic initialization

```text
Actor UnitDefinitionAsset loaded
→ UnitDefinition stats initialize GAS/components
else
→ actor Default* + component fallbacks

BuildingDefinitionAsset loaded
→ storage / UnitCapBonus and building-specific hooks
→ does NOT currently propagate BuildingDefinition.UnitDefinition
  into actor UnitDefinitionAsset
```

### Resource definition

```text
ResourceDefinition resolves
→ mining/range/conversion/threat
→ deposit defaults unless authored node instance differs from CDO baseline
else
→ component/node literals
```

### Build footprint

```text
usable payload/live PlacementFootprintBounds
→ authoritative size/offset
else BuildingDefinition present and FootprintCells valid
→ DA fallback
else BuildingDefinition present but invalid
→ InvalidFootprint
else
→ class fallback
```

### Presentation

```text
GameplayPresentationSettings config
→ team map / health-bar style
else
→ C++ constructor/component literals

Health-bar values
→ GAS Health/MaxHealth runtime state
```

## 7. Known Conflicts

1. BP `PlacementFootprintBounds=2×2` and DataAsset `FootprintCells=4×4`: BP/CDO bounds silently win.
2. Orbital building reservation is rectangular while pre-placed occupancy can be oriented cells.
3. Defensive Turret fallback can resolve to 1×1 despite native bounds/catalog being 2×2.
4. `NavigationObstacle`, placement bounds, capsule, environment overlap box, and visual mesh have independent dimensions and different scale rules.
5. Actor scale grows the nav obstacle but does not grow placement occupancy.
6. Replicated `GridOriginCell/GridFootprintSize` is an AABB; client overlap preview can report cells occupied outside a rotated oriented set.
7. BuildingDefinition linked UnitDefinition can disagree with a BP’s `UnitDefinitionAsset`; actor BP wins because the nested link is not propagated.
8. Defensive Turret settings payload class can disagree with `BuildingDefinition.SpawnedClass`; settings wins.
9. Native Logistics Hub product Cost can disagree with a content DA and Project Settings; authored DA wins, native is overwritten by settings.
10. Project Settings unit/building descent/deploy can disagree with product DAs; product DA wins for normal resolved orders.
11. Current INI 10s/5s timing values can appear authoritative in Project Settings while normal product definitions overwrite them.
12. Resource node instance MaxAmount/miner count can disagree with ResourceDefinition; instance wins when recognized as an authored override.
13. MainBase drop-off range is actor-owned while docs imply building data ownership.
14. `BuildingPlacementOverlapMarginCm` appears functional in Project Settings but is ignored.
15. `ResourceDefinition.Tint` appears authorable but is not consumed.
16. TEMP HUD duplicates authority-facing cost/slot calculations and lives in GPRuntime despite the target GPUIRuntime/MVVM architecture.
17. Old per-segment Wall drop remains a building catalog compatibility slot while player acquisition uses the separate Wall Package product.

Cold-load effects:

- Authored unit/building/package product pending states block purchase rather than selecting a different price.
- Building products additionally wait for nested BuildingDefinition.
- Unit product nested UnitDefinition and payload class are already-loaded-only resolvers; class fallback may be selected if the class is not loaded.
- Settings soft class bridges use synchronous load, so cold state changes hitch risk, not declared precedence.

## 8. Proposed Target Ownership

| Category | One future canonical owner |
| --- | --- |
| Unit vitals/combat/movement/cargo/retaliation | `UGP_UnitDefinition` |
| Building storage, UnitCapBonus, identity/tags | `UGP_BuildingDefinition` |
| Building vitals | linked `UGP_UnitDefinition`; spawn pipeline must apply it |
| Building payload class | `UGP_BuildingDefinition.SpawnedClass` |
| Build-grid size/offset/occupied cells | one authored `PlacementFootprintBounds` geometry source; `FootprintCells` generated/validated mirror or removed after migration |
| Navigation exclusion | explicit `NavigationObstacle`, with validator against gameplay footprint |
| Resource gameplay | `UGP_ResourceDefinition` |
| Resource process/global search/retry/approach | `UGP_ResourceGameplaySettings` |
| Unit product cost/slots/payload/timing | `UGP_OrbitalUnitDropDefinition` |
| Building product cost/timing/link | `UGP_OrbitalDropDefinition` |
| Wall package cost/segments/timing | `UGP_WallPackageDefinition` |
| Shared DropPod class, altitude, spacing, cleanup, deploy radius | `UGP_OrbitalDeliverySettings` |
| MainBase drop-off range | `UGP_BuildingDefinition` or a dedicated global resource-system setting; choose one, not actor + caches |
| Team/health presentation | `UGP_GameplayPresentationSettings` |
| Product/intrinsic icons and display names | explicit UI ViewModel rule: product metadata for purchase UI, intrinsic metadata for selected world entity |
| Native values | explicit bootstrap/failure defaults only, not independently exposed balance |

Recommended footprint model:

1. Author a single placement bounds component on the payload class.
2. Derive preview, reservation, environment overlap, and final occupied cells from the same bounds transform.
3. Generate or validate any DataAsset cell mirror in editor; do not let designers edit both independently.
4. Keep navigation geometry separate only because its semantics differ, but add mismatch validation.
5. Replace duplicate fallback tables with one resolver.
6. Decide whether orbital buildings are always yaw-0; enforce that policy explicitly.

## 9. Cleanup Plan

| Slice | Fields/paths affected | Migration requirement | Tests required | Operator action | Risk |
| --- | --- | --- | --- | --- | --- |
| A. Settings visibility truth | Hide/relabel eight deprecated bridges and timing seeds; mark overlap margin unused | none yet; metadata/docs only | settings reflection/editor contract | verify Project Settings labels | LOW |
| B. Dead overlap setting decision | `BuildingPlacementOverlapMarginCm` | config key removal or deliberate implementation | BuildGrid placement/overlap contracts | validate edge placements | MEDIUM |
| C. Unit product numeric bridge removal | worker/walker settings cost/slots; catalog fallbacks | migrate INI and ensure all environments have valid product DAs/native policy | orbital unit + economy + HUD contracts | verify purchase values cold | MEDIUM |
| D. Unit payload class consolidation | worker/walker settings BP classes → drop DA PayloadClass | authored DA migration | orbital unit, spawn class, economy contracts | assign/validate payload BPs | HIGH |
| E. Building payload class consolidation | Hub/Turret settings classes → BuildingDefinition.SpawnedClass | authored BuildingDefinition migration | building, turret, grid, economy contracts | assign both authored BPs | HIGH |
| F. Native Hub cost bridge removal | `BuildingOrbitalPurchaseCost`, `SyncLegacyLogisticsHubCompatibility` | migrate config to Hub drop DA | building/economy contracts | verify authored Hub cost | MEDIUM |
| G. Delivery timing cleanup | settings descent/deploy seeds vs product fields | config migration; establish required product policy | unit/building/package timing contracts | verify telegraph timings | MEDIUM |
| H. Building vitals wiring | BuildingDefinition.UnitDefinition → actor UnitDefinitionAsset | migrate BPs with conflicting actor refs | unit definition, turret, Hub, combat contracts | inspect authored building definitions/BPs | HIGH |
| I. Footprint single-owner migration | bounds, FootprintCells, fallback tables, reservation | audit every building BP/DA pair; generate validation report | full BuildGrid + building/turret deploy regressions | inspect and approve each footprint | HIGH |
| J. Navigation/footprint validation | NavigationObstacle and placement bounds | authored BP geometry review | movement/nav/haul/grid contracts | rebuild/validate nav where required | HIGH |
| K. MainBase drop-off ownership | `DropOffRangeCm`, command cache/literals | BuildingDefinition or settings migration | mining/haul/drop-off/storage contracts | validate worker approach | MEDIUM |
| L. Resource fallback consolidation | node/component defaults and Tint metadata | resource asset and placed-node override inventory | mining/depletion/storage contracts | inspect placed node overrides | MEDIUM |
| M. TEMP HUD ownership | GPRuntime TEMP HUD → GPUIRuntime ViewModels | UI asset/code migration | HUD + authority display contracts | UI validation | HIGH |
| N. Legacy Wall slot policy | `WallDropDefinition` old READY path vs Wall Package | product/content migration decision | Wall Package + multi-building contracts | confirm no old acquisition use | HIGH |

Do not combine H, I, J, M, or N into one implementation slice.

**Slice A implementation status:** `SETTINGS_VISIBILITY_TRUTH_FINALIZED_READY_FOR_MERGE` on `feature/gp-settings-visibility-truth`. Editor exposure/labels only. Findings and remaining cleanup slices are unchanged. **NOT MERGED.**

**Slice B implementation status:** `DEAD_OVERLAP_SETTING_REMOVAL_FINALIZED_READY_FOR_MERGE` merged to `main` @ `967e6ea3a5b81ddc1a2c19c4bfe292f5ef989507`. Dead C++ property `BuildingPlacementOverlapMarginCm` removed. Runtime placement unchanged. Stale `DefaultGame.ini` key intentionally not touched because protected config exists.

**Slice C implementation status (this combined numeric package):** `UNIT_NUMERIC_COMPAT_CLEANUP_FINALIZED_READY_FOR_MERGE` merged to `main` @ `47a220b480e455f1cf5dfb6ca0613c13cf760a53`. Removed unit numeric Project Settings bridges. Native Worker 25/1 and Salvage Walker 50/2 owned by catalog bootstrap construction. Stale `DefaultGame.ini` keys intentionally not touched.

**Slice D implementation status (this combined payload package):** `UNIT_PAYLOAD_COMPAT_CLEANUP_FINALIZED_READY_FOR_MERGE` merged to `main` @ `75b13fc193531170eb3d4c1eaf9ee3f736d1d160`. Removed `WorkerPayloadClass` and `SalvageWalkerPayloadClass` plus settings resolvers. Canonical authored payload is `UGP_OrbitalUnitDropDefinition.PayloadClass`. Native fallback is `AGP_Worker` / `AGP_SalvageWalker` on catalog bootstrap. Stale `DefaultGame.ini` keys intentionally not touched.

**Slice G implementation status (this combined delivery timing package):** `DELIVERY_TIMING_OWNERSHIP_CLEANUP_FINALIZED_READY_FOR_MERGE` on `feature/gp-delivery-timing-ownership-cleanup`. Removed four Project Settings timing fallback fields. Canonical authored unit timing is `UGP_OrbitalUnitDropDefinition`; native Worker/Walker bootstrap is 2.5 / 1.25. Canonical authored building timing is `UGP_OrbitalDropDefinition`; native building bootstrap is 2.5 / 2.0. Mixed unit manifests keep max aggregation. Pending remains `DefinitionNotReady`. Wall Package timing independently owned and unchanged. Stale `DefaultGame.ini` keys intentionally not touched. Payload/numeric/altitude/cleanup/building procurement unchanged. **NOT MERGED.**

**Combined building procurement + payload ownership safety-gate status:** `BUILDING_PROCUREMENT_PAYLOAD_OWNERSHIP_BLOCKED_BY_AUTHORED_ASSET_MIGRATION` on `feature/gp-building-procurement-payload-ownership`. Unreal inspection found configured Hub drop `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_LogisticsHUB` points to `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB`, whose `SpawnedClass` is empty. It still depends on `BuildingPayloadClass` resolving `BP_GP_LogisticsHUB_C`. Configured Defensive Turret `SpawnedClass` is valid. Required operator migration: assign `BP_GP_LogisticsHUB_C` to the Hub BuildingDefinition `SpawnedClass`, save, then rerun the cleanup. No runtime bridges removed. **NOT MERGED.**

## 10. Do Not Delete Yet

The following fields look obsolete or duplicated but still have proven readers:

- Stale `DefaultGame.ini` keys `WorkerTransportSlotCost`, `SalvageWalkerTransportSlotCost`, `WorkerOrbitalDropCost`, `SalvageWalkerOrbitalDropCost`, `WorkerPayloadClass`, `SalvageWalkerPayloadClass`, `UnitDropDescentDurationSeconds`, `UnitDropPayloadDeployDelaySeconds`, `BuildingDropDescentDurationSeconds`, `BuildingDropPayloadDeployDelaySeconds` — leftover text after C++ removal; no production GConfig/string reader. Config hygiene later.
- `BuildingOrbitalPurchaseCost` — native Logistics Hub cost synchronization.
- `BuildingPayloadClass` — Logistics Hub payload fallback.
- `DefensiveTurretPayloadClass` — currently highest-priority Turret payload override.
- `UnitDropPodClass` — all three orbital product families use it.
- `BuildingDropSpawnAltitudeCm`, `BuildingDropCleanupDelaySeconds` — building and Wall Package.
- `BuildingDefinition.MaxHealth` — compatibility resolver/tests, even though live vitals wiring is incomplete.
- `BuildingDefinition.FootprintCells` — active fallback and invalid-footprint contract.
- actor `Default*` combat fields — live fallback whenever UnitDefinitionAsset is empty.
- component Cargo/Storage defaults — live fallback during missing/failed definition paths.
- `AGP_MainBase::DropOffRangeCm` and command cache — active haul gate.
- native unit/building/drop/package catalog entries — cold-start/failure bootstrap.
- `WallDropDefinition` — old catalog compatibility still exists even though Wall Package is the player acquisition path.
- `GridOriginCell/GridFootprintSize` — replicated preview/debug and pod overlap readers.
- `NavigationObstacle`, `PlacementFootprintBounds`, capsules, `UnitDropZone` — separate active semantics.
- ResourceNode `MaxAmount` / `MaxConcurrentMiners` instance fields — intentional authored overrides.
- TEMP HUD and its catalog reads — still active production UI.

The following may be removed later only after confirming asset/UI intent:

- `BuildingPlacementOverlapMarginCm` — proven no reader, but config migration/editor cleanup is still required.
- `UGP_ResourceDefinition::Tint` — no production reader; may be reserved for future presentation.
- currently unread Icon/Description fields — retain until UI ownership is decided.

### Documentation discrepancies recorded, not corrected here

- `TDD/10` says balance is not hardcoded, but native bootstrap and actor/component fallbacks duplicate values.
- `TDD/10` says orbital settings contain global tuning + soft references only, but deprecated active balance/payload bridges remain.
- GP-S39E/TDD building-vitals ownership omits the missing nested UnitDefinition-to-actor propagation.
- `TDD/06` contains a stale claim that occupancy is inferred from `FootprintCells + actor location`; live bounds and subsystem occupancy now win.
- MainBase drop-off is described as building data in places but is actor-owned.
- `TDD/12` target UI ownership is GPUIRuntime/MVVM; current TEMP HUD is in GPRuntime and directly binds gameplay objects.
- current INI timing values imply authority that product DAs override.
- GDD Logistics Hub container-cap contribution remains future/TBD; runtime implements UnitCapBonus only.
