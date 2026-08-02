# GP-S16 UnitBase Prerequisite
(Minimal compile-safe `AGP_UnitBase` UCLASS scaffold)

## Status
**DONE**

This is a **dependency prerequisite**, not completion of GP-S18.  
No GP-S18 acceptance criteria are claimed.  
Future GP-S18 work may extend or revise UnitBase behavior through a separate reviewed stage.

## Purpose

Provide a real UHT-visible `UCLASS` `AGP_UnitBase` so future `UGP_SelectionComponent` can declare canonical reflected storage:

```cpp
TArray<TWeakObjectPtr<AGP_UnitBase>> SelectedUnits;
```

The class exists **only** as a real abstract type. It does **not** implement gameplay UnitBase behavior.

## Why prerequisite exists

- GP-S16 specification requires typed storage `TArray<TWeakObjectPtr<AGP_UnitBase>>`.
- Forward declaration of a missing UCLASS is insufficient for UHT-reflected `UPROPERTY`.
- `AActor` fallback was rejected as non-canonical.
- Fake interface / placeholder duplicate classes are forbidden.
- Full GP-S18 is too large to pull in only to unblock selection typing.

## Exact scope

| Item | In scope |
| --- | --- |
| Files | `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`, `.../Private/Units/GPUnitBase.cpp` |
| Class | `UCLASS(Abstract, Blueprintable) AGP_UnitBase : public APawn` |
| API | Constructor only |
| Module | `GPRuntime` |
| Build.cs | **Unchanged** |

## Constructor policy

```cpp
PrimaryActorTick.bCanEverTick = false;
bReplicates = true;
SetReplicateMovement(true);
```

Rationale:

- Canonical UnitBase is a replicated Pawn (TDD/13).
- Full movement/replication policy is refined in GP-S18.
- Scaffold has no own tick and no gameplay behavior.

**Not added:** `BeginPlay`, `Tick`, `PossessedBy`, `OnRep`, `GetLifetimeReplicatedProps`, RPC, replicated properties.

## Explicit non-goals

No ASC, AttributeSet, ASI, TeamId, OwningPlayerState, UnitTags, UnitDefinition, BuildingDefinition, CapabilityTags, meshes, scene/root components, collision, movement component, health/death, `OnDeath`, `SetSelectionHighlight`, MID/materials, commands, selection APIs, Gameplay Tags usage, Data Assets, AIController, BuildingBase/MobileUnit/Worker, test BP/map placement.

No changes to PlayerController, Camera, GameMode, PlayerState, GameState, GPGASRuntime, config, maps, assets, GP-S16 SelectionComponent spec status, or TDD implementation order docs.

## Relationship to GP-S16

- Unlocks compile-safe typed selection storage declaration.
- **GP-S16 SelectionComponent is still not implemented.**
- After this prerequisite is merged into `main`, return to GP-S16 code assignment.

## Relationship to full GP-S18

- This prerequisite does **not** authorize full GP-S18.
- Further expansion of `AGP_UnitBase` (ASC, highlight MID, OnDeath, definitions, filtering contracts) happens **only** in a separate GP-S18 task.
- GP-S17 is **not** started by this work.

## Build validation

| Target | Result |
| --- | --- |
| GPEditor Win64 Development | **PASSED** |
| GP Win64 Development | **PASSED** |
| GP Win64 Shipping | **PASSED** |
| UHT (`GPUnitBase.gen.cpp`) | **PASSED** |

No Blueprint / map required for scaffold validation.

## Acceptance criteria

- [x] Real UCLASS exists (`AGP_UnitBase`)
- [x] Abstract Blueprintable APawn
- [x] Exported by `GPRuntime` (`GPRUNTIME_API`)
- [x] No actor tick
- [x] Replicated pawn shell (`bReplicates = true`)
- [x] Movement replication enabled (`SetReplicateMovement(true)`)
- [x] No custom replication properties
- [x] No gameplay behavior
- [x] UHT passed
- [x] GPEditor Development passed
- [x] GP Development passed
- [x] GP Shipping passed
- [x] No Build.cs changes
- [x] No assets/config/maps
- [x] GP-S16 code not started
- [x] GP-S17 not started
- [x] Full GP-S18 not started

## Stop Condition

Prerequisite closed as **DONE**. This is a dependency prerequisite, not completion of GP-S18. Do **not** implement `UGP_SelectionComponent` on this branch. Do **not** start GP-S17 or full GP-S18. After merge to `main`, GP-S16 may be unblocked for typed SelectionComponent implementation under a separate assignment.
