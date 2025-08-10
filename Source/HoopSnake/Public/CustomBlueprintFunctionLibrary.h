// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"


#include "CustomBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class HOOPSNAKE_API UCustomBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable)
	static void ChangeCollisionOnPhysicsBody(USkeletalMeshComponent* skeletalMesh, FName boneName, ECollisionEnabled::Type CollisionType);

	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	static FVector GetConstraintReferencePosition(EConstraintFrame::Type Frame, UPhysicsConstraintComponent* Constraint);

};
