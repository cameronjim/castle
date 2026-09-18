// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mission/MissionFlowController.h"

#include "Castle.h"

EMissionFlowStep UMissionFlowController::Begin(bool bInHasFlashback, bool bInHasNextLevel)
{
	bHasFlashback = bInHasFlashback;
	bHasNextLevel = bInHasNextLevel;
	Step = EMissionFlowStep::EndCard;

	UE_LOG(LogCastle, Log, TEXT("Mission flow: end card (flashback: %s, next level: %s)."),
		bHasFlashback ? TEXT("yes") : TEXT("no"), bHasNextLevel ? TEXT("yes") : TEXT("no"));

	return Step;
}

void UMissionFlowController::Reset()
{
	Step = EMissionFlowStep::Idle;
	bHasFlashback = false;
	bHasNextLevel = false;
}

EMissionFlowStep UMissionFlowController::NextAfter(EMissionFlowStep Current) const
{
	switch (Current)
	{
	case EMissionFlowStep::EndCard:
		if (bHasFlashback)
		{
			return EMissionFlowStep::Flashback;
		}
		// Falls through to the same choice the flashback makes when it ends.
		return bHasNextLevel ? EMissionFlowStep::OpenNextLevel : EMissionFlowStep::FinalCard;

	case EMissionFlowStep::Flashback:
		return bHasNextLevel ? EMissionFlowStep::OpenNextLevel : EMissionFlowStep::FinalCard;

	case EMissionFlowStep::FinalCard:
		// The last mission of the campaign hands the player back to the menu.
		return EMissionFlowStep::OpenMenuLevel;

	case EMissionFlowStep::OpenNextLevel:
	case EMissionFlowStep::OpenMenuLevel:
		return EMissionFlowStep::Done;

	default:
		// Idle and Done are both terminal: Advance on them is a deliberate no-op.
		return Current;
	}
}

EMissionFlowStep UMissionFlowController::Advance()
{
	Step = NextAfter(Step);
	return Step;
}
