# Coding Rules

Operational PR checklist для C++ кодування. Доповнює `/STYLE.md` (style) і `/CONTRIBUTING.md` (engineering rules). Цей файл — **review-actionable**.

## Header / CPP Discipline

- `#pragma once` — обов'язково у кожному header.
- `*.generated.h` — завжди останнім include.
- Forward declare у `.h`, повний `#include` у `.cpp`.
- `Engine.h`, `EngineUtils.h`, `UnrealEd.h` — заборонені у runtime headers.
- Definitions — у `.cpp`. Тільки тривіальні getters / setters / templates inline у `.h`.
- One class declaration per `.h`, one class implementation per `.cpp`. Виключення — closely related minor structs / enums.

## UPROPERTY / UFUNCTION

- Усі UE reflection macros — на власному рядку перед declaration.
- `UPROPERTY` без `Category` — review-blocking.
- Category format: `"GP|<Domain>"`, e.g., `"GP|Combat"`, `"GP|Resources"`.
- `EditAnywhere` — тільки коли реально треба edit per-instance. Інакше `EditDefaultsOnly`.
- `BlueprintReadOnly` за замовчуванням; `BlueprintReadWrite` тільки коли UI bind очікує запис (рідко).
- Replicated `UPROPERTY` — обов'язково в `GetLifetimeReplicatedProps`.

## Replication

- `DOREPLIFETIME_*` блок присутній у кожному класі з replicated properties.
- `DOREPLIFETIME_CONDITION(Class, Field, COND_*)` — для conditional replication (e.g., `COND_OwnerOnly` для player-private state).
- RepNotify functions named `OnRep_<PropertyName>`.
- Якщо RepNotify обробляє state transition — log into `LogGPNet` Verbose.

## RPCs

- Server RPCs — `Server_<Verb><Noun>`, з `WithValidation`.
- Client RPCs — `Client_<Verb><Noun>`, sparingly.
- Multicast — `Multicast_<Verb><Noun>`, **cosmetic only**.
- У header — single-line authority comment перед UFUNCTION.

```cpp
// Server: client requests issuing a command to selected units.
UFUNCTION(Server, Reliable, WithValidation)
void Server_RequestCommand(FGP_CommandRequest Request);
```

## Authority Checks

- Mutator functions, що змінюють server state, починаються з:

```cpp
if (!HasAuthority()) return;
```

— якщо це not-RPC server-only function. Альтернативно: `check(HasAuthority())` у functions, де client call — bug.

- Read-only access — without authority check.

## Tags

- Жодних `FGameplayTag::RequestGameplayTag(FName(TEXT("GP.Command.Move")))` у hot paths.
- Завжди `FGPGameplayTags::Get().Command_Move`.
- Якщо потрібен новий tag — added до `FGPGameplayTags` (native registration), не magic string.

## Logging

- Use named log category: `LogGP`, `LogGPGAS`, `LogGPUI`, `LogGPNet`.
- Verbose / VeryVerbose — для hot paths.
- Log це decisions, не tick spam. Якщо логуєш кожен tick — verbosity рівень `VeryVerbose`.

## Includes (IWYU)

- `#include` тільки потрібного.
- `#include "CoreMinimal.h"` — НЕ обов'язково; engine 5.x не вимагає його у кожному header. Включити, якщо потрібні base types.
- Не включати `Public/` heads з `Private/` без real reason.

## Smart Pointers / Refs

- `TObjectPtr<T>` для UPROPERTY pointers (UE5).
- `TWeakObjectPtr<T>` для transient references, що можуть стати invalid (units, що умирають).
- `TSubclassOf<T>` для class references.
- `TSoftObjectPtr<T>` для async-loadable assets (Data Assets, meshes).
- Не використовувати raw `T*` для UPROPERTY pointers у new code.

## Components

- New component declared as `UCLASS(ClassGroup=(GP), meta=(BlueprintSpawnableComponent))`.
- Кожен component має одну responsibility (single-purpose).
- Якщо component grows > 500 рядків — split.

## Naming Checklist

- `AGP_*` для Actor / Pawn.
- `UGP_*` для UObject / Component.
- `UGP_*Component` — components named with suffix.
- `IGP_*` / `UGP_*` для interfaces.
- `FGP_*` для structs.
- `EGP_*` для enums.

Все — per `/STYLE.md`.

## Forbidden Patterns

- `TArray<UObject*>` без `UPROPERTY()` (GC unsafe).
- Hardcoded balance numbers у `.cpp` (move to Data Asset).
- Magic-string `FGameplayTag::RequestGameplayTag` у gameplay code.
- `BlueprintImplementableEvent` для core gameplay (gameplay flow не повинен залежати від BP override).
- `BlueprintCallable` на server RPC implementation (виклик RPC — через PlayerController).
- Singletons (`static T* Instance` у gameplay class).
- Global state owners outside `UGameInstance` / proper subsystems.

## Tests

- У MVP — no automated tests required. Basic playtest проходить замість CI.
- Якщо логіка деonstrabile-testable (pure functions, no UE deps) — додавати у `Source/<Module>/Tests/` як `UAutomationTest` (Functional Test framework).

## PR Workflow

1. Branch from `main`: `feature/<jira-ticket>-<short-desc>`.
2. Atomic commits.
3. Run editor build + minimal smoke test before push.
4. PR description per template (`/CONTRIBUTING.md` → Pull Request Discipline).
5. Self-review diff before requesting review.

## References

- `/CONTRIBUTING.md` — engineering rules.
- `/STYLE.md` — style and naming.
- `../TDD/03_Multiplayer_Architecture.md` — replication details.
