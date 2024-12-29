// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomMovementComponent.generated.h"

class UAnimMontage;
class UAnimInstance;
class AClimbingSystemCharacter;

DECLARE_DELEGATE(FOnEnterClimbState)
DECLARE_DELEGATE(FOnExitClimbState)


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


#pragma region Delegates
public:
		FOnEnterClimbState OnEnterClimbStateDelegate;
		FOnExitClimbState OnExitClimbStateDelegate;
#pragma endregion


#pragma region Overriden Methods
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, 
		enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;

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

	UAnimInstance* OwnerAnimInstance;
	AClimbingSystemCharacter* OwningPlayerCharacter;

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

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		float ClimbDownTraceOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "CharacterMovement : Movement", meta = (AllowPrivateAccess = "true"))
		float ClimbDownLedgeTraceOffset = 100.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "Animation", meta = (AllowPrivateAccess = "true"))
		UAnimMontage* IdleToClimbMontage;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "Animation", meta = (AllowPrivateAccess = "true"))
		UAnimMontage* ClimbToTopMontage;
	
	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "Animation", meta = (AllowPrivateAccess = "true"))
		UAnimMontage* ClimbDownLedgeMontage;
	
	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "Animation", meta = (AllowPrivateAccess = "true"))
		UAnimMontage* VaultMontage;
	
	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "Animation", meta = (AllowPrivateAccess = "true"))
		UAnimMontage* HopUpMontage;

	UPROPERTY(EditDefaultsOnly, BluePrintReadOnly,
		Category = "Animation", meta = (AllowPrivateAccess = "true"))
		UAnimMontage* HopDownMontage;




#pragma endregion

#pragma region ClimbCore

	bool TraceClimbableSurfaces();
	FHitResult TraceFromEyeHeight(float TraceDistance , float TraceOffset = 0.f ,
		bool bShowDebugShape = false, bool bShowPersistent = false);

	bool CanStartClimbing();
	void StartClimbing();
	void StopClimbing();

	void PlayAnimMontage(UAnimMontage* MontageToPlay);

	UFUNCTION()
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void SetMotionWarpTarget(const FName& InWarpTargetName, const FVector& InTargetPosition);

	void HandleHopUp();

	bool CheckCanHopUp(FVector& OutHopUpTargetPoint);

	void HandleHopDown();

	bool CheckCanHopDown(FVector& OutHopDownTargetPoint);

	void PhysClimb(float deltaTime, int32 Iterations);

	void ProcessClimbableSurfaceInfo();

	void TryStartVaulting();

	bool CanStartVaulting(FVector &OutVaultStartLocation , FVector& OutVaultLandPosition );

	bool CheckShouldStopClimbing();
	bool CheckHasTouchedGround();
	bool CheckHasReachedLedge();
	bool CanClimbDownLedge();

	void SnapMovementToClimbableSurfaces(float deltaTime);

	FQuat GetClimbRotation(float DeltaTime);


public:
	void ToggleClimbing(bool bEnableClimb);
	void RequestHopping();
	bool IsClimbing() const;

	FORCEINLINE FVector GetClimbableSurfaceNormal() const { return CurrentClimbableSurfaceNormal; }

	FVector GetUnrotatedClimbVelocity() const;

#pragma endregion
	
};
