#include "XZWeaponComponent.h"

#include "XZStateComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Handler/XZCombatHandler.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "ProjectXZ/Character/XZCharacter.h"
#include "ProjectXZ/Character/XZPlayerController.h"
#include "ProjectXZ/GameplayTag/XZGameplayTags.h"
#include "ProjectXZ/HUD/XZHUD.h"
#include "ProjectXZ/Weapon/XZDA_Weapon.h"
#include "ProjectXZ/Weapon/XZEquipment.h"
#include "ProjectXZ/Weapon/XZWeaponData.h"
#include "ProjectXZ/Weapon/Combat/XZCombat.h"
#include "Weapon/Aim/XZAim.h"
#include "Weapon/Attachment/XZAttachment.h"

UXZWeaponComponent::UXZWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UXZWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UXZWeaponComponent, EquippedWeaponTag, COND_None, REPNOTIFY_Always);
}

void UXZWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetXZCharacter() && GetXZCharacter()->IsLocallyControlled())
	{
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult); // Crosshair에서 LineTrace를 쏘고 HitResult 값을 업데이트한다.
		HitTarget = HitResult.ImpactPoint;
	}
}

TObjectPtr<AXZCharacter> UXZWeaponComponent::GetXZCharacter()
{
	if (IsValid(OwnerCharacter)) return OwnerCharacter;

	OwnerCharacter = Cast<AXZCharacter>(GetOwner());
	return OwnerCharacter;
}

TObjectPtr<AXZPlayerController> UXZWeaponComponent::GetXZPlayerController()
{
	if (IsValid(XZPlayerController)) return XZPlayerController;

	XZPlayerController = Cast<AXZPlayerController>(GetXZCharacter()->GetController());
	return XZPlayerController;
}

bool UXZWeaponComponent::IsValidWeapon(const FGameplayTag& InTag)
{
	if (UXZWeaponData** FoundData = Datas.Find(InTag))
	{
		if (*FoundData)
		{
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("FoundData(=XZWeaponData) is nullptr"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("Weapon is not found. Check UXZWeaponComponent::EquipWeapon"));
	return false;
}

void UXZWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// 무기 목록
	for (const TTuple<FGameplayTag, UXZDA_Weapon*>& Weapon : WeaponList)
	{
		UXZWeaponData* Temp = NewObject<UXZWeaponData>(this);
		Datas.Add(Weapon.Key, Temp);
	}

	// 시작 무기 생성
	for (const FGameplayTag& Tag : Init_WeaponTags)
	{
		WeaponList[Tag]->CreateInstance(GetXZCharacter(), &Datas[Tag]);
	}
}

void UXZWeaponComponent::AddNewWeapon(const FGameplayTag& InTag)
{
	Server_AddNewWeapon(InTag);
}

void UXZWeaponComponent::Server_AddNewWeapon_Implementation(const FGameplayTag& InTag)
{
	Multicast_AddNewWeapon(InTag);
}

void UXZWeaponComponent::Multicast_AddNewWeapon_Implementation(const FGameplayTag& InTag)
{
	WeaponList[InTag]->CreateInstance(GetXZCharacter(), &Datas[InTag]);
}

void UXZWeaponComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	FVector2D CrosshairLocation(ViewportSize.X / 2.0f, ViewportSize.Y / 2.0f); // 화면 중앙
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection
	); // DeprojectScreenToWorld 성공하면 true, 실패하면 false

	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;

		if (GetXZCharacter())
		{
			float DistanceToCharacter = (GetXZCharacter()->GetActorLocation() - Start).Size();
			Start += CrosshairWorldDirection * (DistanceToCharacter + 100.0f);
			//DrawDebugSphere(GetWorld(), Start, 4.0f, 12, FColor::Blue, false); // 디버깅용
		}

		FVector End = Start + CrosshairWorldDirection * 80000.0f; // TRACE_LENGTH 80000.0f

		GetWorld()->LineTraceSingleByChannel(TraceHitResult, 	Start, End, ECollisionChannel::ECC_Visibility);

		if (TraceHitResult.bBlockingHit == false) // 하늘 같이 충돌할게 없는곳에 쏘는 경우
		{
			TraceHitResult.ImpactPoint = End; // 충돌하는게 없다면 End 값을 ImpactPoint값으로 설정.
		}

	}
}

void UXZWeaponComponent::ShowCrosshair(const FGameplayTag& InTag, bool bShow)
{
	Datas[InTag]->GetAim()->ShowCrosshair(bShow);
}

void UXZWeaponComponent::EquipWeapon(const FGameplayTag& InTag)
{
	if (false == IsValidWeapon(InTag)) return;
	// 현재 장착 중인 무기를 Equip하라고 명령하면 Unequip
	if (EquippedWeaponTag == InTag)
	{
		ShowCrosshair(InTag, false);
		UnequipWeapon(InTag);
		return;
	}
	// 다른 무기를 장착 중이라면
	if (EquippedWeaponTag != InTag && EquippedWeaponTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Weapon")))))
	{
		ShowCrosshair(EquippedWeaponTag, false);
		UnequipWeapon(EquippedWeaponTag); // 현재 무기 Unequip
	}

	ShowCrosshair(InTag, true);
	Server_EquipWeapon(InTag);
}

void UXZWeaponComponent::OnRep_EquippedChanged()
{

}

void UXZWeaponComponent::Server_EquipWeapon_Implementation(const FGameplayTag& InTag)
{
	EquippedWeaponTag = InTag;

	Multicast_EquipWeapon(InTag);
}

void UXZWeaponComponent::Multicast_EquipWeapon_Implementation(const FGameplayTag& InTag)
{
	Datas[InTag]->GetEquipment()->Equip(); // 무기 장착
}

void UXZWeaponComponent::UnequipWeapon(const FGameplayTag& InTag)
{
	Server_UnequipWeapon(InTag);
}

void UXZWeaponComponent::Server_UnequipWeapon_Implementation(const FGameplayTag& InTag)
{
	Multicast_UnequipWeapon(InTag);
	EquippedWeaponTag = FXZTags::GetXZTags().Fist;
}

void UXZWeaponComponent::Multicast_UnequipWeapon_Implementation(const FGameplayTag& InTag)
{
	Datas[InTag]->GetEquipment()->Unequip();
}

void UXZWeaponComponent::Fire()
{
	if (false == EquippedWeaponTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Weapon"))))) return; // 무기 장착 중이 아니라면 return
	if (false == IsValidWeapon(EquippedWeaponTag)) return;
	if (Datas[EquippedWeaponTag]->GetCombat()->GetBulletData().Ammo <= 0) // 총알이 없다면
	{
		Reload(EquippedWeaponTag); // 재장전
		return;
	}

	const USkeletalMeshSocket* MuzzleFlashSocket = Datas[EquippedWeaponTag]->GetAttachment()->GetWeaponMesh()->GetSocketByName(
		Datas[EquippedWeaponTag]->GetCombat()->GetActionData()[0].MuzzleSocketName);
	FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(Datas[EquippedWeaponTag]->GetAttachment()->GetWeaponMesh());
		
	Server_Fire(HitTarget, SocketTransform);
}

void UXZWeaponComponent::Server_Fire_Implementation(const FVector_NetQuantize& HitLocation, const FTransform& SocketTransform)
{
	Multicast_Fire(HitLocation, SocketTransform);
}

void UXZWeaponComponent::Multicast_Fire_Implementation(const FVector_NetQuantize& HitLocation, const FTransform& SocketTransform)
{
	// 발사
	CombatHandler->Attack();

	Datas[EquippedWeaponTag]->GetCombat()->FireAction(HitLocation, SocketTransform);

	// 총알 소모
	if (GetXZCharacter() && false == GetXZCharacter()->HasAuthority() && GetXZCharacter()->IsLocallyControlled())
	{
		Datas[EquippedWeaponTag]->GetCombat()->ConsumeAmmo();
	}
	if (Datas[EquippedWeaponTag]->GetCombat()->GetBulletData().Ammo <= 0) // 총알이 없다면
	{
		Reload(EquippedWeaponTag); // 재장전
	}
}

void UXZWeaponComponent::Reload(const FGameplayTag& InTag)
{
	if (false == IsValidWeapon(InTag)) return;

	if (Datas[InTag]->GetCombat()->GetBulletData().TotalAmmo <= 0) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Run out of Bullets"));
		return; // 총알 없는 경우 return
	}

	const USkeletalMeshSocket* MagazineSocket = Datas[EquippedWeaponTag]->GetAttachment()->GetWeaponMesh()->GetSocketByName(
		Datas[EquippedWeaponTag]->GetCombat()->GetBulletData().CasingSocketName);
	FTransform SocketTransform = MagazineSocket->GetSocketTransform(Datas[EquippedWeaponTag]->GetAttachment()->GetWeaponMesh());


	Server_Reload(InTag, SocketTransform);

	
	// 총알 채우기
	if (Datas[InTag]->GetCombat()->GetBulletData().TotalAmmo >= Datas[InTag]->GetCombat()->GetBulletData().MagCapacity)
	{
		Datas[InTag]->GetCombat()->GetBulletData().TotalAmmo -= (Datas[InTag]->GetCombat()->GetBulletData().MagCapacity - Datas[InTag]->GetCombat()->GetBulletData().Ammo);
		Datas[InTag]->GetCombat()->GetBulletData().Ammo = Datas[InTag]->GetCombat()->GetBulletData().MagCapacity;
	}
	else
	{
		Datas[InTag]->GetCombat()->GetBulletData().Ammo = Datas[InTag]->GetCombat()->GetBulletData().TotalAmmo;
		Datas[InTag]->GetCombat()->GetBulletData().TotalAmmo = 0;
	}
}

void UXZWeaponComponent::Server_Reload_Implementation(const FGameplayTag& InTag, const FTransform& SocketTransform)
{
	Multicast_Reload(InTag, SocketTransform);
}

void UXZWeaponComponent::Multicast_Reload_Implementation(const FGameplayTag& InTag, const FTransform& SocketTransform)
{
	Datas[InTag]->GetCombat()->ReloadAction(SocketTransform);
}

void UXZWeaponComponent::Aiming(bool bAiming)
{
	if (false == IsValid(GetXZCharacter())) return;
	if (false == EquippedWeaponTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Weapon"))))) return;
	
	if (bAiming)
	{
		Datas[EquippedWeaponTag]->GetAim()->StartAiming();
	}
	else
	{
		Datas[EquippedWeaponTag]->GetAim()->EndAiming();
	}

	Server_Aiming(bAiming);
}

void UXZWeaponComponent::Server_Aiming_Implementation(bool bAiming)
{
	if (bAiming)
	{
		GetXZCharacter()->GetStateComponent()->SetState(FXZTags::GetXZTags().StateTag_Alive_Equip_Aim);
		GetXZCharacter()->GetCharacterMovement()->MaxWalkSpeed = AimWalkSpeed;
	}
	else
	{
		GetXZCharacter()->GetStateComponent()->SetState(FXZTags::GetXZTags().StateTag_Alive_Equip_Idle);
		GetXZCharacter()->GetCharacterMovement()->MaxWalkSpeed = MaxWalkSpeed;
	}
}


void UXZWeaponComponent::Init()
{
	CombatHandler = CreateCombatHandler();
}

UXZCombatHandler* UXZWeaponComponent::CreateCombatHandler()
{
	return NewObject<UXZCombatHandler>(this, UXZCombatHandler::StaticClass());
}
