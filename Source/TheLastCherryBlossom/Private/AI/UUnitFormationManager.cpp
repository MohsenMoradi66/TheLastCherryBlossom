

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
    
	// 1. حتماً تایمر قبلی را در همان ابتدا پاک کنید تا پردازش‌های معلق قبلی لغو شوند
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ClusterProcessTimer);
	}

	PendingClusters.Empty();
	bIsProcessingCluster = false; // ریست کردن پرچم وضعیت پردازش

	TArray<TArray<AUnitCharacter*>> Clusters = UUnitClusterLibrary::ClusterUnits(Units, ClusterDistance);

	for (const TArray<AUnitCharacter*>& Cluster : Clusters)
	{
		if (Cluster.Num() == 0) continue;
       
		FClusterRequest Request;
		Request.Units = Cluster;
		Request.Goal = Goal;
		PendingClusters.Enqueue(Request);
	}

	if (GetWorld() && !PendingClusters.IsEmpty())
	{
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
    if (!GetWorld()) 
    {
        bIsProcessingCluster = false;
        return;
    }
    
    if (PendingClusters.IsEmpty())
    {
        bIsProcessingCluster = false;
        return;
    }

    bIsProcessingCluster = true;
    
    FClusterRequest Request;
    PendingClusters.Dequeue(Request);
    
    // ۱. فیلتر کردن یونیت‌های نامعتبر به جای سقط کامل کل خوشه
    TArray<AUnitCharacter*> ValidUnits;
    for (AUnitCharacter* Unit : Request.Units)
    {
        if (Unit && IsValid(Unit) && Unit->GetUnitState() != EUnitState::Dead)
        {
            ValidUnits.Add(Unit);
        }
    }
    
    // اگر هیچ یونیت سالمی در این خوشه نمانده، برو سراغ خوشه بعدی
    if (ValidUnits.Num() == 0)
    {
        if (!PendingClusters.IsEmpty())
        {
            GetWorld()->GetTimerManager().SetTimer(ClusterProcessTimer, this, &UUnitFormationManager::ProcessClusterAsync, 0.01f, false);
        }
        else
        {
            bIsProcessingCluster = false;
        }
        return;
    }

    // ۲. محاسبه مرکز خوشه بر اساس یونیت‌های واقعاً معتبر
    FVector ClusterCenter = (ValidUnits.Num() == 1) ? 
        ValidUnits[0]->GetActorLocation() : 
        CalculateClusterCenter(ValidUnits);

    UGridPathfinderComponent* Pathfinder = ValidUnits[0]->GridPathfinder;
    if (!Pathfinder)
    {
        // اگر پث‌فایندر نبود، بدون قفل کردن سیستم برو بعدی
        if (!PendingClusters.IsEmpty())
        {
            GetWorld()->GetTimerManager().SetTimer(ClusterProcessTimer, this, &UUnitFormationManager::ProcessClusterAsync, 0.01f, false);
        }
        else
        {
            bIsProcessingCluster = false;
        }
        return;
    }

    // ۳. تلاش برای محاسبه مسیر مشترک
    TArray<FVector> Path = Pathfinder->FindPathShared(ClusterCenter, Request.Goal);
    
    if (Path.Num() >= 2)
    {
        // مسیر با موفقیت ساخته شد
        AssignPathToCluster(ValidUnits, Path, Request.Goal);
    }
    else
    {
        // 🌟 واکنش به شکست پث‌فایندر (حل مشکل اصلی شما):
        // اگر مرکز خوشه روی دیوار بود و مسیر مشترک ساخته نشد، یونیت‌های جا مانده را رها نکن!
        // به آن‌ها یک مسیر اضطراری مستقیم یا انفرادی از موقعیت خودشان بده تا حداقل از دیوار کنده شوند.
        for (AUnitCharacter* Unit : ValidUnits)
        {
            if (!Unit) continue;
            
            // تلاش برای ساخت مسیر انفرادی برای یونیت جا مانده از موقعیت دقیق خودش (نه مرکز خوشه)
            TArray<FVector> IndividualPath = Pathfinder->FindPathShared(Unit->GetActorLocation(), Request.Goal);
            
            if (IndividualPath.Num() >= 2)
            {
                ClearUnitMoveState(Unit);
                Unit->SetPathAndMove(IndividualPath);
                if (Unit->SteeringComp)
                {
                    Unit->SetSteeringGroupParams(Unit->GetActorLocation(), (IndividualPath.Last() - IndividualPath[0]).GetSafeNormal());
                }
            }
            else
            {
                // اگر پث‌فایندر گرید کلاً این یونیت را محبوس دانسته، یک مسیر مستقیم خطی به او بده تا فیزیک او را نجات دهد
                TArray<FVector> FallbackPath;
                FallbackPath.Add(Unit->GetActorLocation());
                FallbackPath.Add(Request.Goal);
                
                ClearUnitMoveState(Unit);
                Unit->SetPathAndMove(FallbackPath);
            }
        }
    }

    // ۴. مدیریت زنجیره تایمر برای خوشه‌های بعدی بدون قطع کردن پرچم پردازش
    if (!PendingClusters.IsEmpty())
    {
        GetWorld()->GetTimerManager().SetTimer(
            ClusterProcessTimer,
            this,
            &UUnitFormationManager::ProcessClusterAsync,
            0.02f, // کاهش دیلی از 0.033 به 0.02 برای پاسخ‌دهی سریع‌تر در اسپم کلیک
            false
        );
    }
    else
    {
        bIsProcessingCluster = false;
    }
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
	}
#endif

	for (AUnitCharacter* Unit : Cluster)
	{
		if (!Unit || !IsValid(Unit)) continue;
        
		// ✅ مهم: اول حرکت قبلی رو کامل پاک کن
		ClearUnitMoveState(Unit);
        
		// ✅ مسیر جدید رو تنظیم کن
		Unit->SetPathAndMove(Path);
        
		if (Unit->SteeringComp)
		{
			Unit->SetSteeringGroupParams(ClusterCenter, PathDir);
		}
	}
}








