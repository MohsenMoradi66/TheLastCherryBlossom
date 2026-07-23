// Environment/NaturalObject.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"
#include "NaturalObject.generated.h"

UENUM(BlueprintType)
enum class ENaturalType : uint8
{
	Tree	UMETA(DisplayName = "Tree"),
	Rock	UMETA(DisplayName = "Rock"),
	Bush	UMETA(DisplayName = "Bush"),
	GoldMine UMETA(DisplayName = "Gold Mine")
};

USTRUCT(BlueprintType)
struct FNaturalVariant
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString VariantName = "Default";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UStaticMesh*> Meshes;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* Material = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MeshScale = FVector(1.f, 1.f, 1.f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRandomRotation = true;
	
	// تنظیمات کپسول برای این واریانت
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CapsuleRadius = 60.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CapsuleHalfHeight = 80.f;
	
	// منابع
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsHarvestable = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResourceAmount = 100;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 HarvestPerHit = 10;
};

UCLASS(Blueprintable)
class THELASTCHERRYBLOSSOM_API ANaturalObject : public AActor
{
	GENERATED_BODY()

public:
	ANaturalObject();
    
	UFUNCTION(BlueprintCallable, Category = "Natural")
	int32 Harvest(int32 Amount);
	
	UFUNCTION(BlueprintCallable, Category = "Natural")
	bool IsHarvestable() const { return bIsHarvestable && RemainingResources > 0; }
	
	UFUNCTION(BlueprintCallable, Category = "Natural")
	int32 GetRemainingResources() const { return RemainingResources; }
	
	UFUNCTION(BlueprintCallable, Category = "Natural")
	void SetNaturalVariant(int32 VariantIndex);
	
	UFUNCTION(BlueprintCallable, Category = "Natural")
	void RandomizeAppearance();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	
	// ========== کامپوننت‌ها ==========
	
	// ✅ کپسول برخورد (جایگزین Box)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* CollisionCapsule;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;
	
	// ========== واریانت‌ها ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Natural|Variants")
	TArray<FNaturalVariant> NaturalVariants;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Natural|Variants")
	int32 SelectedVariantIndex = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Natural|Type")
	ENaturalType NaturalType = ENaturalType::Tree;
	
	// ========== تنظیمات کالیشن کپسول ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	float DefaultCapsuleRadius = 60.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	float DefaultCapsuleHalfHeight = 80.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_GameTraceChannel3; // TreesAndRocks

private:
	void ApplyVariant();
	void SetupCollision();
	
	bool bIsHarvestable = true;
	int32 RemainingResources = 0;
	int32 HarvestPerHit = 10;
};