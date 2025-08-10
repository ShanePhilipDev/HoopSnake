// Fill out your copyright notice in the Description page of Project Settings.


#include "HoopSnakeCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "HUDInterface.h"
#include "GameFramework/HUD.h"
#include "Camera/CameraShakeSourceComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

// Sets default values
AHoopSnakeCharacter::AHoopSnakeCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Speed values for character input
	DefaultSpeed = 300.0f;
	HoopSpeed = 1000.0f;

	// Offset mesh from capsule
	MeshOffset = FVector(0.0f, 0.0f, -50.0f);
	GetMesh()->SetRelativeLocation(MeshOffset);

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.0f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = DefaultSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = DefaultSpeed / 2.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 100.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraLagSpeed = 15.0f;
	CameraBoom->ProbeSize = 5.0f;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Create camera shake component
	CameraShakeComponent = CreateDefaultSubobject<UCameraShakeSourceComponent>(TEXT("CameraShakeComponent"));
	CameraShakeComponent->SetupAttachment(FollowCamera);

	// Create niagara component for speed lines, ** NOTE: defaults set in blueprint
	SpeedLineEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SpeedLineEffect"));
	SpeedLineEffect->SetupAttachment(GetCapsuleComponent());

	// Create head meshes. Mesh properties set in blueprint.
	UpperJaw = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperJaw"));
	LowerJaw = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerJaw"));

	// Attach head to mesh.
	UpperJaw->SetupAttachment(GetMesh(), "HeadSocket");
	LowerJaw->SetupAttachment(GetMesh(), "HeadSocket");

	// Defaults
	bHoopModeEnabled = false;
	bIsBiting = false;

	bIsMovementOnCooldown = false;
	MovementCooldownDuration = 1.0f;
	RagdollMovementForce = 300.0f;
	RagdollMovementThreshold = 10.0f;
	HeadBoneName = "Bone"; // bone name from random rope mesh I found, update if a proper snake mesh is found
	AttackForceForward = 2500.0f;
	AttackForceUp = 2000.0f;
	ResetDelay = 0.5f;

	DefaultFOV = 90.0f;
	HoopFOV = 120.0f;
	DefaultArmLength = 250.0f;
	HoopArmLength = 175.0f;
	DefaultCameraOffset = FVector(0.0f, -10.0f, 10.0f);
	HoopCameraOffset = FVector(0.0f, 0.0f, 10.0f);
	DefaultBoomPosition = FVector(-5.0f, 0.0, -30.0f);
	HoopBoomPosition = FVector(-5.0f, 0.0f, 30.0f);
	RagdollBoomPosition = FVector(0.0f, 0.0f, 30.0f);
	CameraInterpSpeed = 10.0f;

	CurrentTilt = 0.0f;
	DesiredTilt = 0.0f;
	TiltModifier = 300.0f;
	TiltInterpSpeed = 7.5f;

	PreviousForward = FVector(0);
	
}

// Called when the game starts or when spawned
void AHoopSnakeCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	GetMesh()->OnComponentHit.AddDynamic(this, &AHoopSnakeCharacter::OnMeshHit);
}

// Called every frame
void AHoopSnakeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update camera properties, including the boom arm it is attached to
	UpdateCamera(DeltaTime);

	if (bHoopModeEnabled)
	{
		// Moves the character forward and applies tilt to the mesh.
		ApplyHoopMovement(DeltaTime);
	}
	else
	{
		// No tilt when not in hoop mode.
		CurrentTilt = 0;
	}

	// Updates some properties that are used by the animation blueprint.
	UpdateAnimationProperties();

	if (GetMesh()->IsSimulatingPhysics())
	{
		// Move capsule to where mesh is when ragdolling, but with no collision. Useful for AI tracking stuff that uses the character's root location (the capsule location).
		GetCapsuleComponent()->SetWorldLocation(GetMesh()->GetSocketLocation(HeadBoneName));
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	}

	// Make noise if moving
	MovementNoise();

	// Set previous forward for next tick.
	PreviousForward = GetCapsuleComponent()->GetForwardVector();
}

// Called to bind functionality to input
void AHoopSnakeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHoopSnakeCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHoopSnakeCharacter::Look);

		// Hoop toggle
		EnhancedInputComponent->BindAction(HoopAction, ETriggerEvent::Triggered, this, &AHoopSnakeCharacter::ToggleHoop);

		// Reset
		EnhancedInputComponent->BindAction(ResetAction, ETriggerEvent::Triggered, this, &AHoopSnakeCharacter::Reset);

		// Pause
		EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Triggered, this, &AHoopSnakeCharacter::Pause);
	}
	else
	{
		//UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

}

void AHoopSnakeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		if (bHoopModeEnabled) // ignore movement input when in hoop mode as hoop should go straight forward
		{
			// **TO DECIDE : in hoop mode would it be better to ignore input and rely on just look direction for navigation, or add movement input as look input? **
			//AddControllerYawInput(MovementVector.X);
		}
		else // if not in hoop mode move normally
		{
			// find out which way is forward
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			// get forward vector
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

			// get right vector 
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			if (GetMesh()->IsSimulatingPhysics()) 
			{
				RagdollMovement(ForwardDirection, RightDirection, MovementVector);
			}
			else
			{
				// add movement 
				AddMovementInput(ForwardDirection, MovementVector.Y);
				AddMovementInput(RightDirection, MovementVector.X);
			}
		}
	}
}

void AHoopSnakeCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AHoopSnakeCharacter::ToggleHoop(const FInputActionValue& Value)
{
	// Can only enter or exit hoop mode when not ragdolling
	if (!GetMesh()->IsSimulatingPhysics())
	{
		if (bHoopToggle)
		{
			if (bHoopModeEnabled) // If in hoop mode, queue an attack.
			{
				// Animation blueprint tracks this value, and sends a message to the snake via interface once the animation is within the correct frame range to attack.
				bAttackQueued = true; 
			}
		}
		else // Enter hoop mode
		{
			// Animation blueprint will use this to transition animation to hoop rolling.
			bHoopModeEnabled = true; 

			// Adjust speed of character
			GetCharacterMovement()->MaxWalkSpeed = HoopSpeed;

			// Play camera shake.
			if(CameraShakeComponent->CameraShake)
			{
				CameraShakeComponent->StartCameraShake(CameraShakeComponent->CameraShake);
			}

			// Emit particles
			SpeedLineEffect->Activate(true);

			// Push crosshair widget to the HUD if it implements the HUDInterface
			AHUD* HUD = UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD();
			if (HUD->GetClass()->ImplementsInterface(UHUDInterface::StaticClass()))
			{
				IHUDInterface::Execute_PushCrosshair(HUD);
			}

			// Add impulse to movement so that hoop immediately travels at top speed.
			GetCharacterMovement()->AddImpulse(GetCapsuleComponent()->GetForwardVector() * HoopSpeed, true);

			// Set previous forward direction for use when calculating hoop tilt.
			PreviousForward = GetCapsuleComponent()->GetForwardVector();
		}

		bHoopToggle = !bHoopToggle; // flip value
	}
}

void AHoopSnakeCharacter::Reset()
{
	// Only reset when ragdolling
	if (GetMesh()->IsSimulatingPhysics())
	{
		/* Unused, but could be useful in a future iteration with an improved snake mesh/skeleton. 
		* Saving a pose snapshot of the ragdolling snake could be used in the animation blueprint to smooth the transition between ragdoll state and animated state.
		* However, the rope mesh skeleton used for the snake is not suitable for this, as the root bone (the head) uses simulated physics.
		* This results in the mesh root being in the incorrect orientation and position when transitioning from pose to animation.
		* A root bone similar to the Unreal mannequin would be needed, with that root bone being set to never simulate physics.
		* Useful video: https://www.youtube.com/watch?v=0H6w3YtLr2Y */
		GetMesh()->GetAnimInstance()->SavePoseSnapshot("RagdollPose");

		// Since we can't smoothly transition between ragdoll and animation using the above method, fade to black while we reset the snake.
		AHUD* HUD = UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD();
		if (HUD->GetClass()->ImplementsInterface(UHUDInterface::StaticClass()))
		{
			IHUDInterface::Execute_PushFadeToBlack(HUD);
		}

		// Delay resetting the snake to give the widget time to fade to black. Timer calls function after half of reset delay time has passed.
		FTimerHandle ResetHandle;
		GetWorld()->GetTimerManager().SetTimer(ResetHandle, this, &AHoopSnakeCharacter::FinishReset, ResetDelay / 2.0f, false);
		
		// End reset, fade back from black after reset has finished. Timer calls function after full reset delay time has passed.
		FTimerHandle UnfadeHandle;
		GetWorld()->GetTimerManager().SetTimer(UnfadeHandle, this, &AHoopSnakeCharacter::ClearHUD, ResetDelay, false);
	}
}

void AHoopSnakeCharacter::FinishReset()
{
	// Stop simulating physics
	GetMesh()->SetSimulatePhysics(false);

	// Revert movement back to defaults
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = DefaultSpeed;

	// Enable collision on head (it is usually disabled when attaching to victims)
	GetMesh()->GetBodyInstance(HeadBoneName)->SetShapeCollisionEnabled(0, ECollisionEnabled::Type::QueryAndPhysics);

	// Move capsule into position and re-enable collision.
	GetCapsuleComponent()->SetWorldLocation(GetMesh()->GetBoneLocation(HeadBoneName));
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block); 

	// Attach mesh and camera boom to capsule
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	CameraBoom->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	GetMesh()->SetRelativeLocation(MeshOffset);
	GetMesh()->SetRelativeRotation(FRotator(0.0f));
	UpperJaw->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	LowerJaw->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	// Reset bools
	bHoopToggle = false;
	bIsBiting = false;
	bIsForcedRagdoll = false;

	// Find and destroy physics constraint if it exists
	APhysicsConstraintActor* Constraint = nullptr;
	if (!Children.IsEmpty())
	{
		Children.FindItemByClass<APhysicsConstraintActor>(&Constraint);

		if (Constraint)
		{
			Constraint->Destroy();
		}
	}
}

void AHoopSnakeCharacter::TriggerAttack_Implementation()
{
	if (bAttackQueued)
	{
		// Unqueue attack
		bAttackQueued = false;

		// Exit hoop mode
		bHoopModeEnabled = false;

		// Stop camera shake and particles
		CameraShakeComponent->StopAllCameraShakes(false);
		SpeedLineEffect->Deactivate();

		// Detach mesh from capsule, this fixes some issues with accurately getting the mesh/bone positions. Should be re-attached upon reset.
		GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		// Enter ragdoll mode
		GetMesh()->SetSimulatePhysics(true);

		// Force based on the camera look direction so that player can aim the lunge.
		FVector LaunchForce = (GetFollowCamera()->GetForwardVector() * AttackForceForward) + FVector(0.0f, 0.0f, AttackForceUp);
		GetMesh()->AddImpulse(LaunchForce, HeadBoneName);

		// Open snake's mouth. When proper snake mesh is found, this will be changed to an animation instead of just snapping open instantly.
		UpperJaw->SetRelativeRotation(FRotator(-45.0f, 90.0f, 0.0f));
		LowerJaw->SetRelativeRotation(FRotator(45.0f, 90.0f, 0.0f));

		// Attach camera to snake
		GetCameraBoom()->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, "HeadSocket");

		// Pop crosshair widget from HUD
		AHUD* HUD = UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD();
		if (HUD->GetClass()->ImplementsInterface(UHUDInterface::StaticClass()))
		{
			IHUDInterface::Execute_PopWidget(HUD);
		}

		// Play some sounds at start of attack
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), WhooshSound, GetMesh()->GetBoneLocation(HeadBoneName));
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), HissSound, GetMesh()->GetBoneLocation(HeadBoneName));

		// Disable movement through character movement component (can still move while ragdolling, but that doesn't use movement component). Should be re-enabled upon reset.
		GetCharacterMovement()->DisableMovement();
	}
}

void AHoopSnakeCharacter::ClearHUD()
{
	// Pop all widgets from the HUD via interface
	AHUD* HUD = UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD();
	if (HUD->GetClass()->ImplementsInterface(UHUDInterface::StaticClass()))
	{
		IHUDInterface::Execute_PopAllWidgets(HUD);
	}
}

void AHoopSnakeCharacter::MovementNoise()
{
	float Speed;
	float Volume;

	// Use bone velocity for speed if simulating physics, otherwise use character movement velocity.
	if (GetMesh()->IsSimulatingPhysics())
	{
		Speed = GetMesh()->GetBoneLinearVelocity(HeadBoneName).Length();
	}
	else
	{
		Speed = GetCharacterMovement()->Velocity.Length();
	}
	
	// Only make a noise if moving.
	if (Speed > 10.0f)
	{
		// Volume is determined by speed. 0 speed = no noise. Travelling at full speed in hoop mode = max noise.
		Volume = UKismetMathLibrary::MapRangeClamped(Speed, 0.0f, HoopSpeed, 0.0f, 1.0f);
		MakeNoise(Volume, this, GetMesh()->GetBoneLocation(HeadBoneName));
	}
}

void AHoopSnakeCharacter::Pause()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	// Toggle pause state.
	UGameplayStatics::SetGamePaused(GetWorld(), !UGameplayStatics::IsGamePaused(GetWorld()));

	AHUD* HUD = PlayerController->GetHUD();
	if (HUD->GetClass()->ImplementsInterface(UHUDInterface::StaticClass()))
	{
		// Create pause menu widget via the HUD if game is set to paused
		if (UGameplayStatics::IsGamePaused(GetWorld()))
		{
			IHUDInterface::Execute_PushPauseMenu(HUD);
		}
		else
		{
			// Otherwise pop pause menu
			IHUDInterface::Execute_PopWidget(HUD);
		}
	}
}

void AHoopSnakeCharacter::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Only care about hits when we're ragdolling
	if (GetMesh()->IsSimulatingPhysics())
	{
		// Attempt to bite whatever we hit.
		AttemptBite(Hit);
	}
}

void AHoopSnakeCharacter::AttemptBite(const FHitResult& Hit)
{
	// Can only bite when not being forced to ragdoll by external object, and when not already biting something
	if (!bIsForcedRagdoll && !bIsBiting)
	{
		USkeletalMeshComponent* HitSkeleton = Cast<USkeletalMeshComponent>(Hit.Component);

		// Hoop snakes are smart enough to only bite things that possess skeletons
		if (HitSkeleton)
		{
			// If victim isn't already simulating physics, start doing so and play impact sound.
			if (!HitSkeleton->IsSimulatingPhysics())
			{
				HitSkeleton->SetSimulatePhysics(true);
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, Hit.ImpactPoint);
			}

			// Hoop snakes can only bite with their head (I hope)
			if (Hit.MyBoneName == HeadBoneName)
			{
				// Bones in mesh are oriented incorrectly, so right is actually forwards. Change this code if a new mesh is found.
				FVector HeadBoneForward = UKismetMathLibrary::GetRightVector(GetMesh()->GetSocketRotation(HeadBoneName));

				FVector TraceStart = GetMesh()->GetSocketLocation(HeadBoneName) + (HeadBoneForward * -10.0f); // start trace slightly behind head incase head is inside mesh
				FVector TraceEnd = GetMesh()->GetSocketLocation(HeadBoneName) + (HeadBoneForward * 20.0f); // end trace in front of head
				
				//DrawDebugCylinder(GetWorld(), TraceStart, TraceEnd, 1.0f, 16, FColor::Red, true);

				/* Sphere trace forward with a radius smaller than the snake's head to see if it was a glancing hit or not, and get the exact point of impact.
				 * Victim blueprint has two meshes: one for physics simulation/collision, and one for traces. Only one of them is rendered.
				 * Physics mesh uses the default mannequin physics asset and ignores traces, while the trace mesh uses a custom convex collision physics asset for more accurate trace results.
				 * The trace mesh has an animation blueprint to copy the physics mesh's pose when it is ragdolling.
				 * This essentially allows for complex collision tracing against a skeletal mesh without the issues of enabling per-poly collision.
				 * TO DO: measure performance impact of doing this */
				FHitResult TraceHit;
				if (GetWorld()->SweepSingleByChannel(TraceHit, TraceStart, TraceEnd, FQuat(0.0f), ECollisionChannel::ECC_Visibility, FCollisionShape::MakeSphere(1.0f)))
				{
					FVector ImpactPoint = TraceHit.ImpactPoint;
					FVector FrameOffset = UKismetMathLibrary::InverseTransformLocation(HitSkeleton->GetBoneTransform(Hit.BoneName), ImpactPoint);
					
					// Setup constraint
					APhysicsConstraintActor* Constraint = GetWorld()->SpawnActor<APhysicsConstraintActor>(BiteConstraintClass, ImpactPoint, FRotator(0.0f));
					
					// So we can easily access and destroy constraint
					Constraint->SetOwner(this);

					if (Constraint)
					{
						UPhysicsConstraintComponent* ConstraintComp = Constraint->GetConstraintComp();
						ConstraintComp->SetConstrainedComponents(GetMesh(), HeadBoneName, Hit.GetComponent(), Hit.BoneName);
						ConstraintComp->SetConstraintReferencePosition(EConstraintFrame::Type::Frame1, FVector(0.0f)); // frame 1 has no offset
						ConstraintComp->SetConstraintReferencePosition(EConstraintFrame::Type::Frame2, FrameOffset); // frame 2 in this position attaches the snake directly at the impact point
						// ** TO DO: also orient snake head to face the bone it is attaching to

						// Disable collision on snake head so it doesn't constantly collide with the victim
						GetMesh()->GetBodyInstance(HeadBoneName)->SetShapeCollisionEnabled(0, ECollisionEnabled::Type::NoCollision);

						// Add impulse to hit body
						FVector Impulse = GetMesh()->GetBoneLinearVelocity(HeadBoneName).Length() * HeadBoneForward * 20.0f;
						Hit.GetComponent()->AddImpulse(Impulse, Hit.BoneName);

						// Play sounds
						UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, GetMesh()->GetBoneLocation(HeadBoneName));
						UGameplayStatics::PlaySoundAtLocation(GetWorld(), BiteSound, GetMesh()->GetBoneLocation(HeadBoneName));

						bIsBiting = true;

						// Jaw rotation for biting.
						UpperJaw->SetRelativeRotation(FRotator(-30.0f, 90.0f, 0.0f));
						LowerJaw->SetRelativeRotation(FRotator(30.0f, 90.0f, 0.0f));
					}
				}
			}
		}
	}
}

void AHoopSnakeCharacter::RagdollMovement(FVector ForwardDirection, FVector RightDirection, FVector2D MovementVector)
{
	// Calculate direction for force
	FVector ForceDirection = (ForwardDirection * MovementVector.Y) + (RightDirection * MovementVector.X);

	if (ForceDirection.Normalize())
	{
		// If biting, use whole body to push or pull on the attached object.
		if (bIsBiting && !bIsMovementOnCooldown)
		{
			APhysicsConstraintActor* Constraint = nullptr;
			USkeletalMeshComponent* OtherBody = nullptr;
			FName OtherBone;

			// When physics constraint is created, this character should be set as the owner so we can find the constraint via children.
			if (!Children.IsEmpty())
			{
				Children.FindItemByClass<APhysicsConstraintActor>(&Constraint);
			}

			// Get the other body and bone from the constraint component
			if (Constraint)
			{
				UPrimitiveComponent* Component, * UnusedComponent; FName UnusedName;

				Constraint->GetConstraintComp()->GetConstrainedComponents(UnusedComponent, UnusedName, Component, OtherBone);

				if (Component)
				{
					OtherBody = Cast<USkeletalMeshComponent>(Component);
				}
			}

			// Apply impulse when trying to move while biting, spreading force across all bones
			GetMesh()->AddImpulseToAllBodiesBelow(ForceDirection * (RagdollMovementForce / GetMesh()->GetNumBones()) * 10.0f);

			// Apply impulse to the other body too, but just the attached bone
			if (OtherBody)
			{
				OtherBody->AddImpulse(ForceDirection * RagdollMovementForce, OtherBone);
				//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Great success!"));
			}

			// Play SFX
			//UGameplayStatics::PlaySound2D(GetWorld(), WhooshSound);
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), WhooshSound, GetMesh()->GetComponentLocation());

			// Start cooldown
			TriggerRagdollMovementCooldown();
			
		}
		else if (GetMesh()->GetPhysicsLinearVelocity().Length() < RagdollMovementThreshold && !bIsMovementOnCooldown) // when moving slowly, lunge in direction
		{
			// Add some upwards movement, influenced by look direction.
			ForceDirection.Z = 0.2f + GetControlRotation().Vector().Z;

			// Apply impulse to root
			GetMesh()->AddImpulse(ForceDirection * RagdollMovementForce * 10.0f);

			// Play SFX
			UGameplayStatics::PlaySound2D(GetWorld(), WhooshSound);

			// Start cooldown
			TriggerRagdollMovementCooldown();
		}
		else // when not biting or going too slow...
		{
			// apply force to normalized direction **TO CHECK: does add force already account for delta time?
			GetMesh()->AddForce(ForceDirection * RagdollMovementForce);
		}
	}
}

void AHoopSnakeCharacter::TriggerRagdollMovementCooldown()
{
	bIsMovementOnCooldown = true;

	// Add some random variation to the interval between impulse applications to make it seem less robotic
	float CooldownTimerVariation = UKismetMathLibrary::RandomFloatInRange(-0.25f, 0.25f);

	// Quick and simple way to do a cooldown timer. Lambda function toggles movement cooldown off after cooldown duration time has passed.
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([&] { bIsMovementOnCooldown = false; }), MovementCooldownDuration + CooldownTimerVariation, false);
}

void AHoopSnakeCharacter::UpdateCamera(float DeltaTime)
{
	// Interpolate towards the desired field of view value.
	float TargetFOV = bHoopModeEnabled ? HoopFOV : DefaultFOV;
	FollowCamera->FieldOfView = UKismetMathLibrary::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaTime, CameraInterpSpeed);

	// Interpolate towards the desired boom arm length.
	float TargetArmLength = bHoopModeEnabled ? HoopArmLength : DefaultArmLength;
	CameraBoom->TargetArmLength = UKismetMathLibrary::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);

	// Interpolate towards the desired camera offset.
	FVector TargetOffset = bHoopModeEnabled ? HoopCameraOffset : DefaultCameraOffset;
	CameraBoom->SocketOffset = UKismetMathLibrary::VInterpTo(CameraBoom->SocketOffset, TargetOffset, DeltaTime, CameraInterpSpeed);
	
	/* Boom offset is handled differently when simulating physics.
	 * Offsetting using relative location would result in the boom arm rolling with the ragdolling mesh and colliding with the ground.
	 * Instead relative location must be reset to 0, and the offset is applied to its world position so it remains in place above the mesh.
	 * No need to interpolate these values currently as camera lag will smooth transition. */
	if (GetMesh()->IsSimulatingPhysics())
	{
		// Reset boom's relative location.
		CameraBoom->SetRelativeLocation(FVector(0));

		// Apply world offset to boom.
		FVector NewPosition = GetMesh()->GetSocketLocation("HeadSocket") + RagdollBoomPosition;
		CameraBoom->SetWorldLocation(NewPosition);
	}
	else // when not simulating physics...
	{
		// Apply relative offset to boom.
		FVector NewPosition = bHoopModeEnabled ? HoopBoomPosition : DefaultBoomPosition;
		CameraBoom->SetRelativeLocation(NewPosition);
	}
}

void AHoopSnakeCharacter::ApplyHoopMovement(float DeltaTime)
{
	// Apply forward input every tick when in hoop mode.
	AddMovementInput(GetCapsuleComponent()->GetForwardVector(), 5.0f);

	// Multiply dot product by modifier to get tilt amount. Modifier is negated to ensure it tilts in the correct direction.
	DesiredTilt = TurningDotProduct() * -TiltModifier;

	// Interpolate towards the desired tilt
	CurrentTilt = UKismetMathLibrary::FInterpTo(CurrentTilt, DesiredTilt, DeltaTime, TiltInterpSpeed);
	GetMesh()->SetRelativeRotation(FRotator(0.0f, 0.0f, CurrentTilt));
}

void AHoopSnakeCharacter::UpdateAnimationProperties()
{
	RotateRate = TurningDotProduct();

	// If rotate rate is roughly zero, we are not rotating. Otherwise we are rotating.
	bIsRotating = !(UKismetMathLibrary::NearlyEqual_FloatFloat(RotateRate, 0.0f)); 

	FVector Forward = GetCapsuleComponent()->GetForwardVector();
	FVector NormalizedVelocity = UKismetMathLibrary::Normal(GetCharacterMovement()->Velocity);

	// Compare velocity and forward vector to determine if we're moving forward.
	bool bIsMovingForward = (UKismetMathLibrary::Dot_VectorVector(Forward, NormalizedVelocity) > 0.1f);

	// Compare velocity and cross of forward vector to determine if we're moving right.
	bool bIsMovingRight = (UKismetMathLibrary::Dot_VectorVector(UKismetMathLibrary::Cross_VectorVector(Forward, UKismetMathLibrary::Vector_Up()), NormalizedVelocity) * -1 > 0.1f);

	// Reverse slither animation when not moving forward or right.
	if (bIsMovingForward || bIsMovingRight) 
	{
		bReverseSlither = false;
	}
	else
	{
		bReverseSlither = true;
	}
	
}

void AHoopSnakeCharacter::ForceRagdoll()
{
	// Track that mesh was forced to ragdoll, unqueue any attacks and exit hoop mode
	bIsForcedRagdoll = true;
	bAttackQueued = false;
	bHoopModeEnabled = false;

	// Simulate physics and adjust attachments
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	GetCameraBoom()->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, "HeadSocket");

	// Stop camera shake and particles
	CameraShakeComponent->StopAllCameraShakes(false);
	SpeedLineEffect->Deactivate();

	// Pop crosshair widget from HUD
	AHUD* HUD = UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD();
	if (HUD->GetClass()->ImplementsInterface(UHUDInterface::StaticClass()))
	{
		IHUDInterface::Execute_PopWidget(HUD);
	}
}

float AHoopSnakeCharacter::TurningDotProduct()
{
	/* Dot product of previous and current forwards would tell us when we're turning, but not which direction.
	 * So we use cross product of previous forward and up vector against the current forward so that direction can be determined from the dot product. */
	FVector PreviousCrossUp = UKismetMathLibrary::Cross_VectorVector(PreviousForward, UKismetMathLibrary::Vector_Up());
	float CrossDotCurrent = UKismetMathLibrary::Dot_VectorVector(PreviousCrossUp, GetCapsuleComponent()->GetForwardVector());

	return CrossDotCurrent;
}
