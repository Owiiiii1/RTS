# ADR-0001 — Project Prefix `GP`

## Status
Accepted

## Context
Проєкт має єдиний project-wide префікс для C++ classes, structs, enums, assets, gameplay tags namespace. Префікс впливає на:
- Naming у source.
- Asset folder convention.
- Tag namespace.
- Memory і review-time recall ("чий це клас?").

Альтернативи:
- `RTS_*` — generic, плутає з marketplace assets.
- `GrimProtocol*` — занадто довго для C++ identifier.
- `GP*` — короткий, унікальний, легко grep-у.

## Decision
Project-wide префікс — **`GP`** (GrimProtocol).

Застосування:
- C++ Actors: `AGP_*`
- C++ UObjects / Components: `UGP_*`
- C++ Interfaces: `IGP_*` / `UGP_*`
- C++ Structs: `FGP_*`
- C++ Enums: `EGP_*`
- Assets: `<TypePrefix>_GP_*` (e.g., `DA_GP_Unit_Worker`, `SM_GP_Building_LogisticsHub_01`)
- Blueprint subclasses: `BP_GP_*`, `WBP_GP_*`, `ABP_GP_*`
- Gameplay Tag root namespace: `GP.*`
- Log categories: `LogGP*`
- Modules: `GP*Runtime` (`GPRuntime`, `GPGASRuntime`, `GPUIRuntime`)
- Content path: `/Game/GrimProtocol/`

## Consequences

### Positive
- Простий, передбачуваний grep target: `AGP_`, `UGP_`, `DA_GP_`.
- Уникнення колізій з UE engine class names і marketplace assets.
- Швидкий visual scan у редакторі content browser.

### Negative
- Усі legacy assets (немає у проєкті — fresh start) мають strict expectations.
- Якщо проєкт ребрендується — масовий rename. Mitigation: проєкт ще на 0-стадії, refactor cheap.

## Alternatives Considered
- `RTS_*` — занадто generic.
- `Grim*` — конфлікт з іншими можливими "grim" assets, плюс довший identifier.
- Mixed prefixes (`AGRP_`, `UGRP_`) — складніше pattern-recall, не вирівнюється з 2-char convention.

## References
- `/STYLE.md` — повна naming convention.
- `Docs/Development/Naming_Conventions.md` — exhaustive table.
