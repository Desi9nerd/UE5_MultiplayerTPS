#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "GameplayTag/XZGameplayTags.h"
#include "XZInventoryComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTXZ_API UXZInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UXZInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FGameplayTag& GetEquipSlot1() { return EquipSlot1; }
	const FGameplayTag& GetEquipSlot2() { return EquipSlot2; }
	const FGameplayTag& GetEquipSlot3() { return EquipSlot3; }
	const FGameplayTag& GetEquipSlot4() { return EquipSlot4; }

protected:
	virtual void BeginPlay() override;

private:
	FGameplayTag EquipSlot1 = FXZTags::GetXZTags().Weapon_Projectile_Pistol;
	FGameplayTag EquipSlot2 = FXZTags::GetXZTags().Weapon_Projectile_Rifle;
	FGameplayTag EquipSlot3 = FXZTags::GetXZTags().Weapon_Projectile_SMG;
	FGameplayTag EquipSlot4 = FXZTags::GetXZTags().Weapon_Hitscan_Shotgun;
		
};
