# Cursor Work Report — GP-S29R Unit Details UX Cleanup

## Status
**GP-S29R_UNIT_DETAILS_CLEANUP_READY_FOR_OPERATOR_VALIDATION**

## Branch
`feature/gp-s29r-combat-los-healthbar-teamcolors`

## Scope
Architecture/UX cleanup after composition audit. **Not** GP-S29R finalization. No Salvage Walker. No behavioral changes.

---

## UPROPERTY categories changed

| Owner | Property | Before | After |
| --- | --- | --- | --- |
| `AGP_MobileUnit` | `MovementComponent` | `GP\|Movement` | `GP\|Components\|Movement` |
| `AGP_Unit` | `UnitVisualComponent` | `GP\|Visual` | `GP\|Components\|Visual` |
| `AGP_Unit` | `CapsuleComponent` | `GP\|Components` | unchanged (`GP\|Components`) |

Unchanged (as required):
- `UGP_MovementComponent` tunables remain `GP|Movement`
- `UGP_UnitVisualComponent` settings (`VisualSourceMode` / `VisualArchetype` / …) remain `GP|Visual`
- TeamPresentation / CombatPresentation / HealthBar categories untouched
- No UObject/class renames, no CreateDefaultSubobject name changes, no serialization property renames

Expected Class Defaults tree:

```
GP
 ├─ Components
 │   ├─ Movement   (actor pointer to MovementComponent)
 │   └─ Visual     (actor pointer to UnitVisualComponent)
 ├─ Movement       (component tunables)
 └─ Visual         (component settings)
```

---

## Composition confirmations (unchanged from audit)

- **Component count:** unchanged (production code = category metadata only; no CreateDefaultSubobject adds/removes).
- **One** `UGP_MovementComponent` on `AGP_MobileUnit` / `AGP_Unit`.
- **One** `UGP_UnitVisualComponent` on `AGP_Unit`.

---

## TDD/05 stale corrections

Minimal sync in `Docs/TDD/05_Unit_Architecture.md`:

1. Hierarchy: `UnitBase → MobileUnit → {Unit, Worker}`; `BuildingBase` sibling; **ResourceNode is separate AActor** (not BuildingBase).
2. UnitBase composition: command / combat presentation / team presentation / health bar / ASC / AttributeSet / TeamId / CapabilityTags / interim combat defaults — **no** Sphere root / direct StaticMesh ownership claims.
3. Movement: one `UGP_MovementComponent`; not CharacterMovement; not NavMesh pathfinding SoT.
4. Commands/combat: `ReceiveCommand` → `UnitCommandComponent`; Attack FSM there; CombatComponent / TargetingComponent deferred.
5. `AGP_Unit`: Capsule + UnitVisual + inherited Movement.
6. `AGP_Worker`: MobileUnit child, **not** Unit child.
7. UnitVisual: NativeFallback vs AuthoredComponents; InfantryMelee cosmetic-only.
8. Salvage Walker: GDD MVP combat unit noted as **pending** — **not implemented**.

---

## Salvage Walker

**Not created.** No class, no Blueprint, no visual archetype change.

---

## Validation

### GPEditor Win64 Development + UHT
**PASS** (`Result: Succeeded`)

### Contract tests (headless `-game -NullRHI` on `L_PrototypeArena`)

| Command | Result |
| --- | --- |
| `gp.Combat.RunHealthBarContractTest` | Complete Failures=0 |
| `gp.Combat.RunTeamColorContractTest` | Complete Failures=0 |
| `gp.Combat.RunLOSFireGateContractTest` | Complete Failures=0 |
| `gp.Resource.RunS28RegressionSuite` | Complete Failures=0 |

GP Win64 Development / Shipping: **not run** (not finalization).

---

## Files changed (committed)

- `GP/Source/GPRuntime/Public/Units/GPMobileUnit.h`
- `GP/Source/GPRuntime/Public/Units/GPUnit.h`
- `Docs/TDD/05_Unit_Architecture.md`
- `Docs/Development/Cursor_Work_Report.md`

## Operator assets untouched (not committed)

- `GP/Config/DefaultEngine.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Blueprint/`
- `GP/Content/GrimProtocol/Materials/`
- authored ResourceNode / Niagara / other operator `.uasset` / `.umap`

---

## Commit SHA

_(filled after commit)_
