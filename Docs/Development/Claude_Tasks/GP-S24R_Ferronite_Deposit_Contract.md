# GP-S24R — Ferronite Deposit Contract on AGP_ResourceNode

## Status
**GP-S24R_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `754b133731065eed000fdcce4bbaa5c45f096e60` (GP-S23R merged)

Branch: `feature/gp-s24r-ferronite-deposit-contract`

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

## In scope
ResourceDefinition soft ref; Ferronite identity/tags; capacity validation; miner slot/queue; Mine target compatibility; diagnostics; BP example compatibility; docs.

## Out of scope
CargoComponent; MiningComponent; Worker; mining timers/cycles; movement-to-deposit; cargo fill; Storage; MainBase; FerroniteThreatValue writes; OrbitalFerronite; FerroniteScore; container launch; regeneration; UI; map population; projectiles; visual redesign.

## Acceptance criteria
- [ ] ResourceNode remains AActor with soft Ferronite definition default
- [ ] Tags Node + Type.Ferronite exposed via capability API
- [ ] Mine validates ResourceNode; rejects unit/depleted/null/plain
- [ ] Slot soft-cap 4 + FIFO waiting + promote on release; no duplicates
- [ ] ConsumeResource authority-only; amounts replicate
- [ ] Inspect diagnostics cover identity + occupancy + Mine cases
- [ ] Authored BP still works; authored collision/nav warnings = 0
- [ ] GPEditor Dev+UHT passed; GP Dev/Shipping deferred until after operator validation

## Operator validation
1. Open `BP_ResourceNode_AuthoredExample` — confirm ResourceDefinition → Ferronite DA
2. Place temporary ResourceNode in prototype arena / transient map — **do not save map** unless necessary
3. `gp.ResourceNode.Inspect` — identity, PrimaryAssetId, tags, Current/Max, VisualSourceMode
4. Occupancy: RequestSlot until Active=4; next → Waiting; Release promotes; duplicate → Already*
5. `gp.Command.InspectMineTarget` — accept ResourceNode; reject unit/depleted/null/plain
6. Deplete via `gp.ResourceNode.Consume`; Mine reject Depleted
7. Listen server + client: amounts consistent; client RequestSlot rejected
8. Authored visual collision/nav warnings = 0; native fallback still works

## Known limitations
- No mining execution / movement / cargo
- Ore enum name retained until rename stage
- MaxConcurrentMiners=4 is TDD prototype soft-cap (not final balance)
- Occupancy actor lists not replicated (counts only)
- Sync definition load only on explicit validate/Mine/diagnostic resolve paths

## Next canonical stage
**GP-S25 — UGP_CargoComponent**
