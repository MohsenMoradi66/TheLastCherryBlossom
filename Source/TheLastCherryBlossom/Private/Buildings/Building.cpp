// Buildings/Building.cpp
#include "Buildings/Building.h"
#include "Characters/AUnitCharacter.h"

ABuilding::ABuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// ========== ساخت Root ==========
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;
	
	// ========== کپسول برخورد (برای فیزیک و برخورد) ==========
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CollisionCapsule->SetupAttachment(RootComponent);
	CollisionCapsule->SetCapsuleRadius(CapsuleRadius);
	CollisionCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionCapsule->SetCollisionObjectType(CollisionChannel);
	
	// ========== مش ساختمان ==========
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	BuildingMesh->SetupAttachment(RootComponent);
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// ========== دایره انتخاب (دقیقاً مثل AUnitCharacter) ==========
	SelectionCircleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionCircle"));
	SelectionCircleMesh->SetupAttachment(RootComponent);
	SelectionCircleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionCircleMesh->SetHiddenInGame(true);
	
	// بارگذاری مش صفحه (Plane) به عنوان دایره
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh.Succeeded())
	{
		SelectionCircleMesh->SetStaticMesh(PlaneMesh.Object);
	}
}

void ABuilding::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetupCollision();
	SetupSelectionCircle();
	ApplyVariant();
}

void ABuilding::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	SetupSelectionCircle();
	ApplyVariant();
	
	// نمایش/مخفی کردن دایره بر اساس وضعیت انتخاب
	if (SelectionCircleMesh)
	{
		SelectionCircleMesh->SetHiddenInGame(!bIsSelected);
	}
}

void ABuilding::SetupCollision()
{
	if (CollisionCapsule)
	{
		CollisionCapsule->SetCapsuleRadius(CapsuleRadius);
		CollisionCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);
		CollisionCapsule->SetCollisionObjectType(CollisionChannel);
	}
}

void ABuilding::SetupSelectionCircle()
{
	if (!SelectionCircleMesh) return;
	
	// موقعیت زیر پای ساختمان (مثل کاراکتر)
	float CapsuleBottom = -CapsuleHalfHeight;
	float ZOffset = CapsuleBottom - 5.f;
	SelectionCircleMesh->SetRelativeLocation(FVector(0.f, 0.f, ZOffset));
	
	// تنظیم سایز دایره
	float ScaleFactor = SelectionCircleRadius / 50.f;
	SelectionCircleMesh->SetRelativeScale3D(FVector(ScaleFactor, ScaleFactor, 1.0f));
}

void ABuilding::ApplyVariant()
{
	if (BuildingVariants.IsValidIndex(SelectedVariantIndex))
	{
		FBuildingVariant& Variant = BuildingVariants[SelectedVariantIndex];
		
		if (Variant.Mesh)
		{
			BuildingMesh->SetStaticMesh(Variant.Mesh);
			BuildingMesh->SetWorldScale3D(Variant.MeshScale);
		}
		
		if (Variant.Material)
		{
			BuildingMesh->SetMaterial(0, Variant.Material);
		}
		
		BuildingName = Variant.VariantName;
	}
}

void ABuilding::SetSelected_Implementation(bool bSelected)
{
	bIsSelected = bSelected;
	OnSelectedChanged(bSelected);
}

void ABuilding::OnSelectedChanged(bool bNowSelected)
{
	// نمایش/مخفی کردن دایره زیر ساختمان (دقیقاً مثل کاراکتر)
	if (SelectionCircleMesh)
	{
		SelectionCircleMesh->SetHiddenInGame(!bNowSelected);
	}
	
	// افکت هایلایت روی مش ساختمان (اختیاری)
	if (BuildingMesh)
	{
		BuildingMesh->SetRenderCustomDepth(bNowSelected);
	}
}

void ABuilding::SetMeshAndMaterial(UStaticMesh* NewMesh, UMaterialInterface* NewMaterial)
{
	if (BuildingMesh)
	{
		if (NewMesh)
			BuildingMesh->SetStaticMesh(NewMesh);
		if (NewMaterial)
			BuildingMesh->SetMaterial(0, NewMaterial);
	}
}

void ABuilding::SetBuildingVariant(int32 VariantIndex)
{
	if (BuildingVariants.IsValidIndex(VariantIndex))
	{
		SelectedVariantIndex = VariantIndex;
		ApplyVariant();
	}
}

void ABuilding::ProduceUnit(TSubclassOf<AUnitCharacter> UnitClass)
{
	if (!UnitClass || !AvailableUnits.Contains(UnitClass)) return;
	
	PendingUnit = UnitClass;
	GetWorld()->GetTimerManager().SetTimer(ProductionTimer, this, &ABuilding::SpawnUnit, ProductionTime, false);
}

void ABuilding::SpawnUnit()
{
	if (PendingUnit)
	{
		FVector SpawnLocation = GetActorLocation() + SpawnOffset;
		GetWorld()->SpawnActor<AUnitCharacter>(PendingUnit, SpawnLocation, GetActorRotation());
		PendingUnit = nullptr;
	}
}