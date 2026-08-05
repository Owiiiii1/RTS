# GP-S24R — Ferronite Deposit Contract on AGP_ResourceNode

## Status
**GP-S24R_FINALIZED_READY_FOR_MERGE**

## Baseline
`main` @ `754b133731065eed000fdcce4bbaa5c45f096e60` (GP-S23R merged)

Branch: `feature/gp-s24r-ferronite-deposit-contract`  
Candidate: `42c1c9167ddd607506d32b470763fc8467a67d66`  
Docs SHA note: `cfc26a4ff8b6bb3c3680b5d92245a1d9881d04b2`

## Canonical roadmap position
Slice-6 reconciliation coding stage for GP-S24:

`GP-S23R ResourceDefinition` → **GP-S24R Deposit contract** → GP-S25 Cargo → GP-S26 Mining → GP-S27 Worker → GP-S28 Storage+ThreatValue

Sources of truth: DOCUMENTATION_INDEX, GP-S27 Worker Analysis, GP-S23R, TDD 13/07/10, GDD 02/06, ADR-0002, ADR-0009. Archive is not requirements.

## Current ResourceNode architecture
- Class remains `AGP_ResourceNode : AActor` (not renamed to `AGP_FerroniteDeposit`)
- Replicated; gameplay `UBoxComponent` collision; `UGP_ResourceNodeVisualComponent` + `EGP_VisualSourceMode`
- Blueprint-authored presentation preserved (`BP_ResourceNode_AuthoredExample`)
- No `AGP_BuildingBase`, no ASC, no Team ownership, no permanent tick

## ResourceDefinition ownership
| Owner | Responsibility |
| --- | --- |
| `UGP_ResourceDefinition` | Identity + mining metadata (type, tag, cycle amount/duration/range, orbital metadata) |
| `AGP_ResourceNode` | Runtime deposit state: Max/Current amount, occupancy/queue, depletion |

Soft reference on node:

- `TSoftObjectPtr<UGP_ResourceDefinition> ResourceDefinition`
- EditDefaultsOnly, BlueprintReadOnly
- Default: `/Game/GrimProtocol/DataAssets/Resources/DA_GP_Resource_Ferronite`
- Resolve via already-loaded soft ptr / Asset Manager primary object
- Explicit sync `LoadSynchronous` only on validate/Mine/diagnostic paths with Verbose log (AlwaysCook primary asset)
- BeginPlay uses non-sync resolve only

## Identity and naming policy
| Layer | Value |
| --- | --- |
| Canonical gameplay identity | Ferronite Deposit |
| Implementation class | `AGP_ResourceNode` |
| Internal enum | `EGP_ResourceType::Ore` |
| Canonical tag | `GP.Resource.Type.Ferronite` |
| Capability tag | `GP.Resource.Node` |

No competing player-facing DisplayName on the node (definition owns DisplayName).

## Gameplay tags
Native registry (`FGPGameplayTags`): `Resource_Node`, `Resource_Type_Ferronite` — no duplicates.

Node API:

- `GetResourceCapabilityTags`
- `HasResourceCapabilityTag`

Always exposes `GP.Resource.Node` + Ferronite type tag (from resolved definition when available).

## Deposit runtime state
- `MaxAmount` (prototype default 5000 retained)
- `CurrentAmount` (defaults to MaxAmount; clamped; authority-only `ConsumeResource`)
- `IsDepleted` = `CurrentAmount <= 0`
- Validation: MaxAmount > 0; CurrentAmount in `[0, MaxAmount]`
- No regeneration
- Client cannot mutate amounts

## Slot / queue design
Deposit-side soft-cap (not MiningComponent):

| Field / API | Notes |
| --- | --- |
| `MaxConcurrentMiners` | Default **4** (TDD `MaxConcurrentWorkers`) |
| `RequestMiningSlot(AActor*)` | Authority-only; Granted / Waiting / Already* / Rejected* |
| `ReleaseMiningSlot(AActor*)` | Authority-only; promotes FIFO waiting head |
| `HasActiveMiningSlot` / `IsWaitingForMiningSlot` | Query |
| Active/Waiting arrays | Server-local `TWeakObjectPtr`; not replicated |
| `ActiveMinerCount` / `WaitingMinerCount` | Replicated counts for inspect/future UI |
| Cleanup | Invalid/dead weak refs on request/release; EndPlay clears |

Generic miner `AActor` — no Worker dependency. No permanent tick.

## Mine command compatibility
Smart-build + server validate:

- `GP.Command.Mine` accepts valid `AGP_ResourceNode`
- Does **not** require `AGP_UnitBase` for resource targets
- Move/Attack rules unchanged; Attack not expanded
- Server checks: valid, not pending kill, same world, ResourceNode contract, capability, resolved definition, `CurrentAmount > 0`
- Mine command acceptance/held only — **no mining execution / consumption via Mine**

Legacy UnitBase + `GP.Resource.Node` path retained for hybrid targets.

## Validation
- `ValidateDepositContract` + editor `IsDataValid`
- Checks definition assignment/resolve, type/tag consistency, Max/Current, MaxConcurrentMiners
- No validation tick

## Networking
- Listen-server + client: Max/Current replicated; ConsumeResource authority-only
- Slot/queue mutation server-only; clients cannot RequestSlot (debug cmds reject client)
- Command validation server-authoritative
- Destroy/EndPlay clears occupancy safely
- Only occupancy counts replicated — not actor arrays

## Tick policy
`PrimaryActorTick.bCanEverTick = false`. No polling. Visual component tick remains disabled (S26B2A).

## Blueprint visual compatibility
S26B2A contract unchanged:

- NativeFallback / AuthoredComponents; no overlap
- Authored meshes NoCollision / not nav relevant
- Gameplay box = C++ collision authority
- Seed/Verify extended to confirm Ferronite `ResourceDefinition` on example BP CDO
- Visual composition unchanged; map unchanged

## Diagnostics (non-shipping)
| Command | Purpose |
| --- | --- |
| `gp.ResourceNode.Inspect [Name]` | Definition path, PrimaryAssetId, type, tags, amounts, occupancy, validation, visual/collision |
| `gp.ResourceNode.InspectOccupancy [Name]` | Active/waiting counts |
| `gp.ResourceNode.RequestSlot <Miner> [Node]` | Authority debug slot request |
| `gp.ResourceNode.ReleaseSlot <Miner> [Node]` | Authority debug slot release |
| `gp.ResourceNode.Consume <Amount> [Node]` | Existing consume diagnostic |
| `gp.Command.InspectMineTarget` | Mine accept/reject cases (null, node, depleted, ordinary unit, plain actor) |

## Operator validation matrix (accepted)

| Item | Result |
| --- | --- |
| ResourceDefinition / PrimaryAssetId / Ore / Ferronite tags | **PASS** |
| Max=5000 Current=5000 IsDepleted=false MaxConcurrentMiners=4 | **PASS** |
| ValidationOk / Errors=0 / Warnings=0 / CanAcceptMine | **PASS** |
| Mine accept ResourceNode; reject null/depleted/unit/plain | **PASS** |
| Consume 5000 → depleted; CanAcceptMine=false MineFail=Depleted | **PASS** |
| FIFO soft-cap 4 + waiting + promote + Already* duplicates | **PASS** |
| Listen server + client amount + occupancy count replication | **PASS** |
| Client RequestSlot rejected | **PASS** |
| Authored visuals warnings=0; TickEnabled=false | **PASS** |
| Map unchanged | **PASS** |

## Builds (finalization)
- GPEditor Dev+UHT: retained from candidate (no C++ changes at finalization)
- GP Win64 Development — **PASSED**
- GP Win64 Shipping — **PASSED**

## In scope
ResourceDefinition soft ref; Ferronite identity/tags; capacity validation; miner slot/queue; Mine target compatibility; diagnostics; BP example compatibility; docs.

## Out of scope
CargoComponent; MiningComponent; Worker; mining timers/cycles; movement-to-deposit; cargo fill; Storage; MainBase; FerroniteThreatValue writes; OrbitalFerronite; FerroniteScore; container launch; regeneration; UI; map population; projectiles; visual redesign.

## Acceptance criteria
- [x] ResourceNode remains AActor with soft Ferronite definition default
- [x] Tags Node + Type.Ferronite exposed via capability API
- [x] Mine validates ResourceNode; rejects unit/depleted/null/plain
- [x] Slot soft-cap 4 + FIFO waiting + promote on release; no duplicates
- [x] ConsumeResource authority-only; amounts replicate
- [x] Inspect diagnostics cover identity + occupancy + Mine cases
- [x] Authored BP still works; authored collision/nav warnings = 0
- [x] GPEditor Dev+UHT passed; GP Dev/Shipping passed at finalization

## Known limitations
- No mining execution / movement / cargo
- Ore enum name retained until rename stage
- MaxConcurrentMiners=4 is TDD prototype soft-cap (not final balance)
- Occupancy actor lists not replicated (counts only)
- Sync definition load only on explicit validate/Mine/diagnostic resolve paths

## Next canonical stage
**GP-S25 — UGP_CargoComponent**

No known blockers. Ready for main merge when requested.
