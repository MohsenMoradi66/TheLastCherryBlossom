#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ARTSGameMode.generated.h"

/**
 * گیم‌مود اصلی برای بازی استراتژیک TheLastCherryBlossom
 * این کلاس می‌تواند قوانین کلی بازی، شرایط پیروزی یا شکست و مدیریت کلی را کنترل کند.
 * در حال حاضر فقط سازنده دارد و از GameModeBase به ارث برده شده.
 */
UCLASS()
class THELASTCHERRYBLOSSOM_API ARTSGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARTSGameMode(); // سازنده پیش‌فرض
};
