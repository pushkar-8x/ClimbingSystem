// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/CustomMovementComponent.h"
#include "ClimbingSystem/ClimbingSystemCharacter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/CapsuleComponent.h"
#include "ClimbingSystem/DebugHelpers.h"

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	/*TraceClimbableSurfaces();
	TraceFromEyeHeight(100.f);*/
}

void UCustomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}
	if(PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		const FRotator DirtyRotation = UpdatedComponent->GetComponentRotation();
		const FRotator CleanStandRotation = FRotator(0.f, DirtyRotation.Yaw, 0.f);

		UpdatedComponent->SetRelativeRotation(CleanStandRotation);
		StopMovementImmediately();
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
			//Debug::Print("Climbing");
			StartClimbing();
		}
		
	}
	else
	{
		//Stop Climbing
		StopClimbing();
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

void UCustomMovementComponent::PhysClimb(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	TraceClimbableSurfaces();
	ProcessClimbableSurfaceInfo();

	if (CheckShouldStopClimbing())
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

FHitResult UCustomMovementComponent::TraceFromEyeHeight(float TraceDistance, float TraceOffset)
{
	const FVector componentLocation = UpdatedComponent->GetComponentLocation();
	const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceOffset);

	const FVector start = componentLocation + EyeHeightOffset;
	const FVector end = start + UpdatedComponent->GetForwardVector() * TraceDistance;

	return DoLineTraceSingleObject(start, end);
}


#pragma endregion