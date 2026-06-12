#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UUnitSteeringComponent.generated.h"

class AUnitCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UUnitSteeringComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnitSteeringComponent();

    void Initialize(AUnitCharacter* InOwner);
    void SetTargetSlot(const FVector& InSlotLocation);
    void ClearTarget();
    void UpdateSteering(float DeltaTime);
    void SetDistanceToGoal(float InDistance) { DistanceToGoal = InDistance; }

    // اصلاح پارامترها برای جلوگیری از خطای Shadow (پنهان‌سازی عضو کلاس)
    void SetGroupParams(const FVector& InCenter, const FVector& InForward);
    void ClearGroupParams();

    void ResetDirection();  

    // فیلتر جدید سیستم فرار ریلی (خروجی اسلات اصلاح شده می‌دهد)
    FVector ApplyEvadeSubsystem(const FVector& DynamicTargetSlot, const FVector& CurrentTangent);

    // گترها و متغیرهای عمومی برای رفع خطای ناظر در AUnitCharacter
    bool IsEvading() const { return bEvading; }
    FVector GetEvadeDirection() const { return EvadeDirection; }

    // متغیرهای عمومی کنترل وضعیت برای استفاده در کاراکتر
    bool bEvading = false;

    // پارامترهای اجتناب
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float AvoidanceRadius = 140.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float AvoidanceAngle = 110.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float AvoidanceForce = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float EvadeTime = 0.5f;

    bool HasValidTarget() const { return bHasTarget; }
    FVector GetGroupCompressionForce() const;

    // متد فشرده‌سازی لترال ریل‌ها
    void UpdateGroupCompression(float DeltaTime, const FVector& CurrentTangent);
    
    FORCEINLINE float GetDesiredLateralOffset() const { return DesiredLateralOffset; }
    FORCEINLINE void SetDesiredLateralOffset(float NewOffset) { DesiredLateralOffset = NewOffset; }
    
    void ComputeEvadeDirection();

private:
    AUnitCharacter* Owner = nullptr;
    bool bHasTarget = false;
    FVector TargetSlot;
    FVector CurrentDirection;
    float DistanceToGoal = 0.f;

    bool bHasGroupCenter = false;
    FVector GroupCenter;
    FVector GroupForward;

    float EvadeTimer = 0.f;
    FVector EvadeDirection;

    float DesiredLateralOffset = 0.f;

    // پارامترهای فشردگی
    float CompressionRadius = 150.f;   
    float CompressionStrength = 1.0f;  
    float OffsetChangeSpeed = 50.f;    
};