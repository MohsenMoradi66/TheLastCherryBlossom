#include "AI/UUnitFormationManager.h"
#include "Characters/AUnitCharacter.h"
#include "AI/UFlowFieldComponent.h"
#include "AI/GridPathfinderComponent.h"
#include "NavigationSystem.h"
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

void UUnitFormationManager::MoveUnitsWithClustering(const TArray<AUnitCharacter*>& Units, const FVector& Goal)
{
	if (Units.Num() == 0) return;

	TArray<TArray<AUnitCharacter*>> Clusters =
		UUnitClusterLibrary::ClusterUnits(Units, 500.f);

	ClusterFlowFields.Empty();

	for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
	{
		TArray<AUnitCharacter*>& Cluster = Clusters[ClusterIndex];
		if (Cluster.Num() == 0) continue;

		AUnitCharacter* Seed = Cluster[0];
		if (!Seed) continue;

		UGridPathfinderComponent* Pathfinder = Seed->FindComponentByClass<UGridPathfinderComponent>();
		if (!Pathfinder) continue;

		TArray<FVector> Path = Pathfinder->FindPathShared(Seed->GetActorLocation(), Goal);
		if (Path.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cluster %d: No Path!"), ClusterIndex);
			continue;
		}

		// مرکز خوشه
		FVector ClusterCenter(0.f);
		for (AUnitCharacter* Unit : Cluster)
			if (Unit) ClusterCenter += Unit->GetActorLocation();
		ClusterCenter /= Cluster.Num();

		FVector PathDirection = (Path[1] - Path[0]).GetSafeNormal();
		float CorridorWidthCm = CalculateCorridorWidthForCluster(Cluster, ClusterCenter, PathDirection, Seed);

		// Back offset calculation
		float MaxBackwardDist = 0.f;
		FVector BackDir = -PathDirection;
		for (AUnitCharacter* Unit : Cluster)
		{
			if (!Unit) continue;
			FVector ToUnit = Unit->GetActorLocation() - Seed->GetActorLocation();
			float BackAmount = FVector::DotProduct(ToUnit, BackDir);

			if (BackAmount > MaxBackwardDist)
				MaxBackwardDist = BackAmount;
		}

		float CapsuleRadius = 0.f;
		if (UCapsuleComponent* Cap = Seed->FindComponentByClass<UCapsuleComponent>())
			CapsuleRadius = Cap->GetScaledCapsuleRadius();

		float Safety = CapsuleRadius * 6.f;
		float TotalOffset = MaxBackwardDist + Safety;

		FVector ExtendedStart = Path[0] - PathDirection * TotalOffset;
		Path.Insert(ExtendedStart, 0);

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
			Unit->SetPathAndMove(Path);
			Unit->SetUnitState(EUnitState::MovingToGoal);
			Unit->PrimaryActorTick.bCanEverTick = true;
		}

		// جایگزین GenerateFinalFormation => AssignOptimalFormation (Hungarian + tweaks)
		AssignOptimalFormation(Cluster, Goal);

		UE_LOG(LogTemp, Warning, TEXT("Cluster %d Started Move"), ClusterIndex + 1);
	}
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
		Matrix = { {1,-1,1,-1,1,-1,1},{-1,-1,-1,1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,1,-1,-1,-1},{1,-1,1,-1,1,-1,1},{-1,-1,-1,1,-1,-1,-1},{1,-1,-1,-1,-1,-1,1} };
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

float UUnitFormationManager::GetFormationSphereRadiusByCluster(const TArray<AUnitCharacter*>& Cluster)
{
	int32 UnitCount = Cluster.Num();
	float SphereRadius = 0.f;

	switch (UnitCount)
	{
	case 2:
	case 4:
		SphereRadius = 250.f;
		break;
	case 3:
	case 5:
	case 6:
		SphereRadius = 300.f;
		break;
	default:
		SphereRadius = 570.f;
		break;
	}

	return SphereRadius;
}

// ---------- BuildFormationSlots ------------
void UUnitFormationManager::BuildFormationSlots(
	const TArray<AUnitCharacter*>& Cluster,
	const FVector& Goal,
	TArray<FVector>& OutSlots)
{
	OutSlots.Empty();

	int32 Count = Cluster.Num();
	if (Count == 0) return;

	const float Spacing = 110.f;
	TArray<TArray<float>> Matrix = GetFormationMatrix(Count);

	int32 Rows = Matrix.Num();
	int32 Columns = Matrix[0].Num();

	FVector ClusterCenter(0);
	for (AUnitCharacter* U : Cluster)
		if (U) ClusterCenter += U->GetActorLocation();
	ClusterCenter /= Count;

	FVector Forward = (Goal - ClusterCenter).GetSafeNormal();
	if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;
	FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();

	float HalfW = (Columns - 1) * 0.5f * Spacing;
	float HalfH = (Rows - 1) * 0.5f * Spacing;

	for (int32 r = 0; r < Rows; r++)
	{
		for (int32 c = 0; c < Columns; c++)
		{
			if (Matrix[r][c] < 0) continue; // خانه خالی

			float X = (c * Spacing - HalfW);
			float Y = (r * Spacing - HalfH);

			X *= Matrix[r][c]; // اعمال وزن مخصوص خانه (در صورت نیاز)

			FVector Offset = Right * X + Forward * (-Y);
			OutSlots.Add(Goal + Offset);
		}
	}

	// debug draw slots
	if (bDrawFormationDebug)
	{
		for (int32 i = 0; i < OutSlots.Num(); ++i)
		{
			DrawDebugSphere(GetWorld(), OutSlots[i], 25.f, 8, FColor::Blue, false, DebugDrawTime, 0, 1.5f);
			DrawDebugString(GetWorld(), OutSlots[i] + FVector(0, 0, 30), FString::Printf(TEXT("%d"), i), nullptr, FColor::White, DebugDrawTime);
		}
	}
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

	for (int32 i = 0; i < N; i++)
	{
		OutCost[i].SetNum(M);
		FVector UnitPos = Cluster[i] ? Cluster[i]->GetActorLocation() : FVector::ZeroVector;

		for (int32 j = 0; j < M; j++)
		{
			OutCost[i][j] = FVector::Dist(UnitPos, Slots[j]);
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

// ---------- MoveClusterToAssignedSlots ------------
void UUnitFormationManager::MoveClusterToAssignedSlots(
	const TArray<AUnitCharacter*>& Cluster,
	const TArray<FVector>& Slots,
	const TArray<int32>& Assignment)
{
	int32 N = Cluster.Num();
	if (N == 0) return;

	// Safety: if mismatched sizes
	if (Slots.Num() < N)
	{
		UE_LOG(LogTemp, Warning, TEXT("Slots < Cluster: %d < %d"), Slots.Num(), N);
	}

	// Apply debug lines and movement
	for (int32 i = 0; i < N; ++i)
	{
		AUnitCharacter* Unit = Cluster[i];
		if (!Unit) continue;

		int32 SlotIndex = (i < Assignment.Num()) ? Assignment[i] : -1;
		if (SlotIndex < 0 || SlotIndex >= Slots.Num())
		{
			// fallback: nearest free slot
			float Best = FLT_MAX; int32 BestJ = -1;
			for (int j = 0; j < Slots.Num(); ++j)
			{
				float d = FVector::Dist(Unit->GetActorLocation(), Slots[j]);
				if (d < Best) { Best = d; BestJ = j; }
			}
			SlotIndex = BestJ;
		}

		FVector Target = Slots[SlotIndex];

		// store and move
		Unit->FormationTarget = Target;
		Unit->SetUnitState(EUnitState::MovingToFormation);
		Unit->SetPathAndMove({ Target }); // از تابع موجود برای حرکت مستقیم استفاده کن

		// دیباگ
		if (bDrawFormationDebug)
		{
			DrawDebugLine(GetWorld(), Unit->GetActorLocation(), Target, FColor::Green, false, DebugDrawTime, 0, 1.2f);
			DrawDebugSphere(GetWorld(), Target, 22.f, 6, FColor::Yellow, false, DebugDrawTime, 0, 1.2f);
		}
	}
}

// ---------- AssignOptimalFormation (کلی) ------------
void UUnitFormationManager::AssignOptimalFormation(const TArray<AUnitCharacter*>& Cluster, const FVector& Goal)
{
	if (Cluster.Num() == 0) return;

	// 1) ساخت اسلات‌ها
	TArray<FVector> Slots;
	BuildFormationSlots(Cluster, Goal, Slots);

	// 2) جلوگیری از اسلات‌های خیلی نزدیک به هم (اختیاری)
	const float MinSeparation = 70.f;
	ApplySimpleSeparation(Slots, MinSeparation);

	// 3) ساخت ماتریس هزینه
	TArray<TArray<float>> Cost;
	BuildCostMatrix(Cluster, Slots, Cost);

	// 4) حل assignment — اگر خوشه بزرگ است، از Greedy fallback استفاده کن
	TArray<int32> Assignment;
	const int32 GreedyThreshold = 60; // اگر بیش از 60 یونیت باشد از greedy استفاده کن
	if (Cluster.Num() > GreedyThreshold)
	{
		UE_LOG(LogTemp, Warning, TEXT("Large cluster (%d) - using greedy assignment fallback."), Cluster.Num());
		GreedyAssign(Cost, Assignment);
	}
	else
	{
		Assignment = HungarianSolve(Cost);
	}

	// 5) اعمال اختصاص به یونیت‌ها و حرکت دادن
	MoveClusterToAssignedSlots(Cluster, Slots, Assignment);
}
