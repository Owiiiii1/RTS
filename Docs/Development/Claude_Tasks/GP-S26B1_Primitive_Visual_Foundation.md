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
- Native `InfantryMelee` definition: Body (Cylinder) + Forward (Cone) + Weapon (Cube)
- Removed legacy single `VisualMesh` Cylinder from `AGP_Unit` (no dual visual)
- Capsule/root collision, selection Visibility, movement, attack, replication unchanged
- Dedicated server: no part construction (`DedicatedVisualSuppressed`)
- No permanent tick
- Non-shipping `gp.UnitVisual.Inspect`
- Team tint: best-effort DMI parameter attempt (Engine basic materials unverified); full team color needs operator material step

## Explicitly not in B1

Combat cosmetics (lunge/hit/death), projectiles, turret logic, movement bob, DataAsset instances, Blueprint/material/level assets, arena generator, S26A contract changes.

## Operator validation

| ID | Case | Expect |
| --- | --- | --- |
| V1 | Listen host | Composite Body/Forward/Weapon; no legacy Cylinder duplicate |
| V2 | Remote client | Same part composition |
| V3 | Facing | Forward cone reads actor forward; rotates with actor |
| V4 | Move / select / attack | Unchanged gameplay |
| V5 | Collision | Capsule query/Visibility only; visual parts NoCollision |
| V6 | Death | Actor/LifeSpan cleanup; parts destroyed with owner |
| V7 | Inspect | Archetype, parts, root, Role/NetMode, tick off, collision disabled, LegacyVisualMesh=absent |
| V8 | Dedicated | Code: parts not built; Inspect DedicatedVisualSuppressed=true |
| V9 | Idle tick | Component tick disabled |

## Build

- GPEditor Win64 Development + UHT — **PASSED**
- GP Dev/Shipping deferred to finalization
