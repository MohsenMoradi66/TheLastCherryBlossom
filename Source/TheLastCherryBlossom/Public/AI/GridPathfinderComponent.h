#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NavigationSystem.h"
#include "Containers/Queue.h"
#include "GridPathfinderComponent.generated.h"

// ========== تعریف کانال‌های Collision ==========
namespace ECollisionChannels
{
    constexpr ECollisionChannel Character = ECC_GameTraceChannel1;  // کاراکترها - نادیده گرفته می‌شوند
    constexpr ECollisionChannel Buildings = ECC_GameTraceChannel2;  // ساختمان‌ها - مانع
    constexpr ECollisionChannel Trees = ECC_GameTraceChannel3;      // درختان و سنگ‌ها - مانع
}

// ========== تعریف FPathRequest ==========
USTRUCT()
struct THELASTCHERRYBLOSSOM_API FPathRequest
{
    GENERATED_BODY()

public:
    FPathRequest() {}
    
    FPathRequest(const FVector& InStart, const FVector& InGoal, TFunction<void(const TArray<FVector>&)> InCallback)
        : Start(InStart), Goal(InGoal), Callback(MoveTemp(InCallback)) {}

    FVector Start = FVector::ZeroVector;
    FVector Goal = FVector::ZeroVector;
    TFunction<void(const TArray<FVector>&)> Callback;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UGridPathfinderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGridPathfinderComponent();

    TArray<FVector> FindPathShared(const FVector& Start, const FVector& Goal);
    bool IsLocationWalkable(const FVector& Location) const;
    bool FindClosestWalkable(const FVector& Origin, FVector& OutLocation) const;

    typedef TFunction<void(const TArray<FVector>&)> FPathResultCallback;
    void RequestPathAsync(FVector StartWorld, FVector GoalWorld, FPathResultCallback OnPathReady);
    void CancelAllRequests();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
    float CharacterRadius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
    float SearchRadius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding|Optimization")
    float SegmentLengthMultiplier = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding|Optimization")
    int32 ResampleStepsPerFrame = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding|Optimization")
    int32 SmoothingStepsPerFrame = 20;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    enum class EPathfindingState : uint8
    {
        Idle,
        DirectLOS,
        NavQuerying,
        Resampling,
        Smoothing,
        Finished
    };

    struct FActiveRequest
    {
        FVector Start;
        FVector Goal;
        FPathResultCallback Callback;
        EPathfindingState State = EPathfindingState::Idle;
        TArray<FVector> RawPath;
        TArray<FVector> ResampledPath;
        TArray<FVector> FinalPath;
        int32 ResampleCurrentIndex = 0;
        float ResampleRemaining = 0.f;
        FVector ResampleCurrentPoint;
        int32 SmoothStartIndex = 0;

        void Reset()
        {
            Start = FVector::ZeroVector;
            Goal = FVector::ZeroVector;
            Callback = nullptr;
            State = EPathfindingState::Idle;
            RawPath.Empty();
            ResampledPath.Empty();
            FinalPath.Empty();
            ResampleCurrentIndex = 0;
            ResampleRemaining = 0.f;
            ResampleCurrentPoint = FVector::ZeroVector;
            SmoothStartIndex = 0;
        }
    };

    TQueue<FPathRequest> PendingRequests;
    FActiveRequest ActiveRequest;
    bool bHasActiveRequest = false;
    bool bCancelCurrent = false;
    
    FPathFindingResult PendingResult;
    bool bWaitingForNavResult = false;

    void StartNextRequest();
    void ProcessActiveRequest();
    void ProcessDirectLOS();
    void ProcessNavQuery();
    void ProcessResampling();
    void ProcessSmoothing();
    void FinishRequest(bool bSuccess, const TArray<FVector>& Path = TArray<FVector>());
    
    TArray<FVector> ResampleFullPath(const TArray<FVector>& InputPath, float SegmentLength) const;
    TArray<FVector> SmoothFullPath(const TArray<FVector>& InputPath);
    
    // تابع کمکی برای تنظیم پارامترهای موانع
  
    void SetupObstacleQueryParams(FCollisionObjectQueryParams& ObjectQueryParams) const;
};