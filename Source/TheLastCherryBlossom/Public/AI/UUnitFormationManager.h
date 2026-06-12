#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "Containers/Queue.h"
#include "UUnitFormationManager.generated.h"

class AUnitCharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UUnitFormationManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UUnitFormationManager();

	void MoveUnitsWithClustering(const TArray<AUnitCharacter*>& Units, const FVector& Goal);
	void CancelAllMoves();

	// New: start computing perpendicular data for a path, spread over frames
	void StartFormationComputationForPath(const TArray<FVector>& Path);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clustering")
	float ClusterDistance = 500.f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebug = true;
#endif

private:
	FVector CalculateClusterCenter(const TArray<AUnitCharacter*>& Cluster) const;
	void AssignSimpleClusterPath(const TArray<AUnitCharacter*>& Cluster, const FVector& Goal);
	void ClearUnitMoveState(AUnitCharacter* Unit);
	void ResetFormation();
	void ProcessNextCluster();  // جدید: پردازش یک خوشه در هر فریم

	// Per‑waypoint processing (spread over frames)
	TArray<FVector> PendingPath;
	int32 CurrentWaypointIndex = 0;
	FTimerHandle FormationProcessTimer;

	void ProcessNextWaypoint();
	void OnAllWaypointsProcessed();

	// Actually compute perpendicular line data for a single point
	void ComputePerpendicularDataForPoint(const FVector& Point, int32 Index);

	// جدید: برای پردازش تدریجی خوشه‌ها
	struct FClusterRequest
	{
		TArray<AUnitCharacter*> Units;
		FVector Goal;
	};
	TQueue<FClusterRequest> PendingClusters;
	bool bIsProcessingCluster = false;
	FTimerHandle ClusterProcessTimer;

	void ProcessClusterAsync();  // تابع جدید برای پردازش غیرهمزمان
	void AssignPathToCluster(const TArray<AUnitCharacter*>& Cluster, const TArray<FVector>& Path, const FVector& Goal);
};