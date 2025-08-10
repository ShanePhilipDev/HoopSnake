// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomBlueprintFunctionLibrary.h"

void UCustomBlueprintFunctionLibrary::ChangeCollisionOnPhysicsBody(USkeletalMeshComponent* skeletalMesh, FName boneName, ECollisionEnabled::Type CollisionType)
{
	skeletalMesh->GetBodyInstance(boneName)->SetShapeCollisionEnabled(0, CollisionType);
}

FVector UCustomBlueprintFunctionLibrary::GetConstraintReferencePosition(EConstraintFrame::Type Frame, UPhysicsConstraintComponent* Constraint)
{
	if (Frame == EConstraintFrame::Frame1)
	{
		return Constraint->ConstraintInstance.Pos1;
	}
	else
	{
		return Constraint->ConstraintInstance.Pos2;
	}
}