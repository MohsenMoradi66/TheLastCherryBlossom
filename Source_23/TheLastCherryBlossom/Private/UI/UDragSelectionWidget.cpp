#include "UI/UDragSelectionWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UDragSelectionWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Hidden);

    if (!SelectionBorder)
    {
        UE_LOG(LogTemp, Error, TEXT("SelectionBorder is NOT BOUND!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SelectionBorder is successfully bound"));
    }

}

void UDragSelectionWidget::UpdateSelectionBox(const FVector2D& Start, const FVector2D& End)
{
    if (!SelectionBorder) return;

    StartPos = Start;

    const FVector2D TopLeft(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y));
    const FVector2D BottomRight(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y));
    const FVector2D Size = BottomRight - TopLeft;

    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SelectionBorder->Slot);
    if (CanvasSlot)
    {
        CanvasSlot->SetPosition(TopLeft);
        SelectionBorder->SetBrushColor(FLinearColor(0.f, 0.f, 1.f, 0.3f)); // آبی کم‌رنگ

        CanvasSlot->SetSize(Size);

    }
}
