
//AUnitCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/Selectable.h"
#include "Ai/GridPathfinderComponent.h"
#include "AUnitCharacter.generated.h"

class UFlowFieldComponent;
class UGridPathfinderComponent;

UENUM(BlueprintType)
enum class EUnitState : uint8
{
    Idle            UMETA(DisplayName="Idle"),
    Moving          UMETA(DisplayName="Moving"),
    Attacking       UMETA(DisplayName="Attacking"),
    Dead            UMETA(DisplayName="Dead"),
    Stunned         UMETA(DisplayName="Stunned"),
    MovingToGoal    UMETA(DisplayName="MovingToGoal"),
    MovingToFormation UMETA(DisplayName="MovingToFormation")
};


UCLASS()
class THELASTCHERRYBLOSSOM_API AUnitCharacter : public ACharacter, public ISelectable
{
    GENERATED_BODY()

public:
    AUnitCharacter(); // سازنده
    
    virtual void Tick(float DeltaTime) override; // اجرا در هر فریم
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override; // ثبت ورودی‌ها

    // پخش تصادفی انیمیشن ضربه خوردن
    void PlayRandomHitMontage();               

    /** گرفتن سرعت فعلی حرکت یونیت */
    UFUNCTION(BlueprintCallable, Category = "Movement")
    float GetSpeed() const;

    // پیاده‌سازی اینترفیس ISelectable برای تعیین وضعیت انتخاب و خواندن آن
    virtual void SetSelected_Implementation(bool bSelected) override;      // ست کردن انتخاب شدن یا نشدن
    virtual bool IsSelected_Implementation() const override;               // چک کردن اینکه یونیت انتخاب شده یا نه

   
    void SetClusterFlowField(UFlowFieldComponent* NewFlow);
    
    void SetFlowFieldDestination(const FVector& Destination, const TArray<FVector>& Path, int32 CorridorWidth = 9);
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Selection", meta = (AllowPrivateAccess = "true"))
    UCapsuleComponent* SelectionCapsule;
    
    // مسیر‌یابی
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI", meta=(AllowPrivateAccess="true"))
    UGridPathfinderComponent* GridPathfinder;

    // ✅ تابع جدید برای دریافت مسیر کامل
    void SetPathAndMove(const TArray<FVector>& Path);

    // تابع برای تغییر سرعت کاراکتر
    UFUNCTION(BlueprintCallable, Category="Movement")
    void SetMaxSpeed(float NewMaxSpeed);

    // سرعت حرکت یونیت
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
    float MaxSpeed = 450.f;
    
    /** تغییر سرعت چرخش یونیت */
    UFUNCTION(BlueprintCallable, Category="Movement")
    void SetRotationSpeed(float NewRotationSpeed);

    // متغیر سرعت چرخش
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
    float RotationSpeed = 900.f;
    
    UFUNCTION(BlueprintCallable, Category="State")
    void SetUnitState(EUnitState NewState);

    UFUNCTION(BlueprintCallable, Category="State")
    EUnitState GetUnitState() const { return CurrentState; }

  

    UPROPERTY()
    UGridPathfinderComponent* PathfinderComp;

    UPROPERTY()
    FVector FormationOffset;

    // اضافه کردن اشاره‌گر به Flow Field خوشه
    UPROPERTY()
    UFlowFieldComponent* ClusterFlowField = nullptr; // Flow Field خوشه

    UPROPERTY()
    bool bHasLoggedFlowField = false; // برای لاگ یک بار هنگام ست شدن Flow Field

    // آیا یونیت هنوز باید از FlowField حرکت کند
    UPROPERTY(BlueprintReadWrite)
    bool bUseFlowField = true;

    // مختصات کره مقصد
    UPROPERTY(BlueprintReadWrite)
    FVector FinalGoalLocation;

    // شعاع کره مقصد
    UPROPERTY(BlueprintReadWrite)
    float FinalGoalRadius = 500.f;

    // نقطه آرایش نهایی برای یونیت
    UPROPERTY(BlueprintReadWrite)
    FVector FormationTarget;

    UPROPERTY(BlueprintReadWrite, Category="Formation")
    float FormationSphereRadius = 500.f;  // مقدار پیش‌فرض قابل تغییر


protected:

    virtual void BeginPlay() override; // اجرا هنگام شروع بازی

    // ✅ اضافه می‌کنیم
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    UGridPathfinderComponent* PathfinderComponent;

    void OnSelectedChanged(bool bNowSelected); // رویدادی که هنگام تغییر وضعیت انتخاب اجرا می‌شود (مثلاً برای نمایش هایلایت)
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
    UFlowFieldComponent* FlowFieldComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    EUnitState CurrentState = EUnitState::Idle;

    bool bReachedFormationTarget = false;
    

private:
    /** لیست انیمیشن‌های ضربه خوردن که یکی از آن‌ها به‌صورت تصادفی پخش می‌شود */
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    TArray<UAnimMontage*> HitMontages;

    /** آیا این یونیت در حال حاضر انتخاب شده است؟ */
    bool bIsSelected = false;
    
    FVector PendingDestination;  // مقصدی که بعد از چرخش حرکت کنیم
    FRotator TargetRotation;     // زاویه هدف
    bool bIsRotating = false;    // آیا در حال چرخش است
  

    TArray<FVector> CurrentPath;
    int32 CurrentPathIndex = 0;
    //float MoveSpeed = 50.f;

    float CurrentSpeed = 0.f;         // سرعت فعلی
   
    float Acceleration = 800.f;       // افزایش سرعت در هر ثانیه
    float Deceleration = 1200.f;       // کاهش سرعت در هر ثانیه
    bool bIsMoving = false;           // آیا در حال حرکت هست یا نه

    // اشاره به Flow Field
    UFlowFieldComponent* FlowFieldComp;

    
};
