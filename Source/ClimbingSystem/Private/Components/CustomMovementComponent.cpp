// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/CustomMovementComponent.h"
#include "ClimbingSystem/ClimbingSystemCharacter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/CapsuleComponent.h"
#include"Kismet/KismetMathLibrary.h"
#include "ClimbingSystem/DebugHelpers.h"
#include "ClimbingSystem/ClimbingSystemCharacter.h"
#include"MotionWarpingComponent.h"
#include "Kismet/GameplayStatics.h" 


void UCustomMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerAnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();

	if (OwnerAnimInstance)
	{
		OwnerAnimInstance->OnMontageEnded.AddDynamic(this, &UCustomMovementComponent::OnClimbMontageEnded);
		OwnerAnimInstance->OnMontageBlendingOut.AddDynamic(this, &UCustomMovementComponent::OnClimbMontageEnded);
	}
	OwningPlayerCharacter = Cast<AClimbingSystemCharacter>(CharacterOwner);
}

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (UpdatedComponent->GetComponentLocation().Z < -50.f)
	{
		FVector NewLocation = FVector(1780.0, 1330.0, 310.0);
		// Set the actor's location without teleporting
		UpdatedComponent->SetWorldLocationAndRotation(NewLocation, FRotator::ZeroRotator);
	}
}

void UCustomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
		OnEnterClimbStateDelegate.ExecuteIfBound();
	}
	if(PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		const FRotator DirtyRotation = UpdatedComponent->GetComponentRotation();
		const FRotator CleanStandRotation = FRotator(0.f, DirtyRotation.Yaw, 0.f);

		UpdatedComponent->SetRelativeRotation(CleanStandRotation);
		StopMovementImmediately();
		OnExitClimbStateDelegate.ExecuteIfBound();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UCustomMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (IsClimbing())
	{
		PhysClimb(deltaTime, Iterations);
	}

	Super::PhysCustom(deltaTime, Iterations);
}

FVector UCustomMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	bool isPlayingRMMontage = IsFalling() && OwnerAnimInstance && OwnerAnimInstance->IsAnyMontagePlaying();
	if (isPlayingRMMontage)
	{
		return RootMotionVelocity;
	}
	else
	{
		return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);
	}
}

#pragma region ClimbTraces
TArray<FHitResult> UCustomMovementComponent::DoCapsuleTraceMultiForObjects(const FVector& start, const FVector& end, bool bShowDebugShape , bool bShowPersistent )
{

	TArray<FHitResult> OutTraceHitResults;
	EDrawDebugTrace::Type debugTraceType = EDrawDebugTrace::None;
	if (bShowDebugShape)
	{
		debugTraceType = EDrawDebugTrace::ForOneFrame;

		if (bShowPersistent)
		{
			debugTraceType = EDrawDebugTrace::Persistent;
		}
	}

	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this,
		start,
		end,
		CapsuleTraceRadius,
		CapsuleTraceHalfHeight,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		debugTraceType,
		OutTraceHitResults,
		false
	);

	return OutTraceHitResults;
}

FHitResult UCustomMovementComponent::DoLineTraceSingleObject(const FVector& start, const FVector& end, bool bShowDebugShape , bool bShowPersistent )
{
	FHitResult OutTraceHitResult;

	EDrawDebugTrace::Type debugTraceType = EDrawDebugTrace::None;
	if (bShowDebugShape)
	{
		debugTraceType = EDrawDebugTrace::ForOneFrame;

		if (bShowPersistent)
		{
			debugTraceType = EDrawDebugTrace::Persistent;
		}
	}
	UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		start,
		end,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		debugTraceType,
		OutTraceHitResult,
		false
	);

	return OutTraceHitResult;
}
#pragma endregion

#pragma region ClimbCore

float UCustomMovementComponent::GetMaxSpeed() const
{
	if (IsClimbing())
	{
		return MaxClimbSpeed;
	}
	else
	{
		return Super::GetMaxSpeed();
	}
}

float UCustomMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())
	{
		return MaxClimbAcceleration;
	}
	else
	{
		return Super::GetMaxAcceleration();
	}
}

void UCustomMovementComponent::ToggleClimbing(bool bEnableClimbing)
{
	if (bEnableClimbing)
	{
		if (CanStartClimbing())
		{
			//StartClimbing();
			PlayAnimMontage(IdleToClimbMontage);
		}
		else if (CanClimbDownLedge())
		{
			//Debug::Print(TEXT("Climb Down Ledge"), 1, FColor::Blue);
			PlayAnimMontage(ClimbDownLedgeMontage);
		}
		else
		{
			//Debug::Print(TEXT("Can not climb down ledge"), 1, FColor::Red);
			TryStartVaulting();
		}
		
	}
	if(!bEnableClimbing)
	{
		//Stop Climbing
		StopClimbing();
	}
	
}

void UCustomMovementComponent::RequestHopping()
{
	const FVector UnrotatedLastInputVector =
		UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), GetLastInputVector());
	Debug::Print(UnrotatedLastInputVector.GetSafeNormal().ToString(), 1, FColor::Cyan);

	const float DotResult = FVector::DotProduct(UnrotatedLastInputVector.GetSafeNormal(), FVector::UpVector);
	Debug::Print(TEXT("DotResult") + FString::SanitizeFloat(DotResult));

	if (DotResult >= 0.9f)
	{
		Debug::Print(TEXT("Hop up!"));
		HandleHopUp();

	}
	else if(DotResult < -0.9f)
	{
		Debug::Print(TEXT("Hop down!"));
		HandleHopDown();
	}
}

bool UCustomMovementComponent::CanStartClimbing()
{
	if (IsFalling()) return false;
	if (!TraceClimbableSurfaces())return false;
	if (!TraceFromEyeHeight(100.f).bBlockingHit)return false;

	return true;
}

void UCustomMovementComponent::StartClimbing()
{
	SetMovementMode(MOVE_Custom, ECustomMovementMode::MOVE_Climb);
}

void UCustomMovementComponent::StopClimbing()
{
	SetMovementMode(MOVE_Falling);
}

void UCustomMovementComponent::PlayAnimMontage(UAnimMontage* MontageToPlay)
{
	if (!MontageToPlay || !OwnerAnimInstance) return;
	if (OwnerAnimInstance->IsAnyMontagePlaying())return;

	OwnerAnimInstance->Montage_Play(MontageToPlay);
}

void UCustomMovementComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	Debug::Print("Climb Montage Ended");

	if (Montage == IdleToClimbMontage || Montage == ClimbDownLedgeMontage)
	{
		StartClimbing();
		StopMovementImmediately();
	}
	if(Montage == ClimbToTopMontage || Montage==VaultMontage)
	{
		SetMovementMode(MOVE_Walking);
	}
}

void UCustomMovementComponent::SetMotionWarpTarget(const FName& InWarpTargetName, const FVector& InTargetPosition)
{
	if (!OwningPlayerCharacter)return;

	OwningPlayerCharacter->GetMotionWarpingComponent()->
		AddOrUpdateWarpTargetFromLocation(InWarpTargetName , InTargetPosition);
}

void UCustomMovementComponent::HandleHopUp()
{
	FVector HopUpTargetLocation;
	if (CheckCanHopUp(HopUpTargetLocation))
	{
		SetMotionWarpTarget(FName("HopupTargetLocation"), HopUpTargetLocation);
		PlayAnimMontage(HopUpMontage);
	}
}

bool UCustomMovementComponent::CheckCanHopUp(FVector& OutHopUpTargetPoint)
{
	FHitResult HopUpHit = TraceFromEyeHeight(100.f, -20.f, true);
	FHitResult SafetyLedgeHit = TraceFromEyeHeight(100.f, 150.f, true);

	if (HopUpHit.bBlockingHit && SafetyLedgeHit.bBlockingHit)
	{
		OutHopUpTargetPoint = HopUpHit.ImpactPoint;
		return true;
	}
	return false;
}

void UCustomMovementComponent::HandleHopDown()
{
	FVector HopDownTargetLocation;
	if (CheckCanHopDown(HopDownTargetLocation))
	{
		SetMotionWarpTarget(FName("HopDownTargetLocation"), HopDownTargetLocation);
		PlayAnimMontage(HopDownMontage);
	}
}

bool UCustomMovementComponent::CheckCanHopDown(FVector& OutHopDownTargetPoint)
{
	FHitResult HopDownHit = TraceFromEyeHeight(100.f, -300.f, true);

	if (HopDownHit.bBlockingHit)
	{
		OutHopDownTargetPoint = HopDownHit.ImpactPoint;
		return true;
	}
	return false;
}

void UCustomMovementComponent::PhysClimb(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	TraceClimbableSurfaces();
	ProcessClimbableSurfaceInfo();

	if (CheckShouldStopClimbing() || CheckHasTouchedGround())
	{
		StopClimbing();
	}

	RestorePreAdditiveRootMotionVelocity();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime,0.f , true, MaxBrakeClimbDeceleration);
	}

	ApplyRootMotionToVelocity(deltaTime);

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);

	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	SnapMovementToClimbableSurfaces(deltaTime);

	if (CheckHasReachedLedge())
	{
		PlayAnimMontage(ClimbToTopMontage);
	}
	else
	{
		//Debug::Print(TEXT("Has not reached Ledge"),1);
	}
}

FQuat UCustomMovementComponent::GetClimbRotation(float DeltaTime)
{
	const FQuat currentQuat = UpdatedComponent->GetComponentQuat();

	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return currentQuat;
	}

	FQuat targetQuat = FRotationMatrix::MakeFromX(-CurrentClimbableSurfaceNormal).ToQuat();
	return FMath::QInterpTo(currentQuat, targetQuat, DeltaTime, 5.f);
}

void UCustomMovementComponent::SnapMovementToClimbableSurfaces(float deltaTime)
{
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();

	const FVector ProjectedCharacterToSurface = (CurrentClimbableSurfaceLocation - ComponentLocation)
		.ProjectOnTo(ComponentForward);

	const FVector SnapVector = -CurrentClimbableSurfaceNormal * ProjectedCharacterToSurface.Length();

	UpdatedComponent->MoveComponent(SnapVector * deltaTime * MaxClimbSpeed ,
		UpdatedComponent->GetComponentRotation() , true);

	
}

void UCustomMovementComponent::ProcessClimbableSurfaceInfo()
{
	CurrentClimbableSurfaceLocation = FVector::ZeroVector;
	CurrentClimbableSurfaceNormal = FVector::ZeroVector;

	if (ClimbableSurfacesTracedResults.IsEmpty())return;

	for (const FHitResult& TracedHitResult : ClimbableSurfacesTracedResults)
	{
		CurrentClimbableSurfaceLocation += TracedHitResult.ImpactPoint;
		CurrentClimbableSurfaceNormal += TracedHitResult.ImpactNormal;
	}

	CurrentClimbableSurfaceLocation /= ClimbableSurfacesTracedResults.Num();
	CurrentClimbableSurfaceNormal = CurrentClimbableSurfaceNormal.GetSafeNormal();

	//Debug::Print(TEXT("CurrentClimbableSurfaceLocation") + CurrentClimbableSurfaceLocation.ToCompactString(),1, FColor::Cyan);
	//Debug::Print(TEXT("CurrentClimbableSurfaceNormal") + CurrentClimbableSurfaceNormal.ToCompactString(), 2, FColor::Cyan);
}

bool UCustomMovementComponent::CheckShouldStopClimbing()
{
	if (ClimbableSurfacesTracedResults.IsEmpty()) return true;

	const float DotProduct = FVector::DotProduct(CurrentClimbableSurfaceNormal, FVector::UpVector);
	const float angleDiff = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	if (angleDiff <= 60.f) return true;

	//Debug::Print(TEXT("Angle Diff") + FString::SanitizeFloat(angleDiff), 1, FColor::Green);

	return false;
}

bool UCustomMovementComponent::CheckHasTouchedGround()
{
	const FVector DownVector = -UpdatedComponent->GetUpVector();
	const FVector StartOffset = DownVector * 50.f;

	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + DownVector;

	const TArray<FHitResult> GroundHitResults = DoCapsuleTraceMultiForObjects(Start, End);

	if (GroundHitResults.IsEmpty()) return false;

	for(const FHitResult &HitResult : GroundHitResults)
	{
		const bool bFloorReached = FVector::Parallel(-HitResult.ImpactNormal, FVector::UpVector)
			&& GetUnrotatedClimbVelocity().Z < -10.f;

		if (bFloorReached)
		{
			return true;
		}
	}
	return false;
}

bool UCustomMovementComponent::CheckHasReachedLedge()
{
	FHitResult LedgeHitResult = TraceFromEyeHeight(100.f, 50.f);
	if (!LedgeHitResult.bBlockingHit)
	{
		const FVector WalkableSurfaceTraceStart = LedgeHitResult.TraceEnd;
		const FVector DownVector = -UpdatedComponent->GetUpVector();
		const FVector WalkableSurfaceTraceEnd = WalkableSurfaceTraceStart + DownVector * 100.f;

		FHitResult WalkableSurfaceHitResult = 
			DoLineTraceSingleObject(WalkableSurfaceTraceStart, WalkableSurfaceTraceEnd);

		if (WalkableSurfaceHitResult.bBlockingHit && GetUnrotatedClimbVelocity().Z > 10.f)
		{
			return true;
		}
	}

	return false;

}

void UCustomMovementComponent::TryStartVaulting()
{
	FVector VaultStartPosition;
	FVector VaultLandPosition;

	if (CanStartVaulting(VaultStartPosition , VaultLandPosition))
	{
		/*Debug::Print(TEXT("VaultStartPosition" + VaultStartPosition.ToCompactString()));
		Debug::Print(TEXT("VaultLandPosition" + VaultLandPosition.ToCompactString()));*/
		SetMotionWarpTarget(FName("VaultStartPoint"), VaultStartPosition);
		SetMotionWarpTarget(FName("VaultEndPoint"), VaultLandPosition);
		StartClimbing();
		PlayAnimMontage(VaultMontage);
	}

	
}

bool UCustomMovementComponent::CanStartVaulting(FVector &OutVaultStartPosition , FVector& OutVaultLandPosition)
{
	if (IsFalling()) return false;

	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector UpVector = UpdatedComponent->GetUpVector();
	const FVector ForwardVector = UpdatedComponent->GetForwardVector();
	const FVector DownVector = -UpdatedComponent->GetUpVector();

	OutVaultStartPosition = FVector::ZeroVector;
	OutVaultLandPosition = FVector::ZeroVector;

	for(int32 i = 0 ; i < 5 ; i++)
	{
		const FVector Start = ComponentLocation + UpVector * 100.f + ForwardVector * 80.f * (i + 1);
		const FVector End = Start + DownVector * 100.f * (i + 1);

		FHitResult VaultTraceHitResult =  DoLineTraceSingleObject(Start, End);
		if (i == 0 && VaultTraceHitResult.bBlockingHit)
		{
			OutVaultStartPosition = VaultTraceHitResult.ImpactPoint;
		}
		
		if (i == 3 && VaultTraceHitResult.bBlockingHit)
		{
			OutVaultLandPosition = VaultTraceHitResult.ImpactPoint;
		}
	}

	if (OutVaultStartPosition != FVector::ZeroVector &&
		OutVaultLandPosition != FVector::ZeroVector)
	{
		return true;
	}
	else
	{
		return false;
	}

	
}


bool UCustomMovementComponent::CanClimbDownLedge()
{
	if (IsFalling()) return false;

	const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
	const FVector ForwardVector = UpdatedComponent->GetForwardVector();
	const FVector DownVector = -UpdatedComponent->GetUpVector();

	const FVector TraceStartPos = CurrentLocation + ForwardVector * ClimbDownTraceOffset;
	const FVector TraceEndPos = TraceStartPos + DownVector * 100.f;


	FHitResult WalkableSurfaceTraceHit = DoLineTraceSingleObject(TraceStartPos, TraceEndPos);

	const FVector LedgeTraceStart = WalkableSurfaceTraceHit.TraceStart + ForwardVector * ClimbDownLedgeTraceOffset;
	const FVector LedgeTraceEnd = LedgeTraceStart + DownVector * 200.f;

	FHitResult LedgeTraceHit = DoLineTraceSingleObject(LedgeTraceStart, LedgeTraceEnd);

	if (WalkableSurfaceTraceHit.bBlockingHit && !LedgeTraceHit.bBlockingHit)
	{
		return true;
	}
	return false;
}

bool UCustomMovementComponent::IsClimbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::MOVE_Climb;

}

bool UCustomMovementComponent::TraceClimbableSurfaces()
{
	const FVector startOffset = UpdatedComponent->GetForwardVector() * 30.f;
	const FVector start = startOffset + UpdatedComponent->GetComponentLocation();
	const FVector end = start + UpdatedComponent->GetForwardVector();

    ClimbableSurfacesTracedResults = DoCapsuleTraceMultiForObjects(start, end );

	return !ClimbableSurfacesTracedResults.IsEmpty();
}

FHitResult UCustomMovementComponent::TraceFromEyeHeight(float TraceDistance, float TraceOffset , bool bShowDebugShape , bool bShowPersistent)
{
	const FVector componentLocation = UpdatedComponent->GetComponentLocation();
	const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceOffset);

	const FVector start = componentLocation + EyeHeightOffset;
	const FVector end = start + UpdatedComponent->GetForwardVector() * TraceDistance;

	return DoLineTraceSingleObject(start, end , bShowDebugShape , bShowPersistent);
}

FVector UCustomMovementComponent::GetUnrotatedClimbVelocity() const
{
	return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), Velocity);
}


#pragma endregion