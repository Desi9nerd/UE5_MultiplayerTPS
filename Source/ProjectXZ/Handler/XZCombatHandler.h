#pragma once
#include "CoreMinimal.h"
#include "Interface/ICombat.h"
#include "UObject/NoExportTypes.h"
#include "XZCombatHandler.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTXZ_API UXZCombatHandler : public UObject
{
	GENERATED_BODY()

public:
	void Attack();

private:
};
