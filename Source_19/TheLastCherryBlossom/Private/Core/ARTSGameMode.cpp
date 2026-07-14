#include "Core/ARTSGameMode.h"
#include "Core/ARTSPlayerController.h"

ARTSGameMode::ARTSGameMode()
{
    PlayerControllerClass = ARTSPlayerController::StaticClass();
}
