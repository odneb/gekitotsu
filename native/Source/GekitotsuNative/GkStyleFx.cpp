#include "GkMatch.h"
#include "Components/PoseableMeshComponent.h"

namespace {
const FLinearColor Pink(.65f,.012f,.14f),Cyan(.025f,.48f,.72f),Paper(1.5f,1.35f,.95f),Black(.006f,.008f,.018f);
void Ribbon(FGkFx& Fx,const FVector& A,const FVector& B,const FVector& C,float Width,float Life,const FLinearColor& Color)
{
 for(int k=0;k<6;++k){const float U=k/6.f,V=(k+1)/6.f;
  const FVector P=(1-U)*(1-U)*A+2*U*(1-U)*B+U*U*C,Q=(1-V)*(1-V)*A+2*V*(1-V)*B+V*V*C;
  const float W=Width*FMath::Sin((.12f+(U+V)*.4f)*PI);
  Fx.Stroke(P-(Q-P)*.12f,Q+(Q-P)*.12f,W,Life,Color);
 }
}
}
void AGkMatch::PaintBurst(const FVector& Point,int32 Who,float Size,bool Armor)
{
 const int Face=S.p[Who].face;const FVector P=Point+FVector(-30,0,0);
 for(int k=0;k<(Armor?7:9);++k){const float A=-1.55f+k*(Armor?.48f:.38f),Length=(k%3==0?105.f:72.f)*Size;
  const FVector Dir(0,Face*FMath::Cos(A),FMath::Sin(A));
  const FVector Start=P+Dir*(Armor?14:24),End=P+Dir*Length;
  Fx.Stroke(Start+FVector(2,0,0),End+FVector(2,0,0),Size*11,.18f,Black);
  Fx.Stroke(Start,End,Size*(k%3==0?7.f:4.f),.16f,Armor?Paper:(k&1?Cyan:Pink));
 }
 // One sharp contact cross rather than a translucent flash disc.
 Fx.Stroke(P+FVector(-3,-24*Size,-50*Size),P+FVector(-3,24*Size,50*Size),8*Size,.09f,Paper);
 Fx.Stroke(P+FVector(-4,-55*Size,15*Size),P+FVector(-4,55*Size,-15*Size),5*Size,.09f,Paper);
}
void AGkMatch::StyleEvent(const gk::Event& E)
{
 if(Screen!=3 || bMenu || bEntrance)return;
 const int Who=E.who>=0?E.who&1:0;const auto& P=S.p[Who];
 const bool DI=P.st==gk::ATK && P.mv>=0 && gk::move(P.mv).di;
 const FVector Contact=At(E.x,E.y);
 switch(E.type){
 case gk::E_DI:{
  const FVector Ch=At(P.x,P.y+48*gk::F,30);
  Ribbon(Fx,Ch+FVector(0,-P.face*190,65),Ch+FVector(0,-P.face*90,110),Ch+FVector(0,P.face*20,10),24,.20f,Pink);
  Ribbon(Fx,Ch+FVector(0,-P.face*210,-25),Ch+FVector(0,-P.face*95,-30),Ch+FVector(0,P.face*10,25),14,.20f,Cyan);
  break;
 }
 case gk::E_ARMOR:{const auto& Other=S.p[1-Who];const bool Clash=DI && Other.st==gk::ATK && Other.mv>=0 && gk::move(Other.mv).di;if(!Clash)PaintBurst(Contact,Who,.65f,true);break;}
 case gk::E_DICLASH:if(StyleClashFrame!=S.frame){StyleClashFrame=S.frame;PaintBurst(Contact,0,1.3f);}break;
 case gk::E_BLOCK:if(DI)PaintBurst(Contact,Who,.8f);break;
 case gk::E_HIT:
  if(E.f&gk::H_IMPACT)PaintBurst(Contact,Who,1.2f);
  else {
   const float Scale=(E.f&gk::H_FINISH)?1.6f:E.a>=2?1.f:.55f;
   const FVector Origin=Contact+FVector(-45,0,0);
   for(int k=0;k<5;++k){const float A=-1.1f+k*.55f;const FVector D(0,P.face*FMath::Cos(A),FMath::Sin(A));Fx.Stroke(Origin+D*15,Origin+D*(70+20*(k&1))*Scale,Scale*5,.11f,k&1?Paper:FLinearColor(1.4,.4,.025));}
  }break;
 default:break;
 }
}
void AGkMatch::StyleMotion(int32 Who)
{
 if(Screen!=3 || bMenu || bEntrance || S.freeze>0){bStyleBones[Who]=false;return;}
 if(StyleFrame[Who]==AnimClock)return;StyleFrame[Who]=AnimClock;
 const auto& P=S.p[Who];const auto* M=P.st==gk::ATK && P.mv>=0?&gk::move(P.mv):nullptr;
 const FVector Chest=At(P.x,P.y+48*gk::F,25);
 if(M && M->di && P.mf<=M->lastAct && P.mf%4==0){
  const float Wind=FMath::Clamp(P.mf/24.f,0.f,1.f),Reach=130+Wind*65;
  for(int k=0;k<3;++k){
   const float AZ=k==0?60.f:k==1?-20.f:-55.f,BZ=k==0?110.f:k==1?-30.f:-40.f,CZ=k==0?10.f:k==1?25.f:0.f;
   const FVector A=Chest+FVector(0,-P.face*Reach,AZ),B=Chest+FVector(0,-P.face*Reach*.45,BZ),C=Chest+FVector(0,P.face*20,CZ);
   Ribbon(Fx,A+FVector(2,0,0),B+FVector(2,0,0),C+FVector(2,0,0),k==0?28:16,.13f,Black);
   Ribbon(Fx,A,B,C,k==0?21:10,.13f,k==0?Pink:k==1?Cyan:Paper*.55f);
  }
 }
 const bool Flight=M && M->has_home && P.mf>=M->home[0] && !P.hmE;
 const bool Dash=P.st==gk::DASHF || P.st==gk::DASHB || P.st==gk::DRUSH;
 const bool Fast=Dash || P.st==gk::RUN || Flight;
 if(Fast && AnimClock%5==Who){
  const float Dir=P.vx<0?-1.f:P.vx>0?1.f:float(P.face);const float Length=Flight?180.f:Dash?125.f:65.f;
  const FLinearColor Color=P.st==gk::DRUSH?FLinearColor(.08,1,.35):Cyan;
  for(int k=0;k<3;++k){const float Z=(k-1)*43.f;const FVector A=Chest+FVector(-40,-Dir*(Length+25),Z),B=Chest+FVector(-40,-Dir*15,Z*.8f);Fx.Stroke(A,B,k==1?4.f:2.f,.12f,k==1?Color:Paper);}
  if(Flight && AnimClock%10==Who){const FVector Nose=Chest+FVector(-30,P.face*55,0);Ribbon(Fx,Nose+FVector(0,-P.face*20,-65),Nose+FVector(0,P.face*35,0),Nose+FVector(0,-P.face*20,65),5,.13f,Paper);}
 }
 // Actual hand/foot paths: short, hard-edged smears only around active attacks.
 if(!Body[Who])return;
 const TCHAR* Bones[]={TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")};
 const bool Swing=M && !M->di && !M->has_proj && !M->sup && P.hs==0 && P.mf>=FMath::Max(0,M->hits[0].s-5) && P.mf<=M->lastAct+2;
 const FString Key=P.mv>=0?FString(UTF8_TO_TCHAR(gk::moveKey(P.mv))):FString();const bool Kick=Key.Contains(TEXT("K")) || Key.Contains(TEXT("TAT")) || Key==TEXT("ASPK");
 for(int k=0;k<4;++k){const FVector Now=Anchor.InverseTransformPosition(Body[Who]->GetBoneLocationByName(Bones[k],EBoneSpaces::WorldSpace))+FVector(-8,0,0);
  if(Swing && bStyleBones[Who] && (Kick?k>=2:k<2)){
   const float Distance=(Now-StyleBones[Who][k]).Size();if(Distance>4 && Distance<150){
    Fx.Stroke(StyleBones[Who][k],Now,Key.Contains(TEXT("H"))?8.f:4.f,.09f,M->sp?Cyan:Paper);
   }
  }StyleBones[Who][k]=Now;
 }bStyleBones[Who]=true;
}
