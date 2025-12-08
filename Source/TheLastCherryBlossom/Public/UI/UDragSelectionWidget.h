#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UDragSelectionWidget.generated.h"

UCLASS()
class THELASTCHERRYBLOSSOM_API UDragSelectionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UBorder* SelectionBorder;

    void UpdateSelectionBox(const FVector2D& Start, const FVector2D& End);

protected:
    virtual void NativeConstruct() override;

private:
    FVector2D StartPos;
};
