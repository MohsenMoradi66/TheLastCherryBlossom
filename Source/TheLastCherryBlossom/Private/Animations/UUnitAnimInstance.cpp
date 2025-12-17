#include "Animations/UUnitAnimInstance.h"
#include "GameFramework/Character.h"
#include "Characters/AUnitCharacter.h"

void UUnitAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    AUnitCharacter* UnitChar = Cast<AUnitCharacter>(TryGetPawnOwner());
    if (!UnitChar) return;

    Anim_Speed = UnitChar->GetSpeed();  // اینجا مقداردهی با نام صحیح

    // اگر می‌خوای سیستم ضربه خوردن فعال باشه
    if (bIsHit)
    {
        HitReactTimer -= DeltaSeconds;
        if (HitReactTimer <= 0.f)
        {
            bIsHit = false;
            HitReactTimer = 0.f;
        }
    }
}
