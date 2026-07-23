#include "AI/UUnitClusterLibrary.h"
#include "Characters/AUnitCharacter.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "../TheLastCherryBlossom.h" 
#include "Engine/EngineTypes.h"

TArray<TArray<AUnitCharacter*>> UUnitClusterLibrary::ClusterUnits(
    const TArray<AUnitCharacter*>& Units,
    float Radius)
{
    return SimpleClusterFixedRadius(Units, Radius);
}

TArray<TArray<AUnitCharacter*>> UUnitClusterLibrary::SimpleClusterFixedRadius(
    const TArray<AUnitCharacter*>& Units,
    float Radius)
{
    TArray<TArray<AUnitCharacter*>> Clusters;
    if (Units.Num() == 0) return Clusters;

    UWorld* World = nullptr;
    // پیدا کردن یک World معتبر از طریق یونیت‌ها برای اجرای LineTrace
    for (AUnitCharacter* Unit : Units)
    {
        if (Unit && IsValid(Unit))
        {
            World = Unit->GetWorld();
            break;
        }
    }
    if (!World) return Clusters;

    // لیست یونیت‌هایی که هنوز داخل هیچ خوشه‌ای نرفته‌اند
    TArray<AUnitCharacter*> UnassignedUnits;
    for (AUnitCharacter* Unit : Units)
    {
        if (Unit && IsValid(Unit)) UnassignedUnits.Add(Unit);
    }

    // تنظیمات ساختاری برای تست برخورد با موانع ثابت محیطی
    FCollisionQueryParams TraceParams;
    TraceParams.bTraceComplex = false; // برای بهینه‌سازی، با کلايدر ساده تست کن
    
    // یونیت‌های انتخابی را از تست برخورد خطی حذف کن تا لیزر به خود سربازها گیر نکند
    for (AUnitCharacter* Unit : UnassignedUnits)
    {
        TraceParams.AddIgnoredActor(Unit);
    }

    // حلقه اصلی خوشه‌بندی هوشمند
    while (UnassignedUnits.Num() > 0)
    {
        // ۱. انتخاب اولین یونیت باقی‌مانده به عنوان لیدر/هسته خوشه جدید
        AUnitCharacter* Seed = UnassignedUnits[0];
        UnassignedUnits.RemoveAt(0);

        TArray<AUnitCharacter*> CurrentCluster;
        CurrentCluster.Add(Seed);

        // ۲. اسکن سایر یونیت‌های باقی‌مانده و سنجش دو فیلتر (فاصله + دیوار) نسبت به لیدر
        for (int32 i = UnassignedUnits.Num() - 1; i >= 0; --i)
        {
            AUnitCharacter* Candidate = UnassignedUnits[i];
            
            FVector SeedLoc = Seed->GetActorLocation();
            FVector CandidateLoc = Candidate->GetActorLocation();

            // ⚡ فیلتر اول (کاملاً سبک): بررسی فاصله اقلیدسی مستقیم
            float DistSq = FVector::DistSquared(SeedLoc, CandidateLoc);
            if (DistSq <= (Radius * Radius))
            {
                // ⚡ فیلتر دوم (فقط در صورت تایید فیلتر اول): بررسی عدم وجود دیوار بین لیدر و کاندیدا
                FHitResult HitResult;
                // خط قدیمی را پیدا کن و پارامتر کانال آن را به ECC_RTS_Obstacle تغییر بده:
                bool bHitWall = World->LineTraceSingleByChannel(
                    HitResult,
                    SeedLoc,
                    CandidateLoc,
                    ECC_RTS_Obstacle, // 🌟 اینجا را تغییر دادیم تا فقط موانع ثابت را چک کند
                    TraceParams
                );

                if (!bHitWall)
                {
                    // تبریک! کاندیدا هم نزدیک است و هم دیواری بین او و لیدر وجود ندارد
                    CurrentCluster.Add(Candidate);
                    UnassignedUnits.RemoveAt(i);
                }
            }
        }

        // ۳. ثبت خوشه نهایی و امن به لیست خوشه‌ها
        Clusters.Add(CurrentCluster);
    }

    return Clusters;
}