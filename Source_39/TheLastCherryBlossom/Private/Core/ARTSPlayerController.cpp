
// ARTSPlayerController.cpp

#include "Core/ARTSPlayerController.h"
#include "AI/UUnitFormationManager.h"
#include "Characters/AUnitCharacter.h"
#include "Interfaces/Selectable.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h" 
#include "EngineUtils.h"


ARTSPlayerController::ARTSPlayerController()
{
    FormationComponent = CreateDefaultSubobject<UUnitFormationManager>(TEXT("FormationComponent"));
    PathfinderComp = CreateDefaultSubobject<UGridPathfinderComponent>(TEXT("PathfinderComp"));


    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;

}

void ARTSPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ClearSelection();
    
    if (DragWidgetClass)
    {

        DragWidget = CreateWidget<UDragSelectionWidget>(this, DragWidgetClass);
        if (DragWidget)
        {

            DragWidget->AddToViewport();
            DragWidget->SetVisibility(ESlateVisibility::Hidden);

        }
    }

}

void ARTSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    InputComponent->BindAction("LeftClick", IE_Pressed, this, &ARTSPlayerController::OnLeftClickPressed);
    InputComponent->BindAction("LeftClick", IE_Released, this, &ARTSPlayerController::OnLeftClickReleased);
    InputComponent->BindAction("RightClick", IE_Pressed, this, &ARTSPlayerController::OnRightClick);
    InputComponent->BindAction("Shift", IE_Pressed, this, &ARTSPlayerController::HandleShiftPressed);
    InputComponent->BindAction("Shift", IE_Released, this, &ARTSPlayerController::HandleShiftReleased);
    InputComponent->BindAxis("MouseX", this, &ARTSPlayerController::OnMouseMoveX);
    InputComponent->BindAxis("MouseY", this, &ARTSPlayerController::OnMouseMoveY);
    
}

void ARTSPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (bIsDragging)
    {
        GetMousePosition(DragEndPos.X, DragEndPos.Y);

        if (DragWidget)
        {

            DragWidget->UpdateSelectionBox(DragStartPos, DragEndPos);

        }
    }
}

void ARTSPlayerController::HandleShiftPressed()
{
    bIsShiftPressed = true;
}

void ARTSPlayerController::HandleShiftReleased()
{
    bIsShiftPressed = false;
}

void ARTSPlayerController::ClearSelection()
{
    for (TWeakObjectPtr<AActor> ActorPtr : SelectedUnits)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            if (Actor->GetClass()->ImplementsInterface(USelectable::StaticClass()))
            {
                ISelectable::Execute_SetSelected(Actor, false);
            }
        }
    }
    SelectedUnits.Empty();
}



void ARTSPlayerController::OnLeftClickPressed()
{
    bIsDragging = true;
    GetMousePosition(DragStartPos.X, DragStartPos.Y);

    if (DragWidget)
    {
        DragWidget->SetVisibility(ESlateVisibility::Visible);
        DragWidget->UpdateSelectionBox(DragStartPos, DragStartPos);
    }

}

void ARTSPlayerController::OnLeftClickReleased()
{
    bIsDragging = false;
    GetMousePosition(DragEndPos.X, DragEndPos.Y);
    SelectActorsInDragBox();

    if (DragWidget)
    {
        DragWidget->SetVisibility(ESlateVisibility::Hidden);
    }
}

void ARTSPlayerController::OnMouseMoveX(float AxisValue)
{
    if (bIsDragging)
    {
        GetMousePosition(DragEndPos.X, DragEndPos.Y);


    }
}

void ARTSPlayerController::OnMouseMoveY(float AxisValue)
{

    if (bIsDragging)
    {
        GetMousePosition(DragEndPos.X, DragEndPos.Y);


    }
}

void ARTSPlayerController::SelectActorsInDragBox()
{
    FVector2D MinPos(FMath::Min(DragStartPos.X, DragEndPos.X), FMath::Min(DragStartPos.Y, DragEndPos.Y));
    FVector2D MaxPos(FMath::Max(DragStartPos.X, DragEndPos.X), FMath::Max(DragStartPos.Y, DragEndPos.Y));

    if ((MaxPos - MinPos).Size() < 5.f)
    {
        OnLeftClick();
        return;
    }

    TArray<AActor*> FoundActors;

    for (TActorIterator<AUnitCharacter> It(GetWorld()); It; ++It)
    {
        AUnitCharacter* Unit = *It;
        if (!IsValid(Unit)) continue;

        FVector2D ScreenPos;
        if (ProjectWorldLocationToScreen(Unit->GetActorLocation(), ScreenPos))
        {
            if (ScreenPos.X >= MinPos.X && ScreenPos.X <= MaxPos.X &&
                ScreenPos.Y >= MinPos.Y && ScreenPos.Y <= MaxPos.Y)
            {
                FoundActors.Add(Unit);
            }
        }
    }

    if (!bIsShiftPressed)
    {
        ClearSelection();

        for (AActor* Actor : FoundActors)
        {
            SelectActorDirectly(Actor);
        }
    }
    else
    {
        for (AActor* Actor : FoundActors)
        {
            AddOrToggleSelectedActor(Actor);
        }
    }
}

void ARTSPlayerController::OnLeftClick()
{
    FHitResult Hit;

    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        AActor* HitActor = Hit.GetActor();

        if (!IsValid(HitActor) || !HitActor->GetClass()->ImplementsInterface(USelectable::StaticClass()))
        {
            if (!bIsShiftPressed)
            {
                ClearSelection();
            }
            return;
        }

        AddOrToggleSelectedActor(HitActor);
    }
    else
    {
        if (!bIsShiftPressed)
        {
            ClearSelection();
        }
    }
}

void ARTSPlayerController::OnRightClick()
{
    FHitResult Hit;
    if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit) || !Hit.bBlockingHit)
    {
        return;
    }

    const FVector TargetLocation = Hit.ImpactPoint;
    UE_LOG(LogTemp, Warning, TEXT("Right-click: Destination set to %s"), *TargetLocation.ToString());

    if (FormationComponent && SelectedUnits.Num() > 0)
    {
        // ✅ خوشه‌بندی یونیت‌ها و ساخت مسیر جداگانه برای هر خوشه
        FormationComponent->MoveUnitsWithClustering(SelectedUnits, TargetLocation);
    }
}

void ARTSPlayerController::MoveSelectedUnitsToLocation(const FVector& TargetLocation)
{
    UE_LOG(LogTemp, Warning, TEXT("MoveSelectedUnitsToLocation called to %s"), *TargetLocation.ToString());
    for (AUnitCharacter* Unit : SelectedUnits)
    {
        if (Unit)
        {
            TArray<FVector> Path = Unit->GridPathfinder->FindPathShared(Unit->GetActorLocation(), TargetLocation);
            if (Path.Num() > 0)
            {
                Unit->SetPathAndMove(Path);
            }
        }
    }
}




void ARTSPlayerController::AddOrToggleSelectedActor(AActor* Actor)
{
if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(USelectable::StaticClass()))
return;


AUnitCharacter* Unit = Cast<AUnitCharacter>(Actor);  
if (!Unit) return;  

if (!bIsShiftPressed)  
{  
    ClearSelection();  
}  

const int32 MaxSelectedUnits = 16;  

if (!SelectedUnits.Contains(Unit))  
{  
    if (SelectedUnits.Num() >= MaxSelectedUnits)  
    {  
        UE_LOG(LogTemp, Warning, TEXT("Maximum selected units reached!"));  
        return;  
    }  

    SelectedUnits.Add(Unit);  
    ISelectable::Execute_SetSelected(Unit, true);  
}  
else if (bIsShiftPressed)  
{  
    SelectedUnits.Remove(Unit);  
    ISelectable::Execute_SetSelected(Unit, false);  
}  


}

void ARTSPlayerController::SelectActorDirectly(AActor* Actor)
{
if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(USelectable::StaticClass()))
return;


AUnitCharacter* Unit = Cast<AUnitCharacter>(Actor);  
if (!Unit) return;  

const int32 MaxSelectedUnits = 16;  

if (!SelectedUnits.Contains(Unit))  
{  
    if (SelectedUnits.Num() >= MaxSelectedUnits)  
    {  
        UE_LOG(LogTemp, Warning, TEXT("Maximum selected units reached!"));  
        return;  
    }  

    SelectedUnits.Add(Unit);  
    ISelectable::Execute_SetSelected(Unit, true);  
}  


}







