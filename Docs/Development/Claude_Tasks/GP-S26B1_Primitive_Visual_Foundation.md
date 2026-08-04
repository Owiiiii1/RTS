# GP-S26B1 Primitive Visual Foundation

## Status
**GP-S26B1_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `bfc762675bba6266011be948c228913c8fc5a324`

Architecture source: `Docs/Development/Claude_Tasks/GP-S26B_Primitive_Visual_MVP_Architecture.md`  
Branch: `feature/gp-s26b1-primitive-visual-foundation`

## Scope delivered

- Native primitive visual types (`EGP_PrimitiveShape`, `EGP_VisualArchetype`, part/definition structs)
- `UGP_UnitVisualComponent` on `AGP_Unit` (default subobject)
- Native `InfantryMelee` definition: Body (Cylinder) + Forward + Weapon (Cube)
- Removed legacy single `VisualMesh` Cylinder from `AGP_Unit` (no dual visual)
- Capsule/root collision, selection Visibility, movement, attack, replication unchanged
- Dedicated server: no part construction (`DedicatedVisualSuppressed`)
- No permanent tick
- Non-shipping `gp.UnitVisual.Inspect`
- Team tint: best-effort DMI parameter attempt (Engine basic materials unverified); full team color needs operator material step

## Operator validation (functional)

**PASS** — composition on listen/client; Move; Attack; selection; death cleanup; visual NoCollision; idle tick off; legacy mesh absent; cadence/presentation unchanged.

## Visual readability correction (pending operator recheck)

InfantryMelee Forward/Weapon transforms adjusted for RTS-camera readability. Body unchanged.

| Part | Field | Old | New |
| --- | --- | --- | --- |
| Forward | Shape | Cone | **Cube** (Cone tip-forward read as disc from RTS cam) |
| Forward | Location | (42, 0, 18) | **(52, 0, 16)** |
| Forward | Rotation | (90, 0, 0) | **(0, 0, 0)** |
| Forward | Scale | (0.22, 0.22, 0.38) | **(0.58, 0.20, 0.16)** elongated +X nose |
| Weapon | Location | (28, 20, 10) | **(42, 30, 14)** further from Body |
| Weapon | Rotation | (0, 12, 0) | **(0, 10, 0)** |
| Weapon | Scale | (0.50, 0.10, 0.10) | **(0.90, 0.16, 0.16)** ~1.8× length, thicker |

Still 3 parts; NoCollision; no tick; no gameplay/asset changes.

## Explicitly not in B1

Combat cosmetics (lunge/hit/death), projectiles, turret logic, movement bob, DataAsset instances, Blueprint/material/level assets, arena generator, S26A contract changes.

## Operator recheck (visual)

| ID | Case | Expect |
| --- | --- | --- |
| R1 | RTS camera | Weapon clearly visible, not buried in Body |
| R2 | RTS camera | Forward reads as direction nose/arrow, not a round stub |
| R3 | Rotate unit | Forward tracks actor forward |

## Build

- GPEditor Win64 Development — **PASSED** (implementation + readability correction)
- GP Dev/Shipping deferred to finalization
