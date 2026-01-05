#include "AI/UUnitFormationManager.h"
#include "Characters/AUnitCharacter.h"
#include "AI/UFlowFieldComponent.h"
#include "AI/GridPathfinderComponent.h"
#include "NavigationSystem.h"
#include "Algo/Sort.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "AI/UUnitClusterLibrary.h"

UUnitFormationManager::UUnitFormationManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UUnitFormationManager::IsUnitPathClear(
	AUnitCharacter* Unit,
	const TArray<FVector>& PathPoints,
	float CheckDistance)
{
	// TODO: Raycasts / NavMesh checks
	return true;
}

float UUnitFormationManager::CalculateCorridorWidthForCluster(
	const TArray<AUnitCharacter*>& Cluster,
	const FVector& ClusterCenter,
	const FVector& PathDirection, AUnitCharacter* Seed)
{
	if (Cluster.Num() == 0) return 0.f;

	float MaxDistance = 0.f;
	FVector PerpDir = FVector::CrossProduct(PathDirection.GetSafeNormal(), FVector::UpVector);

	for (AUnitCharacter* Unit : Cluster)
	{
		if (!Unit) continue;
		FVector ToUnit = Unit->GetActorLocation() - Seed->GetActorLocation();

		float Distance = FMath::Abs(FVector::DotProduct(ToUnit, PerpDir));
		MaxDistance = FMath::Max(MaxDistance, Distance);
	}

	float CapsuleRadius = 0.f;
	if (Cluster[0])
	{
		if (UCapsuleComponent* Capsule = Cluster[0]->FindComponentByClass<UCapsuleComponent>())
			CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	}

	float CorridorWidthCm = MaxDistance * 2.f + CapsuleRadius * 8.f;

	UE_LOG(LogTemp, Warning, TEXT("[CorridorWidth] Cluster=%d, MaxDist=%.1f, Radius=%.1f, Width=%.1f"),
		Cluster.Num(), MaxDistance, CapsuleRadius, CorridorWidthCm);

	return CorridorWidthCm;
}

TArray<TArray<float>> UUnitFormationManager::GetFormationMatrix(int32 UnitCount) const
{
	// مقدار -1 = خانه خالی، مقدار 1 = خانه پر
	TArray<TArray<float>> Matrix;

	switch (UnitCount)
	{
	case 1:
		Matrix = { {1} };
		break;
	case 2:
		Matrix = { {1,-1,1} };
		break;
	case 3:
		Matrix = { {1,-1,1,-1,1} };
		break;
	case 4:
		Matrix = { {1,-1,1},{-1,-1,-1,},{1,-1,1} };
		break;
	case 5:
		Matrix = { {1,-1,1,-1,1,},{-1,-1,-1,-1,-1},{1,-1,-1,-1,1} };
		break;
	case 6:
		Matrix = { {1,-1,1,-1,1,},{-1,-1,-1,-1,-1},{1,-1,1,-1,1,} };
		break;
	case 7:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,-1,1,-1,-1,1} };
		break;
	case 8:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1} };
		break;
	case 9:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{-1,-1,-1,1,-1,-1,-1} };
		break;
	case 10:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,-1,-1,-1,-1,1} };
		break;
	case 11:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,-1,1,-1,-1,1} };
		break;
	case 12:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1} };
		break;
	case 13:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{-1,-1,-1,1,-1,-1,-1} };
		break;
	case 14:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,-1,-1,-1,-1,1} };
		break;
	case 15:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,-1,1,-1,-1,1} };
		break;
	case 16:
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,-1,-1,-1,-1},{1,-1,1,-1,1,-1,1} };
		break;
	default:
	{
		int32 GridSize = FMath::CeilToInt(FMath::Sqrt((float)UnitCount));
		Matrix.SetNum(GridSize);
		for (int32 r = 0; r < GridSize; r++)
		{
			Matrix[r].SetNum(GridSize);
			for (int32 c = 0; c < GridSize; c++)
				Matrix[r][c] = (r*GridSize + c < UnitCount) ? 1.f : -1.f;
		}
	}
	break;
	}

	return Matrix;
}

// ---------- BuildCostMatrix ------------
void UUnitFormationManager::BuildCostMatrix(
	const TArray<AUnitCharacter*>& Cluster,
	const TArray<FVector>& Slots,
	TArray<TArray<float>>& OutCost)
{
	int32 N = Cluster.Num();
	int32 M = Slots.Num();
	OutCost.SetNum(N);

	// <<< درست: از عضو کلاس استفاده کن (که قبلاً در AssignOptimalFormation ست شده) >>>
	// FormationForward = InFormationForward;  ← این خط رو کامل حذف کن

	for (int32 i = 0; i < N; i++)
	{
		OutCost[i].SetNum(M);
		FVector UnitPos = Cluster[i] ? Cluster[i]->GetActorLocation() : FVector::ZeroVector;

		for (int32 j = 0; j < M; j++)
		{
			float Distance = FVector::Dist(UnitPos, Slots[j]);

			// محاسبه چقدر این اسلات "جلو" هست
			FVector ToSlot = Slots[j] - FinalGoal;
			float ForwardAmount = FVector::DotProduct(ToSlot.GetSafeNormal(), FormationForward.GetSafeNormal());

			float ForwardBias = ForwardAmount * 300.f;  // عدد دلخواه – تست کن

			OutCost[i][j] = Distance - ForwardBias;
			OutCost[i][j] = FMath::Max(0.f, OutCost[i][j]);
		}
	}
}

// ---------- HungarianSolve (O(n^3) standard) ------------
TArray<int32> UUnitFormationManager::HungarianSolve(const TArray<TArray<float>>& Cost)
{
	int32 N = Cost.Num();
	// Safety: if empty
	if (N == 0) return TArray<int32>();

	int32 M = Cost[0].Num();
	int32 Dim = FMath::Max(N, M);

	// build square matrix A (Dim x Dim)
	TArray<TArray<float>> A;
	A.SetNum(Dim);
	for (int i = 0; i < Dim; ++i)
	{
		A[i].SetNum(Dim);
		for (int j = 0; j < Dim; ++j)
		{
			if (i < N && j < M) A[i][j] = Cost[i][j];
			else A[i][j] = 1e6f; // large cost for dummy cells
		}
	}

	TArray<float> u; u.Init(0.f, Dim + 1);
	TArray<float> v; v.Init(0.f, Dim + 1);
	TArray<int32> p; p.Init(0, Dim + 1);
	TArray<int32> way; way.Init(0, Dim + 1);

	for (int32 i = 1; i <= Dim; ++i)
	{
		p[0] = i;
		int32 j0 = 0;
		TArray<float> minv; minv.Init(FLT_MAX, Dim + 1);
		TArray<bool> used; used.Init(false, Dim + 1);

		do
		{
			used[j0] = true;
			int32 i0 = p[j0];
			float delta = FLT_MAX;
			int32 j1 = 0;

			for (int32 j = 1; j <= Dim; ++j)
			{
				if (used[j]) continue;
				float cur = A[i0 - 1][j - 1] - u[i0] - v[j];
				if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
				if (minv[j] < delta) { delta = minv[j]; j1 = j; }
			}

			for (int32 j = 0; j <= Dim; ++j)
			{
				if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
				else minv[j] -= delta;
			}
			j0 = j1;
		} while (p[j0] != 0);

		do
		{
			int32 j1 = way[j0];
			p[j0] = p[j1];
			j0 = j1;
		} while (j0);
	}

	TArray<int32> Assignment;
	Assignment.SetNum(N);
	for (int32 j = 1; j <= Dim; ++j)
	{
		if (p[j] > 0 && p[j] <= N && j <= M)
		{
			Assignment[p[j] - 1] = j - 1;
		}
	}

	return Assignment;
}

// ---------- Greedy fallback (برای خوشه‌های خیلی بزرگ) ------------
void UUnitFormationManager::GreedyAssign(const TArray<TArray<float>>& Cost, TArray<int32>& OutAssignment)
{
	int32 N = Cost.Num();
	int32 M = Cost[0].Num();
	OutAssignment.Init(-1, N);
	TArray<bool> SlotUsed; SlotUsed.Init(false, M);

	for (int i = 0; i < N; ++i)
	{
		float Best = FLT_MAX;
		int32 BestJ = -1;
		for (int j = 0; j < M; ++j)
		{
			if (SlotUsed[j]) continue;
			if (Cost[i][j] < Best) { Best = Cost[i][j]; BestJ = j; }
		}
		if (BestJ >= 0) { OutAssignment[i] = BestJ; SlotUsed[BestJ] = true; }
	}
}

// ---------- Collision avoidance tweak (simple separation) ------------
void UUnitFormationManager::ApplySimpleSeparation(TArray<FVector>& Slots, float MinSeparation)
{
	int32 M = Slots.Num();
	if (M < 2) return;

	for (int i = 0; i < M; ++i)
	{
		for (int j = i + 1; j < M; ++j)
		{
			float Dist = FVector::Dist(Slots[i], Slots[j]);
			if (Dist < MinSeparation && Dist > KINDA_SMALL_NUMBER)
			{
				FVector Dir = (Slots[j] - Slots[i]).GetSafeNormal();
				if (Dir.IsNearlyZero()) Dir = FVector::RightVector;
				float Need = (MinSeparation - Dist) * 0.5f;
				Slots[i] += -Dir * Need;
				Slots[j] += Dir * Need;
			}
			else if (Dist <= KINDA_SMALL_NUMBER)
			{
				float Angle = (float)(i - j) * 0.6f;
				FVector Jitter = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (MinSeparation * 0.5f);
				Slots[i] += Jitter;
				Slots[j] -= Jitter;
			}
		}
	}
}

void UUnitFormationManager::AssignOptimalFormation(
    const TArray<AUnitCharacter*>& Units,
    const FVector& Goal,
    const FVector& InFormationForward)
{
    if (Units.Num() == 0) return;

    // جهت تشکیلات (رو به هدف)
    FormationForward = InFormationForward.IsNearlyZero() ? FVector::ForwardVector : InFormationForward.GetSafeNormal();

    // جهت راست – خیلی مهمه که درست باشه (چپ به راست مثبت باشه)
	FVector Right = FVector::CrossProduct(FormationForward, FVector::UpVector).GetSafeNormal(); // امتحان کن این رو
    // اگر تشکیلات معکوس شد، این خط رو کامنت کن و خط زیر رو باز کن:
    // FVector Right = FVector::CrossProduct(FormationForward, FVector::UpVector).GetSafeNormal();

    // ۱. ساخت اسلات‌ها
    TArray<FVector> Slots;
    BuildFormationSlots(Units, Goal, Slots, FormationForward);

    // ۲. جلوگیری از همپوشانی
    ApplySimpleSeparation(Slots, 70.f);

    // ۳. مرتب‌سازی یونیت‌ها بر اساس موقعیت فعلی (نه نسبت به Goal)
    TArray<AUnitCharacter*> SortedUnits = Units;
    Algo::Sort(SortedUnits, [&](const AUnitCharacter* A, const AUnitCharacter* B) -> bool
    {
        if (!A || !B) return A != nullptr;

        FVector PosA = A->GetActorLocation();
        FVector PosB = B->GetActorLocation();

        // پروجکشن روی جهت جلو (بزرگ‌تر = جلوتر)
        float ProjForwardA = FVector::DotProduct(PosA, FormationForward);
        float ProjForwardB = FVector::DotProduct(PosB, FormationForward);

        // پروجکشن روی جهت راست (بزرگ‌تر = راست‌تر)
        float ProjRightA = FVector::DotProduct(PosA, Right);
        float ProjRightB = FVector::DotProduct(PosB, Right);

        // اول: یونیت‌های جلوتر اول بیان
        if (!FMath::IsNearlyEqual(ProjForwardA, ProjForwardB, 30.f))
        {
            return ProjForwardA > ProjForwardB; // جلوتر اول
        }

        // دوم: یونیت‌های چپ‌تر اول بیان (عدد کوچک‌تر = چپ‌تر)
        return ProjRightA < ProjRightB;
    });

    // ۴. مرتب‌سازی اسلات‌ها دقیقاً به همین ترتیب
    TArray<FVector> SortedSlots = Slots;
    Algo::Sort(SortedSlots, [&](const FVector& A, const FVector& B) -> bool
    {
        float ProjForwardA = FVector::DotProduct(A, FormationForward);
        float ProjForwardB = FVector::DotProduct(B, FormationForward);

        float ProjRightA = FVector::DotProduct(A, Right);
        float ProjRightB = FVector::DotProduct(B, Right);

        if (!FMath::IsNearlyEqual(ProjForwardA, ProjForwardB, 30.f))
        {
            return ProjForwardA > ProjForwardB;
        }

        return ProjRightA < ProjRightB;
    });

    // ۵. دیباگ خیلی مهم برای تست
    UE_LOG(LogTemp, Warning, TEXT("=== FINAL MILITARY FORMATION ==="));
    for (int32 i = 0; i < SortedUnits.Num(); ++i)
    {
        if (SortedUnits[i] && i < SortedSlots.Num())
        {
            FVector UnitPos = SortedUnits[i]->GetActorLocation();
            float UnitForward = FVector::DotProduct(UnitPos, FormationForward);
            float UnitRight = FVector::DotProduct(UnitPos, Right);

            FVector SlotPos = SortedSlots[i];
            float SlotForward = FVector::DotProduct(SlotPos, FormationForward);
            float SlotRight = FVector::DotProduct(SlotPos, Right);

            UE_LOG(LogTemp, Warning, TEXT("Unit[%d] %s | Forward: %.0f | Right: %.0f → Slot[%d] | Forward: %.0f | Right: %.0f"),
                i, *SortedUnits[i]->GetName(), UnitForward, UnitRight, i, SlotForward, SlotRight);
        }
    }

    // ۶. اختصاص نهایی
    for (int32 i = 0; i < SortedUnits.Num(); ++i)
    {
        AUnitCharacter* Unit = SortedUnits[i];
        if (!Unit || i >= SortedSlots.Num()) continue;

        Unit->FormationTarget = SortedSlots[i];
        Unit->bReachedFormationTarget = false;

        if (bDrawFormationDebug)
        {
            DrawDebugLine(GetWorld(), Unit->GetActorLocation(), SortedSlots[i], FColor::Cyan, false, 15.f, 0, 4.f);
            DrawDebugSphere(GetWorld(), SortedSlots[i], 40.f, 12, FColor::Green, false, 15.f);
            DrawDebugString(GetWorld(), SortedSlots[i] + FVector(0, 0, 80),
                FString::Printf(TEXT("Slot %d"), i), nullptr, FColor::White, 15.f);
        }
    }
}

void UUnitFormationManager::OnUnitEnteredFormationSphere(AUnitCharacter* Unit)
{
	if (bFormationAlreadyAssigned)
		return; // قبلاً انجام شده

	bFormationAlreadyAssigned = true;

	// تخصیص اسلات‌ها به همه یونیت‌ها و حرکت مستقیم
	AssignFormationToAllUnits();
}

TArray<AUnitCharacter*> UUnitFormationManager::GetAllUnitsInThisCluster() const
{
	// اگر یونیت‌ها در CurrentClusterUnits ذخیره شده‌اند:
	return CurrentClusterUnits;
}

void UUnitFormationManager::ProjectSlotsToNavMesh(TArray<FVector>& Slots)
{
	// این تابع می‌تواند با UNavigationSystemV1::ProjectPointToNavigation پیاده‌سازی شود
	for (FVector& Slot : Slots)
	{
		FNavLocation NavLoc;
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			if (NavSys->ProjectPointToNavigation(Slot, NavLoc))
				Slot = NavLoc.Location;
		}
	}
}

void UUnitFormationManager::AssignFormationToAllUnits()
{
	TArray<AUnitCharacter*> AllUnits = GetAllUnitsInThisCluster();
	if (AllUnits.Num() == 0) return;

	// 1) ساخت اسلات‌ها
	TArray<FVector> Slots;
	BuildFormationSlots(AllUnits, FinalGoal, Slots, FormationForward);

	// 2) جدا کردن اسلات‌ها برای جلوگیری از برخورد
	ApplySimpleSeparation(Slots, 70.f);

	// 3) تثبیت روی NavMesh
	ProjectSlotsToNavMesh(Slots);

	// 4) ساخت ماتریس هزینه و حل Hungarian
	TArray<TArray<float>> CostMatrix;
	BuildCostMatrix(AllUnits, Slots, CostMatrix);
	TArray<int32> Assignment = HungarianSolve(CostMatrix);

	// 5) **فقط** اختصاص اسلات‌ها به یونیت‌ها (بدون فعال‌سازی مسیر مستقیم)
	for (int32 i = 0; i < AllUnits.Num(); ++i)
	{
		AUnitCharacter* Unit = AllUnits[i];
		if (!Unit) continue;
		int32 SlotIndex = (i < Assignment.Num()) ? Assignment[i] : -1;
		if (SlotIndex >= 0 && SlotIndex < Slots.Num())
		{
			Unit->FormationTarget = Slots[SlotIndex];
			Unit->bReachedFormationTarget = false;
			// Do NOT call MoveDirectlyToTarget here!
			// Leave bUseFlowField as-is - so unit continues following FlowField until it enters sphere.
		}
	}
}

void UUnitFormationManager::BuildFormationSlots(
	const TArray<AUnitCharacter*>& Cluster,
	const FVector& Goal,
	TArray<FVector>& OutSlots,
	const FVector& InFormationForward)
{
	OutSlots.Empty();
	int32 Count = Cluster.Num();
	if (Count == 0) return;

	const float Spacing = 75.f;
	TArray<TArray<float>> Matrix = GetFormationMatrix(Count);

	int32 Rows = Matrix.Num();
	int32 Columns = Matrix[0].Num();

	FVector Forward = InFormationForward.IsNearlyZero() ? FVector::ForwardVector : InFormationForward;
	FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();

	float HalfW = (Columns - 1) * 0.5f * Spacing;
	float HalfH = (Rows - 1) * 0.5f * Spacing;

	for (int32 r = 0; r < Rows; r++)
	{
		for (int32 c = 0; c < Columns; c++)
		{
			if (Matrix[r][c] < 0) continue;
			float X = (c * Spacing - HalfW) * Matrix[r][c];
			float Y = r * Spacing - HalfH;
			OutSlots.Add(Goal + Right * X + Forward * (-Y));
		}
	}

	if (bDrawFormationDebug)
	{
		for (int32 i = 0; i < OutSlots.Num(); ++i)
		{
			DrawDebugSphere(GetWorld(), OutSlots[i], 25.f, 8, FColor::Blue, false, DebugDrawTime, 0, 1.5f);
			DrawDebugString(GetWorld(), OutSlots[i] + FVector(0, 0, 30), FString::Printf(TEXT("%d"), i), nullptr, FColor::White, DebugDrawTime);
		}
	}
}



void UUnitFormationManager::MoveUnitsDirectlyToSlots(
	const TArray<AUnitCharacter*>& Units,
	const TArray<FVector>& Slots,
	const TArray<int32>& Assignment)
{
	for (int32 i = 0; i < Units.Num(); i++)
	{
		AUnitCharacter* Unit = Units[i];
		if (!Unit) continue;

		int32 SlotIndex = (i < Assignment.Num()) ? Assignment[i] : -1;
		if (SlotIndex >= 0 && SlotIndex < Slots.Num())
		{
			Unit->FormationTarget = Slots[SlotIndex];
			Unit->SetUnitState(EUnitState::MovingToFormation);
			Unit->bReachedFormationTarget = false;

			// ⚡ فقط در اینجا مسیر مستقیم فعال شود
			Unit->MoveDirectlyToTarget(Slots[SlotIndex]);

			if (bDrawFormationDebug)
			{
				DrawDebugLine(GetWorld(), Unit->GetActorLocation(), Slots[SlotIndex], FColor::Green, false, DebugDrawTime, 0, 1.2f);
				DrawDebugSphere(GetWorld(), Slots[SlotIndex], 22.f, 6, FColor::Yellow, false, DebugDrawTime, 0, 1.2f);
			}
		}
	}
}

void UUnitFormationManager::MoveUnitsWithClustering(const TArray<AUnitCharacter*>& Units, const FVector& Goal)
{
    if (Units.Num() == 0) return;

    bFormationAlreadyAssigned = false;
    this->CurrentClusterUnits = Units;
    this->FinalGoal = Goal;
    this->FormationForward = FVector::ForwardVector;

    // خوشه‌بندی یونیت‌ها
    TArray<TArray<AUnitCharacter*>> Clusters = UUnitClusterLibrary::ClusterUnits(Units, 500.f);
    ClusterFlowFields.Empty();
    TArray<FVector> ClusterDirections;

    for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
    {
        TArray<AUnitCharacter*>& Cluster = Clusters[ClusterIndex];
        if (Cluster.Num() == 0) continue;

        AUnitCharacter* Seed = Cluster[0];
        if (!Seed) continue;

        UGridPathfinderComponent* Pathfinder = Seed->FindComponentByClass<UGridPathfinderComponent>();
        if (!Pathfinder) continue;

        TArray<FVector> Path = Pathfinder->FindPathShared(Seed->GetActorLocation(), Goal);
        if (Path.Num() < 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("Cluster %d: No Path!"), ClusterIndex);
            continue;
        }

        bool bIsSingleUnit = (Cluster.Num() == 1);

        // ===== تک یونیت =====
    	if (bIsSingleUnit)
    	{
    		AUnitCharacter* Unit = Cluster[0];
    		if (!Unit) continue;

    		// مسیر و هدف
    		Unit->FinalGoalLocation = Goal;
    		Unit->FinalGoalRadius = 500.f;

    		// مسیر مستقیم نقطه به نقطه
    		Unit->FollowPathDirectly(Path);  // ← این را جایگزین SetPathAndMove(Path,true) کن
    		Unit->bUseFlowField = false;     // مطمئن شو FlowField خاموش است

    		UE_LOG(LogTemp, Warning, TEXT("Single unit %s moving along path without FlowField"), *Unit->GetName());
    		continue;
    	}


        // ===== خوشه‌های بزرگتر از 1 =====
        FVector ClusterDir = (Path.Last() - Path[0]).GetSafeNormal();
        ClusterDirections.Add(ClusterDir);

        // مرکز خوشه و عرض مسیر
        FVector ClusterCenter(0.f);
        for (AUnitCharacter* Unit : Cluster)
            if (Unit) ClusterCenter += Unit->GetActorLocation();
        ClusterCenter /= Cluster.Num();

        float CorridorWidthCm = CalculateCorridorWidthForCluster(Cluster, ClusterCenter, ClusterDir, Seed);

        // Back offset
        float MaxBackwardDist = 0.f;
        FVector BackDir = -ClusterDir;
        for (AUnitCharacter* Unit : Cluster)
        {
            if (!Unit) continue;
            float BackAmount = FVector::DotProduct(Unit->GetActorLocation() - Seed->GetActorLocation(), BackDir);
            if (BackAmount > MaxBackwardDist) MaxBackwardDist = BackAmount;
        }

        float CapsuleRadius = 0.f;
        if (UCapsuleComponent* Cap = Seed->FindComponentByClass<UCapsuleComponent>())
            CapsuleRadius = Cap->GetScaledCapsuleRadius();

        float Safety = CapsuleRadius * 6.f;
        float TotalOffset = MaxBackwardDist + Safety;
        FVector ExtendedStart = Path[0] - ClusterDir * TotalOffset;
        Path.Insert(ExtendedStart, 0);

        // ===== ساخت FlowField =====
        UFlowFieldComponent* FF = NewObject<UFlowFieldComponent>(this);
        if (FF)
        {
            FF->RegisterComponent();
            FF->Activate();
            FF->GenerateFlowField(Path.Last(), Path, FMath::RoundToInt(CorridorWidthCm));
            FF->TickComponent(0.f, ELevelTick::LEVELTICK_All, nullptr);
            ClusterFlowFields.Add(FF);
        }

        // اختصاص مقصد و FlowField به یونیت‌ها
        for (AUnitCharacter* Unit : Cluster)
        {
            if (!Unit) continue;
            Unit->FinalGoalLocation = Goal;
            Unit->FinalGoalRadius = 500.f;
            Unit->SetClusterFlowField(FF);
            Unit->SetPathAndMove(Path, false); // خوشه چند نفره → FlowField فعال
        }

        UE_LOG(LogTemp, Warning, TEXT("Cluster %d Started Move"), ClusterIndex + 1);
    }

    // ===== اصلاح: ساخت آرایش Formation برای تمام یونیت‌ها =====
    if (Units.Num() > 0)
    {
        FVector TotalDir(0.f);
        for (const FVector& Dir : ClusterDirections)
            TotalDir += Dir;

        this->FormationForward = TotalDir.IsNearlyZero() ? FVector::ForwardVector : TotalDir.GetSafeNormal();

        AssignOptimalFormation(Units, Goal, this->FormationForward);
    }
}

void UUnitFormationManager::MoveClusterToAssignedSlots(
	const TArray<AUnitCharacter*>& Cluster,
	const TArray<FVector>& Slots,
	const TArray<int32>& Assignment)
{
	int32 N = Cluster.Num();
	if (N == 0) return;

	for (int32 i = 0; i < N; ++i)
	{
		AUnitCharacter* Unit = Cluster[i];
		if (!Unit) continue;

		int32 SlotIndex = (i < Assignment.Num()) ? Assignment[i] : -1;
		if (SlotIndex < 0 || SlotIndex >= Slots.Num())
		{
			// fallback: نزدیک‌ترین اسلات
			float Best = FLT_MAX; int32 BestJ = -1;
			for (int j = 0; j < Slots.Num(); ++j)
			{
				float d = FVector::Dist(Unit->GetActorLocation(), Slots[j]);
				if (d < Best) { Best = d; BestJ = j; }
			}
			SlotIndex = BestJ;
		}

		FVector Target = Slots[SlotIndex];

		// فقط مقصد آرایش را ست می‌کنیم، مسیر مستقیم یا تغییر وضعیت فورمیشن انجام نمی‌شود
		Unit->FormationTarget = Target;
		Unit->bReachedFormationTarget = false;

		// FlowField یا مسیر مستقیم یونیت همچنان در Tick حرکت می‌کند
		// ❌ Unit->MoveDirectlyToTarget(Target) حذف شد
	}
}








