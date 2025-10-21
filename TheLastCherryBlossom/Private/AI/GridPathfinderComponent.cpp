#include "Ai/GridPathfinderComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"

void UGridPathfinderComponent::BeginPlay()
{
    Super::BeginPlay();

    // بررسی اینکه Owner واقعا Pawn باشه
    if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
    {
        if (UCapsuleComponent* Capsule = PawnOwner->FindComponentByClass<UCapsuleComponent>())
        {
            CharacterRadius = Capsule->GetUnscaledCapsuleRadius();
            UE_LOG(LogTemp, Log, TEXT("%s: Capsule radius set to %f"),
                *PawnOwner->GetName(), CharacterRadius);
        }
        else
        {
            CharacterRadius = 30.f; // fallback
            UE_LOG(LogTemp, Warning, TEXT("%s: Capsule not found, using default radius %f"),
                *PawnOwner->GetName(), CharacterRadius);
        }
    }
}

UGridPathfinderComponent::UGridPathfinderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UGridPathfinderComponent::IsLocationWalkable(const FVector& Location) const
{
    if (!GetWorld()) return false;

    // ۱) چک NavMesh
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return false;

    FNavLocation NavLocation;
    if (!NavSys->ProjectPointToNavigation(Location, NavLocation))
    {
        return false; // نقطه خارج از NavMesh است
    }

    // ۲) چک Collision (مانع فیزیکی یا یونیت سر راه)
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel1); // موانع
    ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel2); // یونیت‌ها

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());

    bool bBlocked = GetWorld()->OverlapAnyTestByObjectType(
        Location,
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(CharacterRadius * 0.9f), // کمی کوچکتر از کپسول یونیت
        QueryParams
    );

    return !bBlocked;
}

bool UGridPathfinderComponent::FindClosestWalkable(const FVector& Origin, FVector& OutLocation) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return false;

	FNavLocation NavLocation;
	if (NavSys->ProjectPointToNavigation(Origin, NavLocation, FVector(this->SearchRadius)))
	{
		OutLocation = NavLocation.Location;
		return true;
	}

	return false;
}

TArray<FVector> UGridPathfinderComponent::FindPathShared(const FVector& StartWorld, const FVector& GoalWorld)
{
    TArray<FVector> FinalPath;

    // --- مرحله ۰: تست مسیر مستقیم ---
    {
        FCollisionObjectQueryParams ObjectQueryParams;
        ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel1); // موانع
        ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel2); // یونیت‌ها

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(GetOwner());

        FHitResult HitResult;
        bool bBlocked = GetWorld()->SweepSingleByObjectType(
            HitResult,
            StartWorld,
            GoalWorld,
            FQuat::Identity,
            ObjectQueryParams,
            FCollisionShape::MakeSphere(CharacterRadius),
            QueryParams
        );

        if (!bBlocked)
        {
            UE_LOG(LogTemp, Log, TEXT("Direct path is clear. Returning straight line."));

            FinalPath.Add(StartWorld);
            FinalPath.Add(GoalWorld);

            // برای دیدن دیباگ
            DrawDebugLine(GetWorld(), StartWorld, GoalWorld, FColor::Black, false, 5.f, 0, 3.f);

            return FinalPath; // مسیر مستقیم برمی‌گردونیم
        }
    }

    // --- مرحله ۱: NavMesh Path ---
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) 
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathShared: NavigationSystem not found."));
        return FinalPath;
    }

    FVector ActualGoal = GoalWorld;
    if (!IsLocationWalkable(GoalWorld))
    {
        if (!FindClosestWalkable(GoalWorld, ActualGoal))
        {
            UE_LOG(LogTemp, Warning, TEXT("FindPathShared: Goal is not walkable."));
            return FinalPath;
        }
    }

    UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(GetWorld(), StartWorld, ActualGoal);
    if (!NavPath || !NavPath->IsValid() || NavPath->PathPoints.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindPathShared: Failed to generate nav path."));
        return FinalPath;
    }

    UE_LOG(LogTemp, Log, TEXT("FindPathShared: Raw path points: %d"), NavPath->PathPoints.Num());

    // --- مرحله ۲: Resample قبل از Smooth ---
    TArray<FVector> ResampledPath = ResamplePath(NavPath->PathPoints, CharacterRadius * 2.f);
    FinalPath = ProcessFinalPath(ResampledPath);

    UE_LOG(LogTemp, Log, TEXT("FindPathShared: Final path points: %d"), FinalPath.Num());

    return FinalPath;
}

TArray<FVector> UGridPathfinderComponent::ProcessFinalPath(const TArray<FVector>& InputPath)
{
    TArray<FVector> SmoothedPath;

    if(InputPath.Num() < 2)
        return InputPath; // مسیر کوتاه یا خالی

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel1); // موانع
    ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_GameTraceChannel2); // یونیت‌ها

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());

    int32 StartIndex = 0;
    SmoothedPath.Add(InputPath[0]);

    while(StartIndex < InputPath.Num() - 1)
    {
        int32 EndIndex = InputPath.Num() - 1;

        // مسیر مستقیم باز بین Start و End پیدا کن
        while(EndIndex > StartIndex + 1)
        {
            FVector Start = InputPath[StartIndex];
            FVector End = InputPath[EndIndex];

            // استفاده از SphereSweep برای چک مسیر مستقیم
            FHitResult HitResult;
            bool bBlocked = GetWorld()->SweepSingleByObjectType(
                HitResult,
                Start,
                End,
                FQuat::Identity,
                ObjectQueryParams,
                FCollisionShape::MakeSphere(CharacterRadius),
                QueryParams
            );

            if(!bBlocked)
            {
                // مسیر مستقیم باز است → نقاط بین را حذف کن
                break;
            }

            EndIndex--; // مسیر بسته بود → یک نقطه قبل را امتحان کن
        }

        SmoothedPath.Add(InputPath[EndIndex]);
        StartIndex = EndIndex;
    }

    // دیباگ خطوط مسیر اصلی و مسیر پردازش شده
    for(int32 i = 0; i < InputPath.Num() - 1; i++)
    {
        DrawDebugLine(GetWorld(), InputPath[i], InputPath[i + 1], FColor::Red, false, 1.f, 0, 2.f);
    }

    for(int32 i = 0; i < SmoothedPath.Num() - 1; i++)
    {
        DrawDebugLine(GetWorld(), SmoothedPath[i], SmoothedPath[i + 1], FColor::Blue, false, 7.f, 0, 3.f);
        DrawDebugSphere(GetWorld(), SmoothedPath[i], 10.f, 8, FColor::Green, false, 1.f);
    }

    return SmoothedPath;
}

TArray<FVector> UGridPathfinderComponent::ResamplePath(const TArray<FVector>& InputPath, float SegmentLength) const
{
    TArray<FVector> Resampled;

    if (InputPath.Num() < 2)
        return InputPath;

    Resampled.Add(InputPath[0]); // همیشه نقطه شروع نگه می‌داریم
    DrawDebugSphere(GetWorld(), InputPath[0], 12.f, 8, FColor::Green, false, 7.f); // نقطه شروع

    float Remaining = SegmentLength;
    FVector Current = InputPath[0];

    for (int32 i = 1; i < InputPath.Num(); i++)
    {
        FVector Next = InputPath[i];
        FVector Dir = (Next - Current).GetSafeNormal();
        float Dist = FVector::Dist(Current, Next);

        while (Dist >= Remaining)
        {
            FVector NewPoint = Current + Dir * Remaining;
            Resampled.Add(NewPoint);

            // 🔵 نمایش گره‌های اضافه‌شده با رنگ آبی
            DrawDebugSphere(GetWorld(), NewPoint, 10.f, 8, FColor::Blue, false, 1.f);

            Current = NewPoint;
            Dist -= Remaining;
            Remaining = SegmentLength;
        }

        Remaining -= Dist;
        Current = Next;

        // 🟢 نمایش گره‌های اصلی با رنگ سبز
        DrawDebugSphere(GetWorld(), Next, 12.f, 8, FColor::Green, false, 1.f);
    }

    // آخر مسیر همیشه باید نقطه نهایی باشه
    if (!Resampled.Last().Equals(InputPath.Last(), KINDA_SMALL_NUMBER))
    {
        Resampled.Add(InputPath.Last());
        DrawDebugSphere(GetWorld(), InputPath.Last(), 12.f, 8, FColor::Green, false, 7.f);
    }

    return Resampled;
}








