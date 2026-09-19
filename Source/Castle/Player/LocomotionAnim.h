// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class USkeletalMeshComponent;

/**
 * Idle/walk switching without an AnimBP. The mannequin pack's ThirdPerson_AnimBP does not
 * compile in a headless editor, so every character here drives the sequences directly; this is
 * the one copy of that, shared by the guards and by Frank's own true-first-person body.
 */
namespace CastleLocomotion
{
	/**
	 * Plays Wanted through Mesh and writes it to Current, unless it is already playing.
	 * Returns true when the animation changed. A component with no mesh asset still updates
	 * Current: a greybox character tracks what it would be playing.
	 */
	CASTLE_API bool PlayIfChanged(
		USkeletalMeshComponent* Mesh, UAnimSequence* Wanted, TObjectPtr<UAnimSequence>& Current);
}
