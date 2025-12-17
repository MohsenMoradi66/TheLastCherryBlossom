#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Selectable.generated.h"

/**
 * رابط (Interface) برای قابلیت انتخاب شدن اشیاء در بازی.
 * هر کلاس (مثل یونیت‌ها یا ساختمان‌ها) که بخواهد قابلیت انتخاب داشته باشد،
 * باید این اینترفیس را پیاده‌سازی کند.
 */
UINTERFACE(MinimalAPI)
class USelectable : public UInterface
{
    GENERATED_BODY()
};

/**
 * اینترفیس اصلی برای انتخاب‌شونده‌ها.
 * کلاس‌هایی مثل AUnitCharacter باید این توابع را پیاده‌سازی کنند
 * تا بتوانند به سیستم انتخاب متصل شوند.
 */
class THELASTCHERRYBLOSSOM_API ISelectable
{
    GENERATED_BODY()

public:
    /** تنظیم وضعیت انتخاب شدن (انتخاب یا لغو انتخاب) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Selection")
    void SetSelected(bool bSelected);

    /** گرفتن وضعیت انتخاب فعلی (آیا انتخاب شده یا نه؟) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Selection")
    bool IsSelected() const;
};
