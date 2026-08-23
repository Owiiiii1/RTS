# Fog of War

## Scope

Engineering implementation of 3-level FoW (per [`../GDD/11_Fog_of_War`](../GDD/11_Fog_of_War.md)). Replaces previous "no FoW у MVP" decision (pivot 2026-05-16). Visibility grid, sight scan, replication relevance, selection/combat/drop interactions, minimap rendering.

## Current production foundations — finalized (2026-08-20)

The first production slice uses `UGP_FogOfWarComponent` as a non-replicated default subobject of
`AGP_GameState`. It owns authority-only per-team `TBitArray` storage, a 10 Hz registered-sight-source
recompute, and the public three-state query API.

Current-compatible deviations from the older pseudocode:

- no canonical map-bounds actor exists yet, so the component temporarily owns deterministic grid bounds
  (100 cm cells, origin `-100000/-100000`, dimensions `2000 x 2000`);
- active team grids are discovered from PlayerStates and registered sources; there is no `MatchTeams`
  production collection;
- sight sources register after async UnitDefinition readiness and unregister on death/EndPlay; the
  runtime does not discover every actor each update;
- FoW sight is owned only by `UGP_UnitDefinition` as `FogOfWarSightRadiusCm` and
  `bGrantsFogOfWarVision`; buildings inherit it through `UGP_BuildingDefinition::UnitDefinition`;
- combat `SightRangeCm` remains a separate auto-acquire tuning field;
- auto-acquire and server building-placement confirmation consume authority visibility;
- `UGP_LocalFoWComponent` now mirrors exactly one owning team from server-originated PlayerController
  RPC updates; it stores metadata, monotonic Explored, replaceable Visible, and a guarded revision;
- `UGP_FoWViewModel` + `UGP_FoWViewModelAdapter` provide the first push-based GPUIRuntime MVVM
  projection, with a coarse revision notification for future region-based presentation consumers;
- local building placement preview now consumes the trusted mirror conservatively while server
  confirmation remains authoritative;
- single-client transitions, same-coordinate two-player team isolation, and restart/reinitialization
  passed operator validation;
- source-only world/terrain presentation is a viewport-local **PerCellBlurredQuadRenderer**
  (`UGP_FoWWorldPresentationSubsystem` + `UGP_FoWWorldOverlayWidget` + one feathered quad tile per
  non-Visible cell). Post-process and fullscreen/sampled mask approaches are abandoned. Canonical
  gameplay grid is 100 cm / 10 Hz / 2000×2000;
- selection/inspect integration, explicit-Attack last-known behavior, DropPod sight, replication
  relevance, minimap, and the full production HUD remain later FoW slices.
- **Voxel terrain integration is NOT reopened here.** World FoW presentation was finalized against
  effectively planar terrain and a fixed ground-projection assumption. After Voxel Plugin deformation
  ships, world FoW presentation must follow the actual terrain surface. That work belongs to the
  Terrain / Voxel stage (TDD/16). FoW gameplay visibility grid stays independent of terrain rendering
  unless a later design adds terrain occlusion.

## Hard Rules

1. **Server-authoritative.** Visibility is computed server-side. The owning client receives only its
   trusted presentation ranges; actor relevance filtering is a later slice.
2. **Per-team grids.** Each team has independent Explored і Visible bitmaps. No allied vision sharing (no allies у MVP).
3. **Standard UE relevance API (future).** Later actor hiding uses `IsNetRelevantFor` /
   `bOnlyRelevantToOwner`; the trusted mirror does not itself hide replicated actors.
4. **Bit-grid storage.** Authority and local mirror use `TBitArray` internally; raw arrays are never
   replicated. Current deterministic gameplay cell size is 100 cm (10 Hz, 2000×2000, same world origin).
   Visual smoothness is a presentation-only per-cell feathered quad overlay; post-process and
   fullscreen mask paths are abandoned.
5. **No client-side FoW gameplay.** Client can render fog mask, but server arbiters all visibility-gated logic.
6. **No tick-poll у widgets.** FoW reads through `UGP_FoWViewModel` and reacts to coarse Revision
   FieldNotify (Common UI + MVVM per TDD/12).

## Data Structures

### Grid Storage

```cpp
// UGP_FogOfWarComponent (на AGP_GameState, server-side primary)
UCLASS()
class GPRUNTIME_API UGP_FogOfWarComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    /** Cell size у cm. Production default 100 cm (older snippet showed 200). */
    UPROPERTY(EditDefaultsOnly)
    int32 CellSize = 200;

    /** Grid dimensions, computed from map bounds at BeginPlay. */
    UPROPERTY(VisibleAnywhere)
    FIntPoint GridDims;

    /** Bit per cell per team. Server-only authoritative. */
    UPROPERTY()
    TMap<int32 /*TeamId*/, FGP_FoWGrid> ExploredByTeam;

    UPROPERTY()
    TMap<int32 /*TeamId*/, FGP_FoWGrid> VisibleByTeam;

    /** Public query API. */
    UFUNCTION(BlueprintPure)
    bool IsExploredByTeam(int32 TeamId, FVector WorldLoc) const;

    UFUNCTION(BlueprintPure)
    bool IsVisibleToTeam(int32 TeamId, FVector WorldLoc) const;

    /** Cell-space query. */
    bool IsCellVisibleToTeam(int32 TeamId, FIntPoint Cell) const;

protected:
    /** Server tick — recompute Visible from sight sources. */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFn) override;
};

USTRUCT()
struct FGP_FoWGrid
{
    GENERATED_BODY()
    UPROPERTY()  TBitArray<> Bits;
    FORCEINLINE int32 IndexOf(FIntPoint Cell, FIntPoint GridDims) const;
    FORCEINLINE bool Get(FIntPoint Cell, FIntPoint GridDims) const;
    FORCEINLINE void Set(FIntPoint Cell, FIntPoint GridDims, bool bValue);
    void Clear() { Bits.Init(false, Bits.Num()); }
    void UnionWith(const FGP_FoWGrid& Other);
};
```

Replication: server does **not** replicate raw `Bits` arrays. The authoritative component exposes a
read-only one-team extraction API. Each owning PlayerController receives only its TeamId's compressed
row-major cell ranges. Actor relevance filtering remains a later networking slice.

### Per-Client State (Local Mirror)

```cpp
// UGP_LocalFoWComponent (on AGP_PlayerController, presentation-only)
UCLASS()
class GPRUNTIME_API UGP_LocalFoWComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    EGP_FoWState GetStateAtWorldLocation(FVector WorldLoc) const;
    bool IsExplored(FVector WorldLoc) const;
    bool IsVisible(FVector WorldLoc) const;
    bool IsReady() const;
    int32 GetLocalTeamId() const;
    int64 GetRevision() const;
};
```

Protocol:

- initial reliable owner RPC: metadata + full current Explored/Visible ranges + revision;
- ongoing reliable owner RPC at changed authoritative recomputes: newly Explored ranges + compact full
  current Visible ranges + monotonically increasing revision;
- stale/duplicate revisions and invalid ranges are rejected before mutation;
- team/PlayerState/travel reset clears the old mirror; reconnect receives a complete initial snapshot;
- no client-to-server FoW mutation RPC exists, and no mirror API accepts arbitrary TeamId.

The full current Visible set makes removals deterministic without tombstones. Explored transmits only
additions after initial sync. Contiguous range compression avoids sending a 4-million-cell bitmap and
fits the current circle-source topology; there is no per-frame RPC or widget polling.

Payload facts:

- the grid is bounded to 4,000,000 cells (2000×2000 at 100 cm);
- each run stores two `int32` values (8 bytes before RPC serialization overhead);
- one isolated circular source normally contributes at most one run per intersected row
  (`2 * ceil(Radius / CellSize) + 1` before overlap merging);
- there is no separate chunk/byte cap; highly fragmented large reconnect snapshots remain a future
  scalability concern outside the validated MVP match scale.

### World / Terrain Visualization

`UGP_FoWWorldPresentationSubsystem` is one `ULocalPlayerSubsystem` per local player in `GPUIRuntime`.
It binds only to that controller's `UGP_LocalFoWComponent` and drives a hit-test-invisible Slate
overlay (`UGP_FoWWorldOverlayWidget`). Renderer name: **PerCellBlurredQuadRenderer**.
`PostProcessActive=false`. `MaskProjectionActive=false`.

Operator rejected the sampled/projected mask overlay (left-side striping) and the earlier post-process
experiment. SDF / contour / Chaikin / marching-squares remain abandoned. Current temporary MVP stop:

- gameplay grid: CellSize=100 cm, Dims=2000×2000, Interval=0.10 s;
- visual: one world-space quad tile per Unexplored/Explored cell in the current view;
- Visible cells are not drawn;
- each tile has a neighbor-aware feathered edge so adjacent tiles blend;
- the look remains cell-based; cells are 100 cm with soft edges;
- no post-process material, no world-position reconstruction, no fullscreen mask, no coalesced strip surface.

Bounds:

- sampled gameplay cells capped at 65536;
- overlay quads capped at 262144;
- the visible FoW region is never cropped to fit the cap; if the full viewport region exceeds the
  sampled-cell cap, that frame uses conservative full-black fallback;
- no full-world 2000×2000 allocation;
- no cell UObject/component allocation.

Enemy world presentation remains a separate temporary local gate, not replication relevance:

- each `AGP_UnitBase` registers with `UGP_LocalFoWUnitPresentationSubsystem`; no `TActorIterator` or
  whole-world discovery is used;
- LocalFoW revisions trigger immediate registered-list evaluation;
- a 10 Hz bounded registered-list pass catches replicated actor movement across unchanged FoW;
- own-team actors remain presented; cross-team actors are locally presented only while their current
  location is `Visible` on **that local client's** `UGP_LocalFoWComponent`;
- local unit FoW presentation must never use Actor-level replicated/shared hidden state
  (`SetActorHiddenInGame` / `AActor::bHidden`). Each client gates owned visual
  `UPrimitiveComponent`s (including Blueprint-added meshes) with local `SetHiddenInGame`,
  restoring the component's original hidden flag when shown;
- health and combat presentation keep explicit local gates; team tint is refreshed on become-visible;
- actor replication/relevancy is unchanged. This is listen-server/multiplayer presentation
  correctness, not FoW network secrecy or relevancy culling. FoW world visualization is a
  separate overlay path and was not this coupling bug;
- operator-validated in a 2-player PIE/network session: each client's world overlay stayed
  independent; Player 2 saw enemies from Player 2 FoW only; Player 1 visibility no longer
  revealed/hid units for Player 2; own-team units stayed visible;
- `UGP_HealthBarComponent` composes `owner/death && LocalFoW && damaged-health`: full health (within
  `max(KINDA_SMALL_NUMBER, abs(MaxHealth) * 1e-4)`), zero, and dead are hidden; only damaged living
  actors may show a bar.

This local gate does not destroy actors, change collision, mutate gameplay, or alter replication.
Enemy actors still replicate. `IsNetRelevantFor` filtering and last-known snapshots remain deferred.

### Sight Source Contract

Actors з sight contribute via interface OR property:

```cpp
// In UGP_UnitDefinition. Buildings resolve this through BuildingDefinition.UnitDefinition.
UPROPERTY(EditAnywhere, Category = "GP|Vision")
float FogOfWarSightRadiusCm = 900.f; // cm

UPROPERTY(EditAnywhere, Category = "GP|Vision")
bool bGrantsFogOfWarVision = true;   // explicit flag
```

`AGP_UnitBase::GrantsFogOfWarVision()` requires definition readiness,
`bGrantsFogOfWarVision && FogOfWarSightRadiusCm > 0`, and a live actor.

## Sight Tick (Server)

`UGP_FogOfWarComponent::TickComponent` (10 Hz server tick):

```
for each TeamId у MatchTeams:
    NewVisible = empty grid

    for each Actor у World з IsSightSource() && Actor.TeamId == TeamId:
        Center = Actor.Location → cell
        Radius_cells = ceil(Actor.SightRadius / CellSize)
        for each Cell within radius (circle iteration):
            NewVisible.Set(Cell, true)
            if !ExploredByTeam[TeamId].Get(Cell):
                ExploredByTeam[TeamId].Set(Cell, true)
                NewlyExploredCells[TeamId].Add(CellIndex)

    VisibleByTeam[TeamId] = NewVisible

for each changed TeamId:
    increment TeamGrid.Revision
    broadcast OnTeamStateChanged(TeamId, Revision)

each authoritative PlayerController subscribed for its owning TeamId:
    initial: send metadata + all Explored runs + current Visible runs
    update: send newly Explored runs + current Visible runs
```

DropPod temporary sight is a deferred target; current in-flight pods do not contribute FoW vision.

## Replication Relevance — target design, not implemented

Override на key actor types:

```cpp
// AGP_UnitBase
virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
                              const FVector& SrcLocation) const override
{
    // Always relevant to owner team
    if (const APlayerController* PC = Cast<APlayerController>(RealViewer))
    {
        if (const AGP_PlayerState* PS = PC->GetPlayerState<AGP_PlayerState>())
        {
            if (PS->GetTeamId() == TeamId) return true;
            // Cross-team: relevant only if currently visible to viewer's team
            return GetFoWComponent()->IsVisibleToTeam(PS->GetTeamId(), GetActorLocation());
        }
    }
    return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}
```

This remains the target connection-level filtering model. It is not part of either finalized foundation;
hidden enemy actors may still replicate until the dedicated relevance/last-known slice.

**Buildings:** `IsNetRelevantFor` similar, але once explored, last-known state persists локально (no unrelevance once seen — buildings static, OK to keep).

**FerroniteDeposits:** same as buildings.

## Selection / Inspect Interaction — target design, not implemented

- `UGP_SelectionComponent::TrySelectAt(FVector Loc)`:
  - after relevance filtering exists, line traces should only hit relevant/actively-visible enemies;
  - until then, future local selection/inspect presentation must consult the trusted mirror explicitly.
- `Inspect` target rule: only a `Visible` enemy can be inspected.
- Control group target rule: hidden members keep identity but present unknown position/grey state until
  re-sighted. This behavior is not implemented by the current foundations.

## Combat Interaction (Updates to GP-0204)

`UGP_TargetingComponent::FindClosestEnemy`:

```
candidates = OverlapSphere(AcquireRange) filter by:
    - Visible OR Owner-team
    - Not Dead
    ...
```

`OverlapSphere` returns only actors relevant to attacker's team — i.e., server tick on attacker (which is server-side) sees all candidates regardless of FoW. But filter checks `IsVisibleToTeam(AttackerTeamId, candidate.Location)`. Auto-acquire `false` if hidden.

Explicit attack on hidden enemy: command persists, attacker moves to last-known. On reach, evaluate visibility again.

## Drop Targeting Interaction (Updates to TDD/14)

Drop validation step 3:
```
IsVisibleToTeam(OwnerTeamId, LandingLoc) → required if DropDef.bRequiresActiveVisibility (default true).
```

Drop reticle on client:
- Mouse raycast hits ground.
- Query `UGP_LocalFoWComponent::IsVisibleToTeam(LocalTeam, HitLoc)`.
- Tint reticle accordingly.

## Minimap Rendering (Per TDD/12 Update)

`UGP_MinimapSubsystem` snapshot uses 3 layers:

```cpp
struct FGP_MinimapCellRender
{
    EGP_FoWState State;            // Unexplored / Explored / Visible
    FColor TerrainTint;
    TArray<FGP_MinimapBlip> Blips;
};
```

- Unexplored: black overlay (full alpha).
- Explored: dim grey overlay (~0.5 alpha) + frozen blips for last-known static actors.
- Visible: no overlay + live blips.

Blip fading rules (per [`../GDD/11_Fog_of_War`](../GDD/11_Fog_of_War.md)):
- Dynamic actors: fade out 5 s after `Visible → Explored` transition.
- Static actors: persistent у Explored.

`UGP_MinimapVM` extended:

```cpp
UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
TArray<FGP_MinimapCellRender> Cells;
```

VM Adapter polls `UGP_LocalFoWComponent` snapshot at 10 Hz (matches sight tick).

## Tag Surface

```
GP.FoW.Unexplored            // cell state tags (for queries, not on actors)
GP.FoW.Explored
GP.FoW.Visible

GP.Capability.GrantsVision   // marks actors that contribute vision
GP.Capability.AlwaysVisible  // reserved post-MVP for special landmarks
```

## DataAsset Refs

| DataAsset | Field | Type |
| --- | --- | --- |
| `UGP_UnitDefinition` | `FogOfWarSightRadiusCm`, `bGrantsFogOfWarVision` | scalars / bool |
| `UGP_BuildingDefinition` | `UnitDefinition` bridge | soft definition ref |
| `UGP_OrbitalDropDefinition` | `bPodGrantsVision` | bool — deferred after runtime foundation |
| `UGP_FactionDefinition` | `InitialFoWReveal` | optional initial-explored radius around landing |

Per [`ADR-0002`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md) — all values are DA-driven, soft refs only.

## Anti-Patterns

- ❌ Replicating raw FoW grid arrays.
- ❌ Per-frame visibility scan на all-vs-all.
- ❌ Client decides visibility — server-only.
- ❌ Last-known state writes to live state.
- ❌ Ordinary HUD widget code calling FoW authority/mirror directly — use `UGP_FoWViewModel`.
- ✅ The dedicated world FoW overlay adapter may read its one bound `UGP_LocalFoWComponent`; it is
  the project-owned presentation adapter, not gameplay authority or a general HUD widget.
- ❌ Removing `Explored` flag once set (no re-fog у MVP).

## Performance Budget

- 10 Hz sight tick × O(units × area_cells_covered). With 50 units and 100 cm cells, per-source coverage
  is 4× denser than the former 200 cm grid; authority work stays circle-fill, not a full 4M scan.
- Relevance check called by engine per actor per client. Cheap (per-cell bit query + team check).
- Multicast and replication budget unchanged from existing.
- World FoW presentation: viewport-local per-cell feathered quads (max 65536 sampled cells, 262144
  overlay quads). The visible region is never cropped; over-cap frames fall back to full black.

## Validation per Pillars

**Pillar 8 (Simple Core):**
- 1-2 sentence: "Three layers: black (never seen), grey (seen once but not now), full (currently looking)."
- Fun у v1: yes — scout fantasy + ambush.
- New decision: vision pyramid investment, pre-attack reveal, scouting cost.
- Cheap: standard bit-grid + UE relevance API.
- Scales via content: more sight sources via DataAsset `SightRadius`.

**Pillar 9 (Technical):** server-authoritative ✓, data-driven ✓ (sight radii via DA), GAS-friendly (no GAS state needed for FoW itself).

## Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Initial state | Map starts black except small bubble around each MainBase + Workers. |
| 2 | Worker explore | Move Worker outward → cells transition Unexplored → Visible → Explored on retreat. |
| 3 | Enemy detection | Enemy unit enters own Visible bubble → suddenly appears на client. |
| 4 | Enemy hide | Enemy unit moves out of Visible → disappears from local scene; last-known marker fades 5 s on minimap. |
| 5 | Building last-known | Enemy MainBase у Explored zone → still rendered (frozen damage state). Re-explore → live state. |
| 6 | Cheat resistance | Packet sniff confirms hidden enemy actors NOT replicated to opponent. |
| 7 | Drop zone gating | Try drop on Explored — red reticle, reject. Visible — green, allow. |
| 8 | Inspect hidden | Enemy goes hidden during inspect → InspectPanel auto-clears. |
| 9 | Combat re-engage | Attack target → target hides → attacker moves to last-known → target re-emerges → re-engage. |
| 10 | Minimap layers | All 3 layers rendered correctly з proper alpha. |
| 11 | Drop pod telegraph | Pod adds temporary vision at landing site (DropDef.bPodGrantsVision = true). Pod destroyed → vision contribution gone. |
| 12 | Performance | 50 actors, 10 Hz sight tick, no frame-time spike > 1 ms server-side. |

## Out of MVP

- Allied vision sharing.
- Stealth units / cloak abilities.
- Scanner reveal abilities (ping reveal).
- Always-visible landmarks.
- Fog re-application (visibility downgrade Explored → Unexplored).
- Vision-cone direction (omni 360° у MVP).
- Height / elevation vision effects.
- High-ground vision bonus.
- Reopening the current planar world-FoW overlay in this FoW stage (terrain-surface adaptation is a Terrain/Voxel integration task, TDD/16).

## References

- Fog of War GDD — [`../GDD/11_Fog_of_War`](../GDD/11_Fog_of_War.md).
- Selection rules (FoW interactions) — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md).
- Combat (LOS + FoW filter) — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md) §Detailed Attack Command Rules.
- Orbital Delivery (drop zone gating) — [`14_Orbital_Delivery`](14_Orbital_Delivery.md).
- Voxel terrain / FoW surface integration (later) — [`16_Voxel_Terrain_And_Foundations`](16_Voxel_Terrain_And_Foundations.md).
- UI / MVVM (minimap, FoW VM) — [`12_UI_Architecture`](12_UI_Architecture.md).
- Multiplayer relevance — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).
- Data assets — [`10_Data_Assets`](10_Data_Assets.md), [`../Architecture_Decisions/ADR_0002_Data_Driven_First`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md).
