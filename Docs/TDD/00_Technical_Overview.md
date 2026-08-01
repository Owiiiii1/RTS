# Technical Overview

## Engine and Stack

- Unreal Engine 5.8.1.
- C++ first, Blueprint — для UI і presentation only.
- Gameplay Ability System (GAS) — head plugin, обов'язковий.
- Steam Online Subsystem (OSS) — primary multiplayer transport.
- Enhanced Input — обов'язковий.
- Common UI (deferred decision; MVP — base UMG достатньо).

## Project Prefix

`GP`. Усі C++ classes, structs, enums, asset prefixes — за `/STYLE.md`.

## Module Layout (Three Runtime Modules)

```
GP/Source/
  GPRuntime/        // gameplay, match, units, buildings, resources, commands
  GPGASRuntime/     // ASC, AttributeSets, Abilities, Effects, Tags
  GPUIRuntime/      // HUD, widgets, presentation
```

Dependencies:

```
GPUIRuntime  ->  GPRuntime  ->  GPGASRuntime
                     |              ^
                     +--------------+
```

`GPGASRuntime` — самодостатній (тільки на core engine + GameplayAbilities plugin). `GPRuntime` залежить від `GPGASRuntime`. `GPUIRuntime` залежить від `GPRuntime` (і transitive від `GPGASRuntime`).

Details — [`01_Module_Architecture`](01_Module_Architecture.md).

## Core Class Architecture (MVP)

| Class | Role | Owner Module |
| --- | --- | --- |
| `AGP_GameMode` | Match setup, server authority root | `GPRuntime` |
| `AGP_GameState` | Replicated match state | `GPRuntime` |
| `AGP_PlayerController` | Input orchestration, command dispatch | `GPRuntime` |
| `AGP_PlayerState` | ASC owner, player attributes, faction | `GPRuntime` (ASC component from `GPGASRuntime`) |
| `AGP_CameraPawn` | Player camera, local input | `GPRuntime` |
| `AGP_UnitBase` | Abstract unit base (ASC + UnitAttributeSet) | `GPRuntime` |
| `AGP_MobileUnit` | Mobile unit (workers, troopers) | `GPRuntime` |
| `AGP_BuildingBase` | Static unit (buildings, resource nodes) | `GPRuntime` |
| `UGP_PlayerAttributeSet` | Resource, Unit Cap, Modifiers | `GPGASRuntime` |
| `UGP_UnitAttributeSet` | Health, Armor, Damage, Attack stats | `GPGASRuntime` |
| `UGP_UnitDefinition` | Unit Data Asset (config) | `GPRuntime` (it's a UDataAsset child; module placement — Definition lives where consumers live) |
| `UGP_BuildingDefinition` | Building Data Asset (config) | `GPRuntime` |
| `UGP_FactionDefinition` | Faction Data Asset (config) | `GPRuntime` |
| `FGPGameplayTags` | Native gameplay tag registry | `GPGASRuntime` |

## Authority Map (One-Liner)

| Concern | Authority |
| --- | --- |
| Match state transitions | Server (AGP_GameMode) |
| Player resources | Server (AGP_PlayerState ASC) |
| Unit/building spawn | Server (AGP_GameMode + AGP_PlayerController-issued requests) |
| Unit health / damage | Server (AGP_UnitBase ASC; GAS-driven) |
| Unit movement | Server-authoritative position; client interpolation. Per-unit movement не client-predicted у MVP. |
| Selection state | Client-local (UGP_SelectionComponent) |
| Camera state | Client-local (AGP_CameraPawn) |
| Command intent | Client → server RPC |
| Cosmetic VFX/SFX | Server-multicast |

Details — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).

## Data Pipeline

```
Designer creates DA_GP_* Data Asset
   |
   v
GPRuntime / GPGASRuntime classes consume Data Asset on spawn / init
   |
   v
GAS applies initial attributes / abilities from Data Asset
   |
   v
Gameplay state replicated to clients
```

Жодного hardcoded balance — все через Data Assets. Details — [`05_Unit_Architecture`](05_Unit_Architecture.md), [`06_Building_Architecture`](06_Building_Architecture.md).

## Input Pipeline

```
Enhanced Input (Action / Mapping Context)
   |
   v
AGP_PlayerController (local)
   |
   +-- UGP_SelectionComponent  (local-authoritative)
   |
   +-- UGP_CommandComponent    (local intent -> Server_RequestCommand RPC)
                                    |
                                    v
                              Server validation
                                    |
                                    v
                              Unit / building behavior (server)
                                    |
                                    v
                              Replicated state -> clients
```

Details — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md).

## Multiplayer Topology

- Listen server (host hosts і грає одночасно).
- Steam OSS для matchmaking / invite / join.
- Dedicated server — поза MVP, але архітектура не блокує future migration.

Details — [`08_Steam_Matchmaking`](08_Steam_Matchmaking.md).

## Engine Plugins (MVP)

Required:
- `GameplayAbilities` (GAS)
- `EnhancedInput`
- `OnlineSubsystem`
- `OnlineSubsystemSteam`
- `OnlineSubsystemUtils`

Deferred (post-MVP evaluation):
- `CommonUI`
- `ModelViewViewModel`
- `MassEntity` (NO — explicit reject per pillars)
- `GameFeatures` (NO — explicit reject per ADR-0005, ADR-0006)

## Logging Categories

| Category | Module | Purpose |
| --- | --- | --- |
| `LogGP` | `GPRuntime` | General gameplay |
| `LogGPGAS` | `GPGASRuntime` | GAS-related |
| `LogGPNet` | `GPRuntime` | Replication / RPC diagnostics |
| `LogGPUI` | `GPUIRuntime` | UI-related |

## References

- Module split deep-dive — [`01_Module_Architecture`](01_Module_Architecture.md).
- GAS implementation — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- Multiplayer authority — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).
- Architecture decisions — [`../Architecture_Decisions/`](../Architecture_Decisions/README.md).
