# GP-S16 Selectable UnitBase Prerequisite
(TeamId + interim CapabilityTags + concrete `AGP_Unit`)

## Status
**Status: READY_FOR_IMPLEMENTATION**

Architecture checkpoint **approved**. Implementation **not started** in this documentation pass.
This is **not** full GP-S18 and **not** GP-S16 Phase B2.

Parent GP-S16 selection status:
**`PHASE_B1_DONE_UNITBASE_PREREQUISITE_READY`**

---

## Relationship

| Stage | State |
| --- | --- |
| Compile-safe UnitBase scaffold | **DONE** (abstract empty `AGP_UnitBase`) |
| GP-S16 Phase A / B1 | **DONE** (selection container + mutation API) |
| This prerequisite | **Architecture approved → READY_FOR_IMPLEMENTATION** |
| GP-S16 Phase B2 | Blocked until this prerequisite code is **merged and validated** |
| Full GP-S18 | Separate future stage |
| GP-S17 | Not started |

---

## Goal

Minimum canonical contract so future selection input can:

- recognize a selectable unit actor;
- read actor team and local player team;
- select friendly selectable units;
- inspect enemy/neutral inspectable actors;
- place a concrete operator-testable actor in PIE;
- without claiming full GP-S18.

---

## Approved team contract

### `AGP_PlayerState` (future implementation)

```cpp
UPROPERTY(ReplicatedUsing = OnRep_TeamId)
int32 TeamId = -1;

int32 GetTeamId() const;
void SetTeamId(int32 NewTeamId);
```

Rules:

- `-1` = unassigned
- `0` = neutral
- `1+` = playable teams
- Server-authoritative setter
- Replicated to clients
- No RPC required in this prerequisite
- `OnRep_TeamId` may be empty/native hook unless a real listener is required
- Do **not** use `WinnerTeamId` as player team
- Do **not** derive unit team solely from possessed PlayerState

### `AGP_UnitBase` (future implementation)

```cpp
UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_TeamId, Category = "GP|Team")
int32 TeamId = -1;

int32 GetTeamId() const;
void SetTeamId(int32 NewTeamId);
bool IsNeutral() const;
bool HasAssignedTeam() const;
```

Rules:

- Server-authoritative runtime setter
- Editor instance default allowed for operator validation
- Replicated with `COND_None`
- Client selection reads locally
- No RPC
- No owning connection requirement
- No dependency on possession

---

## Approved capability contract (interim)

### Property on `AGP_UnitBase`

```cpp
UPROPERTY(EditDefaultsOnly, Category = "GP|Selection")
FGameplayTagContainer CapabilityTags;
```

### Public read / query API

```cpp
const FGameplayTagContainer& GetCapabilityTags() const;
bool HasCapabilityTag(FGameplayTag CapabilityTag) const;
bool IsSelectable() const;
bool IsInspectable() const;
bool IsSelectionTypeUnit() const;
bool IsSelectionTypeBuilding() const;
```

Queries use registered native tags:

- `GP.Capability.Selectable`
- `GP.Capability.Inspectable`
- `GP.Selection.Type.Unit`
- `GP.Selection.Type.Building`

### CapabilityTags rules

- CDO / class defaults only
- **Not** replicated in this prerequisite
- No runtime mutation API
- No Blueprint write API
- No ASC ownership
- No ability tags

### Interim → UnitDefinition migration boundary

- CapabilityTags on UnitBase is an **INTERIM** prerequisite contract
- Future **UnitDefinition** becomes the canonical source
- UnitBase query methods remain the **stable consumer API**
- Later implementation must **not** keep two independent editable sources
- Migration to UnitDefinition is **separate reviewed work**
- This checkpoint does **not** claim completion of definition architecture

---

## Eligibility responsibility lock

| Actor | Responsibility |
| --- | --- |
| `AGP_UnitBase` | Exposes team and capability **facts** / queries |
| `AGP_PlayerState` | Exposes local player team |
| `AGP_PlayerController` / future input policy | Friendly/enemy/neutral comparison; select vs inspect; future FoW gate |
| `UGP_SelectionComponent` | Canonicalizes and stores selection only |
| PlayerController | Must **not** mutate `SelectedUnits` arrays directly |
| Phase B1 mutation API | Receives **already validated** units |

Conceptual future rule (**do not implement in this checkpoint**):

```text
Friendly + Selectable      → SelectedUnits
Enemy/Neutral + Inspectable → InspectedTarget
Otherwise                   → no selection mutation
```

---

## Concrete actor lock — `AGP_Unit`

```cpp
class AGP_Unit : public AGP_UnitBase
```

Purpose:

- First **production-oriented** concrete unit actor
- Placeable in Editor
- Usable by future gameplay code
- **Not** named `TestUnit`
- **Not** validation-only disposable code

Keep `AGP_UnitBase` **Abstract**.

### Minimum future components

- `UCapsuleComponent` as `RootComponent`
- Simple visible primitive for PIE visibility
- Collision responding to **Visibility** trace
- No tick
- Inherited replication
- No custom movement component
- No navigation / pathfinding
- No production mesh or animation

### Preferred visible primitive (implementation chooses exactly one)

1. `UStaticMeshComponent` using an engine / basic shape asset, **or**
2. Another compile-safe built-in primitive approach

The implementation task must pick **one** production-safe option and document it.
Do **not** create Content assets in this architecture checkpoint.

### Default CapabilityTags (CDO)

`{ GP.Capability.Selectable, GP.Capability.Inspectable, GP.Selection.Type.Unit }`

---

## Operator validation lock (after implementation)

Must prove:

1. `AGP_Unit` can be placed in a map
2. It is visible in PIE
3. Visibility cursor trace can hit it
4. `TeamId` can be configured per instance
5. `TeamId` replicates in 2-player listen-server PIE
6. Capability queries return expected defaults
7. **No** selection input required yet

Allowed signals:

- Output Log
- Console / debug command
- Actor details + trace logging
- Narrowly scoped temporary Development logging

Do **not** require highlight or UI.

Any temporary validation logging must be production-appropriate diagnostic logging **or** removed before checkpoint commit.

---

## Fog of War boundary

- No FoW implementation
- No `bVisibleToTeam` on UnitBase
- No permanent visibility bool
- Phase B2 temporary assumption: **all relevant actors are visible**
- Future FoW policy adds eligibility gate **before** selection mutation
- Behavior when a selected actor becomes hidden remains **deferred**

---

## Exact future implementation files

### Modify

- `GP/Source/GPRuntime/Public/Player/GPPlayerState.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerState.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `Docs/Development/Claude_Tasks/GP-S16_Selectable_UnitBase_Prerequisite.md`
- `Docs/Development/Claude_Tasks/GP-S16_Selection_Component.md`
- `Docs/Development/AI_Project_Log.md`

### Create

- `GP/Source/GPRuntime/Public/Units/GPUnit.h`
- `GP/Source/GPRuntime/Private/Units/GPUnit.cpp`

### Build.cs

Potentially **no** change — `GPRuntime` already depends on `GameplayTags` and `GPGASRuntime`.

### Assets / maps

No assets or map changes during initial code pass unless separately approved.

---

## Explicit exclusions

Do **not** include:

- ASC / attribute sets
- Health / death / `OnDeath`
- UnitDefinition
- Abilities
- Commands
- Movement component
- AI controller
- Navigation / pathfinding
- Production mesh / animation
- Highlight MID / selection visual
- Fog of War
- Building subclass
- Spawning
- Save / persistence
- Input assets
- Cursor selection / marquee resolution
- GP-S17
- Full GP-S18

---

## Analysis background (summary)

Verified before approval:

- No TeamId on PlayerState / UnitBase today
- No `FGenericTeamId` / `IGenericTeamAgentInterface` usage
- Capability / selection tags already registered in `FGPGameplayTags`
- TDD long-term CapabilityTags home = UnitDefinition (missing)
- SelectionComponent must not authorize eligibility

Rejected paths: GenericTeam-only; team solely via possession/PlayerState owner; bool-only capability flags; temporary `AGP_TestUnit`; validation-only Blueprint as the sole concrete path.

---

## Stop condition

**READY_FOR_IMPLEMENTATION.** Documentation checkpoint only.
Do **not** write C++ / create assets / change maps in this pass.
Await explicit tech-lead **implementation** assignment.
Do **not** start Phase B2 / GP-S17 / full GP-S18 here.
