#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SFSoldierTypes.h"
#include "ScoreFrenzy/Pathfinding/SFPathComponent.h"
#include "ScoreFrenzy/Perception/SFPerceptionComponent.h"
#include "ScoreFrenzy/Grid/SFGridActor.h"
#include "SFSoldierController.generated.h"

class ASFSoldierCharacter;

/**
 * AI controller for Soldier characters.
 *
 * Implements a C++ state machine with three difficulty modes (Easy / Medium / Hard).
 * The difficulty is usually set at runtime by the GameMode based on the player's score.
 *
 * Difficulty overview
 * -------------------
 * Easy   – Random patrol; chase/attack on sight; lose sight → back to patrol.
 * Medium – Ordered waypoint patrol; footstep awareness; alert sharing (nearest 1 AI every 5 s);
 *          cover-based attack cycle (attack 3 s / hide 2 s); investigate last known position;
 *          retreat + heal at low health.
 * Hard   – All Medium features; immediate alert to 3 nearest AIs + 3 more every 3 s;
 *          find cover before healing; avoid stacking at same attack position;
 *          faster attack/hide cycle (1 s / 1 s).
 *
 * Blueprint integration
 * ---------------------
 * - Set PatrolWaypoints in the Blueprint subclass or from the level.
 * - Read bCanShoot to know when to fire (play fire animation / spawn projectile in Blueprint).
 * - Bind to GASoldierCharacter::OnDeath for cleanup.
 * - Call SetDifficulty() whenever the player's score tier changes.
 */
UCLASS(BlueprintType, Blueprintable)
class SCOREFRENZY_API ASFSoldierController : public AAIController
{
	GENERATED_BODY()

public:
	ASFSoldierController();

	// ---- Components ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<USFPathComponent> PathComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<USFPerceptionComponent> PerceptionComp;

	// ---- Difficulty ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Difficulty")
	ESFSoldierDifficulty Difficulty = ESFSoldierDifficulty::Easy;

	// Apply a new difficulty, reconfiguring all behaviour parameters.
	UFUNCTION(BlueprintCallable, Category = "Difficulty")
	void SetDifficulty(ESFSoldierDifficulty NewDifficulty);

	// ---- Patrol ----

	// Waypoints cycled in order for Medium/Hard. If empty, falls back to random cells.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	TArray<FVector> PatrolWaypoints;

	// ---- Combat parameters (overridden by SetDifficulty) ----

	// Distance within which the soldier will switch from Chase to Attack
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackRange = 800.0f;

	// Distance the soldier must reach from the player before it enters Heal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float RetreatDistance = 2000.0f;

	// Radius searched for cover cells
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float CoverSearchRadius = 1500.0f;

	// Seconds spent exposing to attack during the cover cycle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackPhaseDuration = 3.0f;

	// Seconds spent hiding during the cover cycle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HidePhaseDuration = 2.0f;

	// ---- Footstep awareness (Medium/Hard) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float FootstepRadius = 600.0f;

	// Awareness per second added when the player is nearby and moving
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float FootstepAwarenessRate = 0.4f;

	// ---- Alert sharing ----

	// Seconds between successive alert-share pulses
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	float AlertShareInterval = 5.0f;

	// Number of AIs notified per pulse
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	int32 AlertShareCount = 1;

	// ---- Position spreading (Hard) ----

	// Maximum number of AIs allowed at the same attack position
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 MaxAIPerAttackPosition = 2;

	// Radius used to determine "same position"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float PositionSpreadRadius = 300.0f;

	// ---- State (readable from Blueprint) ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ESFSoldierState CurrentState = ESFSoldierState::Patrol;

	// True during phases when this soldier should be firing at the player.
	// Blueprint reads this to trigger shooting animation / projectile.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bCanShoot = false;

	// Awareness threshold above which this AI considers itself "aware" of the player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float AwarenessThreshold = 0.9f;

	// ---- AAIController interface ----

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- Public API ----

	// Called by another soldier to share the player's last-known position with this one
	UFUNCTION(BlueprintCallable, Category = "Alert")
	void ReceiveAlertMessage(FGuid TargetGuid, float AwarenessValue = 1.0f);

private:
	// ---- Feature flags (set by SetDifficulty) ----
	bool bRandomPatrol          = true;
	bool bFootstepAwareness     = false;
	bool bAlertSharing          = false;
	bool bAlertShareImmediate   = false;  // Hard: share immediately on first detection
	bool bInvestigate           = false;
	bool bCoverAttack           = false;
	bool bCanRetreatAndHeal     = false;
	bool bFindCoverBeforeHeal   = false;
	bool bSpreadPositions       = false;

	// ---- Patrol ----
	int32 CurrentWaypointIndex = 0;
	bool  bWaypointInProgress  = false;

	// ---- Cover cycle ----
	ESFCoverCyclePhase CoverPhase = ESFCoverCyclePhase::MovingToCover;
	float CoverCycleTimer       = 0.0f;
	FVector CoverPosition       = FVector::ZeroVector;
	FVector AttackPosition      = FVector::ZeroVector;
	bool  bHasCover             = false;

	// ---- Retreat ----
	FVector RetreatDestination  = FVector::ZeroVector;
	bool  bRetreatPathSet       = false;

	// ---- Alert sharing ----
	float AlertShareTimer       = 0.0f;
	bool  bAlertInitiated       = false;  // whether we've fired the first-detection alert

	// ---- Grid cache ----
	mutable TWeakObjectPtr<ASFGridActor> CachedGrid;

	// ---- Per-state update methods ----
	void UpdatePatrol   (float DeltaSeconds);
	void UpdateChase    (float DeltaSeconds);
	void UpdateAttack   (float DeltaSeconds);
	void UpdateInvestigate(float DeltaSeconds);
	void UpdateRetreat  (float DeltaSeconds);
	void UpdateHeal     (float DeltaSeconds);

	// Evaluate all possible state transitions and switch if needed
	void CheckStateTransitions();

	// Clean up the old state and initialise the new one
	void TransitionTo(ESFSoldierState NewState);

	// ---- Behaviour helpers ----

	// Footstep awareness: if the player is within FootstepRadius and moving, add awareness
	void UpdateFootstepAwareness(float DeltaSeconds);

	// Share the player's last-known position with up to Count nearest unaware AIs
	void ShareAlert(int32 Count);

	// ---- Spatial helpers ----

	ASFGridActor* GetGrid() const;

	// A random traversable cell anywhere on the grid
	FVector GetRandomPatrolPoint() const;

	// Nearest cover cell (player has no LOS to it) within CoverSearchRadius.
	// If bSpreadPositions is true, positions already occupied by MaxAIPerAttackPosition AIs are skipped.
	// Returns false if no suitable cover was found.
	bool FindCoverAndAttackPositions(FVector PlayerPos, FVector& OutCoverPos, FVector& OutAttackPos) const;

	// Find a cell that is at least RetreatDistance from PlayerPos.
	// If bFindCoverBeforeHeal, the destination also has no player LOS.
	FVector FindRetreatDestination(FVector PlayerPos) const;

	// How many OTHER soldiers are at or moving toward Position
	int32 CountAIAtPosition(FVector Position) const;

	// Convenience: last known player world position from the perception system
	bool GetPlayerInfo(FVector& OutPosition, bool& OutImmediate) const;

	// Distance to the player's last known position (MAX_FLT if unknown)
	float GetDistanceToPlayer() const;
};
