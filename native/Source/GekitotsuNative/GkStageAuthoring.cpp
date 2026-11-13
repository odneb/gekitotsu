#include "GkStageAuthoring.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "InstancedFoliageActor.h"
#include "InstancedFoliage.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#endif

int32 UGkStageAuthoring::PrepareForest(UWorld* World,FVector Center,float Yaw)
{
 int32 Changed=0;
#if WITH_EDITOR
 if(!World || World->IsGameWorld()) return -1;
 const FTransform Stage(FRotator(0,Yaw,0),Center);
 for(TActorIterator<ALandscape> It(World);It;++It)
 {
  ALandscape* Land=*It; ULandscapeInfo* Info=Land->GetLandscapeInfo();if(!Info)continue;
  int32 MinX,MinY,MaxX,MaxY;if(!Info->GetLandscapeExtent(MinX,MinY,MaxX,MaxY))continue;
  FTransform LT=Land->LandscapeActorToWorld(); FBox Area(ForceInit);
  for(int32 X:{-1,1})for(int32 Y:{-1,1})Area+=LT.InverseTransformPosition(Stage.TransformPosition(FVector(X*1000,Y*1800,0)));
  const int32 X1=FMath::Max(MinX,FMath::FloorToInt(Area.Min.X)),Y1=FMath::Max(MinY,FMath::FloorToInt(Area.Min.Y));
  const int32 X2=FMath::Min(MaxX,FMath::CeilToInt(Area.Max.X)),Y2=FMath::Min(MaxY,FMath::CeilToInt(Area.Max.Y));
  if(X1>=X2||Y1>=Y2)continue;
  TArray<uint16> Data;Data.SetNumZeroed((X2-X1+1)*(Y2-Y1+1));
  FLandscapeEditDataInterface Edit(Info); Edit.GetHeightDataFast(X1,Y1,X2,Y2,Data.GetData(),0);
  Land->Modify();
  for(int32 Y=Y1;Y<=Y2;++Y)for(int32 X=X1;X<=X2;++X)
  {
   uint16& H=Data[(Y-Y1)*(X2-X1+1)+X-X1];
   FVector W=LT.TransformPosition(FVector(X,Y,LandscapeDataAccess::GetLocalHeight(H)));
   FVector P=Stage.InverseTransformPosition(W);
   float Edge=FMath::Max((FMath::Abs(P.X)-420.f)/580.f,(FMath::Abs(P.Y)-1100.f)/700.f);
   float Weight=1.f-FMath::SmoothStep(0.f,1.f,Edge);if(Weight<=0)continue;
   W.Z=FMath::Lerp(W.Z,Center.Z,double(Weight));
   H=LandscapeDataAccess::GetTexHeight(LT.InverseTransformPosition(W).Z);++Changed;
  }
  Edit.SetHeightData(X1,Y1,X2,Y2,Data.GetData(),0,true);Edit.Flush();for(ULandscapeComponent* C:Land->LandscapeComponents){C->UpdateCollisionData(true);C->MarkPackageDirty();}Land->RecreateCollisionComponents();Land->MarkPackageDirty();
 }
 TArray<AActor*> Remove;
 for(TActorIterator<AActor> It(World);It;++It)
 {
  AActor* A=*It;
  if(A->GetClass()->GetName().Contains(TEXT("GroundFogTool"))){Remove.Add(A);continue;}
  TInlineComponentArray<UStaticMeshComponent*> Meshes;A->GetComponents(Meshes);
  for(UStaticMeshComponent* C:Meshes)
  {
   if(!C->GetStaticMesh())continue;
   FString Name=C->GetStaticMesh()->GetName().ToLower();
   if(!C->GetStaticMesh()->GetPathName().Contains(TEXT("/Foliage/")))continue;
   auto Obstructs=[&](const FTransform& T){
    FBox B=C->GetStaticMesh()->GetBoundingBox().TransformBy(T).TransformBy(Stage.Inverse());
    return B.Intersect(FBox(FVector(-2600,-1400,-150),FVector(-80,1400,750)));
   };
   if(UInstancedStaticMeshComponent* I=Cast<UInstancedStaticMeshComponent>(C))
   {
    TArray<int32> Indices;for(int32 k=0;k<I->GetInstanceCount();++k){FTransform T;I->GetInstanceTransform(k,T,true);if(Obstructs(T))Indices.Add(k);}
    if(Indices.Num()){
     A->Modify();I->Modify(); bool FoliageRemoved=false;
     if(AInstancedFoliageActor* F=Cast<AInstancedFoliageActor>(A))for(const auto& Pair:F->GetFoliageInfos()){
      FFoliageInfo* Info=F->FindInfo(Pair.Key);if(Info&&Info->GetComponent()==I){Info->RemoveInstances(Indices,true);FoliageRemoved=true;break;}
     }
     if(!FoliageRemoved)I->RemoveInstances(Indices);
     I->MarkPackageDirty();A->MarkPackageDirty();Changed+=Indices.Num();
     UE_LOG(LogTemp,Display,TEXT("GkFilm clear %d instances %s"),Indices.Num(),*Name);
    }
   }
   else if(Obstructs(C->GetComponentTransform())){Remove.AddUnique(A);UE_LOG(LogTemp,Display,TEXT("GkFilm clear actor %s %s"),*A->GetName(),*Name);}
  }
 }
 for(AActor* A:Remove){A->Modify();World->DestroyActor(A);++Changed;}
 UE_LOG(LogTemp,Display,TEXT("GkFilm stage preparation: %d terrain samples / scene objects changed"),Changed);
#endif
 return Changed;
}

#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "UObject/UObjectIterator.h"
#endif
int32 UGkStageAuthoring::RefreshNinja()
{
 int32 Changed=0;
#if WITH_EDITOR
 UNiagaraSystem* System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/FluidNinjaLive/NinjaLiveCore.NinjaLiveCore"));if(!System)return -1;
 for(TObjectIterator<UNiagaraGraph> It;It;++It)
 {
  UNiagaraGraph* Graph=*It;if(!Graph->GetOutermost()->GetName().StartsWith(TEXT("/Game/FluidNinjaLive/")))continue;
  bool Dirty=false;
  TArray<UEdGraphNode*> Nodes;for(UEdGraphNode* N:Graph->Nodes)Nodes.Add(N);
  for(UEdGraphNode* N:Nodes)if(UNiagaraNodeFunctionCall* NN=Cast<UNiagaraNodeFunctionCall>(N))if(!NN->FunctionScript && NN->Signature.Name.ToString().Contains(TEXT("ClosestPoint")))if(NN->RefreshFromExternalChanges()){++Changed;Dirty=true;}
  if(Dirty){Graph->MarkPackageDirty();Graph->NotifyGraphChanged();UE_LOG(LogTemp,Display,TEXT("GkNinja refreshed %s"),*Graph->GetOutermost()->GetName());}
 }
 System->RequestCompile(true);System->WaitForCompilationComplete(false,false);System->MarkPackageDirty();
#endif
 return Changed;
}
