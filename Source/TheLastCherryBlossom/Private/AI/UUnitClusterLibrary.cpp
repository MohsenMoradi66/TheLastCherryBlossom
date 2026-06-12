#include "AI/UUnitClusterLibrary.h"
#include "Characters/AUnitCharacter.h"

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
	TArray<AUnitCharacter*> Remaining = Units;

	while (Remaining.Num() > 0)
	{
		// compute center
		FVector Center(0.f);
		for (AUnitCharacter* Unit : Remaining)
		{
			if (Unit) Center += Unit->GetActorLocation();
		}
		Center /= Remaining.Num();

		// find seed (closest to center)
		int32 SeedIndex = 0;
		float ClosestDist = FLT_MAX;
		for (int32 i = 0; i < Remaining.Num(); ++i)
		{
			AUnitCharacter* Unit = Remaining[i];
			if (!Unit) continue;
			float Dist = FVector::Dist(Unit->GetActorLocation(), Center);
			if (Dist < ClosestDist)
			{
				ClosestDist = Dist;
				SeedIndex = i;
			}
		}

		AUnitCharacter* Seed = Remaining[SeedIndex];
		TArray<AUnitCharacter*> Cluster;
		Cluster.Add(Seed);
		Remaining.RemoveAt(SeedIndex);

		// add all within radius of seed
		for (int32 i = Remaining.Num() - 1; i >= 0; --i)
		{
			AUnitCharacter* Unit = Remaining[i];
			if (!Unit) continue;

			float Dist = FVector::Dist(Unit->GetActorLocation(), Seed->GetActorLocation());
			if (Dist <= Radius)
			{
				Cluster.Add(Unit);
				Remaining.RemoveAt(i);
			}
		}

		Clusters.Add(Cluster);
	}

	return Clusters;
}
