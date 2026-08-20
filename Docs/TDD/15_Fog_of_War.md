# Fog of War

## Scope

Engineering implementation of 3-level FoW (per [`../GDD/11_Fog_of_War`](../GDD/11_Fog_of_War.md)). Replaces previous "no FoW у MVP" decision (pivot 2026-05-16). Visibility grid, sight scan, replication relevance, selection/combat/drop interactions, minimap rendering.

## Current production foundations — finalized (2026-08-20)

The first production slice uses `UGP_FogOfWarComponent` as a non-replicated default subobject of
`AGP_GameState`. It owns authority-only per-team `TBitArray` storage, a 5 Hz registered-sight-source
recompute, and the public three-state query API.

Current-compatible deviations from the older pseudocode:

- no canonical map-bounds actor exists yet, so the component temporarily owns deterministic grid bounds
  (200 cm cells, origin `-100000/-100000`, dimensions `1000 x 1000`);
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
- source-only world/terrain presentation uses a per-LocalPlayer 1000² packed Known/Visible
  post-process mask (GPU 9-tap + 0.20 s GPU temporal lerp, one upload per revision) after an operator
  FAIL of RenderTarget-pointer binding and CPU million-sample filtering; gameplay FoW remains 200 cm /
  5 Hz; a 50 cm / 10 Hz grid change is deferred;
- selection/inspect integration, explicit-Attack last-known behavior, DropPod sight, replication
  relevance, minimap, and the full production HUD remain later FoW slices.

## Hard Rules

1. **Server-authoritative.** Visibility is computed server-side. The owning client receives only its
   trusted presentation ranges; actor relevance filtering is a later slice.
2. **Per-team grids.** Each team has independent Explored і Visible bitmaps. No allied vision sharing (no allies у MVP).
3. **Standard UE relevance API (future).** Later actor hiding uses `IsNetRelevantFor` /
   `bOnlyRelevantToOwner`; the trusted mirror does not itself hide replicated actors.
4. **Bit-grid storage.** Authority and local mirror use `TBitArray` internally; raw arrays are never
   replicated. Current deterministic gameplay cell size remains 200 cm (5 Hz). Visual smoothness is a
   presentation-only post-process texture; 50 cm / 10 Hz is not approved yet.
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
    /** Cell size у cm. DA-driven, default 200 cm. */
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
additions after initial sync. Contiguous range compression avoids sending a one-million-cell bitmap and
fits the current circle-source topology; there is no per-frame RPC or widget polling.

Payload facts:

- the grid is bounded to 1,000,000 cells;
- each run stores two `int32` values (8 bytes before RPC serialization overhead);
- one isolated circular source normally contributes at most one run per intersected row
  (`2 * ceil(Radius / CellSize) + 1` before overlap merging);
- there is no separate chunk/byte cap; an alternating-cell theoretical worst case is 500,000 runs
  (~4 MB before serialization), so highly fragmented large reconnect snapshots remain a future
  scalability concern outside the validated MVP match scale.

### World / Terrain Visualization

`UGP_FoWWorldPresentationSubsystem` is one `ULocalPlayerSubsystem` per local player in `GPUIRuntime`.
It binds only to that controller's `UGP_LocalFoWComponent` and injects a per-view post-process
blendable. The Slate/SDF/contour/raster overlay path was removed.

Current MVP rendering method:

- on LocalFoW revision, bulk-extract a packed 1000×1000 BGRA8 mask (one texel per gameplay cell;
  R=Known, G=Visible) without 1M world-location queries;
- ping-pong Previous/Target GPU textures and upload the new target only; lerp in the material over 0.20 s;
- GPU bilinear + 9-tap mask sampling (`FoWMaskTexelSize`, `FoWBlurRadiusTexels`);
- Unexplored: black; Explored: SceneColor × 0.35; Visible: unchanged SceneColor;
- NotReady forces `FoWReady=0` (full black);
- local-view injection uses PlayerIndex / ViewActor, not RenderTarget pointer identity;
- scene-pixel world XY is reconstructed from SceneDepth + SvPosition, not AbsoluteWorldPosition;
- camera pan/zoom/yaw does not rebuild the mask;
- each LocalPlayer owns distinct textures and a unique MID (no shared PostProcessVolume).

Bounds:

- presentation texture is independent of the 1000×1000 gameplay grid;
- 2 GPU textures × 4,194,304 bytes plus CPU float working buffers;
- rebuild/upload only on LocalFoW revision, not per frame;
- no cell UObject/component allocation;
- old Slate world renderer is inactive.

Gameplay FoW remains 200 cm / 5 Hz. A 50 cm / 10 Hz grid change is deferred until this renderer is
operator-evaluated. The current arena contains no Landscape and uses planar blockout ground.

No material, render target, map, Blueprint, or global Material Parameter Collection is required. This
keeps masks isolated per LocalPlayer/listen client and avoids global team-state leakage.

Enemy world presentation is a separate temporary local gate, not replication relevance:

- each `AGP_UnitBase` registers with `UGP_LocalFoWUnitPresentationSubsystem`; no `TActorIterator` or
  whole-world discovery is used;
- LocalFoW revisions trigger immediate registered-list evaluation;
- a 10 Hz bounded registered-list pass catches replicated actor movement across unchanged FoW;
- own-team actors remain presented; cross-team actors are locally presented only while their current
  location is `Visible`;
- actor hidden-in-game state covers authored/generated primitives and team tint, while health and
  combat presentation have explicit local gates;
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

`UGP_FogOfWarComponent::TickComponent` (5 Hz server tick):

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

VM Adapter polls `UGP_LocalFoWComponent` snapshot at 5 Hz (matches sight tick).

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
- ✅ The dedicated world FoW post-process adapter may read its one bound `UGP_LocalFoWComponent`; it is
  the project-owned presentation adapter, not gameplay authority or a general HUD widget.
- ❌ Removing `Explored` flag once set (no re-fog у MVP).

## Performance Budget

- 5 Hz sight tick × O(units × area_cells_covered). With 50 units × ~100 cells average = 25k cell ops/sec — acceptable.
- Relevance check called by engine per actor per client. Cheap (per-cell bit query + team check).
- Multicast and replication budget unchanged from existing.
- World FoW presentation: 1000² packed mask textures per LocalPlayer, GPU 9-tap + 0.20 s GPU lerp,
  one after-tonemap post-process pass, one target upload per LocalFoW revision. Camera motion does not
  rebuild the mask.

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
| 12 | Performance | 50 actors, 5 Hz sight tick, no frame-time spike > 1 ms server-side. |

## Out of MVP

- Allied vision sharing.
- Stealth units / cloak abilities.
- Scanner reveal abilities (ping reveal).
- Always-visible landmarks.
- Fog re-application (visibility downgrade Explored → Unexplored).
- Vision-cone direction (omni 360° у MVP).
- Height / elevation vision effects.
- High-ground vision bonus.

## References

- Fog of War GDD — [`../GDD/11_Fog_of_War`](../GDD/11_Fog_of_War.md).
- Selection rules (FoW interactions) — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md).
- Combat (LOS + FoW filter) — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md) §Detailed Attack Command Rules.
- Orbital Delivery (drop zone gating) — [`14_Orbital_Delivery`](14_Orbital_Delivery.md).
- UI / MVVM (minimap, FoW VM) — [`12_UI_Architecture`](12_UI_Architecture.md).
- Multiplayer relevance — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).
- Data assets — [`10_Data_Assets`](10_Data_Assets.md), [`../Architecture_Decisions/ADR_0002_Data_Driven_First`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md).
