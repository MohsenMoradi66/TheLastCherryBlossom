// Environment/NaturalObject.cpp
#include "Environment/NaturalObject.h"
#include "Components/CapsuleComponent.h"

ANaturalObject::ANaturalObject()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// ============================================
	// ✅ اول RootComponent را بساز
	// ============================================
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	
	// ============================================
	// ✅ کپسول برخورد
	// ============================================
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CollisionCapsule->SetupAttachment(RootComponent);
	CollisionCapsule->SetCapsuleRadius(DefaultCapsuleRadius);
	CollisionCapsule->SetCapsuleHalfHeight(DefaultCapsuleHalfHeight);
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionCapsule->SetCollisionObjectType(CollisionChannel);
	
	// ============================================
	// ✅ مش بصری - به Root متصل کن
	// ============================================
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// مقادیر پیش‌فرض
	RemainingResources = 100;
	HarvestPerHit = 10;
}

void ANaturalObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetupCollision();
	ApplyVariant();
}

void ANaturalObject::BeginPlay()
{
	Super::BeginPlay();
	ApplyVariant();
}

void ANaturalObject::SetupCollision()
{
	if (CollisionCapsule)
	{
		CollisionCapsule->SetCapsuleRadius(DefaultCapsuleRadius);
		CollisionCapsule->SetCapsuleHalfHeight(DefaultCapsuleHalfHeight);
		CollisionCapsule->SetCollisionObjectType(CollisionChannel);
	}
}

void ANaturalObject::ApplyVariant()
{
	if (NaturalVariants.IsValidIndex(SelectedVariantIndex))
	{
		FNaturalVariant& Variant = NaturalVariants[SelectedVariantIndex];
		
		// انتخاب مش تصادفی
		if (Variant.Meshes.Num() > 0)
		{
			int32 RandomIndex = FMath::RandRange(0, Variant.Meshes.Num() - 1);
			MeshComp->SetStaticMesh(Variant.Meshes[RandomIndex]);
		}
		
		if (Variant.Material)
		{
			MeshComp->SetMaterial(0, Variant.Material);
		}
		
		MeshComp->SetWorldScale3D(Variant.MeshScale);
		
		// ✅ تنظیم کپسول بر اساس واریانت
		if (CollisionCapsule)
		{
			CollisionCapsule->SetCapsuleRadius(Variant.CapsuleRadius);
			CollisionCapsule->SetCapsuleHalfHeight(Variant.CapsuleHalfHeight);
		}
		
		// چرخش تصادفی
		if (Variant.bRandomRotation)
		{
			FRotator RandomRotation(0.f, FMath::RandRange(0.f, 360.f), 0.f);
			SetActorRotation(RandomRotation);
		}
		
		// تنظیم منابع
		bIsHarvestable = Variant.bIsHarvestable;
		RemainingResources = Variant.ResourceAmount;
		HarvestPerHit = Variant.HarvestPerHit;
	}
}

void ANaturalObject::RandomizeAppearance()
{
	if (NaturalVariants.IsValidIndex(SelectedVariantIndex))
	{
		FNaturalVariant& Variant = NaturalVariants[SelectedVariantIndex];
		
		if (Variant.Meshes.Num() > 0)
		{
			int32 RandomIndex = FMath::RandRange(0, Variant.Meshes.Num() - 1);
			MeshComp->SetStaticMesh(Variant.Meshes[RandomIndex]);
		}
		
		if (Variant.bRandomRotation)
		{
			FRotator RandomRotation(0.f, FMath::RandRange(0.f, 360.f), 0.f);
			SetActorRotation(RandomRotation);
		}
	}
}

void ANaturalObject::SetNaturalVariant(int32 VariantIndex)
{
	if (NaturalVariants.IsValidIndex(VariantIndex))
	{
		SelectedVariantIndex = VariantIndex;
		ApplyVariant();
	}
}

int32 ANaturalObject::Harvest(int32 Amount)
{
	if (!IsHarvestable()) return 0;
	
	int32 Harvested = FMath::Min(Amount, RemainingResources);
	RemainingResources -= Harvested;
	
	if (RemainingResources <= 0)
	{
		Destroy();
	}
	
	return Harvested;
}