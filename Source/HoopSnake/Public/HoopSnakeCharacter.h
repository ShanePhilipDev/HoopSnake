// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CharacterAnimationInterface.h"

#include "HoopSnakeCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UCameraShakeSourceComponent;
class UInputMappingContext;
class UInputAction;
class USoundCue;
class UNiagaraComponent;
class APhysicsConstraintActor;
struct FInputActionValue;

UCLASS()
class HOOPSNAKE_API AHoopSnakeCharacter : public ACharacter, public ICharacterAnimationInterface
{
	GENERATED_BODY()

public:
	/** Sets default values for this character's properties */
	AHoopSnakeCharacter();

	/** Use static meshes for snake head since I couldn't find a free snake model that was rigged correctly */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* UpperJaw;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* LowerJaw;
	/***/

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	// *** INPUT *** //
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/** Hoop Toggle Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* HoopAction;

	/** Reset Snake Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ResetAction;

	/** Pause Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* PauseAction;

	/** Current state of hoop toggle input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input, meta = (AllowPrivateAccess = "true"))
	bool bHoopToggle;

	/** Whether an attack has been queued */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input, meta = (AllowPrivateAccess = "true"))
	bool bAttackQueued;

	/** Movement speed when in hoop mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input, meta = (AllowPrivateAccess = "true"))
	float HoopSpeed;

	/** Movement speed when not in hoop mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input, meta = (AllowPrivateAccess = "true"))
	float DefaultSpeed;
	// ************* //

	// *** SOUND *** //
	/** Whoosh Sound Effect - used when ragdolling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (AllowPrivateAccess = "true"))
	USoundCue* WhooshSound;

	/** Hiss Sound Effect - used when attacking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (AllowPrivateAccess = "true"))
	USoundCue* HissSound;

	/** Hiss Sound Effect - used when colliding */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (AllowPrivateAccess = "true"))
	USoundCue* ImpactSound;

	/** Bite sound effect */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (AllowPrivateAccess = "true"))
	USoundCue* BiteSound;
	// ************* //

	/** Bite physics constraint class. Must be set in blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Default, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<APhysicsConstraintActor> BiteConstraintClass;

	/** Basically a camera shake emitter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraShakeSourceComponent* CameraShakeComponent;

	/** Line emitter to convey speed in hoop mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Default, meta = (AllowPrivateAccess = "true"))
	UNiagaraComponent* SpeedLineEffect;

protected:
	/** Called when the game starts or when spawned */
	virtual void BeginPlay() override;

	/** Movement function for ragdoll mode. Applies force in a direction instead of using the movement component. */
	void RagdollMovement(FVector ForwardDirection, FVector RightDirection, FVector2D MovementVector);

	/** Sets a timer as a cooldown between ragdoll hops */
	void TriggerRagdollMovementCooldown();

	/** Update the camera properties depending on the state of the character */
	void UpdateCamera(float DeltaTime);

	/** Constantly apply forward movement when in hoop mode */
	void ApplyHoopMovement(float DeltaTime);

	/** Updates certain properties that are accessed by the animation blueprint. */
	void UpdateAnimationProperties();

	/** Force the snake into a ragdoll state */
	UFUNCTION(BlueprintCallable, Category = Ragdoll)
	void ForceRagdoll();

	/** Returns the dot product of the character's forward vector between ticks.
	 * When dot product is 0, we are moving straight forward. When it is less than 0, we are turning right. When it is more than 0, we are turning left. */
	float TurningDotProduct();

	/** Hoop Mode State */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = HoopMode)
	bool bHoopModeEnabled;

	// NOTE: when need to get ragdoll state, check if mesh is simulating physics

	/** The force applied when attempting to move as a ragdoll */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	float RagdollMovementForce;

	/** The speed threshold below which a ragdoll is allowed to move */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	float RagdollMovementThreshold;

	/** Whether ragdoll movement is on cooldown or not */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	bool bIsMovementOnCooldown;

	/** How long the ragdoll movement cooldown period should last */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	float MovementCooldownDuration;

	/** Whether the snake is biting an object or not. Only possible while ragdolling. */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	bool bIsBiting;

	/** The name of the bone located at the snake's head */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	FName HeadBoneName;

	/** Forward and up force to be applied when attacking */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	float AttackForceForward;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	float AttackForceUp;
	/***/

	/** The delay when switching back from ragdoll mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	float ResetDelay;

	/** If the mesh has been forced to ragdoll by something other than an attack i.e. colliding with a wall while in hoop mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Ragdoll)
	bool bIsForcedRagdoll;

	/** The default camera field of view */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	float DefaultFOV;

	/** The camera field of view when in hoop mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	float HoopFOV;

	/** The default camera boom arm length */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	float DefaultArmLength;

	/** The camera boom arm length when in hoop mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	float HoopArmLength;

	/** The default camera offset position relative to the end of the boom */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	FVector DefaultCameraOffset;

	/** The camera offset position when in hoop mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	FVector HoopCameraOffset;

	/** The default boom position, relative to the collision capsule */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	FVector DefaultBoomPosition;

	/** The boom position when in hoop mode, relative to the collision capsule */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	FVector HoopBoomPosition;

	/** The boom position when ragdolling, relative to the mesh's head */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	FVector RagdollBoomPosition;

	/** The capsule component's forward vector in the previous tick */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	FVector PreviousForward;

	/** The speed used when interpolating camera properties */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Camera)
	float CameraInterpSpeed;

	/** The current tilt of the mesh while turning in hoop mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = HoopMode)
	float CurrentTilt;

	/** The desired tilt of the mesh while turning in hoop mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = HoopMode)
	float DesiredTilt;

	/** Affects the strength of tilt applied while turning in hoop mode */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = HoopMode)
	float TiltModifier;

	/** The speed used when interpolating tilt */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = HoopMode)
	float TiltInterpSpeed;

	/** Whether the snake's slither animation should be reversed */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Animation)
	bool bReverseSlither;

	/** Whether the character is currently rotating */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Animation)
	bool bIsRotating;

	/** The rate at which the character is rotating */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Animation)
	float RotateRate;

	/** Mesh offset from capsule component to the ground */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Default)
	FVector MeshOffset;

public:	
	/** Called every frame */
	virtual void Tick(float DeltaTime) override;

	/** Called to bind functionality to input */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Toggles hoop mode, with an attack being queued when toggled off */
	void ToggleHoop(const FInputActionValue& Value);

	/** Reset the snake from ragdoll mode */
	void Reset();

	/** Finish the reset process, should be called through a timer */
	void FinishReset();

	/** Interface function override for triggering attacks based on animation state */
	void TriggerAttack_Implementation() override;

	/** Removes all HUD elements from the screen */
	void ClearHUD();

	/** Make noise when snake is moving to alert AI controlled characters */
	void MovementNoise();

	/** Pause the game */
	void Pause();

	/** Handles mesh collision */
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Attempt to attach to a victim */
	void AttemptBite(const FHitResult& Hit);

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
