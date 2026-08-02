# GP-S17 — UGP_CommandComponent
(BuildSmartCommand + Server_RequestCommand dispatch — analysis)

## Slice Group
Slice 4 — Selection + Smart Commands (TDD/13: after GP-S16; before full GP-S18 / GP-S19 as currently ordered)

## Code Allowed
**No** on this analysis pass. Documentation only.

## Asset Changes Allowed
**No** on this analysis pass.

## Depends On
- GP-S16 **DONE_WITH_DEFERRED_INTEGRATIONS** (selection state + click/marquee/control-group input)
- TDD/04 command pipeline, TDD/09 tags, TDD/12 `IMC_GP_Commands`, TDD/13 ownership + RPC inventory
- Native `GP.Command.*` tags already registered (GP-S02)

## Goal (original TDD/13)
`UGP_CommandComponent`: smart-command resolution (`BuildSmartCommand`) and `Server_RequestCommand` dispatch.

Expanded from TDD/04:

- Local intent from input → `FGP_CommandRequest`
- Smart RMB: ground→Move, enemy→Attack, resource→Mine, owned repairable→Repair
- PC `Server_RequestCommand` validates ownership / applicability
- Server `ExecuteServerCommand` → `AGP_UnitBase::ReceiveCommand` → behavior components
- **Not** client-authoritative movement

## Status
**Status: ANALYSIS_READY_FIRST_CHECKPOINT_PENDING**

First implementation checkpoint (locked): **Phase A — CommandComponent shell**.

GP-S17 implementation **not started**.
GP-S18 / GP-S19 **not started** (names unchanged; not begun).
No code / assets / maps / config changed in this analysis finalize.

---

## 1. Current repository facts

### Existing GP-S17 artifacts
| Artifact | State |
| --- | --- |
| `UGP_CommandComponent` | **Absent** |
| `FGP_CommandRequest` | **Absent** |
| GP-S17 Claude task file | **Created by this analysis** |
| `IA_Command` / `IMC_GP_Commands` | **Absent** |
| `Server_RequestCommand` / `Client_NotifyCommandRejected` | **Absent** |
| `AGP_UnitBase::ReceiveCommand` | **Absent** |
| Unit `AIController` / `UGP_MovementComponent` | **Absent** |

### PlayerController (`AGP_PlayerController`)
| Fact | State |
| --- | --- |
| Hosts SelectionComponent | Yes (`CreateDefaultSubobject` + getter) |
| Hosts CommandComponent | **No** |
| Input IMCs | Camera (100), Selection (110) only |
| LMB click / marquee lifecycle | Present (local-only) |
| RMB / command input | **None** — safe to add later via separate IMC (do not pollute Camera/Selection) |
| Local-controller lifecycle | BeginPlayingState / EndPlay / IsLocalController patterns exist |
| RPCs | **None** on PC today |

### Selection (`UGP_SelectionComponent`)
| Fact | State |
| --- | --- |
| Local-only / no replication / no RPC | Yes |
| Read API | `GetSelectedUnits`, `GetSelectionCount`, `HasSelection`, `IsUnitSelected`, inspect accessors |
| Cap 24 + weak refs + prune | Yes |
| Mutation by CommandComponent | **Forbidden** — CommandComponent must only read |

### Units
| Fact | `AGP_UnitBase` | `AGP_Unit` |
| --- | --- | --- |
| Base | Abstract `APawn` | Concrete placeable |
| TeamId | Replicated | Inherited |
| CapabilityTags | Interim CDO container + query API | Selectable / Inspectable / Selection.Type.Unit |
| ASC on unit | **No** | **No** |
| Movement component | **No** | **No** |
| AIController / AutoPossessAI | **No** | **No** |
| `ReceiveCommand` | **No** | **No** |
| OwningPlayerState | **No** | **No** |
| Collision | — | Capsule Visibility-block; `CanEverAffectNavigation=false` |

Player possesses **`AGP_CameraPawn`** (`GameMode::DefaultPawnClass`). Units are world actors, not possessed by the human PC.

### GAS / tags
| Fact | State |
| --- | --- |
| `GP.Command.Move/Stop/Attack/AttackMove/Mine/Repair/Sell/Demolish/OrderDrop/CancelOrder` | **Registered** in `FGPGameplayTags` |
| Capability / selection tags | Registered + used by selection |
| Ability execution for commands | Not wired on units |
| Request structs / target data for commands | **None** |
| Player ASC | On `AGP_PlayerState` (not unit ASC) |

### Navigation / movement
| Fact | State |
| --- | --- |
| Project NavMesh / MoveTo path for units | **Not present** as GP gameplay contract |
| Unit AIController | **Absent** |
| `UGP_MovementComponent` | **Absent** (GP-S20) |
| Can selected pawn execute Move today? | **No** |

### Networking
| Fact | State |
| --- | --- |
| Intended CommandComponent owner | `AGP_PlayerController` (TDD/13) |
| Component replication | Local intent component; not a replicated selection mirror |
| RPC boundary | Client PC → `Server_RequestCommand(FGP_CommandRequest)` on server PC |
| Ownership check (design) | Server: target unit TeamId == player TeamId (+ later AllowedCommands) |
| Listen-server / remote client | Same PC component pattern as selection; authority on server for execution |

---

## 2. Answers to architecture questions (facts)

| # | Question | Answer |
| --- | --- | --- |
| 1 | `UGP_CommandComponent` exists? | **No** |
| 2 | `FGP_CommandRequest` exists? | **No** |
| 3 | Command gameplay tags? | **Yes** (native `FGPGameplayTags`) |
| 4 | Right-click IA/IMC? | **No** |
| 5 | Server RPC command endpoint? | **No** |
| 6 | Can `AGP_UnitBase` accept Move now? | **No** |
| 7 | AIController / NavMovement contract? | **No** for units |
| 8 | Real movement in GP-S17 without premature full GP-S18? | **No** — unit Move needs ReceiveCommand + Movement/AI/Nav (GP-S20–S22; fuller UnitBase) |
| 9 | TDD S17 vs S19 vs later | **S17:** CommandComponent + smart build + `Server_RequestCommand` dispatch. **S18:** fuller UnitBase + highlight. **S19:** `FGP_CommandRequest` + native tag mapping. **S20–S22:** movement execution path |
| 10 | What is GP-S17? | **Combination:** local orchestration shell + request construction + (later) input + server validation + dispatch **call**. **Not** unit-layer Move/Attack/Mine execution, formation, FoW, or ability systems |

---

## 3. Dependency matrix

| Dependency | Current state | Needed for | Blocking? | Owner slice |
| --- | --- | --- | --- | --- |
| SelectionComponent read API | Done | All command phases | No | GP-S16 |
| Concrete `AGP_Unit` + TeamId | Done (interim) | Operator smart-command classification / ownership | No for shell; Yes for validated ops | UnitBase prerequisite / S18 later |
| `FGP_CommandRequest` | **Missing** | BuildSmartCommand, RPC payload | **Yes for Phase B+** | **GP-S19 (order correction required)** |
| Native command tags | Present | Tag fields / validation | No | GP-S02 |
| Right-click IA + `IMC_GP_Commands` | Missing | Phase C input | Yes for input phase | GP-S17 Phase C |
| Target actor/location resolve | PC deproject+trace pattern exists (selection) | Smart command | No for shell | GP-S17 Phase C |
| Ownership validation (TeamId) | PlayerState + Unit TeamId exist | Server RPC | No for shell | GP-S17 Phase D |
| UnitDefinition AllowedCommands | Missing | Full TDD validation | Soft-block full validation | Full UnitBase / definitions |
| `Server_RequestCommand` | Missing | Networked intent | Yes for Phase D | GP-S17 Phase D |
| `ReceiveCommand` on UnitBase | Missing | Server dispatch | Yes for real dispatch | Partial UnitBase / GP-S18 |
| `UGP_MovementComponent` + AI/Nav | Missing | Actual Move | **Yes for executable Move** | GP-S20–S22 |
| Attack / Combat components | Missing | Attack smart-command execution | Yes for Attack exec | Combat slices |
| Interact / Mine / Repair | Missing actors/components | Smart branches execution | Yes for those cmds | Worker/resource slices |
| GAS unit abilities | Missing on units | Ability-routed commands | Yes | GP-S18+ GAS unit work |
| Formation movement | Out of MVP | — | N/A | Out of MVP |
| FoW | Deferred | Visible-only targeting | No for S17 shell | FoW slice |
| UI / modal IMC gating | Deferred | Match-input suspend | Soft | UI / TDD/12 later |

**Hard ban:** no temporary fake command interfaces, alternate request structs, or placeholder UnitBase invented to bypass S19 / S20 contracts.

---

## 4. Critical order correction — GP-S17 ↔ GP-S18 ↔ GP-S19

TDD/13 currently lists:

1. GP-S17 CommandComponent (`BuildSmartCommand`, `Server_RequestCommand`)
2. GP-S18 UnitBase abstract + highlight
3. GP-S19 `FGP_CommandRequest` + native tag mapping

**Locked facts for this analysis:**

| Fact | Decision |
| --- | --- |
| TDD assigns `FGP_CommandRequest` to **GP-S19** | Keep S19 name; do **not** rename or mark S19 started |
| `BuildSmartCommand` + RPC require the **canonical** request type | Phase **B+** blocked until S19 request contract is pulled forward |
| Temporary alternate request struct | **Forbidden** |
| Phase A shell | **Does not** depend on request type |
| Executable Move | **Not** part of the first GP-S17 checkpoint; belongs to movement/AI/Nav (GP-S20–S22; do not mark those started) |
| Full UnitBase receiver / highlight / death | Remains GP-S18 / later — not started here |

**Recommended correction (do not invent alternate structs):**

| Step | Action |
| --- | --- |
| Keep | GP-S17 Phase A shell **without** request API |
| Pull forward | Minimal canonical `FGP_CommandRequest` (TDD/04 fields) **before** GP-S17 Phase B — either as early GP-S19 checkpoint or as an explicit “S19-scope pull” substep of S17 |
| Defer | Full UnitBase MID/death/ASC, `ReceiveCommand` routing to real components → GP-S18 / later |
| Defer | Actual MoveTo / Nav / AIController → GP-S20–S22 |
| Never | Create a temporary non-canonical request type to “unblock” S17 |

---

## 5. Recommended phase split (fact-based)

| Phase | Role |
| --- | --- |
| **A** | Component shell / ownership (**first checkpoint — safe now**) |
| **Prerequisite pull-forward** | Canonical `FGP_CommandRequest` from GP-S19 scope |
| **B** | Request construction / `BuildSmartCommand` |
| **C** | RMB input and target classification |
| **D** | RPC and server ownership validation |
| **E** | Executable Move — deferred to movement/AI/Nav slices (GP-S20–S22) |

Order of Phase **B–D** may be refined after the canonical request contract lands.
**Phase A is already safe** and does not wait on that contract.

### Phase A — CommandComponent shell (FIRST IMPLEMENTATION CHECKPOINT)

Exact Phase A scope:

- create `UGP_CommandComponent`
- PC-owned default subobject
- non-replicated (`SetIsReplicatedByDefault(false)`)
- **no** request struct
- **no** input
- **no** RPC
- **no** command execution
- **no** own selected-units state
- **no** speculative public API (`BuildSmartCommand`, etc.)
- **no** Move / Attack / Interact / Ability
- **no** UnitBase / AI / Nav changes

| Field | Detail |
| --- | --- |
| Goal | Ownership shell only |
| Dependencies | None beyond existing PC / GPRuntime |
| C++ | `GPCommandComponent.h/.cpp`; PC subobject + `GetCommandComponent()` |
| Assets | None |
| Networking | None |
| Exclusions | Everything listed above |

### Phase B — Request construction / `BuildSmartCommand`
Blocked until S19 request pull-forward. Local build only; no RPC.

### Phase C — RMB input and target classification
Separate Commands IMC; local classification; no execution.

### Phase D — RPC and server ownership validation
`Server_RequestCommand` + TeamId checks; no fake Move.

### Phase E — Executable Move (deferred)
Owner: GP-S20–S22 (+ unit command receiver). **Not** first GP-S17 checkpoint.

---

## 6. Exact first checkpoint

### Decision: **Phase A — CommandComponent shell**

**Why first:**

1. Compile-safe today — no missing USTRUCT, Nav, AI, or ReceiveCommand.
2. Mirrors the proven GP-S16 Phase A pattern (ownership + local component before input/network).
3. Useful: establishes PC ownership, accessor, non-replicated lifetime before request/RPC surface grows.
4. Avoids speculative `BuildSmartCommand` signatures that require `FGP_CommandRequest` before the order correction lands.
5. Avoids fake execution, movement, Attack, abilities, formation, FoW.

**Phase A acceptance (when later assigned):**

- [ ] `UGP_CommandComponent` exists in `GPRuntime`
- [ ] Default-subobject on `AGP_PlayerController` + getter
- [ ] Tick off; `SetIsReplicatedByDefault(false)`
- [ ] No Selection mutation; no own selected-units array
- [ ] No public BuildSmartCommand / RPC / input binds
- [ ] Three build targets pass
- [ ] No assets / maps / config / UnitBase / AI / Nav edits

**Immediately after Phase A:** resolve S19 struct pull-forward, then Phase B (order of B–D refinable).

---

## 7. Architecture decisions (recommended)

### Phase A ownership contract (locked)

| Topic | Decision |
| --- | --- |
| Owner | `AGP_PlayerController` |
| Creation | Default subobject (`CreateDefaultSubobject`) |
| Replication | `SetIsReplicatedByDefault(false)` |
| Instances | Component exists on corresponding PC instances (host + clients each have their own) |
| Authoritative selection | **No** — CommandComponent is never the selection source of truth |
| SelectionComponent | Read-only dependency **in a future phase**; Phase A must **not** add unused Selection API |
| Server authority / RPC | **Not implemented** in Phase A |
| Local intent / server execution | **Future phases** (B–E) |

Do **not** add Phase A public API that Phase A does not use.

### Authority boundary (future phases; not Phase A)
| Topic | Decision |
| --- | --- |
| Forms intent | Local CommandComponent (from input + Selection read) — later |
| Validates units | **Server** PC (`Server_RequestCommand_Implementation`) — later |
| Executes command | Server → unit `ReceiveCommand` / helpers (when exist) — later |
| RPC | `Server_RequestCommand` on PC — later |
| Client-authoritative movement | **Forbidden** |

### Selection boundary
- Future phases: CommandComponent **only reads** `UGP_SelectionComponent`
- Does **not** mutate selection / inspect / control groups
- Does **not** keep a second authoritative selected-units list
- Phase A: no selection wiring required

### Request boundary (Phase B+, after S19 pull)
Canonical TDD/04 fields only — no alternate struct:

- `FGameplayTag CommandTag`
- Targets (issuing units)
- `FVector TargetLocation`
- optional target actor
- `bool bQueue` (MVP may ignore / replace-only)

### Execution boundary
| Early GP-S17 (B–D) | Not first checkpoint / not Phase A |
| --- | --- |
| Orchestrate build → RPC → validate → dispatch call | NavMesh MoveTo, Attack/Mine, abilities, formation, FoW |

---

## 8. Expected files (Phase A implementation — not this pass)

| Path | Change |
| --- | --- |
| `GP/Source/GPRuntime/Public/Player/GPCommandComponent.h` | New |
| `GP/Source/GPRuntime/Private/Player/GPCommandComponent.cpp` | New |
| `GP/Source/GPRuntime/Public/Player/GPPlayerController.h` | Subobject + getter |
| `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp` | Construct subobject |

**Assets (Phase A):** none  
**Assets (Phase C later):** Commands IA/IMC under `/Game/GrimProtocol/Input/Commands/` (exact names OD-locked at that phase)

`Build.cs`: no change expected for a local `UActorComponent` (same as Selection shell).

---

## 9. Phase A validation boundary

Shell has no user-facing command behavior. Operator validation is **minimal**.

### Standalone
- Editor launches
- PlayerController is created
- CommandComponent exists on that PC
- Camera works
- Click / marquee / control groups still work
- No component construction errors
- No commands issued; no unit movement

### 2-player Listen Server
- Host PC has its own CommandComponent
- Client PC has its own CommandComponent
- No shared mutable command state
- No RPC
- No replication warnings for the component
- Selection remains local-only
- No unit movement

**Diagnostics:** a temporary one-shot construction / BeginPlay log is allowed **only at the Phase A implementation checkpoint** if needed for operator proof. This analysis document does **not** require a permanent production log.

**Builds (implementation checkpoint):** GPEditor Dev / GP Dev / GP Shipping (+ UHT as needed).

---

## 10. Strict exclusions (this analysis + Phase A)

- no CommandComponent / CommandRequest code in this docs pass
- no fake Move / Attack / Interact / Stop / Hold / Patrol execution
- no alternate `FGP_CommandRequest` type
- no RMB assets / RPC in Phase A
- no AIController / NavMesh / MovementComponent
- no starting GP-S18 or GP-S19 implementation (S19 pull-forward is a later explicit assignment)
- no FoW, formation, abilities, maps, config, `.uproject`

---

## 11. Open decisions / blockers

1. **Order correction approval:** pull minimal `FGP_CommandRequest` before Phase B (S19-ahead or S17-owned S19-scope pull). S19 remains unstarted until that assignment.
2. **RPC serialization of Targets:** confirm UE RPC-safe representation at Phase D.
3. **Interim AllowedCommands:** without UnitDefinition, later server policy TBD.
4. **ReceiveCommand stub timing:** before Phase D claims full dispatch.
5. **Executable Move:** GP-S20–S22 — not Phase A success.
6. **Hold / Patrol:** out of MVP / deferred (TDD/04).
7. **DOCUMENTATION_INDEX** sync: separate docs hygiene; not part of this analysis commit.

---

## Stop Condition
Status **ANALYSIS_READY_FIRST_CHECKPOINT_PENDING**.
Analysis finalized. First implementation checkpoint locked as **Phase A — CommandComponent shell**.
Do **not** implement code/assets in this pass.
Do **not** start GP-S18 / GP-S19 / Move execution from this analysis.
Await explicit Phase A implementation assignment.
