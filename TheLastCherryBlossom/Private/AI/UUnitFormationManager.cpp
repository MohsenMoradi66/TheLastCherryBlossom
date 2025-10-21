#include "AI/UUnitFormationManager.h"
#include "Characters/AUnitCharacter.h"
#include "AI/UFlowFieldComponent.h"
#include "AI/GridPathfinderComponent.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

UFormationComponent::UFormationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

static TArray<TArray<AUnitCharacter*>> SimpleCluster(const TArray<AUnitCharacter*>& Units, float MaxDist)
{
    TArray<TArray<AUnitCharacter*>> Clusters;
    TArray<AUnitCharacter*> Remaining = Units;

    while (Remaining.Num() > 0)
    {
        AUnitCharacter* Seed = Remaining[0];
        Remaining.RemoveAt(0);

        TArray<AUnitCharacter*> Cluster;
        Cluster.Add(Seed);

        bool bAdded;
        do
        {
            bAdded = false;
            for (int32 i = Remaining.Num() - 1; i >= 0; --i)
            {
                AUnitCharacter* Unit = Remaining[i];
                for (AUnitCharacter* CUnit : Cluster)
                {
                    if (FVector::Dist(Unit->GetActorLocation(), CUnit->GetActorLocation()) <= MaxDist)
                    {
                        Cluster.Add(Unit);
                        Remaining.RemoveAt(i);
                        bAdded = true;
                        break;
                    }
                }
            }
        } while (bAdded);

        Clusters.Add(Cluster);
    }

    return Clusters;
}

void UFormationComponent::MoveUnitsWithClustering(const TArray<AUnitCharacter*>& Units, const FVector& Goal)
{
    if (Units.Num() == 0) return;

    // --- محاسبه میانگین شعاع یونیت‌ها بر اساس Capsule ---
    float UnitRadius = 0.f;
    int32 CountWithCapsule = 0;

    for (AUnitCharacter* Unit : Units)
    {
        if (!Unit) continue;

        UCapsuleComponent* Capsule = Unit->FindComponentByClass<UCapsuleComponent>();
        if (Capsule)
        {
            UnitRadius += Capsule->GetUnscaledCapsuleRadius();
            CountWithCapsule++;
        }
    }

    if (CountWithCapsule > 0)
    {
        UnitRadius /= CountWithCapsule;
    }
    else
    {
        UnitRadius = 17.f; // مقدار پیش‌فرض
    }

    float SpreadFactor = 1.5f; // ضریب اطمینان

    auto CalculateMaxClusterDistance = [=](int32 UnitCount) -> float
    {
        return FMath::Sqrt(static_cast<float>(FMath::Max(UnitCount, 1))) * UnitRadius * SpreadFactor;
    };

    // --- خوشه‌بندی دینامیک ---
    float DynamicMaxDist = CalculateMaxClusterDistance(Units.Num());
    TArray<TArray<AUnitCharacter*>> Clusters = SimpleCluster(Units, DynamicMaxDist);

    UE_LOG(LogTemp, Warning, TEXT("Total Clusters Found: %d"), Clusters.Num());

    ClusterFlowFields.Empty();

    for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
    {
        TArray<AUnitCharacter*>& Cluster = Clusters[ClusterIndex];
        if (Cluster.Num() == 0) continue;

        // بررسی همه یونیت‌ها برای Pathfinder
        bool bAllHavePathfinder = true;
        for (AUnitCharacter* Unit : Cluster)
        {
            if (!Unit || !Unit->FindComponentByClass<UGridPathfinderComponent>())
            {
                bAllHavePathfinder = false;
                UE_LOG(LogTemp, Warning, TEXT("Cluster %d: Unit %s missing Pathfinder! Skipping cluster."), 
                    ClusterIndex + 1, Unit ? *Unit->GetName() : TEXT("nullptr"));
                break;
            }
        }
        if (!bAllHavePathfinder) continue;

        // پیدا کردن مرکز خوشه
        FVector ClusterCenter(0.f);
        for (AUnitCharacter* Unit : Cluster)
        {
            ClusterCenter += Unit->GetActorLocation();
        }
        ClusterCenter /= Cluster.Num();

        // گرفتن Pathfinder از اولین یونیت
        UGridPathfinderComponent* Pathfinder = Cluster[0]->FindComponentByClass<UGridPathfinderComponent>();
        if (!Pathfinder) continue;

        // مسیر خوشه از مرکز به Goal
        TArray<FVector> Path = Pathfinder->FindPathShared(ClusterCenter, Goal);
        if (Path.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Cluster %d: Path not found! Skipping cluster."), ClusterIndex + 1);
            continue;
        }

        // --- ساخت Flow Field مشترک برای خوشه ---
        UFlowFieldComponent* ClusterFlow = NewObject<UFlowFieldComponent>(this);
        ClusterFlow->RegisterComponent();
        ClusterFlow->SetActive(true);
        ClusterFlow->Activate(true);

        ClusterFlowFields.Add(ClusterFlow);

        // --- محاسبه عرض کریدور متناسب با خوشه و CellSize ---
        int32 CorridorWidth = FMath::CeilToInt(DynamicMaxDist / ClusterFlow->GetCellSize()
);
        CorridorWidth = FMath::Max(1, CorridorWidth); // حداقل یک سلول

        ClusterFlow->GenerateFlowField(Path.Last(), Path, CorridorWidth);
        ClusterFlow->TickComponent(0.f, ELevelTick::LEVELTICK_All, nullptr);

        UE_LOG(LogTemp, Warning, TEXT("Cluster %d: Units=%d, FlowFields Stored=%d, CorridorWidth=%d"), 
            ClusterIndex + 1, Cluster.Num(), ClusterFlowFields.Num(), CorridorWidth);

        // --- اختصاص Flow Field به یونیت‌ها ---
        for (AUnitCharacter* Unit : Cluster)
        {
            if (!Unit) continue;
            Unit->SetClusterFlowField(ClusterFlow);
            Unit->SetPathAndMove(Path);
            Unit->SetUnitState(EUnitState::Moving);
            Unit->PrimaryActorTick.bCanEverTick = true;
        }
    }
}








