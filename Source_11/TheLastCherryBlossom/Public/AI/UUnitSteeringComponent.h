#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UUnitSteeringComponent.generated.h"

class AUnitCharacter;

USTRUCT(BlueprintType)
struct FPathSegmentInfo
{
    GENERATED_BODY()
    
    int32 SegmentIndex = 0;
    float T = 0.f;
    FVector ClosestPoint = FVector::ZeroVector;
    FVector Tangent = FVector::ForwardVector;
    float DistanceToPointSq = 0.f;
    float LengthFromStart = 0.f;
};

// کش مسیر برای بهینه‌سازی
struct FPathCache
{
    TArray<float> SegmentLengths;
    TArray<float> AccumulatedLengths;
    float TotalLength = 0.f;
    
    void Reset()
    {
        SegmentLengths.Empty();
        AccumulatedLengths.Empty();
        TotalLength = 0.f;
    }
    
    bool IsValid() const { return SegmentLengths.Num() > 0; }
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UUnitSteeringComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnitSteeringComponent();

    void Initialize(AUnitCharacter* InOwner);
    void ExecuteMovement(float DeltaTime);
    void SetPath(const TArray<FVector>& NewPath);
    void ClearPath();
    void SetGroupParams(const FVector& InCenter, const FVector& InForward);
    void ClearGroupParams();
    
    void SetDesiredLateralOffset(float NewOffset) { DesiredLateralOffset = NewOffset; }
    float GetDesiredLateralOffset() const { return DesiredLateralOffset; }
    
    void SetTargetSlot(const FVector& InSlotLocation)
    {
        TargetSlot = InSlotLocation;
        bHasTarget = true;
    }
    
    bool IsMoving() const { return bHasPath && bHasTarget; }
    bool IsEvading() const { return bEvading; }
    void ForceStop();
    void ClearTarget();
    void ResetDirection();

    void ApplyMovementWithTarget(float DeltaTime, const FVector& InTarget);
    bool IsMovingTowardGoal() const;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float AvoidanceRadius = 140.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float AvoidanceAngle = 110.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering|Avoidance")
    float EvadeTime = 0.5f;
    
    UPROPERTY(EditAnywhere, Category = "Movement")
    float LookAheadDistance = 150.f;


private:
    // ========== توابع داخلی ==========
    
    void UpdatePathCache();
    float GetDistanceAlongPath(const FVector& Location) const;
    bool GetPointOnPathFast(float DistanceFromStart, FVector& OutPoint, FVector& OutTangent) const;
    float GetDistanceToEndFast(const FVector& Location) const;
    
    float CalculateDynamicLookAhead(float DistanceToEnd) const;
    
    void ComputeEvadeDirection();
    void UpdateGroupCompression(float DeltaTime, const FVector& CurrentTangent);
    void ApplyMovement(float DeltaTime);
    void UpdateRotation(float DeltaTime);
    
    // ========== متغیرهای مسیر ==========
    
    TArray<FVector> CurrentPath;
    float TotalPathLength = 0.f;
    bool bHasPath = false;
    
    FPathCache PathCache;
    
    // ========== متغیرهای حرکتی ==========
    
    AUnitCharacter* Owner = nullptr;
    bool bHasTarget = false;
    FVector TargetSlot;
    FVector CurrentDirection;
    float DistanceToGoal = 0.f;
    
    // ========== متغیرهای گروه ==========
    
    bool bHasGroupCenter = false;
    FVector GroupCenter;
    FVector GroupForward;
    
    // ========== متغیرهای فرار ==========
    
    bool bEvading = false;
    float EvadeTimer = 0.f;
    FVector EvadeDirection;
    
    // ========== متغیرهای فشرده‌سازی ==========
    
    float DesiredLateralOffset = 0.f;
    float CompressionRadius = 150.f;
    float CompressionStrength = 1.0f;
    float LinearCompressionSpeed = 80.f;
    
    // ========== مدیریت فریم اول ==========
    
    bool bIsFirstFrameOfNewPath = false;
    float TimeSinceLastMoveCommand = 0.f;
    
    // ========== ثابت‌ها ==========
    static constexpr float STOP_DISTANCE = 20.f;
    static constexpr float BRAKING_DISTANCE = 400.f;
    static constexpr float MIN_LOOK_AHEAD = 50.f;
    static constexpr float FIRST_FRAME_TIME = 0.04f;
    static constexpr float LATERAL_OFFSET_EPSILON = 0.01f;
};