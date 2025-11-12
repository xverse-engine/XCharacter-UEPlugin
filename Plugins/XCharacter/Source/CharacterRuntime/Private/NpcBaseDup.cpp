// Copyright Xverse. All Rights Reserved.

#include "NpcBaseDup.h"

// Sets default values
ANpcBaseDup::ANpcBaseDup()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SkeletalMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMeshComp->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ANpcBaseDup::BeginPlay()
{
	Super::BeginPlay();
	
} 

// Called every frame
void ANpcBaseDup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

