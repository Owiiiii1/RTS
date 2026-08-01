# Orbital Delivery System

## Scope

Engineering implementation of orbital drop pod mechanic (per [`../GDD/10_Orbital_Delivery`](../GDD/10_Orbital_Delivery.md)). Defines order flow, validation, drop pod actor, replication, multicast cosmetic budget, subsystem ownership. Замінює pre-pivot `UGP_ProductionComponent` + `UGP_ConstructionComponent` + `AGP_GhostBuilding` flows.

## Hard Rules

1. **All non-initial assets arrive from orbit.** No local production / construction.
2. **Server-authoritative.** Client builds order intent; server validates spend, validates drop zone, schedules pod.
3. **Drop zone requires Actively Visible FoW.** Per [`15_Fog_of_War`](15_Fog_of_War.md) — blind drops у Explored / Unexplored — banned.
4. **Telegraph window 2-3 s.** Mandatory — pod descent visible to all players + SWARM AI.
5. **No client prediction.** Pod state authoritative on server; client renders cosmetic via multicast + RepNotify.
6. **DataAsset-driven.** Drop types, costs, footprints, descent times — all у `UGP_OrbitalDropDefinition` DataAsset family.

## Architecture

```
Client UI                          Server                                Cosmetic
─────────                          ──────                                ────────
WBP_GP_OrderMenu  ─┐
  (BuildMenu       │  Server_RequestOrbitalDrop(DropDef, FVector Loc, FRotator Rot)
  reworked)        ▼      │
                ─────────►├─► UGP_OrbitalDeliverySubsystem::TryEnqueueOrder
                          │     ├─ Validate Owner.OrbitalFerronite >= DropDef.Cost
                          │     ├─ Validate Drop zone (FoW + NavMesh + bounds + footprint)
                          │     ├─ Apply GE_GP_SpendOrbital(Cost)
                          │     ├─ Spawn AGP_DropPod at altitude over Loc
                          │     ├─ AGP_DropPod.Init(DropDef, OwnerTeamId, LandingLoc, Rotation)
                          │     └─ Server replicates pod actor
                          │
                          │  AGP_DropPod (server tick):
                          │     ├─ DescentTimer counts up
                          │     ├─ At StartTime + 0.1s — Multicast_PlayDescentVFX
                          │     ├─ Movement: linear interp from altitude to landing
                          │     ├─ At DescentDuration elapsed:
                          │     │    ├─ Spawn payload actor (DropDef.PayloadClass — soft-loaded)
                          │     │    ├─ Apply GE_GP_Init_<UnitType> to payload
                          │     │    ├─ Set payload TeamId, OwningPlayerState
                          │     │    ├─ Multicast_PlayLandingFX
                          │     │    └─ Schedule self-destroy +0.5s
                          │     └─ Pod destroyed
                          │
                          │  UI binds via UGP_OrderMenuVM → reflects new pool, new pod, etc.
                          │
                          └──► Cosmetic: pod descent VFX, sound, dust impact, payload reveal
```

## UGP_OrbitalDeliverySubsystem

```cpp
UCLASS()
class GPRUNTIME_API UGP_OrbitalDeliverySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Called by PC server-side через Server_RequestOrbitalDrop. */
    bool TryEnqueueOrder(
        AGP_PlayerController* Requester,
        const TSoftObjectPtr<UGP_OrbitalDropDefinition>& DropDef,
        FVector LandingLocation,
        FRotator LandingRotation,
        EGP_OrderRejectReason& OutReason);

    /** Per-server-tick maintenance. */
    void Tick(float DeltaSeconds);

    /** Active pods (for HUD count / global cap). */
    int32 GetActivePodCount(int32 TeamId) const;

protected:
    UPROPERTY()
    TArray<TWeakObjectPtr<AGP_DropPod>> ActivePods;

    UPROPERTY(EditDefaultsOnly)
    int32 MaxActivePodsPerTeam = 3;        // DA-driven у MVP

    bool ValidateDropZone(
        AGP_PlayerController* Owner,
        const UGP_OrbitalDropDefinition* DropDef,
        FVector Loc,
        EGP_OrderRejectReason& OutReason) const;
};
```

Single subsystem (`UWorldSubsystem` scoped to match world, server only). One MVP-justified subsystem per [`ADR-0006`](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md). Joins `UGP_SessionSubsystem` and `UGP_MatchAssetLoader` як the 3 sanctioned subsystems.

## AGP_DropPod

```cpp
UCLASS()
class GPRUNTIME_API AGP_DropPod : public AActor
{
    GENERATED_BODY()
public:
    AGP_DropPod();

    /** Server-only. Triggered by Subsystem. */
    void Init(TSoftObjectPtr<UGP_OrbitalDropDefinition> DropDef,
              int32 TeamId,
              FVector LandingLoc,
              FRotator Rotation);

protected:
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(Replicated)
    FVector LandingLocation;

    UPROPERTY(Replicated)
    int32 OwnerTeamId = 0;

    UPROPERTY(Replicated)
    float DescentProgress01 = 0.f;          // 0 → 1 over DescentDuration

    UPROPERTY()
    TSoftObjectPtr<UGP_OrbitalDropDefinition> DropDefRef;

    UPROPERTY(Transient)
    TObjectPtr<UGP_OrbitalDropDefinition> CachedDropDef;     // resolved after async load

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayDescentVFX();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayLandingFX();

    void OnAsyncLoadComplete();
    void OnLandingComplete();              // server-only: spawn payload + destroy self
};
```

- `bReplicates = true`, replicates while in flight.
- `bNetUseOwnerRelevancy = false` — visible to all clients (включаючи opponent) — це telegraph.
- Replication frequency moderate (10 Hz suffices for descent visualization).

## UGP_OrbitalDropDefinition (DataAsset)

```cpp
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_OrbitalDropDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    FGameplayTagContainer DropTags;        // GP.Drop.Type.{Unit, Building, Module}

    UPROPERTY(EditAnywhere, Category = "GP|Cost")
    int32 Cost = 0;                        // OrbitalFerronite spend

    UPROPERTY(EditAnywhere, Category = "GP|Descent")
    float DescentDuration = 2.5f;          // seconds

    UPROPERTY(EditAnywhere, Category = "GP|Descent")
    float SpawnAltitude = 8000.f;          // cm above landing

    UPROPERTY(EditAnywhere, Category = "GP|Payload")
    TSoftClassPtr<AActor> PayloadClass;    // spawned on landing; UGP_UnitBase / AGP_BuildingBase subclass

    UPROPERTY(EditAnywhere, Category = "GP|Payload")
    TSoftObjectPtr<UGP_UnitDefinition> UnitDefIfUnit;       // for unit drops
    UPROPERTY(EditAnywhere, Category = "GP|Payload")
    TSoftObjectPtr<UGP_BuildingDefinition> BuildingDefIfBuilding;   // for building drops

    UPROPERTY(EditAnywhere, Category = "GP|Validation")
    float FootprintRadius = 200.f;         // sphere overlap check at landing

    UPROPERTY(EditAnywhere, Category = "GP|Validation")
    bool bRequiresActiveVisibility = true; // enforced by drop-zone validator

    UPROPERTY(EditAnywhere, Category = "GP|Validation")
    float MinDistanceFromMainBase = 0.f;   // 0 = no constraint

    UPROPERTY(EditAnywhere, Category = "GP|Feedback")
    TSoftObjectPtr<UGP_FeedbackBundle> FeedbackBundle;     // VFX/SFX refs
};
```

All asset refs — soft (per ADR-0002). Resolved by `UGP_MatchAssetLoader::PreloadForMatch` during loading state.

## Drop Zone Validation (Server)

Per 2026-05-16 grid pivot — drop targeting is **grid-aligned**. Validation chain використовує `UGP_BuildGridSubsystem`.

`UGP_OrbitalDeliverySubsystem::ValidateDropZone`:

1. **Snap to grid:** `OriginCell = BuildGrid.WorldToCell(LandingLoc)`. Adjust footprint center.
2. **Spend gate:** `Owner.OrbitalFerronite >= DropDef.Cost`.
3. **Pod cap:** `GetActivePodCount(TeamId) < MaxActivePodsPerTeam`.
4. **FoW visibility:** `UGP_FogOfWarComponent::IsVisibleToTeam(TeamId, Loc)` — if `DropDef.bRequiresActiveVisibility`. Else allow Explored.
5. **Grid placement:** `BuildGrid.CanPlaceFootprint(OriginCell, DropDef.FootprintCells, DropDef.ClearanceCells, nullptr, OutReason)`.
   - Returns reject reasons: `CellOccupied`, `ClearanceViolation`, `OutOfBounds`, `NotNavigable`.
6. **Wall-mount path** (if `DropDef.bMountsOnWall`):
   - `BuildGrid.GetActorAtCell(OriginCell)` must be `AGP_Wall` (own team).
   - Wall must not already host wall-mount (one-mount-per-wall rule).
   - Reject `EReason::NotOnWall` if fails.
7. **Wall drag-build path** (if `DropDef.Type == GP.Drop.Type.Wall` AND drag mode):
   - Server runs `BuildGrid.PathfindFreeCells(Start, End, 2, 2, OutPath)`.
   - Cost = `OutPath.Num() × WallSegmentCost`.
   - Validate `Owner.OrbitalFerronite >= Cost`.
   - Reject if no path або insufficient funds.
8. **Within map bounds:** standard world-bounds check OR explicit drop-allowed volume.
9. **Distance to MainBase:** if `MinDistanceFromMainBase > 0`, enforce (grid cells).

Reject reasons enumerated:

```cpp
UENUM()
enum class EGP_OrderRejectReason : uint8
{
    Unknown = 0,
    InsufficientOrbital,
    PodCapReached,
    NotVisible,           // FoW
    Unreachable,          // NavMesh
    CellOccupied,         // grid: cell already used
    ClearanceViolation,   // grid: too close to other structure
    OutOfBounds,
    TooCloseToBase,
    NotOnWall,            // wall-mount only
    WallSlotOccupied,     // wall already hosts mount
    NoPathPossible,       // wall drag-build A* failed
    ValidationFailed
};
```

On reject → `Client_NotifyCommandRejected(GP.Command.OrderDrop, Reason)`. Client UI shows specific toast (per TDD/12 Notification system).

## Order Flow (Client-Side)

1. Hotkey `O` opens `WBP_GP_OrderMenu` (Common UI Activatable).
2. VM `UGP_OrderMenuVM`:
   - Reads `Player.AllowedDrops` (from `UGP_FactionDefinition.AllowedOrbitalDrops` — soft list).
   - Reads `Player.OrbitalFerronite`, `MaxUnits`, `CurrentUnits`.
   - Provides per-drop `CanAfford` boolean.
3. Player clicks drop type → menu closes → drop-targeting mode active.
4. Cursor reticle (local-only `AGP_DropReticle` actor або decal):
   - Cursor raycast to ground plane.
   - Per-frame: query FoW visibility + NavMesh + overlap → tint green/red.
   - Render footprint circle (`FootprintRadius`).
5. LMB on valid: `Server_RequestOrbitalDrop(DropDef, Loc, Rot)`.
6. RMB / Esc: cancel mode, no spend.

`AGP_DropReticle` — local-only, `bReplicates=false`. Material parameter `TintColor`.

## ViewModel — UGP_OrderMenuVM

```cpp
UCLASS()
class GPUIRUNTIME_API UGP_OrderMenuVM : public UMVVMViewModelBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    TArray<FGP_OrderMenuEntry> Entries;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    int32 OrbitalFerronite;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    int32 CurrentUnits;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    int32 MaxUnits;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    int32 ActivePodCount;
};

USTRUCT()
struct FGP_OrderMenuEntry
{
    GENERATED_BODY()
    UPROPERTY()  FText  Name;
    UPROPERTY()  TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY()  int32  Cost;
    UPROPERTY()  bool   bCanAfford;
    UPROPERTY()  bool   bWouldExceedUnitCap;
    UPROPERTY()  TSoftObjectPtr<UGP_OrbitalDropDefinition> DropDef;
};
```

Adapter `UGP_OrderMenuVMAdapter` subscribes to:
- `UGP_PlayerAttributeSet.OrbitalFerronite` change.
- `UGP_PlayerAttributeSet.MaxUnits` change.
- `UGP_PlayerAttributeSet.CurrentUnits` change.
- `UGP_OrbitalDeliverySubsystem.OnActivePodCountChanged`.
- Faction static (one-time setup on possession).

## Replication

| Field | Condition |
| --- | --- |
| `AGP_DropPod.LandingLocation, OwnerTeamId, DescentProgress01` | `COND_None` (visible to all clients — це telegraph) |
| `UGP_OrbitalDeliverySubsystem` state | Server-only (no replicate). UI reads через VM-mirrored RPCs / delegates. |
| Player `OrbitalFerronite, MaxUnits, CurrentUnits` | `COND_OwnerOnly` (per TDD/07 update) |

Multicast budget: pod has 2 multicasts per lifetime (DescentVFX + LandingFX). Negligible.

## RPC List (New)

| RPC | Caller | Receiver | Validation |
| --- | --- | --- | --- |
| `Server_RequestOrbitalDrop(TSoftObjectPtr<UGP_OrbitalDropDefinition>, FVector, FRotator)` | Client PC | Server PC | Owner check, drop validation chain |
| `Client_NotifyCommandRejected(FGameplayTag CmdTag, uint8 Reason)` | Server | Owner client | (existing — reused for OrderDrop) |
| `Multicast_PlayDescentVFX` | Server (pod) | All clients | — |
| `Multicast_PlayLandingFX` | Server (pod) | All clients | — |

## Tag Surface

```
GP.Command.OrderDrop                  // command intent
GP.State.PodInFlight                  // loose tag on pod actor
GP.Drop.Type.Unit                     // DropDef classification
GP.Drop.Type.Building
GP.Drop.Type.Module                   // reserved post-MVP
GP.Capability.GrantsVision            // sight-source marker (per TDD/15)
GP.Capability.DroppableTarget         // reserve for post-MVP combat anti-drop
```

## Feel / MVP Fun Maximization (Validator Pass)

Per 5-component rubric (Clarity / Motivation / Response / Satisfaction / Fit), drop-pod fantasy must **feel earned and visceral у v1** (Helldivers reference). Targeted requirements (DA-driven, placeholder values, TBD у balance pass):

### Pod Descent — Layered Telegraph (Clarity + Satisfaction)

- **Scale ramp:** pod silhouette grows from spec у sky to full-size at landing. Visible from ~3 s out.
- **Audio Doppler:** descent SFX pitch-down on approach (final 0.5 s — bass-heavy "incoming" rumble).
- **Light beam:** vertical beam-of-light decal at landing cell, visible to ALL players + SWARM (telegraph cost — opponent can react).
- **Smoke trail:** Niagara trail від pod entering atmosphere.
- **Impact:** camera shake (radius-scaled, per `DropDef.ImpactShakeIntensity`), dust burst, ground decal (scorch ring).

### Per-Payload-Type Differentiation (Fit + Clarity)

- Worker pod: small (1×1 cell scale), light blue light, mining-themed shape.
- Combat unit pod: medium (2×2 scale), red-orange light, military-industrial shape.
- Building pod: large (matches building footprint), white-yellow light, structural shape.
- Player IMMEDIATELY reads "what's coming" from pod silhouette + light color.

### Order Menu Feedback (Motivation + Response)

- On affordable order: glow on icon, soft cue.
- On unaffordable: dim icon, soft denial chime.
- On click → enter-targeting transition: HUD overlay dim, reticle appears with grid-snap animation.
- On valid drop: 200 ms green pulse on reticle before pod spawn (acknowledges player intent immediately).
- On reject: 400 ms red pulse + denial sound + tooltip explaining reason ("Need Visible terrain", "Pod queue full", etc.).

### Pod Queue Visibility (Clarity)

HUD bottom-right: small icons showing active in-flight pods (e.g., "3/3 pods in flight"). Player sees that 4th order will queue OR reject.

### Reward Flash on Landing (Satisfaction)

- Asset deploys → 0.5 s emissive flash on payload (material parameter).
- "Ready for orders" SFX (subtle, не over-loud).
- Selection auto-highlight new asset for 1 s (signals "your new thing is here").

### Pillar 8 Re-Check

- 1-2 sentence: "Drag from order menu, click on map, pod drops with VFX cascade, asset lands."
- Fun у v1: confirmed via Helldivers reference + layered telegraph + per-type differentiation.
- New decision: when / where / what to drop, while watching telegraph cost.
- Cheap: 1 pod actor + per-payload scale param + DA-driven cosmetic bundles.
- Scales: more DropDef DataAssets add content без code change.

## Anti-Patterns

- ❌ Client direct write to `OrbitalFerronite`.
- ❌ Bypass `UGP_OrbitalDeliverySubsystem` — spawning building/unit directly у gameplay code outside match init or pod landing flow.
- ❌ `LoadObject` for drop payload class (must be soft + Asset Manager).
- ❌ `AGP_GhostBuilding` (deleted у pivot — orbital reticle replaces).
- ❌ Production / construction component creation у new code (these are removed).
- ❌ Pod cosmetic events as gameplay-affecting (multicast Unreliable; cannot be trusted for state).

## Validation per Pillars

**Pillar 8 (Simple Core):**
- 1-2 sentence: "Open Order Menu, click drop type, click on map, pod arrives."
- Fun у v1: yes — drop is intrinsically satisfying.
- New decision: timing + location + opportunity cost.
- Cheap: ONE pod actor class, ONE drop reticle actor, ONE subsystem, ONE menu.
- Scales via content: more drop types via `DA_GP_OrbitalDrop_*`.

**Pillar 9 (Technical):** server-authoritative, data-driven (soft refs), GAS-native (spend via GE). All boxes checked.

## Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Order Worker | Click menu, click visible terrain, pod arrives, Worker active. |
| 2 | Insufficient orbital | Try when OrbitalFerronite < Cost → toast, no spend. |
| 3 | Unit cap reached | Try Worker when at cap → toast, no spend. |
| 4 | Drop into Explored area | Reticle red, no drop. |
| 5 | Drop into Unexplored area | Cursor doesn't register hit. |
| 6 | Drop on water/cliff | Red, no drop. |
| 7 | Drop on existing building | Red (overlap), no drop. |
| 8 | Pod cap reached | 4th simultaneous order → toast "Pod queue full". |
| 9 | Pod descent telegraph | Both players see pod descending; SWARM AI optionally redirects. |
| 10 | Drop targeting cancel | Esc / RMB → mode exits, no spend. |
| 11 | Drop placement same as building destroyed | After enemy MainBase destroyed AND falls in their old territory now visible (post-fight) — can drop. |
| 12 | Order menu replication | OrderMenuVM updates ≤ 1 frame after OrbitalFerronite change. |

## Out of MVP

- Drop pod intercept abilities (anti-drop combat).
- Drop pod redirect (after launch — change landing point).
- Auto-deploy at drop site (e.g., "scout drop" — Worker immediately mines nearest deposit).
- Cooldowns per drop type.
- Per-faction drop catalog asymmetry.
- Squad-drops (multiple units in one pod).
- Special structures (radar, scanner, communication relay).
- Black market alternate adressees.

## References

- Orbital Delivery GDD — [`../GDD/10_Orbital_Delivery`](../GDD/10_Orbital_Delivery.md).
- Container System (resource side) — [`07_Resource_Architecture`](07_Resource_Architecture.md) §Container System Update.
- Building scope changes — [`06_Building_Architecture`](06_Building_Architecture.md) §"Building Lifecycle — Orbital Drop".
- FoW (drop-zone gating) — [`15_Fog_of_War`](15_Fog_of_War.md).
- UI / MVVM — [`12_UI_Architecture`](12_UI_Architecture.md).
- Multiplayer authority — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).
- ADRs — [`../Architecture_Decisions/ADR_0002_Data_Driven_First`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md), [`../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering`](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md).
