

#include "AI/UUnitFormationManager.h"
#include "AI/UUnitSteeringComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/AUnitCharacter.h"
#include "AI/GridPathfinderComponent.h"
#include "AI/UUnitClusterLibrary.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"          // for FTimerHandle
#include "Engine/World.h"          // for GetWorld()

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif


// ================ تابع کمکی برای رسم خطوط عمود و اسلات‌ها ================
#if !UE_BUILD_SHIPPING
void DrawPerpendicularLinesAndSlots(const UWorld* World, const TArray<FVector>& Path, float LineLength, int32 NumSlots, float SlotSpacing, float Duration)
{
    if (!World || Path.Num() < 2) return;

    for (int32 i = 1; i < Path.Num(); i++)
    {
        const FVector& Point = Path[i];
        FVector Tangent;

        if (i == 0)
            Tangent = (Path[1] - Path[0]).GetSafeNormal();
        else if (i == Path.Num() - 1)
            Tangent = (Path[i] - Path[i-1]).GetSafeNormal();
        else
        {
            FVector DirPrev = (Path[i] - Path[i-1]).GetSafeNormal();
            FVector DirNext = (Path[i+1] - Path[i]).GetSafeNormal();
            Tangent = (DirPrev + DirNext).GetSafeNormal();
        }

        FVector RightDir = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
        FVector LeftDir = -RightDir;

        FVector Start = Point + RightDir * LineLength;
        FVector End = Point + LeftDir * LineLength;
        DrawDebugLine(World, Start, End, FColor::Green, false, Duration, 0, 3.f);

        for (int32 s = -NumSlots/2; s <= NumSlots/2; s++)
        {
            float Offset = s * SlotSpacing;
            if (FMath::Abs(Offset) > LineLength) continue;
            FVector SlotPoint = Point + RightDir * Offset;
            DrawDebugSphere(World, SlotPoint, 20.f, 8, FColor::Yellow, false, Duration, 0, 2.f);
        }

        DrawDebugSphere(World, Point, 25.f, 10, FColor::Red, false, Duration, 0, 2.f);
    }
}
#endif

UUnitFormationManager::UUnitFormationManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

FVector UUnitFormationManager::CalculateClusterCenter(const TArray<AUnitCharacter*>& Cluster) const
{
    if (Cluster.Num() == 0) return FVector::ZeroVector;
    FVector Center = FVector::ZeroVector;
    int32 Valid = 0;
    for (AUnitCharacter* Unit : Cluster)
    {
        if (Unit && IsValid(Unit))
        {
            Center += Unit->GetActorLocation();
            Valid++;
        }
    }
    return (Valid > 0) ? Center / Valid : FVector::ZeroVector;
}

void UUnitFormationManager::ClearUnitMoveState(AUnitCharacter* Unit)
{
    if (Unit)
        Unit->ClearMovementState();
}

void UUnitFormationManager::CancelAllMoves()
{
    // در این نسخه ساده، نیازی به نگاشت نداریم؛ فقط باید حرکت همه یونیت‌ها را لغو کنیم.
    // اما چون دسترسی به لیست یونیت‌ها نداریم، این تابع را خالی می‌گذاریم.
    // اگر نیاز به لغو همه یونیت‌ها دارید، باید لیست آن‌ها را نگه دارید یا از راه دیگری استفاده کنید.
}

void UUnitFormationManager::ResetFormation()
{
    // دیگر هیچ فلو فیلدی وجود ندارد
}

void UUnitFormationManager::StartFormationComputationForPath(const TArray<FVector>& Path)
{
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("StartFormationComputationForPath: No valid world"));
        return;
    }
    PendingPath = Path;
    CurrentWaypointIndex = 0;

    // Clear any existing timer
    GetWorld()->GetTimerManager().ClearTimer(FormationProcessTimer);

    // Start timer: call ProcessNextWaypoint every ~0.033 sec (one frame at 30 fps)
    GetWorld()->GetTimerManager().SetTimer(
        FormationProcessTimer,
        this,
        &UUnitFormationManager::ProcessNextWaypoint,
        0.033f,
        true   // loop
    );
}

void UUnitFormationManager::ProcessNextWaypoint()
{
    if (CurrentWaypointIndex >= PendingPath.Num())
    {
        // All waypoints processed
        GetWorld()->GetTimerManager().ClearTimer(FormationProcessTimer);
        OnAllWaypointsProcessed();
        return;
    }

    const FVector& Point = PendingPath[CurrentWaypointIndex];
    ComputePerpendicularDataForPoint(Point, CurrentWaypointIndex);

    CurrentWaypointIndex++;
}

void UUnitFormationManager::OnAllWaypointsProcessed()
{
    UE_LOG(LogTemp, Log, TEXT("All waypoints processed for formation path"));
    // Here you could trigger a delegate or set a flag that data is ready.
}

void UUnitFormationManager::ComputePerpendicularDataForPoint(const FVector& Point, int32 Index)
{
    // ** This is where you compute the tangent, right vector, slots, etc. **
    // For now, a placeholder that just draws debug (if enabled) for this single point.
    // In your real implementation, you would store the computed data in a cache.

#if !UE_BUILD_SHIPPING
    if (bDrawDebug && GetWorld() && PendingPath.Num() >= 2)
    {
        // Compute tangent at this point using neighbours (simplified)
        FVector Tangent;
        int32 Last = PendingPath.Num() - 1;
        if (Index == 0)
            Tangent = (PendingPath[1] - PendingPath[0]).GetSafeNormal();
        else if (Index == Last)
            Tangent = (PendingPath[Last] - PendingPath[Last-1]).GetSafeNormal();
        else
        {
            FVector DirPrev = (PendingPath[Index] - PendingPath[Index-1]).GetSafeNormal();
            FVector DirNext = (PendingPath[Index+1] - PendingPath[Index]).GetSafeNormal();
            Tangent = (DirPrev + DirNext).GetSafeNormal();
        }

        FVector RightDir = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
        float LineLength = 500.f;
        FVector Start = Point + RightDir * LineLength;
        FVector End   = Point - RightDir * LineLength;

        // Draw only this perpendicular line for this one point (visible for a short time)
        DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 0.5f, 0, 2.f);
        DrawDebugSphere(GetWorld(), Point, 20.f, 8, FColor::Red, false, 0.5f, 0, 2.f);
    }
#endif
}

void UUnitFormationManager::AssignPathToCluster(const TArray<AUnitCharacter*>& Cluster, const TArray<FVector>& Path, const FVector& Goal)
{
	if (Cluster.Num() == 0) return;
    
	FVector PathDir = (Path.Last() - Path[0]).GetSafeNormal();
	FVector ClusterCenter = (Cluster.Num() == 1) ? Cluster[0]->GetActorLocation() : CalculateClusterCenter(Cluster);

#if !UE_BUILD_SHIPPING
	if (bDrawDebug && GetWorld())
	{
		for (int32 i = 0; i < Path.Num() - 1; i++)
		{
			DrawDebugLine(GetWorld(), Path[i], Path[i+1], FColor::Blue, false, 5.f, 0, 2.f);
		}
		StartFormationComputationForPath(Path);
	}
#endif

	for (AUnitCharacter* Unit : Cluster)
	{
		if (!Unit) continue;
		ClearUnitMoveState(Unit);
		Unit->SetPathAndMove(Path);
		if (Unit->SteeringComp)
		{
			Unit->SetSteeringGroupParams(ClusterCenter, PathDir);
			// خط تداخل‌زا (ClearTarget) از اینجا کاملاً حذف شد!
		}
	}
}

void UUnitFormationManager::AssignSimpleClusterPath(const TArray<AUnitCharacter*>& Cluster, const FVector& Goal)
{
	if (Cluster.Num() <= 1) return;

	AUnitCharacter* Seed = Cluster[0];
	if (!Seed) return;

	UGridPathfinderComponent* Pathfinder = Seed->GridPathfinder;
	if (!Pathfinder) return;

	FVector ClusterCenter = CalculateClusterCenter(Cluster);
	TArray<FVector> Path = Pathfinder->FindPathShared(ClusterCenter, Goal);
	if (Path.Num() < 2) return;

	FVector PathDir = (Path.Last() - Path[0]).GetSafeNormal();

	for (AUnitCharacter* Unit : Cluster)
	{
		if (!Unit) continue;
		ClearUnitMoveState(Unit);
		Unit->SetPathAndMove(Path);   
		Unit->SetSteeringGroupParams(ClusterCenter, PathDir);
		// خط تداخل‌زا (ClearTarget) از اینجا کاملاً حذف شد!
	}

#if !UE_BUILD_SHIPPING
	if (bDrawDebug)
	{
		for (int32 i = 0; i < Path.Num() - 1; i++)
		{
			DrawDebugLine(GetWorld(), Path[i], Path[i+1], FColor::Blue, false, 5.f, 0, 2.f);
		}
	}
#endif
}

void UUnitFormationManager::MoveUnitsWithClustering(const TArray<AUnitCharacter*>& Units, const FVector& Goal)
{
	if (Units.Num() == 0) return;
    
	// ۱. پاک کردن صف قبلی برای جلوگیری از انباشته شدن دستورات قدیمی
	PendingClusters.Empty();

	TArray<TArray<AUnitCharacter*>> Clusters = UUnitClusterLibrary::ClusterUnits(Units, ClusterDistance);

	for (const TArray<AUnitCharacter*>& Cluster : Clusters)
	{
		if (Cluster.Num() == 0) continue;
       
		FClusterRequest Request;
		Request.Units = Cluster;
		Request.Goal = Goal;
		PendingClusters.Enqueue(Request);
	}

	// ۲. اصلاح کلیدی: به جای اجرای مستقیم و درجا، پردازش را "حتماً" با یک تاخیر ناچیز به فریم بعدی واگذار میکنیم
	// این کار باعث می‌شود اسپم کلیک نتواند چرخه استیرینگ کاراکتر را در یک فریم چندبار Overwrite کند.
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ClusterProcessTimer);
        
		// جادوی اصلی: اجرای غیرهمزمان واقعی با دیلی یک فریم (0.01 ثانیه یا حتی 0.0f ثانیه در فریم بعد)
		GetWorld()->GetTimerManager().SetTimer(
			ClusterProcessTimer,
			this,
			&UUnitFormationManager::ProcessClusterAsync,
			0.01f, 
			false
		);
	}
}

void UUnitFormationManager::ProcessClusterAsync()
{
	if (!GetWorld()) return;
    
	if (PendingClusters.IsEmpty())
	{
		bIsProcessingCluster = false;
		return;
	}

	bIsProcessingCluster = true;
    
	FClusterRequest Request;
	PendingClusters.Dequeue(Request);
    
	// بررسی معتبر بودن کاراکترها
	if (Request.Units.Num() == 0 || !IsValid(Request.Units[0]))
	{
		bIsProcessingCluster = false;
		ProcessClusterAsync();
		return;
	}

	// محاسبه مرکز خوشه
	FVector ClusterCenter = (Request.Units.Num() == 1) ? 
	   Request.Units[0]->GetActorLocation() : 
	   CalculateClusterCenter(Request.Units);

	// گرفتن Pathfinder
	UGridPathfinderComponent* Pathfinder = Request.Units[0]->GridPathfinder;
	if (!Pathfinder)
	{
		bIsProcessingCluster = false;
		ProcessClusterAsync();
		return;
	}

	// محاسبه مسیر مشترک
	TArray<FVector> Path = Pathfinder->FindPathShared(ClusterCenter, Request.Goal);
    
	if (Path.Num() >= 2)
	{
		AssignPathToCluster(Request.Units, Path, Request.Goal);
	}

	// اگر هنوز خوشه‌ای در صف باقی مانده، فریم بعدی پردازشش میکنیم
	if (!PendingClusters.IsEmpty())
	{
		GetWorld()->GetTimerManager().SetTimer(
		   ClusterProcessTimer,
		   this,
		   &UUnitFormationManager::ProcessClusterAsync,
		   0.033f,
		   false
		);
	}
	else
	{
		bIsProcessingCluster = false;
	}
}






