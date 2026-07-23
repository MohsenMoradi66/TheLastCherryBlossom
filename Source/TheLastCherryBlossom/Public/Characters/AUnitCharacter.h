#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/Selectable.h"
#include "AI/UUnitSteeringComponent.h" 
#include "Ai/GridPathfinderComponent.h"
#include "AUnitCharacter.generated.h"

UENUM(BlueprintType)
enum class EUnitState : uint8
{
    Idle, Moving, Attacking, Dead, Stunned,Stuck
};

UCLASS()
class THELASTCHERRYBLOSSOM_API AUnitCharacter : public ACharacter, public ISelectable
{
    GENERATED_BODY()

public:
    AUnitCharacter();
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void PlayRandomHitMontage();
    UFUNCTION(BlueprintCallable, Category = "Movement") float GetSpeed() const;

    virtual void SetSelected_Implementation(bool bSelected) override;
    virtual bool IsSelected_Implementation() const override;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI") UGridPathfinderComponent* GridPathfinder;

    void SetPathAndMove(const TArray<FVector>& Path);
    void SetMaxSpeed(float NewMaxSpeed);
    void SetRotationSpeed(float NewRotationSpeed);
    void SetUnitState(EUnitState NewState);
    EUnitState GetUnitState() const { return CurrentState; }

    UPROPERTY(BlueprintReadWrite) FVector FinalGoalLocation;
    UPROPERTY(BlueprintReadWrite) float FinalGoalRadius = 600.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Steering") class UUnitSteeringComponent* SteeringComp;

    void SetSteeringGroupParams(const FVector& Center, const FVector& Forward);
    void ClearSteeringGroupParams();
    void ClearMovementState();
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Selection") UStaticMeshComponent* SelectionCircleMesh;

    UPROPERTY(EditAnywhere, Category = "Movement") float MaxSpeed = 350.f;
    

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    class UUnitMovementComponent* UnitMovement;

    float CurrentSpeed = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rotation")
    float RotationInterpSpeed = 12.f; // سرعت چرخش (پیش‌فرض)
    virtual FVector GetVelocity() const override;
protected:
    virtual void BeginPlay() override;
    void OnSelectedChanged(bool bNowSelected);
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State") EUnitState CurrentState = EUnitState::Idle;


private:
    UPROPERTY(EditDefaultsOnly, Category = "Animation") TArray<UAnimMontage*> HitMontages;

    bool bIsSelected = false;

    void SetIdleCollision();
    void SetMovingCollision();
    void SetStuckCollision();
    void SetDeadCollision();
    void SetAttackingCollision();
    void OnStateChanged(EUnitState OldState, EUnitState NewState);
    
    
};