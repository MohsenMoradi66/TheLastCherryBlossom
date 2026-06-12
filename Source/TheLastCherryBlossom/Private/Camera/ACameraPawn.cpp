// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/ACameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"

ACameraPawn::ACameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    RootComponent = SpringArm;
    SpringArm->TargetArmLength = 1500.0f;
    SpringArm->bDoCollisionTest = false;
    SpringArm->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);
}

void ACameraPawn::BeginPlay()
{
    Super::BeginPlay();
}

void ACameraPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    float MouseX, MouseY;
    int32 ViewportX, ViewportY;

    if (!PC->GetMousePosition(MouseX, MouseY)) return;
    PC->GetViewportSize(ViewportX, ViewportY);

    FVector Direction = FVector::ZeroVector;

    if (MouseX <= EdgeSize) Direction += FVector::LeftVector;
    if (MouseX >= ViewportX - EdgeSize) Direction += FVector::RightVector;
    if (MouseY <= EdgeSize) Direction += FVector::ForwardVector;
    if (MouseY >= ViewportY - EdgeSize) Direction += FVector::BackwardVector;

    if (!Direction.IsZero())
    {
        AddActorWorldOffset(Direction.GetSafeNormal() * MovementSpeed * DeltaTime, true);
    }
}

void ACameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this, &ACameraPawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &ACameraPawn::MoveRight);
    PlayerInputComponent->BindAxis("Zoom", this, &ACameraPawn::ZoomCamera);
}

void ACameraPawn::MoveForward(float Value)
{
    if (Value != 0.f)
    {
        AddActorWorldOffset(FVector::ForwardVector * Value * MovementSpeed * GetWorld()->GetDeltaSeconds(), true);
    }
}

void ACameraPawn::MoveRight(float Value)
{
    if (Value != 0.f)
    {
        AddActorWorldOffset(FVector::RightVector * Value * MovementSpeed * GetWorld()->GetDeltaSeconds(), true);
    }
}

void ACameraPawn::ZoomCamera(float Value)
{
    if (Value != 0.f && SpringArm)
    {
        float NewLength = FMath::Clamp(SpringArm->TargetArmLength + (-Value) * ZoomSpeed, MinZoom, MaxZoom);
        SpringArm->TargetArmLength = NewLength;
    }
}
