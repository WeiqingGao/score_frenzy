#pragma once

#include "CoreMinimal.h"
#include "SFSoldierTypes.generated.h"

// Difficulty modes driven by the player's score
UENUM(BlueprintType)
enum class ESFSoldierDifficulty : uint8
{
	Easy   UMETA(DisplayName = "Easy"),
	Medium UMETA(DisplayName = "Medium"),
	Hard   UMETA(DisplayName = "Hard"),
};

// States of the soldier AI state machine
UENUM(BlueprintType)
enum class ESFSoldierState : uint8
{
	Patrol,      // Wandering / patrolling waypoints
	Chase,       // Moving toward last known player position
	Attack,      // Engaging the player (includes cover cycle for Medium/Hard)
	Investigate, // Moving to last known position after losing sight (Medium/Hard only)
	Retreat,     // Moving away from player due to low health (Medium/Hard only)
	Heal,        // Out of combat, recovering health (Medium/Hard only)
};

// Sub-phases of the cover-based attack cycle (Medium/Hard)
UENUM(BlueprintType)
enum class ESFCoverCyclePhase : uint8
{
	MovingToCover,  // Approaching cover while shooting if LOS available
	Attacking,      // Stepped out to attack position, shooting for AttackDuration
	Hiding,         // Behind cover, not shooting, waiting for CoverDuration
};
