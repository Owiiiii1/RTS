# Technical Design Document

Engineering-canonical документація GrimProtocol. Описує **як** реалізовано gameplay, описаний у [GDD/](../GDD/README.md).

## Pages

- [00_Technical_Overview](00_Technical_Overview.md) — high-level architecture map, engine choices.
- [01_Module_Architecture](01_Module_Architecture.md) — `GPRuntime`, `GPGASRuntime`, `GPUIRuntime` boundaries, dependencies, build rules.
- [02_GAS_Architecture](02_GAS_Architecture.md) — ASC owners, AttributeSets, Abilities, Effects, Tag taxonomy, replication modes.
- [03_Multiplayer_Architecture](03_Multiplayer_Architecture.md) — authority model, RPC discipline, replication strategy, listen-server model.
- [04_RTS_Selection_And_Commands](04_RTS_Selection_And_Commands.md) — Enhanced Input → Selection → Command → Server validation pipeline.
- [05_Unit_Architecture](05_Unit_Architecture.md) — `AGP_UnitBase`, `AGP_MobileUnit`, `UGP_UnitDefinition`, components.
- [06_Building_Architecture](06_Building_Architecture.md) — `AGP_BuildingBase`, BuildingDefinition, construction/production/storage components.
- [07_Resource_Architecture](07_Resource_Architecture.md) — Resource model, attribute mapping, mining flow.
- [08_Steam_Matchmaking](08_Steam_Matchmaking.md) — Steam OSS integration, lobby, listen-server, join flow.
- [09_Gameplay_Tags](09_Gameplay_Tags.md) — tag taxonomy, ownership, native vs config tags.
- [10_Data_Assets](10_Data_Assets.md) — Data Asset ownership, tuning surfaces, runtime usage.
- [11_RTS_Camera](11_RTS_Camera.md) — RTS camera pawn, DataAsset-driven tuning, Enhanced Input set, edge cases.
- [12_UI_Architecture](12_UI_Architecture.md) — Common UI + MVVM framework, HUD info architecture, ViewModel inventory, state ownership.
- [13_Architecture_Proposal](13_Architecture_Proposal.md) — Consolidated C++ class list, module ownership, DataAssets, Tags, Attributes, RPCs, risks, 12-slice implementation order. Stop point before C++ code.
- [14_Orbital_Delivery](14_Orbital_Delivery.md) — Drop pod mechanic, order menu, drop validation, `UGP_OrbitalDeliverySubsystem`, replaces local production/construction.
- [15_Fog_of_War](15_Fog_of_War.md) — 3-level FoW (Unexplored/Explored/Visible), sight scan, replication relevance, selection/combat/drop interactions.
- [16_Voxel_Terrain_And_Foundations](16_Voxel_Terrain_And_Foundations.md) — Voxel Plugin terrain backend direction, generic deformation contract, local engineering jobs, BuildGrid vs voxels, per-cell foundation, Wall-without-foundation, nav and FoW surface integration requirements.

## Cross-Reference з ADR

Кожне non-trivial архітектурне рішення має ADR у `../Architecture_Decisions/`. TDD сторінка посилається на відповідний ADR у секції `Decision Source`.

## Update Rules

- TDD сторінка оновлюється до або разом з PR, що міняє відповідну архітектуру.
- Якщо PR порушує описаний у TDD design — або PR корегується, або TDD оновлюється з ADR, що пояснює зміну.
- TDD не дублює GDD: gameplay-rationale в GDD, technical-rationale в TDD.
