#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/Selectable.h"
#include "Ai/GridPathfinderComponent.h"
#include "AUnitCharacter.generated.h"

class UUnitSteeringComponent;

UENUM(BlueprintType)
enum class EUnitState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Moving_Cluster  UMETA(DisplayName = "Moving_Cluster"),   // تنها حالت حرکت (چه تک، چه گروه)
    Attacking       UMETA(DisplayName = "Attacking"),
    Dead            UMETA(DisplayName = "Dead"),
    Stunned         UMETA(DisplayName = "Stunned")
};

struct FPerpendicularLine
{
    FVector PointOnPath;   // نقطه روی مسیر (مثلاً Pi)
    FVector RightDir;      // جهت عمود بر مسیر در آن نقطه (نرمال شده)
    float DistanceAlong;   // طول مسیر تا آن نقطه (اختیاری)
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
    UFUNCTION(BlueprintCallable, Category = "Movement")
    float GetSpeed() const;

    virtual void SetSelected_Implementation(bool bSelected) override;
    virtual bool IsSelected_Implementation() const override;

    

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UGridPathfinderComponent* GridPathfinder;

    // تابع اصلی برای شروع حرکت (مسیر از قبل محاسبه شده)
    void SetPathAndMove(const TArray<FVector>& Path);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetMaxSpeed(float NewMaxSpeed);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MaxSpeed = 450.f;

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetRotationSpeed(float NewRotationSpeed);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float RotationSpeed = 900.f;

    UFUNCTION(BlueprintCallable, Category = "State")
    void SetUnitState(EUnitState NewState);

    UFUNCTION(BlueprintCallable, Category = "State")
    EUnitState GetUnitState() const { return CurrentState; }

    UPROPERTY()
    FVector FormationOffset;

    UPROPERTY(BlueprintReadWrite)
    FVector FinalGoalLocation;

    UPROPERTY(BlueprintReadWrite)
    float FinalGoalRadius = 600.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Steering")
    class UUnitSteeringComponent* SteeringComp;

    void SetSteeringGroupParams(const FVector& Center, const FVector& Forward);
    void ClearSteeringGroupParams();
    void ClearMovementState();

    // متغیرهای روش "مسیر مرجع + t"
    UPROPERTY()
    float PathProgress = 0.f;
    UPROPERTY()
    float TotalPathLength = 0.f;

    // در بخش public یا private (بسته به جایی که از آن استفاده می‌شود)
    FVector GetTargetOnPerpendicularLine(float HalfWidth, float& OutDistanceToEnd) const;
    
    // در بخش private، به همراه سایر متغیرها
    float DesiredLateralOffset = 0.f;   // فاصله‌ی عرضی ثابت (مثبت به راست مسیر)

    UPROPERTY(EditAnywhere, Category = "Movement")
    float LookAheadDistance = 150.f;   // فاصله جلوتر روی مسیر (بر حسب واحد)



    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Selection")
    UStaticMeshComponent* SelectionCircleMesh;

    
protected:
    virtual void BeginPlay() override;
    void OnSelectedChanged(bool bNowSelected);
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    EUnitState CurrentState = EUnitState::Idle;

private:
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    TArray<UAnimMontage*> HitMontages;

    bool bIsSelected = false;
    TArray<FVector> CurrentPath;
    float CurrentSpeed = 0.f;

    // توابع کمکی برای روش t
    float ComputeInitialProgress(const FVector& Location, const TArray<FVector>& Path);
    FVector GetDirectionAtProgress(float Progress) const;
    float GetTotalPathLength() const { return TotalPathLength; }
    // در بخش private
    void FindClosestPointAndTangent(const FVector& Location, const TArray<FVector>& Path, FVector& OutPoint, FVector& OutTangent) const;
    float CalculateCurrentLateralOffset() const;
    // پیدا کردن نزدیک‌ترین نقطه و مماس روی مسیر، و محاسبه فاصله تا انتها
    FVector GetClosestTangent(const FVector& Location, float& OutDistanceToEnd) const;

    // در بخش private یا protected
    int32 CurrentTargetWaypointIndex = 1;   // اندیس نقطه‌ی مسیر که خط عمود آن را دنبال می‌کنیم (به جز نقطه شروع)
    float WaypointReachedDistance = 30.f;   // فاصله‌ای که اگر به هدف رسیدیم، برویم به نقطه بعدی

    bool bIsWaitingForNextWaypoint = false;
    float TimeSinceWaypointReached = 0.f;
    float WaypointWaitTime = 0.05f;  // 0.1 ثانیه صبر بعد از رسیدن به هر نقطه
    bool bHasReachedFinalDestination = false;

    float TimeSinceLastMoveCommand = 0.f;
};
