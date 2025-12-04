
//ARTSPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Math/UnrealMathUtility.h"
#include "UI/UDragSelectionWidget.h"
#include "Characters/AUnitCharacter.h"
#include "ARTSPlayerController.generated.h"

class AUnitCharacter;

UCLASS()
class THELASTCHERRYBLOSSOM_API ARTSPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ARTSPlayerController();

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UDragSelectionWidget> DragWidgetClass;

    UPROPERTY()
    class UUnitFormationManager* FormationComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RTS")
    UGridPathfinderComponent* PathfinderComp;

    UFUNCTION()
    void MoveSelectedUnitsToLocation(const FVector& TargetLocation);

    UPROPERTY()
    TArray<AUnitCharacter*> SelectedUnits;

    



protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    
    void OnLeftClickPressed();
    void OnLeftClickReleased();
    void OnRightClick();
    void OnMouseMoveX(float AxisValue);
    void OnMouseMoveY(float AxisValue);

    void OnLeftClick();
    void ClearSelection();
    void AddOrToggleSelectedActor(AActor* Actor);
    void SelectActorsInDragBox();
    void SelectActorDirectly(AActor* Actor);

    void HandleShiftPressed();
    void HandleShiftReleased();

private:
    bool bIsDragging = false;
    bool bIsShiftPressed = false;

    UPROPERTY()
    UDragSelectionWidget* DragWidget;

    FVector2D DragStartPos;
    FVector2D DragEndPos;
};
