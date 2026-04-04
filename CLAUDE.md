# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Open

**Open the project:**
- Double-click `ScoreFrenzy.uproject` to launch Unreal Editor
- Or open `ScoreFrenzy.sln` in Visual Studio (Windows)

**Regenerate project files** (after adding/removing .h/.cpp files):
```bash
# Windows
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="ScoreFrenzy.uproject" -game -rocket -progress

# Mac
/path/to/UE_5.7/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh -project="$(pwd)/ScoreFrenzy.uproject" -game
```

**Build configuration:** `Development Editor` / `Win64` (or `Mac`)

**Module dependencies** (from `ScoreFrenzy.Build.cs`): Core, CoreUObject, Engine, InputCore, EnhancedInput, ProceduralMeshComponent, NavigationSystem.

## Architecture Overview

All AI subsystems are `UActorComponent` subclasses attached to the **AI Controller** (not the pawn), except `UGATargetComponent` (attached to perceivable actors) and `UGAPerceptionSystem` (world-level coordinator).

```
AScoreFrenzyGameMode
├─ Spawns AGAPlayerCharacter (BP_PlayerCharacter blueprint)
└─ Spawns AGACharacter subclasses (Blueprint-based AIs)
   └─ Each AI has a Controller with:
      ├─ UGAPerceptionComponent   — vision cone, awareness tracking
      ├─ UGAPathComponent         — A* + Dijkstra path following
      └─ UGASpatialComponent      — composable cell-scoring for positioning
```

**The grid (`AGAGridActor`) is a singleton world actor** that all other systems reference. It provides cell ↔ world-space conversion and stores traversability flags. `FGAGridMap` is a float overlay on top of the grid used for distance maps, heatmaps, and scoring buffers.

## Key Design Patterns

**Grid-aligned reasoning:** All pathfinding, perception LOS tests, and spatial decisions operate on `FCellRef` grid coordinates, not raw world vectors.

**Spatial function composition:** `UGASpatialComponent::ChoosePosition()` samples cells in a radius, then runs them through a `UGASpatialFunction` (Blueprint-subclassable). Each function is a sequential chain of `FFunctionLayer` entries:
```
Accumulator = (((I0 op I1) op I2) op I3) ...
```
Where `ESpatialInput` picks what to measure (target range, LOS, cover from player, etc.) and `ESpatialOp` is Add or Multiply. `UGASpatialFunction_Cover` is the provided concrete subclass.

**Pathfinding — two-phase:** `GAPathComponent` first runs Dijkstra from the start (producing a `FGAGridMap` distance map), then backracks from the destination. `AStar()` wraps both phases. Path smoothing runs a separate pass after.

**Perception — awareness accumulation:** `UGAPerceptionComponent` tracks a `TMap<FGuid, FTargetView>` where `FTargetView.Awareness` floats between 0 and 1. Front cone (`FrontVisionAngle`) raises awareness faster than peripheral (`PeripheralVisionAngle`). Distance falloff applies. A target is "acquired" once awareness crosses a threshold.

**Occupancy maps:** `UGATargetComponent` maintains a `FGAGridMap OccupancyMap` representing where observers think the target might be. It spikes at the last known position and diffuses over time — used by AIs to search when LOS is lost.

**Perception system singleton:** `UGAPerceptionSystem::GetPerceptionSystem(WorldContextObject)` returns the world-level coordinator. It handles alert propagation (`PropagateAlertToNearest`) and, in Hard mode, tracks which cells AIs are attacking from (`RegisterAttackPosition`) to prevent stacking.

## AI Character Systems

**`AGACharacter`** (base for all AI pawns) owns:
- Health + `EHealingState` state machine: `Normal → Retreating → Healing`. Regeneration (`HealRate`) is suppressed while `bInCombat`.
- Patrol: `TArray<FScoutPatrolPoint> PatrolPoints` consumed by Behavior Tree tasks. Each point has per-direction scan pauses.
- `EDifficultyMode` (Easy/Medium/Hard).

**Behavior Trees** (Blueprint assets, not C++) implement the actual decision logic for each AI role (Scout, Fodder, Leader). C++ exposes BT Tasks and Services by inheriting from `UBTTaskNode`/`UBTService` where needed; the `AILogic/` directory is the intended home for those.

## Source Layout

```
Source/ScoreFrenzy/
├─ ScoreFrenzy.Build.cs          — module dependencies
├─ ScoreFrenzyGameMode.{h,cpp}   — game mode, default pawn setup
├─ AICharacter/                  — AGACharacter base pawn
├─ AILogic/Patrol/               — FScoutPatrolPoint struct; BT task stubs
├─ Grid/                         — AGAGridActor, FGAGridMap, FCellRef, FGridBox
├─ Pathfinding/                  — UGAPathComponent (A*, Dijkstra, smoothing)
├─ Perception/                   — UGAPerceptionSystem, UGAPerceptionComponent, UGATargetComponent
├─ Player/                       — AGAPlayerCharacter (Enhanced Input, spring-arm camera)
└─ Spatial/                      — UGASpatialComponent, UGASpatialFunction, UGASpatialFunction_Cover
```

## Test Maps

`Content/Maps/`: `TestMap` through `TestMap_Assignment5` — each validates a specific assignment's AI feature.
