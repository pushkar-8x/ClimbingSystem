// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomMovementComponent.generated.h"


UENUM(BluePrintType)
namespace ECustomMovementMode
{
	enum type
	{
		MOVE_Climb UMETA(DisplayName = "Climb Mode")
	};
}
/**
 * 
 */
UCLASS()
class CLIMBINGSYSTEM_API UCustomMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

#pragma region Overriden Methods
protected:
	virtual void TickComponent(float DeltaTime, 
		enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

public:
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
#pragma endregion

private:

#pragma region ClimbTraces

	TArray<FHitResult> DoCapsuleTraceMultiForObjects(const FVector& start, const FVector& end, bool bShowDebugShape = false , bool bShowPersistent = false);
	FHitResult DoLineTraceSingleObject(const FVector& start, const FVector& end, bool bShowDebugShape = false,bool bShowPersistent= false);

#pragma endregion

#pragma region ClimbCoreVariables

	TArray<FHitResult> ClimbableSurfacesTracedResults;
	FVector CurrentClimbableSurfaceLocation;
	FVector CurrentClimbableSurfaceNormal;

#pragma endregion

#pragma region ClimbBPVariables

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly, 
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		TArray<TEnumAsByte<EObjectTypeQuery>> ClimbableSurfaceTraceTypes;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly, 
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		float CapsuleTraceRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly, 
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		float CapsuleTraceHalfHeight = 72.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		float MaxBrakeClimbDeceleration = 72.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		float MaxClimbSpeed = 100.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		float MaxClimbAcceleration = 100.f;

#pragma endregion

#pragma region ClimbCore

	bool TraceClimbableSurfaces();
	FHitResult TraceFromEyeHeight(float TraceDistance , float TraceOffset = 0.f);

	bool CanStartClimbing();
	void StartClimbing();
	void StopClimbing();


	void PhysClimb(float deltaTime, int32 Iterations);

	void ProcessClimbableSurfaceInfo();

	bool CheckShouldStopClimbing();

	void SnapMovementToClimbableSurfaces(float deltaTime);

	FQuat GetClimbRotation(float DeltaTime);


public:
	void ToggleClimbing(bool bEnableClimb);
	bool IsClimbing() const;

	FORCEINLINE FVector GetClimbableSurfaceNormal() const { return CurrentClimbableSurfaceNormal; }

#pragma endregion
	
};
