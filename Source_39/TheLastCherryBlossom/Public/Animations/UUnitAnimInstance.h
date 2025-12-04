#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "UUnitAnimInstance.generated.h"

/**
 * کلاس انیمیشن برای کنترل حالت‌های انیمیشنی کاراکتر یونیت
 * این کلاس وضعیت‌های موردنیاز برای پخش انیمیشن‌ها (مثل سرعت، ضربه خوردن و ...) را مدیریت می‌کند.
 */
UCLASS()
class THELASTCHERRYBLOSSOM_API UUnitAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    /** تابع اصلی به‌روزرسانی انیمیشن که هر فریم فراخوانی می‌شود */
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    /** سرعت حرکت کاراکتر – برای تعیین اینکه انیمیشن Idle، Walk یا Run پخش شود */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    float Anim_Speed;

    /** آیا کاراکتر اخیراً ضربه خورده؟ اگر بله، انیمیشن HitReact پخش شود */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    bool bIsHit;

    /** یک تایمر ساده برای کنترل مدت زمانی که کاراکتر در حالت HitReact باقی می‌ماند */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    float HitReactTimer;
};
