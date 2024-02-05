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
		StopMovementImmediately();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UCustomMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (IsClimbing())
	{

	}

	Super::PhysCustom(deltaTime, Iterations);
}

#pragma region ClimbTraces
TArray<FHitResult> UCustomMovementComponent::DoCapsuleTraceMultiForObjects(const FVector& start, const FVector& end, bool bShowDebugShape , bool bShowPersistent)
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

FHitResult UCustomMovementComponent::DoLineTraceSingleObject(const FVector& start, const FVector& end, bool bShowDebugShape , bool bShowPersistent)
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

void UCustomMovementComponent::ToggleClimbing(bool bEnableClimbing)
{
	if (bEnableClimbing)
	{
		if (CanStartClimbing())
		{
			Debug::Print("Climbing");
			StartClimbing();
		}
		else
		{
			Debug::Print("Not Climbing");

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

bool UCustomMovementComponent::IsClimbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::MOVE_Climb;

}

bool UCustomMovementComponent::TraceClimbableSurfaces()
{
	const FVector startOffset = UpdatedComponent->GetForwardVector() * 30.f;
	const FVector start = startOffset + UpdatedComponent->GetComponentLocation();
	const FVector end = start + UpdatedComponent->GetForwardVector();

    ClimbableSurfacesTracedResults = DoCapsuleTraceMultiForObjects(start, end, true,true);

	return !ClimbableSurfacesTracedResults.IsEmpty();
}

FHitResult UCustomMovementComponent::TraceFromEyeHeight(float TraceDistance, float TraceOffset)
{
	const FVector componentLocation = UpdatedComponent->GetComponentLocation();
	const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceOffset);

	const FVector start = componentLocation + EyeHeightOffset;
	const FVector end = start + UpdatedComponent->GetForwardVector() * TraceDistance;

	return DoLineTraceSingleObject(start, end, true,true);
}


#pragma endregion