#include "GkMatch.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "ReferenceSkeleton.h"

namespace
{
// Original, frame-driven choreography. All targets are centimetres in the same
// +Y-forward frame as Manny; the solver retains the mesh's actual bone lengths.
// The pose description is independent of robot parts and works on a humanoid.
struct FPose
{
 FVector Hip={0,0,94}, LeftHand={31,25,133}, RightHand={-28,13,117};
 FVector LeftFoot={15,20,13}, RightFoot={-15,-24,13};
 FVector PoleL={90,-20,110},PoleR={-90,-20,110};
 float Pitch=0,Yaw=22,Chest=0,Head=-20,OpenL=1,OpenR=1;
};
float Ease(float A,float B,float T){return FMath::SmoothStep(A,B,T);}
FPose Mix(const FPose& A,const FPose& B,float T)
{
 FPose P;P.Hip=FMath::Lerp(A.Hip,B.Hip,T);P.LeftHand=FMath::Lerp(A.LeftHand,B.LeftHand,T);P.RightHand=FMath::Lerp(A.RightHand,B.RightHand,T);
 P.LeftFoot=FMath::Lerp(A.LeftFoot,B.LeftFoot,T);P.RightFoot=FMath::Lerp(A.RightFoot,B.RightFoot,T);P.PoleL=FMath::Lerp(A.PoleL,B.PoleL,T);P.PoleR=FMath::Lerp(A.PoleR,B.PoleR,T);
 P.Pitch=FMath::Lerp(A.Pitch,B.Pitch,T);P.Yaw=FMath::Lerp(A.Yaw,B.Yaw,T);P.Chest=FMath::Lerp(A.Chest,B.Chest,T);P.Head=FMath::Lerp(A.Head,B.Head,T);
 P.OpenL=FMath::Lerp(A.OpenL,B.OpenL,T);P.OpenR=FMath::Lerp(A.OpenR,B.OpenR,T);return P;
}
struct FBodyPose
{
 const FReferenceSkeleton& R; TArray<FTransform> C;
 FBodyPose(const FReferenceSkeleton& In,const TArray<FTransform>& Local):R(In){C.SetNum(Local.Num());for(int B=0;B<C.Num();++B){int P=R.GetParentIndex(B);C[B]=P>=0?Local[B]*C[P]:Local[B];}}
 int Bone(const TCHAR* N)const{return R.FindBoneIndex(N);}
 bool Child(int B,int Root)const{while(B>=0){if(B==Root)return true;B=R.GetParentIndex(B);}return false;}
 FVector At(const TCHAR* N)const{const int B=Bone(N);return B>=0?C[B].GetLocation():FVector::ZeroVector;}
 void Rotate(int B,const FQuat& Q){if(B<0)return;const FVector Pivot=C[B].GetLocation();for(int J=B;J<C.Num();++J)if(Child(J,B)){
  C[J].SetLocation(Pivot+Q.RotateVector(C[J].GetLocation()-Pivot));FQuat Rot=Q*C[J].GetRotation();Rot.Normalize();C[J].SetRotation(Rot);
 }}
 void Rotate(const TCHAR* N,const FVector& Axis,float Degrees){Rotate(Bone(N),FQuat(Axis,FMath::DegreesToRadians(Degrees)));}
 void Move(int B,const FVector& To){if(B<0)return;const FVector D=To-C[B].GetLocation();for(int J=B;J<C.Num();++J)if(Child(J,B))C[J].AddToTranslation(D);}
 void Aim(int B,int End,const FVector& To){if(B<0||End<0)return;Rotate(B,FQuat::FindBetweenNormals((C[End].GetLocation()-C[B].GetLocation()).GetSafeNormal(),(To-C[B].GetLocation()).GetSafeNormal()));}
 void Limb(const TCHAR* Upper,const TCHAR* Lower,const TCHAR* End,const FVector& Target,const FVector& Pole)
 {
  const int A=Bone(Upper),B=Bone(Lower),E=Bone(End);if(A<0||B<0||E<0)return;
  const FVector Root=C[A].GetLocation();const float L1=FVector::Dist(Root,C[B].GetLocation()),L2=FVector::Dist(C[B].GetLocation(),C[E].GetLocation());
  FVector D=Target-Root;const float Dist=FMath::Clamp(float(D.Size()),FMath::Abs(L1-L2)+.01f,L1+L2-.05f);D.Normalize();
  FVector Bend=(Pole-Root)-D*FVector::DotProduct(Pole-Root,D);if(!Bend.Normalize())Bend=FVector::CrossProduct(D,FVector(1,0,0)).GetSafeNormal();
  const float Along=(L1*L1-L2*L2+Dist*Dist)/(2*Dist);const FVector Mid=Root+D*Along+Bend*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
  Aim(A,B,Mid);Aim(B,E,Root+D*Dist);
 }
 void Palm(const TCHAR* Hand,const TCHAR* Middle,const TCHAR* Index,const TCHAR* Pinky,const FVector& Fingers,const FVector& Normal)
 {
  const int H=Bone(Hand);if(H<0)return;const FVector F=(At(Middle)-At(Hand)).GetSafeNormal();
  FVector N=FVector::CrossProduct(F,At(Index)-At(Pinky)).GetSafeNormal();if(FString(Hand)==TEXT("hand_r"))N=-N;
  const FQuat Q=FRotationMatrix::MakeFromXZ(Fingers,Normal).ToQuat()*FRotationMatrix::MakeFromXZ(F,N).ToQuat().Inverse();Rotate(H,Q);
 }
 void Hand(bool Left,float Open)
 {
  const FString Side=Left?TEXT("_l"):TEXT("_r");const int H=Bone(*(TEXT("hand")+Side));if(H<0)return;
  // Open, separated fingers in presentation/parry; a relaxed fist for a strike.
  for(const TCHAR* Finger:{TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")})for(int J=1;J<=3;++J){
   const int B=Bone(*FString::Printf(TEXT("%s_%02d%s"),Finger,J,*Side));if(B>=0){const FVector Ax=C[B].GetRotation().RotateVector(FVector(0,1,0));Rotate(B,FQuat(Ax,FMath::DegreesToRadians((Left?-1.f:1.f)*(1-Open)*65.f)));}
  }
 }
 void Local(TArray<FTransform>& Out)const{Out.SetNum(C.Num());for(int B=0;B<C.Num();++B){int P=R.GetParentIndex(B);Out[B]=P>=0?C[B].GetRelativeTransform(C[P]):C[B];Out[B].NormalizeRotation();}}
};
void MakePose(const FReferenceSkeleton& R,const FPose& P,TArray<FTransform>& Out)
{
 FBodyPose B(R,R.GetRefBonePose());const float Scale=B.At(TEXT("pelvis")).Z/94.f;
 const FVector X(1,0,0),Z(0,0,1);
 B.Move(B.Bone(TEXT("pelvis")),P.Hip*Scale);
 B.Rotate(TEXT("pelvis"),Z,P.Yaw);B.Rotate(TEXT("pelvis"),X,P.Pitch);
 B.Rotate(TEXT("spine_03"),Z,P.Chest);B.Rotate(TEXT("head"),Z,P.Head);
 B.Limb(TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),P.LeftFoot*Scale,FVector(30,85,55)*Scale);
 B.Limb(TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),P.RightFoot*Scale,FVector(-30,85,55)*Scale);
 B.Limb(TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),P.LeftHand*Scale,P.PoleL*Scale);
 B.Limb(TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),P.RightHand*Scale,P.PoleR*Scale);
 // Planted feet keep their sole orientation. Raised feet follow the shin.
 FBodyPose Ref(R,R.GetRefBonePose());for(const TCHAR* Foot:{TEXT("foot_l"),TEXT("foot_r")}){const int F=B.Bone(Foot);if(F>=0&&B.C[F].GetLocation().Z<18*Scale){B.Rotate(F,Ref.C[F].GetRotation()*B.C[F].GetRotation().Inverse());}}
 // Preserve a real floor contact even at extreme hip angles; never stretch
 // the support leg to make a pose fit an arbitrary pelvis height.
 if(P.LeftHand.Z>=18 && (P.LeftFoot.Z<=18 || P.RightFoot.Z<=18)) {
  float Drop=1e6f;
  if(P.LeftFoot.Z<=18)Drop=FMath::Min(Drop,float(B.At(TEXT("foot_l")).Z-Ref.At(TEXT("foot_l")).Z));
  if(P.RightFoot.Z<=18)Drop=FMath::Min(Drop,float(B.At(TEXT("foot_r")).Z-Ref.At(TEXT("foot_r")).Z));
  B.Move(B.Bone(TEXT("pelvis")),B.At(TEXT("pelvis"))-FVector(0,0,Drop));
 }
 B.Hand(true,P.OpenL);B.Hand(false,P.OpenR);
 if(P.OpenR>.9f)B.Palm(TEXT("hand_r"),TEXT("middle_03_r"),TEXT("index_01_r"),TEXT("pinky_01_r"),FVector(0,0,1),FVector(0,1,0));
 if(P.LeftHand.Z<18)B.Palm(TEXT("hand_l"),TEXT("middle_03_l"),TEXT("index_01_l"),TEXT("pinky_01_l"),FVector(0,1,0),FVector(0,0,1));
 B.Local(Out);
}
void Mirror(const FReferenceSkeleton& R,TArray<FTransform>& Pose)
{
 FBodyPose B(R,Pose),Ref(R,R.GetRefBonePose());TArray<FTransform> M=B.C;
 auto Q=[](const FQuat& A){return FQuat(A.X,-A.Y,-A.Z,A.W);};
 for(int I=0;I<M.Num();++I){FString N=R.GetBoneName(I).ToString();int T=I;if(N.EndsWith(TEXT("_l")))T=R.FindBoneIndex(*(N.LeftChop(2)+TEXT("_r")));else if(N.EndsWith(TEXT("_r")))T=R.FindBoneIndex(*(N.LeftChop(2)+TEXT("_l")));if(T<0)T=I;
  FVector V=B.C[T].GetLocation();V.X=-V.X;FQuat Rot=Q(B.C[T].GetRotation())*Q(Ref.C[T].GetRotation()).Inverse()*Ref.C[I].GetRotation();Rot.Normalize();M[I]=FTransform(Rot,V,B.C[T].GetScale3D());
 }B.C=MoveTemp(M);B.Local(Pose);
}
}

bool AGkMatch::BuildChoreography(int32 Who,FName Slot,TArray<FTransform>& Out)
{
 if((Screen!=3 && Screen!=4 && !(Screen==6 && S.p[Who].st==gk::WIN)) || !Body[Who] || !Body[Who]->GetSkinnedAsset())return false;
 if(Screen==4 && bLabDrop && LabTryAt>=-1)return false; // explicit purchased-clip audition remains available
 auto P=S.p[Who];auto O=S.p[1-Who];const FString Name=(Screen!=4 && P.st==gk::ATK && P.mv>=0)?FString(TEXT("mv_"))+ANSI_TO_TCHAR(gk::moveKey(P.mv)):Slot.ToString();
 if(Screen==4){
  P.face=1;P.sf=P.mf=LabFrame;P.superHold=0;P.hitOK=0;P.sub=gk::STAND;P.st=gk::IDLE;
  if(Name.StartsWith(TEXT("mv_"))){P.mv=gk::moveByKey(TCHAR_TO_UTF8(*Name.RightChop(3)));if(P.mv>=0)P.st=gk::ATK;}
  else if(Name.StartsWith(TEXT("parry")))P.st=gk::PARRY;
  else if(Name==TEXT("win"))P.st=gk::WIN;
  else if(Name==TEXT("run"))P.st=gk::RUN;
  else if(Name==TEXT("drush"))P.st=gk::DRUSH;
  else if(Name==TEXT("tech"))P.st=gk::TECH;
  else if(Name.StartsWith(TEXT("throw")) && Name!=TEXT("thrown")){P.st=gk::THROWING;P.tk=FMath::Clamp(FCString::Atoi(*Name.Right(1)),1,4);P.tf=LabFrame;}
  else if(Name!=TEXT("idle"))return false;
 }
 const float T=P.st==gk::ATK?P.mf:P.sf;
 FPose Idle;Idle.Hip={0,-3,92};Idle.Yaw=38;Idle.Chest=12;Idle.Head=-35;Idle.LeftHand={35,-9,153};Idle.RightHand={-30,35,121};
 FPose A=Idle;bool Have=false;
 if(P.superHold && O.st==gk::ATK && O.mv==gk::moveByKey("SA2") && O.mf<40)
 {
  Have=true;A.Hip={0,-3,91};A.Pitch=12;A.Yaw=-12;A.Chest=0;A.Head=15;A.LeftHand={43,-6,157};A.RightHand={-40,25,164};A.LeftFoot={20,23,13};A.RightFoot={-20,-21,13};
 }
 else if(P.st==gk::PARRY || ((P.st==gk::IDLE || P.st==gk::PREC) && AnimClock-ParryBeat[Who]<16))
 {
  Have=true;A.Yaw=-55;A.Chest=-25;A.Head=105;A.Hip={0,-5,96};A.LeftHand={24,-6,169};A.RightHand={-20,61,137};A.LeftFoot={19,18,13};A.RightFoot={-17,-24,13};
  const float Beat=AnimClock-ParryBeat[Who];if(Beat>=0 && Beat<16){
   const float Height=FMath::Clamp(float((ParryContact[Who].Y-P.y)*Cm),28.f,173.f);
   const float Reach=FMath::Clamp(float((ParryContact[Who].X-P.x)*P.face*Cm),24.f,65.f);
   const float Snap=1-Ease(6,16,Beat);
   if(ParryVariant[Who]==3){A.RightFoot=FMath::Lerp(A.RightFoot,FVector(-19,40,FMath::Clamp(Height,34.f,65.f)),Snap);A.Hip.Z-=4*Snap;}
   else{A.RightHand=FMath::Lerp(A.RightHand,FVector(-19,Reach,Height),Snap);A.Chest-=4*Snap;}
  }
 }
 else if(P.st==gk::ATK && (Name.StartsWith(TEXT("mv_SRK"))||Name==TEXT("mv_QSRK")||Name==TEXT("mv_AUP")))
 {
  Have=true;const auto& M=gk::move(P.mv);const float Active=M.nhits?M.hits[0].s:M.su;
  FPose Plant=Idle;Plant.Hip={0,5,63};Plant.Pitch=-94;Plant.Yaw=8;Plant.Chest=0;Plant.Head=15;Plant.LeftHand={23,52,12};Plant.RightHand={-20,40,91};Plant.LeftFoot={17,-32,23};Plant.RightFoot={-17,17,99};
  FPose Heel=Plant;Heel.Hip={0,0,91};Heel.Pitch=-116;Heel.LeftHand={23,54,13};Heel.RightHand={-23,62,99};Heel.LeftFoot={18,-36,93};Heel.RightFoot={-13,23,179};
  FPose Scissor=Idle;Scissor.Hip={0,-3,82};Scissor.Pitch=-38;Scissor.Yaw=35;Scissor.Head=-40;Scissor.LeftFoot={20,54,139};Scissor.RightFoot={-18,-44,53};Scissor.LeftHand={39,-8,151};Scissor.RightHand={-35,40,106};
  if(Name==TEXT("mv_AUP")){Plant=Scissor;Plant.RightFoot={-20,8,105};}
  if(T<Active-2)A=Mix(Idle,Plant,Ease(0,FMath::Max(1.f,Active-2),T));
  else if(T<Active+3)A=Mix(Plant,Heel,Ease(Active-2,Active,T));
  else if(T<Active+12)A=Mix(Heel,Scissor,Ease(Active+5,Active+11,T));
  else{FPose Fall=Idle;Fall.LeftFoot={18,17,32};Fall.RightFoot={-18,-18,40};Fall.RightHand={-42,32,135};A=Mix(Scissor,Fall,Ease(FMath::Max(Active+14,float(M.lastAct)+3),FMath::Max(Active+27,float(M.lastAct)+16),T));}
 }
 else if(P.st==gk::THROWING || P.st==gk::THROWN)
 {
  Have=true;const auto& Thrower=P.st==gk::THROWING?P:O;const int ThrowKind=Thrower.tk?Thrower.tk:(Thrower.pmN?Thrower.pmN:1);const float F=Thrower.tk?Thrower.tf:0;
  if(P.st==gk::THROWING){
   FPose Grip=Idle;Grip.Hip={0,7,87};Grip.Yaw=-25;Grip.LeftHand={20,52,135};Grip.RightHand={-20,49,112};Grip.LeftFoot={17,25,13};Grip.RightFoot={-17,-24,13};
   FPose Load=Grip;Load.Hip={0,-2,70};Load.Pitch=12;Load.Yaw=ThrowKind==2?-70:5;Load.LeftHand={18,37,134};Load.RightHand={-22,35,121};
   FPose Send=Idle;Send.Hip={0,ThrowKind==2?-8.f:7.f,86};Send.Yaw=ThrowKind==2?-120:30;Send.Pitch=ThrowKind==2?24:-20;Send.LeftHand={28,ThrowKind==2?-49.f:59.f,ThrowKind==3?180.f:143.f};Send.RightHand={-24,ThrowKind==2?-42.f:52.f,ThrowKind==3?171.f:123.f};
   if(ThrowKind==4){Send.Hip={0,12,62};Send.Pitch=-35;Send.LeftHand={20,59,24};Send.RightHand={-20,52,31};}
   FPose Finish=Idle;Finish.LeftHand={49,19,157};Finish.RightHand={-45,32,125};Finish.Head=-45;Finish.Yaw=45;
   if(!Thrower.tk)A=Mix(Idle,Grip,Ease(0,5,Thrower.sf));else if(F<7)A=Mix(Grip,Load,Ease(0,7,F));else if(F<18)A=Mix(Load,Send,Ease(7,18,F));else A=Mix(Send,Finish,Ease(20,30,F));
  }else{
   A.Hip={0,0,91};A.Yaw=0;A.Pitch=10;A.Head=0;A.LeftHand={30,30,145};A.RightHand={-30,40,131};A.LeftFoot={18,18,13};A.RightFoot={-18,-20,13};
   const float Lift=Ease(3,13,F);A.Pitch=FMath::Lerp(10.f,ThrowKind==2?105.f:ThrowKind==4?-105.f:-80.f,Lift);A.LeftFoot=FMath::Lerp(A.LeftFoot,FVector(24,-36,100),Lift);A.RightFoot=FMath::Lerp(A.RightFoot,FVector(-24,-15,145),Lift);
   A.LeftHand=FMath::Lerp(A.LeftHand,FVector(32,23,119),Lift);A.RightHand=FMath::Lerp(A.RightHand,FVector(-32,12,108),Lift);
   if(ThrowKind==4){A.Hip.Z=FMath::Lerp(91.f,10.f,Ease(10,16,F));A.Pitch=FMath::Lerp(A.Pitch,-90.f,Ease(10,18,F));}
  }
 }
 else if(P.st==gk::IDLE && P.sub==gk::STAND)
 {
  Have=true;A=Idle;A.Hip.Z+=.7f*FMath::Sin(S.frame*.035f);A.LeftHand={36,-10,107};A.RightHand={-28,32,129};
  const float Beat=FMath::Fmod(float(P.sf),360.f);const float PoseBeat=Ease(260,275,Beat)*(1-Ease(320,338,Beat));
  A.LeftHand=FMath::Lerp(A.LeftHand,FVector(37,-3,167),PoseBeat);A.Head-=8*PoseBeat;A.Chest+=7*PoseBeat;
 }
 else if(P.st==gk::TECH)
 {
  Have=true;FPose Break=Idle;Break.Hip.Z=83;Break.Pitch=12;Break.LeftHand={36,38,158};Break.RightHand={-36,39,139};Break.LeftFoot={20,26,13};Break.RightFoot={-22,-29,13};
  A=Mix(Break,Idle,Ease(10,24,T));
 }
 else if(P.st==gk::WIN)
 {
  Have=true;FPose Bow=Idle;Bow.Hip={0,0,86};Bow.Pitch=-23;Bow.Yaw=50;Bow.LeftHand={38,7,125};Bow.RightHand={-38,50,100};Bow.RightFoot={-22,-27,13};
  FPose Hero=Idle;Hero.Hip={0,-8,89};Hero.Yaw=45;Hero.Chest=20;Hero.Head=-55;Hero.LeftHand={43,-3,179};Hero.RightHand={-39,54,139};Hero.LeftFoot={21,36,13};Hero.RightFoot={-24,-33,13};
  FPose Photo=Hero;Photo.LeftHand={36,0,169};Photo.RightHand={-51,38,166};Photo.Head=-42;Photo.Hip.Z=94;
  if(T<18)A=Mix(Idle,Bow,Ease(0,18,T));else if(T<30)A=Mix(Bow,Hero,Ease(18,27,T));else if(T<100)A=Hero;else A=Mix(Hero,Photo,Ease(100,112,T));
 }
 else if(P.st==gk::RUN || P.st==gk::DRUSH)
 {
  Have=true;A.Hip={0,5,77};A.Pitch=-22;A.Yaw=20;A.Chest=20;A.Head=-30;
  const float Phase=FMath::Fmod(float(S.frame),16.f)/16.f;const float Stride=FMath::Clamp(float(FMath::Abs(P.vx)*Cm*4),25.f,51.f);
  auto Foot=[&](float U,float Side){U=FMath::Fmod(U,1.f);const float Y=U<.5f?FMath::Lerp(Stride,-Stride,U*2):FMath::Lerp(-Stride,Stride,Ease(.5f,1.f,U));return FVector(Side,Y,13+(U<.5f?0:44*FMath::Sin((U-.5f)*2*PI)));};
  A.LeftFoot=Foot(Phase,18);A.RightFoot=Foot(Phase+.5f,-18);A.Hip.Z+=3*FMath::Cos(Phase*4*PI);
  A.LeftHand={37,-23,139};A.RightHand={-32,40,158};A.PoleR={-70,55,115};
  if(P.st==gk::DRUSH){A.Hip.Z-=8;A.Pitch=-30;A.LeftHand={29,-40,107};A.RightHand={-32,25,155};}
 }
 else if(P.st==gk::ATK && P.mv>=0)
 {
  const auto& M=gk::move(P.mv);const FString Key=ANSI_TO_TCHAR(gk::moveKey(P.mv));FPose Wind=Idle,Hit=Idle,End=Idle;
  const float Active=M.nhits?M.hits[0].s:M.has_proj?M.pf:M.su;
  const float Last=M.lastAct>Active?M.lastAct:Active+3;
  const float Finish=M.t<900?M.t:Last+20;
  const bool Low=M.crouch;
  if(Low){Wind.Hip={0,-3,52};Wind.Pitch=-13;Wind.LeftFoot={21,24,13};Wind.RightFoot={-23,-20,13};Wind.LeftHand={30,12,116};Wind.RightHand={-27,25,94};Hit=Wind;End=Wind;}
  if(Key==TEXT("throw")){
   Have=true;Wind.Hip.Z=88;Hit.Hip={0,10,83};Hit.Pitch=-12;Hit.Yaw=0;Hit.LeftHand={22,63,136};Hit.RightHand={-22,60,118};Hit.LeftFoot={18,28,13};Hit.RightFoot={-20,-25,13};
  }else if(Key==TEXT("SA2")){
   Have=true;Wind.Hip={0,2,85};Wind.Pitch=-10;Wind.Yaw=45;Wind.LeftHand={43,8,169};Wind.RightHand={-40,4,153};
   Hit.Hip={0,12,89};Hit.Yaw=0;Hit.Pitch=-18;Hit.LeftHand={24,57,132};Hit.RightHand={-23,56,118};
   if(P.hitOK && T<40){FPose Catch=Hit;Hit.Hip={0,13,91};Hit.Pitch=-28;Hit.LeftHand={22,61,137};Hit.RightHand={-25,57,117};Hit.LeftFoot={26,34,55};Hit.RightFoot={-26,27,43};Hit.Head=-28;Hit.OpenR=0;Hit.OpenL=0;Hit=Mix(Catch,Hit,Ease(7,14,T));}
   if(T>=40){Hit.Hip={0,-14,70};Hit.Pitch=28;Hit.LeftHand={49,-17,153};Hit.RightHand={-47,31,152};Hit.LeftFoot={21,37,13};Hit.RightFoot={-21,-30,13};}
  }else if(Key==TEXT("5LP") || Key==TEXT("2LP")){
   Have=true;Wind.RightHand={-31,-2,132};Hit.Hip.Y=8;Hit.RightHand={-17,58,Low?88.f:136.f};Hit.LeftHand={37,2,156};Hit.Yaw=-12;Hit.Chest=-15;
  }else if(Key==TEXT("5MP") || Key==TEXT("CB2")){
   Have=true;Wind.Yaw=65;Wind.RightHand={-32,-15,154};Hit.Yaw=-48;Hit.Chest=-15;Hit.Hip.Y=17;Hit.RightHand={-15,32,145};Hit.PoleR={-25,110,120};Hit.LeftHand={38,0,164};if(Key==TEXT("CB2")){Wind.RightHand.Z=190;Hit.Hip.Z=79;Hit.RightHand.Z=123;Hit.Pitch=-20;}
  }else if(Key==TEXT("5HP") || Key==TEXT("2HP") || Key==TEXT("DI") || Key==TEXT("DREV")){
   Have=true;Wind.Yaw=60;Wind.Chest=20;Wind.RightHand={-45,-20,139};Wind.LeftHand={52,12,168};Hit.Hip.Y=22;Hit.Pitch=-14;Hit.Yaw=-15;Hit.RightHand={-19,87,138};Hit.LeftHand={47,-7,165};
   if(Key==TEXT("2HP")){Hit.Hip.Z=65;Hit.Pitch=-23;Hit.RightHand={-17,40,161};Hit.LeftHand={34,5,111};}
   if(Key==TEXT("DI")){Wind.Hip.Z=82;Wind.LeftHand={46,22,172};Hit.Hip.Y=27;Hit.Yaw=-55;Hit.PoleR={-15,110,126};Hit.RightHand={-28,65,151};}
  }else if(Key==TEXT("5LK") || Key==TEXT("2LK") || Key==TEXT("2MK") || Key==TEXT("2HK")){
   Have=true;Wind.RightFoot={-21,0,Low?30.f:51.f};Hit.Hip.Y=15;Hit.RightFoot={-19,Key==TEXT("5LK")?67.f:90.f,Key==TEXT("5LK")?44.f:16.f};Hit.LeftHand={40,-2,Low?111.f:166.f};Hit.RightHand={-37,5,Low?85.f:125.f};Hit.Pitch=13;Hit.Yaw=55;
   if(Key==TEXT("2HK")){Hit.Hip.Z=42;Hit.LeftHand={24,-16,9};Hit.RightFoot={-17,91,28};Hit.RightHand={-33,19,108};Hit.Pitch=-22;}
  }else if(Key==TEXT("5MK") || Key==TEXT("5HK") || Key==TEXT("TFK2") || Key==TEXT("TFK3") || Key==TEXT("TK") || Key==TEXT("FSK")){
   Have=true;Wind.Hip.Z=88;Wind.RightFoot={-18,21,85};Wind.LeftHand={39,-2,169};Hit.Hip={0,18,93};Hit.Pitch=25;Hit.Yaw=55;Hit.RightFoot={-19,100,115};Hit.LeftFoot={18,-17,13};Hit.LeftHand={38,-16,166};Hit.RightHand={-40,21,138};
   if(Key==TEXT("5HK")||Key==TEXT("TK")){Wind.RightFoot={-18,14,173};Hit.RightFoot.Z=128;Hit.RightFoot.Y=92;Wind.Pitch=20;}
   if(Key==TEXT("TFK2")){Hit.RightFoot={-20,71,126};Hit.Hip.Y=21;}if(Key==TEXT("TFK3"))Hit.RightFoot={-18,86,159};
  }else if(Key==TEXT("jLP")||Key==TEXT("jMP")||Key==TEXT("jHP")||Key==TEXT("jLK")||Key==TEXT("jMK")||Key==TEXT("jHK")){
   Have=true;Wind.Hip.Z=81;Wind.LeftFoot={22,14,39};Wind.RightFoot={-22,-23,54};Wind.LeftHand={39,-12,159};Hit=Wind;End=Wind;Hit.Yaw=40;Hit.LeftHand={41,-8,166};
   if(Key==TEXT("jLP"))Hit.RightHand={-18,54,134};
   if(Key==TEXT("jMP")){Hit.Pitch=-30;Hit.RightHand={-19,71,88};}
   if(Key==TEXT("jHP")){Wind.RightHand={-28,12,188};Hit.Pitch=-90;Hit.Hip.Y=8;Hit.RightHand={-19,44,30};Hit.LeftFoot={22,-34,105};Hit.RightFoot={-22,-9,136};Hit.LeftHand={38,28,91};}
   if(Key==TEXT("jLK")){Hit.RightFoot={-18,43,22};Hit.LeftFoot={22,-20,68};}
   if(Key==TEXT("jMK")){Hit.Pitch=20;Hit.RightFoot={-18,81,55};Hit.LeftFoot={23,-41,54};}
   if(Key==TEXT("jHK")){Hit.Pitch=18;Hit.RightFoot={-18,75,30};Hit.LeftFoot={21,-23,70};Hit.RightHand={-44,2,151};}
  }else if(Key.StartsWith(TEXT("JIN")) || Key.StartsWith(TEXT("KAZE")) || Key.StartsWith(TEXT("GORAI")) || Key.StartsWith(TEXT("SENKA")) || Key==TEXT("KASAI")){
   Have=true;Wind.Hip.Z=85;Wind.LeftHand={42,-3,169};Wind.RightHand={-40,19,147};Wind.RightFoot={-20,20,88};Hit=Wind;Hit.Yaw=48;Hit.Pitch=10;Hit.Hip.Y=22;Hit.RightFoot={-19,93,102};Hit.LeftFoot={19,-24,13};
   if(Key.StartsWith(TEXT("KAZE"))){Wind.RightFoot.Z=50;Hit.Hip.Z=58;Hit.RightFoot={-21,89,21};Hit.Pitch=5;Hit.RightHand={-40,28,114};}
   if(Key.StartsWith(TEXT("GORAI"))){Wind.RightFoot={-18,25,183};Wind.Pitch=20;Hit.RightFoot={-18,91,119};Hit.Pitch=16;}
   if(Key.StartsWith(TEXT("SENKA"))){Hit.RightFoot={-19,80,158};Hit.Pitch=25;Hit.RightHand={-45,12,144};}
   if(Key==TEXT("KASAI")){Wind.RightFoot={-20,-15,13};Hit.Hip={0,24,81};Hit.RightFoot={-23,-29,13};Hit.RightHand={-18,85,128};Hit.LeftHand={48,-8,163};Hit.Pitch=-12;Hit.Yaw=-15;}
  }else if(Key.StartsWith(TEXT("DL")) || Key==TEXT("QDL") || Key==TEXT("ASPK")){
   Have=true;Wind.Hip.Z=80;Wind.Pitch=12;Wind.LeftFoot={22,-10,52};Wind.RightFoot={-20,19,108};Wind.LeftHand={45,-8,165};Wind.RightHand={-38,33,153};Hit=Wind;Hit.Pitch=23;Hit.RightFoot={-18,58,9};Hit.LeftFoot={23,-23,77};Hit.RightHand={-43,14,155};Hit.Yaw=42;End=Hit;
  }else if(Key.StartsWith(TEXT("TAT")) || Key==TEXT("QTAT") || Key.StartsWith(TEXT("ATAT"))){
   Have=true;Wind.Hip.Z=85;Wind.RightFoot={-20,22,92};Wind.RightHand={-40,9,165};Hit.Hip={0,6,86};Hit.Pitch=20;Hit.Yaw=55;Hit.Chest=15;Hit.Head=-65;Hit.RightFoot={-18,96,106};Hit.LeftFoot={20,-15,M.isAir?43.f:13.f};Hit.LeftHand={45,-18,166};Hit.RightHand={-41,25,144};
   // A distinct knee chamber between strike beats; do not windmill the body.
   for(int Beat=0;Beat<M.nhits;++Beat)if(T>=M.hits[Beat].e+1 && Beat+1<M.nhits && T<M.hits[Beat+1].s){Hit.RightFoot={-23,19,88};Hit.Yaw=-20;Hit.Head=10;}
  }else if(Key==TEXT("SA1")){
   Have=true;Wind.Hip={0,-3,78};Wind.Pitch=-10;Wind.LeftHand={45,13,174};Wind.RightHand={-42,-8,146};Hit.Hip={0,12,88};Hit.Yaw=45;Hit.Head=-45;Hit.LeftHand={43,-5,170};Hit.RightHand={-39,34,146};Hit.RightFoot={-17,75,151};Hit.LeftFoot={20,-16,13};Hit.Pitch=15;
   if(T>=14 && T<35){const int Beat=int(T/5)%2;Hit.Hip.Z=80;Hit.RightFoot={-20,Beat?76.f:22.f,Beat?61.f:98.f};Hit.LeftFoot={22,Beat?14.f:67.f,Beat?90.f:52.f};Hit.Pitch=Beat?-12:18;Hit.Chest=Beat?20:-20;}
   if(T>=35){Hit.Pitch=26;Hit.RightFoot={-19,99,94};Hit.LeftFoot={21,-35,57};Hit.RightHand={-45,18,154};}
  }else if(M.has_home){
   Have=true;Wind.Hip.Z=80;Wind.Pitch=-15;Wind.RightHand={-32,18,164};Hit.Hip={0,2,78};Hit.Pitch=-66;Hit.Yaw=25;Hit.Head=-20;Hit.RightHand={-18,106,105};Hit.LeftHand={38,13,131};Hit.LeftFoot={22,-61,88};Hit.RightFoot={-21,-32,56};Hit.OpenR=0;End=Hit;
  }else if(M.has_proj || Key==TEXT("ABEAM") || Key==TEXT("SA3") || Key==TEXT("CA")){
   Have=true;Wind.Hip.Z=87;Wind.Yaw=60;Wind.LeftHand={37,0,165};Wind.RightHand={-25,5,111};Hit.Hip={0,-9,89};Hit.Pitch=12;Hit.Yaw=18;Hit.LeftHand={38,-8,163};Hit.RightHand={-19,70,100};Hit.LeftFoot={21,27,13};Hit.RightFoot={-23,-33,13};
   if(Key==TEXT("ABEAM")){Wind.LeftFoot={23,15,51};Wind.RightFoot={-20,-19,63};Hit.LeftFoot={23,29,48};Hit.RightFoot={-21,-23,65};Hit.Pitch=25;}
   if(Key==TEXT("SA3")||Key==TEXT("CA")){Wind.RightHand={-39,-24,160};Wind.LeftHand={45,24,178};Hit.RightHand.Z=114;Hit.Hip.Z=76;Hit.Pitch=19;}
  }
  if(Have){
   if(T<Active)A=Mix(Wind,Hit,Ease(FMath::Max(0.f,Active-4),Active,T));
   else if(T<=Last+2)A=Hit;
   else A=Mix(Hit,End,Ease(Last+2,Finish,T));
   if(S.freeze>0 && S.frzWho==Who)A=Wind;
  }
 }
 if(!Have)return false;
 const FReferenceSkeleton& R=Body[Who]->GetSkinnedAsset()->GetRefSkeleton();MakePose(R,A,Out);if(P.face<0)Mirror(R,Out);return true;
}

void AGkMatch::UpdateThrowContact()
{
 if(Screen!=3)return;
 for(int Who=0;Who<2;++Who){const auto& P=S.p[Who];const auto& O=S.p[1-Who];const bool Super=P.st==gk::ATK && P.mv==gk::moveByKey("SA2") && O.superHold==Who+1;
  if((!Super && (P.st!=gk::THROWING || O.st!=gk::THROWN)) || !Body[Who] || !Body[1-Who])continue;
  auto* Att=Body[Who].Get();auto* Def=Body[1-Who].Get();if(!Att->GetSkinnedAsset()||!Def->GetSkinnedAsset())continue;
  // Correct the grip after both bodies have been evaluated. Palm targets follow
  // the other skeleton, never a stale screen coordinate or a stretched arm.
  FBodyPose D(Def->GetSkinnedAsset()->GetRefSkeleton(),Def->BoneSpaceTransforms),A(Att->GetSkinnedAsset()->GetRefSkeleton(),Att->BoneSpaceTransforms);
  const bool Left=P.face<0;const TCHAR* HandA=Left?TEXT("hand_r"):TEXT("hand_l");const TCHAR* HandB=Left?TEXT("hand_l"):TEXT("hand_r");
  const float Blend=Super?1.f:Ease(0,4,P.sf);
  auto ToAtt=[&](const FVector& V){return Att->GetComponentTransform().InverseTransformPosition(Def->GetComponentTransform().TransformPosition(V));};
  auto ToDef=[&](const FVector& V){return Def->GetComponentTransform().InverseTransformPosition(Att->GetComponentTransform().TransformPosition(V));};
  const TCHAR* DefHandA=Left?TEXT("hand_l"):TEXT("hand_r");const TCHAR* DefHandB=Left?TEXT("hand_r"):TEXT("hand_l");
  // Both partners participate in the solve: a grip cannot be repaired by
  // extending only the attacker's arm beyond its anatomical reach.
  for(int Pass=0;Pass<3;++Pass){
   D.Limb(Left?TEXT("upperarm_l"):TEXT("upperarm_r"),Left?TEXT("lowerarm_l"):TEXT("lowerarm_r"),DefHandA,FMath::Lerp(D.At(DefHandA),ToDef(A.At(HandA)),Blend),FVector(Left?75:-75,35,110));
   D.Limb(Left?TEXT("upperarm_r"):TEXT("upperarm_l"),Left?TEXT("lowerarm_r"):TEXT("lowerarm_l"),DefHandB,FMath::Lerp(D.At(DefHandB),ToDef(A.At(HandB)),Blend),FVector(Left?-75:75,35,110));
   A.Limb(Left?TEXT("upperarm_r"):TEXT("upperarm_l"),Left?TEXT("lowerarm_r"):TEXT("lowerarm_l"),HandA,ToAtt(D.At(DefHandA)),FVector(Left?-85:85,15,100));
   A.Limb(Left?TEXT("upperarm_l"):TEXT("upperarm_r"),Left?TEXT("lowerarm_l"):TEXT("lowerarm_r"),HandB,ToAtt(D.At(DefHandB)),FVector(Left?85:-85,15,100));
  }
  if(bScript && P.sf>=4 && P.sf%3==0)UE_LOG(LogTemp,Display,TEXT("GkGrip who=%d kind=%d frame=%d left=%.2f right=%.2f"),Who,P.tk,P.tf,FVector::Dist(A.At(HandA),ToAtt(D.At(DefHandA))),FVector::Dist(A.At(HandB),ToAtt(D.At(DefHandB))));
  D.Local(Def->BoneSpaceTransforms);PoseShown[1-Who]=Def->BoneSpaceTransforms;Def->MarkRefreshTransformDirty();
  A.Local(Att->BoneSpaceTransforms);PoseShown[Who]=Att->BoneSpaceTransforms;Att->MarkRefreshTransformDirty();
 }
}
