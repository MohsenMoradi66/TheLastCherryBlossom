// Buildings/Building.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Selectable.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Building.generated.h"

class AUnitCharacter;

USTRUCT(BlueprintType)
struct FBuildingVariant
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString VariantName = "Default";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* Material = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MeshScale = FVector(1.f, 1.f, 1.f);
};

UCLASS(Blueprintable)
class THELASTCHERRYBLOSSOM_API ABuilding : public AActor, public ISelectable
{
	GENERATED_BODY()

public:
	ABuilding();
	
	virtual void SetSelected_Implementation(bool bSelected) override;
	virtual bool IsSelected_Implementation() const override { return bIsSelected; }
	
	UFUNCTION(BlueprintCallable, Category = "Building")
	void ProduceUnit(TSubclassOf<AUnitCharacter> UnitClass);
	
	UFUNCTION(BlueprintCallable, Category = "Building")
	void SetBuildingVariant(int32 VariantIndex);
	
	UFUNCTION(BlueprintCallable, Category = "Building")
	void SetMeshAndMaterial(UStaticMesh* NewMesh, UMaterialInterface* NewMaterial);
	
	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsBuildingSelected() const { return bIsSelected; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	
	// ========== کامپوننت‌ها (مشابه AUnitCharacter) ==========
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootScene;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* CollisionCapsule;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BuildingMesh;
	
	// ✅ فقط دایره انتخاب (مثل کاراکتر) - بدون کپسول اضافی
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* SelectionCircleMesh;
	
	// ========== خواص ساختمان ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FString BuildingName = "Building";
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	int32 MaxHealth = 500;
	
	// ========== واریانت‌ها ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Variants")
	TArray<FBuildingVariant> BuildingVariants;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Variants")
	int32 SelectedVariantIndex = 0;
	
	// ========== تولید ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Production")
	TArray<TSubclassOf<AUnitCharacter>> AvailableUnits;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Production")
	float ProductionTime = 3.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Spawn")
	FVector SpawnOffset = FVector(150.f, 0.f, 0.f);
	
	// ========== تنظیمات کالیشن ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	float CapsuleRadius = 100.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	float CapsuleHalfHeight = 120.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_GameTraceChannel2;
	
	// ========== تنظیمات دایره انتخاب (مثل کاراکتر) ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	float SelectionCircleRadius = 120.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	FLinearColor SelectionCircleColor = FLinearColor::Blue;

private:
	void SetupCollision();
	void SetupSelectionCircle();
	void ApplyVariant();
	void OnSelectedChanged(bool bNowSelected);
	void SpawnUnit();
	
	FTimerHandle ProductionTimer;
	TSubclassOf<AUnitCharacter> PendingUnit;
	bool bIsSelected = false;
	int32 CurrentHealth = 0;
};