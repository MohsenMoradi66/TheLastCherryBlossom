// Environment/NaturalObject.cpp
#include "Environment/NaturalObject.h"
#include "Components/CapsuleComponent.h"



// ۱. اصلاح سازنده برای حذف متغیرهای دستی و هماهنگی با هدر پروژه
ANaturalObject::ANaturalObject()
{
	PrimaryActorTick.bCanEverTick = false;
    
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CollisionCapsule->SetupAttachment(RootComponent);
    
	// استفاده از ماکروی هدر اصلی پروژه (ساختمان‌ها و درختان باید در یک کانال یا کانال‌های اسکن‌شده باشند)
	// تغییر از کانال ۳ به کانال ۲ پروژه
	CollisionChannel = ECC_GameTraceChannel2; 

	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionCapsule->SetCollisionObjectType(CollisionChannel);
    
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
	RemainingResources = 100;
	HarvestPerHit = 10;
}

// ۲. اصلاح تابع SetupCollision برای جلوگیری از ریست شدن ابعاد کپسول
void ANaturalObject::SetupCollision()
{
	if (CollisionCapsule)
	{
		// اجازه بده ابعاد توسط واریانت یا ادیتور مشخص شوند و اینجا فقط لایه شیء را محکم‌کاری کن
		CollisionCapsule->SetCollisionObjectType(CollisionChannel);
		CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	}
}

// ۳. اصلاح تابع BeginPlay برای اطمینان از اعمال فیزیک در زمان اجرای بازی
void ANaturalObject::BeginPlay()
{
	Super::BeginPlay();
	SetupCollision(); // 🌟 اضافه شد: محکم کاری فیزیک در فریم اول بازی
	ApplyVariant();
}

void ANaturalObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetupCollision();
	ApplyVariant();
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