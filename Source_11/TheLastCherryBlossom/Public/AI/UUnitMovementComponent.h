// UUnitMovementComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UUnitMovementComponent.generated.h"

class AUnitCharacter;

/**
 * سیستم حرکت اختصاصی برای بازی‌های استراتژیک (RTS)
 * - حرکت آنی با سرعت ثابت
 * - چرخش فوری یا نرم
 * - توقف کامل بدون اصطکاک
 * - کاملاً مستقل از CharacterMovementComponent
 * - فقط حرکت را اجرا می‌کند (بدون تشخیص برخورد)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THELASTCHERRYBLOSSOM_API UUnitMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnitMovementComponent();

    // مقداردهی اولیه
    void Initialize(AUnitCharacter* InOwner);

    // تنظیم سرعت
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetMaxSpeed(float NewSpeed);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    float GetMaxSpeed() const { return MaxSpeed; }

    // توقف کامل و آنی
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void StopImmediately();

    // حرکت به سمت یک هدف
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void MoveTo(const FVector& TargetLocation, bool bInstantRotation = true);

    // حرکت در یک جهت مشخص
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void MoveDirection(const FVector& Direction);

    // تنظیم چرخش فوری
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetRotationInstant(const FRotator& NewRotation);

    // چرخش نرم (برای واحدهای خاص مثل تانک)
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetRotationSmooth(const FRotator& TargetRotation, float InterpSpeed = 10.f);

    // وضعیت حرکت
    UFUNCTION(BlueprintCallable, Category = "Movement")
    bool IsMoving() const { return bIsMoving; }

    UFUNCTION(BlueprintCallable, Category = "Movement")
    FVector GetCurrentVelocity() const { return CurrentVelocity; }

    UFUNCTION(BlueprintCallable, Category = "Movement")
    float GetCurrentSpeed() const { return CurrentVelocity.Size(); }

    // قفل کردن حرکت (برای Stun/Dead)
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetMovementLocked(bool bLocked) { bMovementLocked = bLocked; }

    // Tick
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Movement|Rotation")
    void SetRotationInterpSpeed(float NewSpeed);

    UFUNCTION(BlueprintCallable, Category = "Movement|Rotation")
    float GetRotationInterpSpeed() const { return RotationInterpSpeed; }

    // 🌟 جدید: تنظیم موقعیت مستقیم (برای Steering)
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetLocationDirect(const FVector& NewLocation);

protected:
    virtual void BeginPlay() override;

private:
    // حرکت اصلی
    void ApplyMovement(float DeltaTime);

    // چرخش
    void ApplyRotation(float DeltaTime);

    // Owner
    UPROPERTY()
    AUnitCharacter* Owner = nullptr;

    // پارامترهای حرکت
    UPROPERTY(EditAnywhere, Category = "Movement")
    float MaxSpeed = 450.f;

    // وضعیت
    FVector TargetLocation = FVector::ZeroVector;
    FVector CurrentVelocity = FVector::ZeroVector;
    FVector DesiredDirection = FVector::ZeroVector;

    // چرخش هدف (برای چرخش نرم)
    FRotator TargetRotation = FRotator::ZeroRotator;
    bool bHasTargetRotation = false;
    bool bInstantRotation = true;

    // پرچم‌ها
    bool bIsMoving = false;
    bool bMovementLocked = false;

    // ثابت‌ها
    static constexpr float STOP_DISTANCE = 5.f; // فاصله توقف

    float RotationInterpSpeed = 12.f; // سرعت چرخش (هرچه بیشتر، سریع‌تر)

    // تنظیمات چرخش
    UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
    bool bUseSmoothRotation = true; // آیا چرخش نرم باشد؟

    UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
    float RotationSpeed = 720.f; // درجه در ثانیه (مثل Warcraft 3)
};