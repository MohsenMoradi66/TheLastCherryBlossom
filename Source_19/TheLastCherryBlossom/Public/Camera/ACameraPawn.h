// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ACameraPawn.generated.h"

UCLASS()
class THELASTCHERRYBLOSSOM_API ACameraPawn : public APawn
{
    GENERATED_BODY()

public:
    ACameraPawn();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void ZoomCamera(float Value);

private:
    UPROPERTY(EditAnywhere)
    class USpringArmComponent* SpringArm;

    UPROPERTY(EditAnywhere)
    class UCameraComponent* Camera;

    UPROPERTY(EditAnywhere, Category = "Camera Movement")
    float MovementSpeed = 1000.0f;

    UPROPERTY(EditAnywhere, Category = "Camera Zoom")
    float ZoomSpeed = 1000.0f;

    UPROPERTY(EditAnywhere, Category = "Camera Zoom")
    float MinZoom = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Camera Zoom")
    float MaxZoom = 2000.0f;

    UPROPERTY(EditAnywhere, Category = "Edge Scroll")
    float EdgeSize = 15.0f;
};
