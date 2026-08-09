# Cursor Work Report — GP-S30R Facing + Sight Range

## Status
**GP-S30R_FACING_AND_SIGHT_READY_FOR_OPERATOR_RETEST**

NOT MERGED. Do **not** finalize until operator retest PASS.

---

## 1. Branch / SHAs
| | |
|---|---|
| Branch | `feature/gp-s30r-combat-auto-acquire` |
| Base (`main`) | `ba98383ffca90dafc4645b8761bfaeb93fa5cdc2` |
| Prior remote head (operator base-acquire PASS) | `c3f231368b72891972ecda8b68d0dc2172b3665c` |
| This candidate | *(set after commit)* |

## 2. AttackRange factual source
- **Runtime fire/engage range:** `UGP_UnitAttributeSet::AttackRange` via `UGP_UnitCommandComponent::TryResolveEffectiveAttackRange` (GAS preferred; component `AttackRange` fallback only if GAS ≤0).
- **Salvage Walker CDO seed:** `AGP_SalvageWalker::DefaultAttackRange = 600.f` → applied to GAS on spawn (`GPUnitBase`).
- Editable: unit CDO / Blueprint defaults for `DefaultAttackRange`, or live GAS attribute; **not** duplicated as a new auto-acquire constant.

## 3. Damage factual source
- **Runtime:** `UGP_UnitAttributeSet::Damage` (existing Attack hit / damage application path).
- **SW CDO seed:** `DefaultDamage = 20.f`.
- No second damage SoT for auto-acquire.

## 4. AttackCooldown factual source
- **Runtime:** `UGP_UnitAttributeSet::AttackCooldown` via `ResolveSanitizedAttackCooldown`.
- **SW CDO seed:** `DefaultAttackCooldown = 1.f`.
- Cadence unchanged; auto-acquire only issues `HandleCommand(Attack)`.

## 5. New / updated tuning (component seam)
| Property | Default | Editable |
|---|---|---|
| `AutoAcquireSightRangeCm` | **900** | `UGP_UnitCommandComponent` EditAnywhere `GP\|Combat\|AutoAcquire`; SW CDO sets 900 |
| `AutoAcquireScanIntervalSeconds` | **0.35** | same category |
| `AttackFacingRotationSpeedDegreesPerSecond` | **360** | EditAnywhere `GP\|Combat\|Facing`; SW CDO sets 360 |

## 6. Effective Sight ≥ Attack semantics
- Runtime: `GetEffectiveAutoAcquireRange() = max(AutoAcquireSightRangeCm, AttackRange)`.
- Designer may set Sight < AttackRange; acquire still uses at least AttackRange (predictable; no hard reject).
- Sight is **not** fire range. Fire remains AttackRange + existing LOS gate.

## 7. Rotation implementation
- Authority-only, **yaw only** on owning Pawn/Actor (`SetActorRotation` pitch/roll 0).
- Active only in Attack FSM **Ready** (Approaching facing stays movement-driven via `UGP_MovementComponent`).
- `FMath::RInterpConstantTo` at `AttackFacingRotationSpeedDegreesPerSecond` (same pattern as movement; no instant snap).
- Explicit Attack and auto-acquired Attack share Attack FSM → same facing path.
- No separate aim/animation system; no child-mesh-only rotation.

## 8. Contract stages (updated)
`gp.Combat.RunAutoAcquireContractTest` extended:
- Prior A–H auto-acquire / Stop / reacquire stages retained
- **Sight:** enemy in Sight, outside AttackRange → acquire + Approaching; no damage OOR; enter AttackRange → Ready/damage
- **Facing A:** side target while Ready → yaw toward target (angular tolerance)
- **Facing B:** repositioned target → facing tracks
- **Facing C:** Idle / no attack → no arbitrary combat facing spin

## 9. Builds
GPEditor Win64 Development + UHT: **PASS**  
GP Dev / Shipping: **NOT RUN**

## 10. Contracts / regressions (NullRHI `-game`) — Failures=0
| Command | Result |
|---|---|
| `gp.Combat.RunAutoAcquireContractTest` | **0** |
| `gp.Combat.RunSalvageWalkerContractTest` | **0** |
| `gp.Combat.RunLOSFireGateContractTest` | **0** |
| `gp.Combat.RunHealthBarContractTest` | **0** |
| `gp.Combat.RunTeamColorContractTest` | **0** |
| `gp.Resource.RunS28RegressionSuite` | **0** |
| `gp.Resource.RunDropOffResilienceContractTest` | **0** |
| `gp.Resource.RunContainerLaunchContractTest` | **0** |
| `gp.Resource.RunOrbitalUnitDropContractTest` | **0** |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **0** |

## 11. Changed files
- `GPUnitCommandComponent.h/.cpp` — SightRange, effective acquire max, Ready facing
- `GPSalvageWalker.cpp` — CDO defaults for Sight 900 + facing 360
- `GPCombatAutoAcquireContractTest.h/.cpp` — sight + facing stages
- Docs: task, AI log, DOCUMENTATION_INDEX, Claude_Tasks README, Cursor_Work_Report

## 12. Operator assets untouched
DefaultEngine/Game.ini, map, Blueprint/, Materials/, VFX, Tools/, `.uasset`/`.umap` — not committed.

## 13. Next
Operator retest of facing + sight semantics. Do **not** merge / finalize until PASS. Do **not** auto-assign Attack-Move.
