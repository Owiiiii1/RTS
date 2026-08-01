# Resource Architecture

> **CANONICAL MODEL (post-pivot 2026-05-16).** The active resource model is the **Two-State Container model** documented in §"Container System (CANONICAL)" below. Ferronite lives in two states: **Planetary** (raw, mined by Workers, stored in MainBase containers via `UGP_StorageComponent`, NOT spendable) and **Orbital** (`OrbitalFerronite`, spendable currency, created only on container launch). SWARM pressure is driven by **`FerroniteThreatValue`** — the raw Planetary Ferronite *currently* stored at base (a fluctuating stock on `AGP_GameState`), NOT by any monotonic shipped/mined accumulator.
>
> **Sections below marked `[SUPERSEDED — pre-pivot]` describe the OLD single-pool model (`Ferronite`/`MaxFerronite` attribute, drop-off income GE chain, spend-on-build, Assembly Yard, Server_BuildAt/Server_QueueProduction). They are retained for migration reference only and are NOT current canon.** Read the Container System section as authoritative wherever the two conflict.

## Scope

Technical implementation of Ferronite resource flow per [`../GDD/06_Resources`](../GDD/06_Resources.md). Single MVP resource (Ferronite) як **universal exchange currency** (per Pillar 3, see [`../GDD/01_Game_Pillars`](../GDD/01_Game_Pillars.md)).

## Decision Source

- Owner decision 2026-05-16 — score-based win з finite Ferronite deposits + Main Base drop-off.
- ADR-0002 (Data-Driven First) — balance values у Data Assets.
- ADR-0003 (GAS First) — currency through Attributes + Effects, не direct property write.
- ADR-0007 (Building-As-Pawn, Draft) — Ferronite Deposit як `AGP_BuildingBase`.

## Attribute Model `[SUPERSEDED — pre-pivot]`

> **SUPERSEDED.** This single-pool `Ferronite`/`MaxFerronite` attribute model is pre-pivot. The canonical attribute set is `OrbitalFerronite` (COND_OwnerOnly) / `FerroniteScore` (COND_None) / `MaxUnits` / `CurrentUnits` — see §"Container System (CANONICAL)" → "Attribute Model (CANONICAL)". `Ferronite` and `MaxFerronite` are REMOVED. Retained below for migration reference only.

Resource — **player-level attribute** на `AGP_PlayerState.ASC`. Worker-level temporary carry capacity — окрема attribute на `AGP_UnitBase.ASC`.

```
UGP_PlayerAttributeSet (pre-pivot, SUPERSEDED):
  Ferronite          (current pool; default 0; clamped 0..MaxFerronite)
  FerroniteScore     (cumulative score; monotonically increasing; default 0)
  MaxFerronite       (optional cap; default 9999 effectively unlimited у MVP)
  MaxUnits           (capacity ceiling; default 5)
  CurrentUnits       (active unit count; default 0)

UGP_UnitAttributeSet:
  CarriedFerronite   (Worker only; cleared on drop-off; capped at WorkerCarryCapacity)
```

Replication:

- `Ferronite`, `MaxFerronite`, `MaxUnits`, `CurrentUnits` — `COND_OwnerOnly` (other players не бачать).
- `FerroniteScore` — replicated to all (opponent score visibility per `09_UI_UX`).
- `CarriedFerronite` — replicated (UI може показувати carry indicator над worker).

```cpp
// In UGP_PlayerAttributeSet::GetLifetimeReplicatedProps
DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, Ferronite, COND_OwnerOnly, REPNOTIFY_Always);
DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, FerroniteScore, COND_None,   REPNOTIFY_Always);
DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, MaxUnits,       COND_OwnerOnly, REPNOTIFY_Always);
DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, CurrentUnits,   COND_OwnerOnly, REPNOTIFY_Always);
```

## Mining Flow `[SUPERSEDED — pre-pivot drop-off income chain]`

> **SUPERSEDED for drop-off semantics.** The mining state machine (MovingToDeposit → Mining → AutoReturnToBase) is still accurate, but the **DroppingOff** step below is pre-pivot: it applies `GE_GP_AddFerronite` + `GE_GP_SendToOrbit` and increments `SwarmAggressionLevel`. Canonical drop-off = `UGP_StorageComponent` container fill (NO player income GE) + `FerroniteThreatValue +=` on `AGP_GameState`; income GEs fire only on container LAUNCH. See §"Container System (CANONICAL)" → "Updated Mining Flow" and "Auto-Ship Flow".

```
Worker selected, player issues GP.Command.Mine on Ferronite Deposit
   |
   v
AGP_PlayerController::Server_RequestCommand validates ownership + capability
   |
   v
Worker's UGP_CommandComponent dispatches to UGP_MiningComponent::BeginMining(Deposit)
   |
   v
UGP_MiningComponent server-only state machine:
   MovingToDeposit -> Mining -> AutoReturnToBase -> DroppingOff -> AutoReturnToDeposit (chain)
```

### State: MovingToDeposit

- `UCharacterMovementComponent::MoveTo(Deposit->Location)`.
- On arrival (distance < `MiningRange`, default 200 cm): transition to `Mining`.

### State: Mining

- Timer tick every `MiningInterval` (1.0 s default; lives у `DA_GP_Resource_Ferronite.MineRatePerWorker`).
- On tick:
  - Decrement `Deposit.Capacity -= MineRatePerWorker`.
  - Increment Worker's `CarriedFerronite += MineRatePerWorker`.
- Exit conditions:
  - `CarriedFerronite >= WorkerCarryCapacity` → transition to `AutoReturnToBase`.
  - `Deposit.Capacity <= 0` → deposit disabled (visual change), Worker transitions to `AutoReturnToBase` з `CarriedFerronite > 0`, OR returns to idle if 0.
  - New command received → abort state machine.

### State: AutoReturnToBase

- `MoveTo(OwningPlayerState->MainBase->Location)`.
- On arrival у `DropOffRange` (400 cm, per `DA_GP_Building_MainBase`): transition to `DroppingOff`.

### State: DroppingOff `[SUPERSEDED — see Container System CANONICAL]`

> **SUPERSEDED.** Canonical drop-off does NOT apply player income GEs. It writes raw Ferronite into a MainBase container (Storage state mutation) and raises swarm pressure. See "Updated Mining Flow" under §"Container System (CANONICAL)". Pre-pivot text retained below for migration reference.

- ~~Apply `GE_GP_AddFerronite` to `OwningPlayerState.ASC`: `Ferronite += CarriedFerronite`.~~
- ~~Apply `GE_GP_SendToOrbit` to `OwningPlayerState.ASC`: `Ferronite -= CarriedFerronite`; `FerroniteScore += ...`.~~
- ~~Increment `AGP_GameState.SwarmAggressionLevel += CarriedFerronite * AggressionPerUnitMined`.~~
- **Canonical replacement:** `StorageComponent.AddPlanetaryFerronite(CarriedFerronite)` (container fill); `AGP_GameState.FerroniteThreatValue += AcceptedAmount × ThreatPerStoredUnit` (swarm pressure rises); income (`GE_GP_AddOrbital` + `GE_GP_AddScore`) deferred to container launch.
- Worker's `CarriedFerronite -= AcceptedAmount`.
- Transition to `AutoReturnToDeposit` if last deposit still alive і container slot free, else `Idle`.

### State: AutoReturnToDeposit (Chain Command)

- Якщо last deposit alive: `MoveTo(LastDeposit->Location)` → re-enter `Mining`.
- Якщо deposit dead: idle. Player issues new command manually.

### Worker → PlayerState Lookup

```cpp
AGP_PlayerState* UGP_MiningComponent::GetOwningPlayerState() const
{
    AGP_UnitBase* Unit = GetOwner<AGP_UnitBase>();
    if (!Unit) return nullptr;
    return Unit->GetOwningPlayerState();
}
```

`AGP_UnitBase` має `UPROPERTY(Replicated) TWeakObjectPtr<AGP_PlayerState> OwningPlayerState`. Окремо від `TeamId`.

## Resource Application via GAS `[SUPERSEDED — pre-pivot GE chain]`

> **SUPERSEDED.** This section describes the pre-pivot drop-off income chain and spend-on-build effects. Canonical GE schema is in §"Container System (CANONICAL)" → "GE Schema (CANONICAL)": income GEs (`GE_GP_AddOrbital` + `GE_GP_AddScore`) fire on container LAUNCH; spend is `GE_GP_SpendOrbital` on accepted orbital drop order; `GE_GP_Cost_RepairTick` for Worker repair. `GE_GP_AddFerronite`, `GE_GP_SendToOrbit`, `GE_GP_SpendFerronite`, `GE_GP_UnitCap_Plus5` (Assembly-Yard-driven) are REMOVED. Unit-cap growth now comes from a **Logistics Hub orbital drop**, not Assembly Yard. Retained below for migration reference only.

Усі resource transactions — через `UGameplayEffect`. Жодних direct attribute writes.

### ~~Add Ferronite to Pool~~ `[REMOVED]`

~~`GE_GP_AddFerronite.uasset`~~ — removed. Drop-off is now a `UGP_StorageComponent` container fill (non-attribute Storage mutation), not a player GE.

### ~~Send to Orbit~~ `[REMOVED]`

~~`GE_GP_SendToOrbit.uasset`~~ — removed. Split into `GE_GP_AddOrbital` (`OrbitalFerronite += Volume × OrbitalConversionRate`) + `GE_GP_AddScore` (`FerroniteScore += Volume × ScoreConversionRate`), both fired on container LAUNCH (not drop-off).

### ~~Spend Ferronite on Builds/Production~~ `[REMOVED]`

~~`GE_GP_Cost_AssemblyYard` / `GE_GP_SpendFerronite`~~ — removed. There is no on-planet build/production. Spend is `GE_GP_SpendOrbital` (`OrbitalFerronite -= DropDef.Cost`) applied on accepted orbital drop order, plus `GE_GP_Cost_RepairTick` for Worker repair.

### ~~Increase Unit Cap via Assembly Yard~~ `[REMOVED]`

~~`GE_GP_UnitCap_Plus5` on Assembly Yard~~ — removed. `MaxUnits` increases via a **Logistics Hub orbital drop** (Infinite GE while the Hub lives, removed on Hub destroyed). Per Pillar 4 (Capacity Is Strategy) — cap grows only through an explicit OrbitalFerronite spend (the Hub drop). No auto-cap-growth GEs.

## Ferronite Deposit Actor

`AGP_BuildingBase` з:

- `BuildingTags = {GP.Resource.Node, GP.Resource.Type.Ferronite}`.
- `BuildingRole`: implicit "environment" (не assigned до player team — `TeamId = 0` neutral).
- Spawned with level (map placement), не player-built.
- Static mesh — visual TBD (rock outcrop з glowing ferronite veins; per `Lore_Setting`).
- Not directly attackable у MVP (`bDamageable = false` через `UGP_BuildingDefinition` flag або filter у `UGP_TargetingComponent`).
- Finite capacity tracked у `AGP_BuildingBase.CurrentCapacity` (replicated).

### Capacity Model (Finite, Per Owner Directive)

```cpp
UPROPERTY(ReplicatedUsing=OnRep_Capacity)
float CurrentCapacity = 1000.0f; // Default; final values per DA + map placement.

UPROPERTY(EditDefaultsOnly, Category="GP|Resource")
float MaxCapacity = 1000.0f; // Per DA_GP_Building_FerroniteDeposit + per-instance override у map.

UPROPERTY(EditDefaultsOnly, Category="GP|Resource")
int32 MaxConcurrentWorkers = 4;
```

On Mining tick:

- Server decrements `CurrentCapacity`.
- `OnRep_Capacity` тригерить client visual update (e.g., depleting glow effect).
- При `CurrentCapacity <= 0`: deposit disabled (visual change → "empty husk"), `MiningComponent` reject `BeginMining` calls.

Optional respawn / replenishment — поза MVP.

## Resource Display (UI)

Per [`../GDD/09_UI_UX`](../GDD/09_UI_UX.md):

- `WBP_GP_HUD_ResourceReadout` bound to:
  - `UGP_PlayerAttributeSet.Ferronite` change delegate → numeric pool display.
  - `UGP_PlayerAttributeSet.FerroniteScore` change delegate → score display + `+N` flash animation.
  - Opponent `UGP_PlayerAttributeSet.FerroniteScore` (via remote `AGP_PlayerState.ASC` lookup) → opponent score readout.
- Worker carry indicator (optional MVP, recommended) → mini icon над worker з `CarriedFerronite / WorkerCarryCapacity` visualization.
- Deposit capacity indicator → glowing intensity scales з `CurrentCapacity / MaxCapacity`.

Жодного tick-polling. Усе через attribute change delegates або RepNotify (per A9 anti-pattern у `gp-mechanics-validator`).

## Multi-Resource Future-Proofing

Per Pillar 3 (One Resource, Many Tradeoffs) — multi-resource economy **review-blocked** для MVP. Architecture, однак, не блокує future addition:

- Додавати `DECLARE_ATTRIBUTE_CAPTUREDEF(<NewResource>)` у `UGP_PlayerAttributeSet`.
- Додавати `GE_GP_Add<NewResource>` і `GE_GP_Cost_<X>_<NewResource>` Effects.
- Додавати `GP.Resource.Type.<NewResource>` tag.
- Додавати `DA_GP_Resource_<NewResource>` Data Asset.
- UI bindings — додаткові readouts.

Це O(1) extension, не requires refactor. Tag namespace `GP.Resource.Type.*` дозволяє multi-resource node-side distinguishing.

Будь-яка спроба додати другий resource у MVP — порушення Pillar 3 і A15 (Multi-Resource Drift). Reconsider — pillar amendment.

## Out of MVP

- Multi-resource economy.
- Resource decay over time.
- Storage caps with overflow penalties.
- Player-built storage buildings (silo / depot).
- Resource theft / sabotage.
- Market / trade between players.
- Resource transfer between allies (no allies у 2-player MVP).
- Deposit respawn / replenishment.
- Multi-tier deposits (rich / standard / poor variants).
- Convoy / shipment mechanic як explicit "Launch Shipment" action.

## Detailed Resource Primary Rules (GP-0303)

> **PARTIALLY SUPERSEDED.** This GP-0303 section predates the container pivot. The deposit/mining mechanics (deposit actor, capacity, concurrent-worker queue, abuse cases) remain valid. But anything describing the **single `Ferronite` pool, `MaxFerronite`, drop-off income GE chain, `AggressionPerUnitMined`/`SwarmAggressionLevel`, spend-on-`Server_BuildAt`/`Server_QueueProduction`, or Assembly Yard / Barracks** is pre-pivot — superseded by §"Container System (CANONICAL)". Inline notes below flag each.

Stage — design only (per [`Claude_Tasks/GP-0303_Resource_Primary`](../Development/Claude_Tasks/GP-0303_Resource_Primary.md)). Поверх вже існуючої архітектури вище. Цей розділ закриває decision gaps і consolidates abuse-cases.

### DataAsset: DA_GP_Resource_Ferronite

Resource-type metadata (per GDD/06):

| Field | Type | Default | Notes |
| --- | --- | --- | --- |
| `DisplayName` | `FText` | "Ferronite" | UI label |
| `ResourceTag` | `FGameplayTag` | `GP.Resource.Type.Ferronite` | Single MVP resource |
| `ScoreConversionRate` | `float` | `1.0` | FerroniteScore per unit shipped to orbit; TBD balance |
| `OrbitalConversionRate` | `float` | `1.0` | OrbitalFerronite per unit shipped; TBD balance |
| `ThreatPerStoredUnit` | `float` | `1.0` | `FerroniteThreatValue` scalar per unit raw Ferronite stored at base (default 1.0 = threat is stored volume); TBD balance |
| `ThreatToWaveSize` | `TSoftObjectPtr<UCurveFloat>` | (asset) | Wave size keyed on `FerroniteThreatValue` |
| `ThreatToWaveFrequency` | `TSoftObjectPtr<UCurveFloat>` | (asset) | Wave frequency keyed on `FerroniteThreatValue` |
| `Tint` | `FLinearColor` | teal-blue glow | UI / VFX |
| `Icon` | `TSoftObjectPtr<UTexture2D>` | (asset) | HUD readout |

Balance values — TBD у dedicated balance pass. Schema fixed.

### DataAsset: DA_GP_Building_FerroniteDeposit

Instance properties for deposit actor:

| Field | Type | Default | Notes |
| --- | --- | --- | --- |
| `ResourceTypeRef` | `TSoftObjectPtr<UGP_ResourceDefinition>` | `DA_GP_Resource_Ferronite` | Type identity |
| `MaxCapacity` | `float` | `3000` (placeholder, TBD balance) | Per-deposit reserves |
| `MineRatePerWorker` | `float` | `10` units/s | Single source of truth; Worker DA mirrors |
| `MiningInterval` | `float` | `1.0` s | Tick rate |
| `MiningRange` | `float` | `200` cm | Worker→Deposit max distance |
| `MaxConcurrentWorkers` | `int32` | `4` | **Soft cap + queue** behavior |
| `DepletedBehavior` | `EGP_DepletedBehavior` | `Destroy` | Actor despawns on `CurrentCapacity <= 0` |
| `Mesh` / `Materials` / `TintParam` | art refs | per asset pass | Visual |
| `RubbleMesh` | `TSoftObjectPtr<UStaticMesh>` | optional | If `DepletedBehavior=InertHusk` — post-MVP fallback |

`EGP_DepletedBehavior` enum:

```cpp
UENUM()
enum class EGP_DepletedBehavior : uint8
{
    Destroy,     // MVP — actor despawns + DestroyVFX multicast
    InertHusk    // post-MVP — mesh stays, mineable=false
};
```

Map artist can override `MaxCapacity` per-instance у level (rich/poor variants без окремого DA — `bAllowInstanceOverride=true` у UPROPERTY).

### Concurrent Workers — Soft Cap + Queue

`AGP_FerroniteDeposit` (`AGP_BuildingBase` subclass у MVP, або direct in-place у building base via `BuildingDefinition`) tracks:

```cpp
UPROPERTY(Replicated)
TArray<TWeakObjectPtr<AGP_Worker>> ActiveMiners;    // currently mining (≤ MaxConcurrentWorkers)

UPROPERTY(Replicated)
TArray<TWeakObjectPtr<AGP_Worker>> WaitingMiners;   // queued
```

Rules:

1. Worker enters `Mining` state → calls `Deposit->RequestSlot(Worker)`:
   - If `ActiveMiners.Num() < MaxConcurrentWorkers` → add to `ActiveMiners`, return `EGP_SlotResult::Granted`.
   - Else → add to `WaitingMiners`, return `EGP_SlotResult::Queued`.
2. Worker з `Queued` status — server idles worker біля deposit (no movement, no mining tick). Picks "idle nearby" position (random offset 200-400 cm from deposit center, sticky per-worker).
3. Active miner finishes (cargo full / deposit deplete / command interrupt) → `Deposit->ReleaseSlot(Worker)`:
   - Remove from `ActiveMiners`.
   - If `WaitingMiners.Num() > 0`:
     - Pop oldest (FIFO).
     - Promote to `ActiveMiners`. Trigger Worker's `MiningComponent` resume → enters `Mining` state.
4. Worker death / interrupt while `Queued` → remove from `WaitingMiners`.
5. Replication: `ActiveMiners.Num()` + `WaitingMiners.Num()` (counts only) для HUD indicator. Full lists — server-only.

Trade-offs: clean visual, no overlap stacking, гравець бачить queue length як deposit indicator (Open Question #5).

### Mining Tick — Final Form (consolidates earlier section)

Already specified у §Mining Flow above. GP-0303 confirmation:

- `MineRatePerWorker` units/s decrements `Deposit.CurrentCapacity` and increments `Worker.CarriedFerronite` per `MiningInterval` (1 s default).
- When `Worker.Cargo.IsFull()` → exit to `MovingToBase` (per GP-0302 Cargo Component).
- When `Deposit.CurrentCapacity <= 0` → Deposit triggers `OnDepleted` (server) → notifies всі `ActiveMiners` + `WaitingMiners` → їх `MiningComponent` switches to AutoRetarget per GP-0302 rule.
- Then Deposit performs `EGP_DepletedBehavior::Destroy` flow (delay 0.5 s VFX, Destroy() actor).

### Spend Rules `[UPDATED — post-pivot]`

`UGP_PlayerAttributeSet.OrbitalFerronite` — single point of authoritative spend (post-pivot). There is no on-planet construction or production; all assets arrive as orbital drops.

- Orbital drop order: `GE_GP_SpendOrbital` Instant (`OrbitalFerronite -= DropDef.Cost`); applied server-side on accepted `Server_RequestOrbitalDrop` via `UGP_OrbitalDeliverySubsystem` (per [`14_Orbital_Delivery`](14_Orbital_Delivery.md)). **Removed:** `Server_BuildAt`, `Server_QueueProduction`.
- Repair: `GE_GP_Cost_RepairTick` Instant on each Worker repair tick (Worker repair STAYS in MVP; cost TBD).
- All other spend points — error. Direct attribute write banned (per ADR-0003).

**Validation:** single resource type spent (OrbitalFerronite). The pre-pivot "Barracks/Assembly Yard costs exactly 1 resource" stop condition now maps to "every orbital drop costs exactly 1 resource (OrbitalFerronite)" — no Assembly Yard exists. Cost lives in `UGP_OrbitalDropDefinition.Cost` (TBD balance).

### Income Caps / Storage Cap `[UPDATED — post-pivot]`

- No single `Ferronite` pool / `MaxFerronite` attribute exists post-pivot.
- Raw Planetary Ferronite is bounded by **container capacity** on `UGP_StorageComponent` (per-container `Capacity` × `MaxContainerCount`). When all containers are Ready/Launching/full → Worker drop-off overflow is lost; HUD warns "Storage full — launch containers!".
- `OrbitalFerronite` — effectively unlimited in MVP (no `MaxOrbitalFerronite` enforcement planned). Reserve a cap mechanic for post-MVP.

### Score / Income Pipeline Integration `[UPDATED — post-pivot]`

Canonical flow (income deferred to launch):

```
Worker drop-off
  → StorageComponent.AddPlanetaryFerronite(Amount)        (container fill — Storage mutation, no player GE)
  → AGP_GameState.FerroniteThreatValue += Accepted × ThreatPerStoredUnit   (swarm pressure RISES)

Container launch (Launching → Departed)
  → GE_GP_AddOrbital  (OrbitalFerronite += Volume × OrbitalConversionRate)
  → GE_GP_AddScore    (FerroniteScore   += Volume × ScoreConversionRate)
  → AGP_GameState.FerroniteThreatValue -= Volume          (swarm pressure DROPS — relief)
```

`FerroniteScore` is monotonic and never reduced by spending OrbitalFerronite. `FerroniteThreatValue` is a fluctuating STOCK (up on drop-off, down on launch), NOT a cumulative accumulator.

Match end:
- Primary: first to `DeliveryQuotaFerroniteScore` (DA placeholder 5000) wins immediately.
- Fallback: highest `FerroniteScore` at 10:00 timer expiry (per GDD/08).
- Loss: MainBase destroyed if `bAnnihilationCountsAsWin` (true).
- Tie-break: highest `FerroniteScore`, then `CurrentUnits`, then deterministic seed. OrbitalFerronite is never the victory score.

### Abuse Cases & Mitigations

| Abuse | Mitigation |
| --- | --- |
| Client fakes `Server_RequestCommand(Mine)` on enemy unit | Per-target ownership check (TDD/04 validation step 5). Reject. |
| Client spams Mine on depleted deposit | Server reject у `BeginMining`: `Deposit.CurrentCapacity <= 0` → no-op + log Verbose. |
| Worker teleport mining (no movement, instant range) | Mining tick checks `Distance(Worker, Deposit) <= MiningRange` on each tick. If teleport via cheat — server distance check fails, mining aborts. |
| Drop-off without travel (client claims arrival) | Drop-off triggered тільки коли server-side worker `MovingToBase` reaches `DropOffRange`. Client cannot trigger directly. |
| Server-side drop-off double-application | `UGP_MiningComponent` state machine — `DroppingOff` is single-shot transition. `Cargo.EmptyAll()` returns 0 on second call. |
| Worker carries > MaxCargo (overflow) | `Cargo.AddCargo` clamps at `MaxCargo`, returns leftover. Excess discarded (deposit "wastes" mined chunk if it would overflow). |
| Concurrent worker > MaxConcurrentWorkers | Soft-cap queue — extra workers wait, not mine. Server-authoritative. |
| Deposit destroyed during mining tick | `TWeakObjectPtr` null-check → worker abort → AutoRetarget if `bAutoCycle`. |
| Two miners drop-off simultaneously — race | GAS attribute add is serial server-side (UE GAS handles via attribute changes). No race. |
| Faking carry to skip travel | `CarriedFerronite` is server-only write. Client read-only. |
| Mining deposit owned by neutral but with `Player.TeamId=0` collision | Resource nodes `TeamId=0` (neutral, per existing spec). Worker validation: target is `GP.Resource.Node`, not enemy. Same-team check skipped for neutrals. |
| MainBase destroyed → workers idle with full cargo | Cargo not lost. If new MainBase spawns (post-MVP) — drop-off resumes. У MVP no respawn → cargo wasted у idle worker until death. Acceptable. |

### Tag Surface

| Tag | Purpose |
| --- | --- |
| `GP.Resource.Type.Ferronite` | Resource identity (single MVP) |
| `GP.Resource.Node` | Marks actor as mineable (deposit) |
| `GP.Capability.Inspectable` | Deposit shows HP/capacity у HUD on click |

**No** `GP.Capability.Selectable` on deposit — гравець не selects deposits (smart RMB resolves to Mine command).

### UI Surface (cross-ref до GP-0401) `[UPDATED — post-pivot]`

- `WBP_GP_HUD_ResourceReadout`: `OrbitalFerronite` (own spendable, COND_OwnerOnly), `FerroniteScore` (own + opponent).
- Container row: raw Planetary Ferronite stored at base (visualizes `FerroniteThreatValue`).
- Worker carry indicator: small bar над worker, fills with cargo.
- Deposit indicator on hover: capacity bar, current miners (e.g., "2/4"), queue count.
- Threat readout (separate widget per GDD/09): `AGP_GameState.FerroniteThreatValue` replicated (rises on hoard, drops on launch).

### Validation Checklist (Stop Condition) `[UPDATED — post-pivot]`

- [x] One resource only — `UGP_PlayerAttributeSet` has `OrbitalFerronite` + `FerroniteScore` (+ MaxUnits/CurrentUnits); no other resource attributes у MVP. Raw Planetary Ferronite is container state, not a player attribute.
- [x] Every orbital drop costs exactly 1 resource (OrbitalFerronite, per `UGP_OrbitalDropDefinition.Cost`) — single attribute decrement. No Assembly Yard / Barracks exists.
- [x] Resource server-authoritative — all transactions via `UGameplayEffect` server-applied, client read-only; container state server-authoritative on `UGP_StorageComponent`.

### Open Questions

1. **Capacity initial placeholder 3000 — pacing check:** at MineRate 10/s, 24 workers parallel (split 4/deposit × 6 deposits) means rough 240 units/s aggregate. 3000 deposit drains у 12-15 s of full saturation. Map needs ~8-12 deposits для 10-min match. Confirm у first playtest.
2. **Score conversion 1:1 vs penalty:** simple 1:1 у MVP; the SWARM `FerroniteThreatValue` loop provides natural tension. Re-evaluate post-playtest.
3. **`bAllowInstanceOverride` for MaxCapacity** — convention vs explicit flag. UE4-style "Edit" instances by default if `EditAnywhere`. Mark explicitly у DataAsset comments.
4. **Currency cap (`OrbitalFerronite`):** uncapped у MVP, OK. Raw Ferronite is bounded by container capacity. Reserve a currency-cap mechanic for post-MVP.
7. **Global escalation floor (design TBD):** because shipping reduces `FerroniteThreatValue`, a pure fast-shipper match may never escalate to a climax. A slow secondary global escalation floor (mild time- or score-driven baseline under the threat curve) may be needed so every match escalates and ends. Starting value + test plan in balance pass — no firm numbers yet.
5. **Queue indicator UI:** show "2/4 mining, 1 queued" on deposit hover? Recommend yes для clarity, low cost у HUD.
6. **DepletedBehavior — Destroy + RubbleMesh:** swap actor with `AStaticMeshActor` for visual landmark? Reduces actor count, but adds spawn cost. Defer to perf pass.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Single worker, one deposit | Mining cycle completes 1 full trip: deposit −50, container fill +50, FerroniteThreatValue +50; no score/orbital yet (deferred to launch). |
| 2 | 4 workers on one deposit | All 4 mine concurrently. 5th worker enters queue. |
| 3 | Queue promotion | One of 4 leaves (cargo full) → queued 5th auto-promotes, starts mining. |
| 4 | Deposit deplete mid-trip | Deposit hits 0 while 4 active → all 4 receive AutoRetarget signal. |
| 5 | Deposit destroy | After deplete, 0.5 s VFX → actor despawns. No null-ref у HUD. |
| 6 | Drop-off → container fill | Worker delivers 50 Ferronite → container fill +50, FerroniteThreatValue += 50 × ThreatPerStoredUnit. No score/orbital change until launch. |
| 6b | Container launch | Full container (Volume 100) launches → OrbitalFerronite += 100 × OrbitalConversionRate, FerroniteScore += 100 × ScoreConversionRate, FerroniteThreatValue −= 100. |
| 7 | Insufficient OrbitalFerronite spend | Player has 100, tries an orbital drop costing 150 → ability fails, HUD "Insufficient Orbital Ferronite" feedback. |
| 8 | Cheat resistance — fake Mine RPC | Client modded to send Mine on enemy actor → server rejects (ownership/capability). |
| 9 | Cheat resistance — fake CarriedFerronite | Client cannot write to attribute. Cargo cap enforced. |
| 10 | Concurrent drop-offs | 2 workers arrive simultaneously → both apply GE serially, both increment score correctly. |
| 11 | Storage cap (containers) | All containers full/launching → further drop-off overflow lost; HUD "Storage full". OrbitalFerronite itself uncapped in MVP. |
| 12 | Worker survives deposit destruction with cargo | Worker has 30 cargo, deposit dies → worker auto-return to base (cargo intact). |

## Container System (CANONICAL) (2026-05-16 Pivot)

> **This is the ACTIVE / canonical resource model.** Everything above marked `[SUPERSEDED]` is pre-pivot reference. Read this section as authoritative.

**Major change.** Pre-pivot model (superseded): Worker drop-off → instant `Ferronite += N` + `FerroniteScore += N`. Pool was immediately spendable.

**Canonical model (per [`../GDD/10_Orbital_Delivery`](../GDD/10_Orbital_Delivery.md) і memory rule `project_container_system`):**

Ferronite живе у двох станах:

1. **Planetary** — raw, у containers на MainBase. **Не spendable.** Vulnerable. Drives SWARM via `FerroniteThreatValue`.
2. **Orbital** — після container ship-to-orbit. **Spendable currency.**

### Attribute Model (CANONICAL)

```
UGP_PlayerAttributeSet (CANONICAL):
  OrbitalFerronite     (current spendable pool; default 0; COND_OwnerOnly)
  FerroniteScore       (cumulative shipped value = victory score; monotonic; default 0; COND_None — all clients; drives delivery quota)
  MaxUnits             (capacity ceiling; COND_OwnerOnly)
  CurrentUnits         (active unit count; COND_OwnerOnly)
```

**Removed/deprecated:** old single `Ferronite` pool attribute and `MaxFerronite`. Planetary Ferronite is NOT a player attribute — it lives як container state on MainBase's `UGP_StorageComponent`. `OrbitalFerronite` is uncapped in MVP (no `MaxOrbitalFerronite` enforcement). `FerroniteThreatValue` is NOT a player attribute either — it is a per-player stock on `AGP_GameState`.

### Container State (on MainBase StorageComponent)

```cpp
UENUM()
enum class EGP_ContainerState : uint8
{
    Idle,         // empty
    Filling,      // accepting drop-offs
    Ready,        // full, awaiting launch
    Launching,    // launch sequence active (2-3 s vulnerable window)
    Departed      // shipped; entry available for reuse
};

USTRUCT()
struct FGP_Container
{
    GENERATED_BODY()
    UPROPERTY()  int32 Volume = 0;
    UPROPERTY()  int32 Capacity = 100;        // DA-driven default
    UPROPERTY()  EGP_ContainerState State = EGP_ContainerState::Idle;
    UPROPERTY()  float StateEnteredTime = 0.f;
};

// In UGP_StorageComponent (на AGP_MainBase):
UPROPERTY(ReplicatedUsing=OnRep_Containers)
TArray<FGP_Container> Containers;

UPROPERTY(EditDefaultsOnly)
int32 MaxContainerCount = 5;                  // DA placeholder, TBD balance
```

Replicated for HUD / opponent inspection visibility (per FoW gating — see [`15_Fog_of_War`](15_Fog_of_War.md)).

### Updated Mining Flow

```
Worker.MineDeposit:
  Worker.CarriedFerronite += MineRate × dt
  if Worker.CarriedFerronite >= MaxCargo:
    transition → MovingToBase

Worker arrives MainBase + InRange:
  transition → DroppingOff

DroppingOff:
  ChunkAdded = StorageComponent.AddPlanetaryFerronite(Worker.CarriedFerronite)
  // returns amount accepted (could be < CarriedFerronite if storage full)
  AGP_GameState.FerroniteThreatValue += ChunkAdded × ThreatPerStoredUnit   // swarm pressure RISES
  Worker.CarriedFerronite -= ChunkAdded
  if Worker.CarriedFerronite > 0 (storage was full):
    notify "Storage full — launch containers!" toast
    Worker idle until storage frees
  else:
    transition → Returning (auto-cycle per GP-0302)
```

`UGP_StorageComponent::AddPlanetaryFerronite(int32 Amount)`:
1. Knock through `Containers` list; fill first non-Departed slot до capacity.
2. If container reaches capacity → state = Ready.
3. Continue з overflow до next slot.
4. If all slots Ready/Launching/Filling-full → return leftover amount.

### Auto-Ship Flow

`UGP_StorageComponent::Tick` (server, 0.5 s):
- Scan Containers. If any state == Ready AND no other Launching → trigger Launch:
  - state = Launching.
  - StateEnteredTime = Now.
  - Multicast cosmetic: container lift VFX + SFX.
- Container з state == Launching AND `Now - StateEnteredTime >= ShipDelaySeconds` (DA default 2.5 s):
  - state = Departed.
  - Apply `GE_GP_AddOrbital(Volume)`: `OrbitalFerronite += Volume × OrbitalConversionRate`.
  - Apply `GE_GP_AddScore(Volume)`: `FerroniteScore += Volume × ScoreConversionRate` (drives delivery quota).
  - `AGP_GameState.FerroniteThreatValue -= Volume` — raw stock leaves base → **swarm pressure DROPS** (shipping is relief/safety). NOTE: launch does NOT add to any aggression accumulator; the swarm driver is the live stored stock, so shipping reduces pressure.
  - Multicast cosmetic: rocket lift / launch flash.
  - State after 1 s cooldown → Idle (slot reusable).

### GE Schema (CANONICAL)

Canonical GAS effects (post-pivot). Container fill on drop-off is a Storage state mutation, NOT a player GE.

| Effect | Trigger | Modifier |
| --- | --- | --- |
| `GE_GP_AddOrbital` | Container launch (Launching → Departed) | `OrbitalFerronite += Volume × OrbitalConversionRate` |
| `GE_GP_AddScore` | Container launch (same transition) | `FerroniteScore += Volume × ScoreConversionRate` |
| `GE_GP_SpendOrbital` | Accepted orbital drop order | `OrbitalFerronite -= DropDef.Cost` |
| `GE_GP_RefundOrbital` | Order canceled / drop failed (post-MVP) | `OrbitalFerronite += refund` |
| `GE_GP_Cost_RepairTick` | Worker repair tick (Worker repair STAYS in MVP) | `OrbitalFerronite -= TickCost` (TBD) |

**Removed (pre-pivot, do not use):** `GE_GP_AddFerronite`, `GE_GP_SendToOrbit`, `GE_GP_SpendFerronite`. Worker drop-off triggers NO player GE — it calls `UGP_StorageComponent::AddPlanetaryFerronite` (container fill) and raises `AGP_GameState.FerroniteThreatValue` directly. There is no `GE_GP_AddPlanetaryFerronite`.

### Spend Path (Orbital Drops)

Order menu (per [`14_Orbital_Delivery`](14_Orbital_Delivery.md)):

1. Player clicks order → `Server_RequestOrbitalDrop(DropTypeDef, FVector Loc)`.
2. Server validates:
   - `OrbitalFerronite >= DropType.Cost`.
   - Drop zone validation (FoW visible, navigable, etc.).
3. Apply `GE_GP_SpendOrbital(DropType.Cost)`.
4. Enqueue drop pod via `UGP_OrbitalDeliverySubsystem`.

If `OrbitalFerronite < Cost`: ability fails → `Client_NotifyCommandRejected(EReason::InsufficientOrbital)` → HUD toast "Insufficient Orbital Ferronite".

### Replication Conditions (Updated)

| Field | Condition |
| --- | --- |
| `OrbitalFerronite` | `COND_OwnerOnly` (private currency; uncapped in MVP) |
| `FerroniteScore` | `COND_None` (visible to all — drives delivery quota race) |
| `Containers` (on Storage) | `COND_None` for state visibility у FoW-active enemy view; volume content `COND_OwnerOnly` (opponent sees fill counts але не exact volumes) — TBD playtest |
| Worker `CarriedFerronite` | `COND_None` (over-head indicator) |
| Deposit `CurrentCapacity` | `COND_None` |

### Updated Abuse Cases

Additions to previous abuse table:

| Abuse | Mitigation |
| --- | --- |
| Client direct write to `OrbitalFerronite` | GAS attribute — server-only writes. |
| Client fake `Server_RequestOrbitalDrop` з non-owned funds | Server checks `OwnerPlayerState.OrbitalFerronite >= Cost`. |
| Client claims container Departed before server | StorageComponent state machine server-authoritative; OnRep mirrors на client. |
| Drop pod intercepted mid-flight (post-MVP) | Out of MVP. |
| Pre-pivot tests reference old `Ferronite` attribute | Migration note: rename test fixtures, update GE refs. |

### Feel / MVP Fun Maximization — Container System (Validator Pass)

Container System має **central risk-vs-greed loop**. Per 5-component rubric, кожен step циклу — visible і feel-able:

**Container fill (Clarity + Motivation):**
- HUD bottom-left під SelectionPanel: row of N container icons (per `MaxContainerCount`).
- Each container icon: vertical fill bar 0-100% із tint (warm-up effect at 50%+, glow at 90%+).
- Approach-full state (>= 80%): icon pulses softly. Player knows ship-time approaching.

**Launch sequence (Satisfaction):**
- Container Ready → Launching: subtle "armed" SFX, container icon flash white.
- 2-3 s descent telegraph (window of vulnerability):
  - Container actor у scene starts rocket VFX (engine ignite + smoke billow).
  - HUD container icon shows "launching" overlay z countdown ring.
  - SWARM AI може react (post-MVP: redirect waves).
- Launch complete: rocket lift-off VFX (vertical, fast), audio rumble.
- HUD: container icon empties, dimmed, slot reusable.

**Orbital Ferronite increment (Satisfaction + Motivation):**
- `OrbitalFerronite += Volume` → HUD `+N` flash animation на ResourceReadout (per GP-0401 feedback matrix).
- FerroniteScore similarly flashes.
- Sound: "credits-in" tone (subtle, не overlapping rocket SFX).
- Score progress bar (per delivery quota — see Win Condition): visible nudge.

**Storage Full Warning (Clarity):**
- All containers Ready / Launching simultaneously → HUD red overlay on container row + "Storage Full" toast.
- Drop-off Worker arriving: warning chime ("Your worker has nowhere to drop!").
- Resolves automatically as containers launch.

**SWARM Threat Tie-In Feedback (Motivation):**
- On Worker drop-off: `FerroniteThreatValue += N` → HUD threat bar rises (hoarding raw Ferronite at base is the danger).
- On container Departed: `FerroniteThreatValue -= Volume` → HUD threat bar visibly drops (shipping = relief).
- Player **sees the greed-vs-safety loop** visually: stockpiling raw Ferronite raises the swarm threat magnet; launching containers lowers it.

### Win Condition (Updated)

Per [`../GDD/08_Win_Lose_Conditions`](../GDD/08_Win_Lose_Conditions.md) needs update:

- **Primary:** `FerroniteScore >= DeliveryQuota` (DA-driven placeholder, e.g., 5000 units). First to hit → match ends, that player wins.
- **Timer expiry without quota:** highest `FerroniteScore` wins. Tie-break ladder unchanged.
- **Annihilation:** still possible (MainBase destroyed, no more containers can ship) — opponent wins by default. Annihilation не required для primary win.

## References

- AttributeSet schema — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- Ferronite Deposit як building — [`06_Building_Architecture`](06_Building_Architecture.md).
- GDD resource model — [`../GDD/06_Resources`](../GDD/06_Resources.md).
- Orbital Delivery GDD — [`../GDD/10_Orbital_Delivery`](../GDD/10_Orbital_Delivery.md).
- Worker mining + cargo — [`05_Unit_Architecture`](05_Unit_Architecture.md) §"Detailed Worker Unit Rules (GP-0302)".
- Main Base drop-off + container — [`06_Building_Architecture`](06_Building_Architecture.md) §"Detailed Main Base Rules (GP-0301)" (rework pending pivot).
- Orbital Delivery System — [`14_Orbital_Delivery`](14_Orbital_Delivery.md).
- Fog of War — [`15_Fog_of_War`](15_Fog_of_War.md).
- Resource task — [`../Development/Claude_Tasks/GP-0303_Resource_Primary`](../Development/Claude_Tasks/GP-0303_Resource_Primary.md).
- Core gameplay loop spec — [`../GDD/02_Core_Gameplay_Loop`](../GDD/02_Core_Gameplay_Loop.md).
- Score replication — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).
- Pillars (Pillar 3 One Resource, Pillar 4 Capacity Is Strategy) — [`../GDD/01_Game_Pillars`](../GDD/01_Game_Pillars.md).
- ADR-0002 Data-Driven First — [`../Architecture_Decisions/ADR_0002_Data_Driven_First`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md).
- ADR-0007 Building-As-Pawn (Draft) — [`../Architecture_Decisions/ADR_0007_Building_As_Pawn`](../Architecture_Decisions/ADR_0007_Building_As_Pawn.md).
