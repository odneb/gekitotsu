#include "GkMatch.h"
#include "Animation/AnimSequence.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Engine/Font.h"
#include "Misc/Paths.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/CompositeFont.h"

namespace {
const TCHAR* Categories[]={TEXT("MOVEMENT / PERSONALITY"),TEXT("GUARD / PARRY"),TEXT("REACTIONS"),TEXT("PUNCHES"),TEXT("KICKS"),TEXT("AIR / HOMING"),TEXT("SPECIALS"),TEXT("SUPERS"),TEXT("THROWS / PARTNERS")};
const TCHAR* Packs[]={TEXT("ALL PACKS"),TEXT("Anime Martial Arts"),TEXT("Anime Power Up"),TEXT("Superhero Ground"),TEXT("Mega Taunt"),TEXT("Mega Emotes"),TEXT("UNARMED"),TEXT("HERO"),TEXT("ENERGY"),TEXT("FIGHT")};
const TCHAR* Filters[]={TEXT("ALL MOTIONS"),TEXT("MOVEMENT"),TEXT("BLOCKS"),TEXT("HIT REACTIONS"),TEXT("FALL / GET UP"),TEXT("PUNCHES"),TEXT("KICKS"),TEXT("COMBOS"),TEXT("ENERGY"),TEXT("FLYING"),TEXT("TAUNTS / POSES")};
bool Same(const AGkMatch::FGkClip& A,const AGkMatch::FGkClip& B){return A.Anim==B.Anim && FMath::IsNearlyEqual(A.From,B.From) && FMath::IsNearlyEqual(A.To,B.To) && FMath::IsNearlyEqual(A.Impact,B.Impact) && A.bLoop==B.bLoop;}
}
bool AGkMatch::SetupMatches(const FLabClip& C) const {
 if(!bSetupVariants && (C.Path.Contains(TEXT("_MR")) || C.Path.Contains(TEXT("Loop")) || C.Path.Contains(TEXT("/Root_Motion/"))))return false;
 return (SetupPack==0 || C.Pack.Contains(Packs[SetupPack])) && (SetupFilter<0 || C.Cat==SetupFilter) && (SetupSearch.IsEmpty() || (C.Label+TEXT(" ")+C.Pack).Contains(SetupSearch));
}
void AGkMatch::SetupSelect(int32 Slot){
 if(!LabSlots.IsValidIndex(Slot))return;
 if(LabSlots.IsValidIndex(LabSlot) && LabTryAt>=-1)SetupDrafts.FindOrAdd(LabSlots[LabSlot].Slot)=LabTry;
 LabSlot=Slot;LabCat=LabSlots[Slot].Cat;
 const FName Name=LabSlots[Slot].Slot;
 const FGkClip* Draft=SetupDrafts.Find(Name);const FGkClip* Saved=Clips[LabSet].Find(Name);
 LabTry=Draft?*Draft:Saved?*Saved:FGkClip{};LabTryAt=LabTry.Anim?0:-1;bLabDrop=true;bLabPair=false;
 LabClock=0;LabFrame=0;LabScroll=0;bSetupSearch=false;
}
void AGkMatch::SetupApply(){
 if(!LabSlots.IsValidIndex(LabSlot))return;
 LabTry.bAuthored=true;const FName Slot=LabSlots[LabSlot].Slot;
 Clips[LabSet].Add(Slot,LabTry);SetupDrafts.Remove(Slot);Picked.Add(FString::Printf(TEXT("%d|%s"),LabSet,*Slot.ToString()));LabSave();MenuSound(1);
}
void AGkMatch::SetupTest(){
 if(!LabSlots.IsValidIndex(LabSlot))return;
 const FName Slot=LabSlots[LabSlot].Slot;SetupOriginal=Clips[LabSet].FindRef(Slot);SetupDrafts.FindOrAdd(Slot)=LabTry;Clips[LabSet].Add(Slot,LabTry);
 SetupOldMode=Mode;SetupOldDummy=Dummy;Mode=1;Dummy=0;Screen=3;bMenu=false;bSetupTesting=true;Restart();
 for(int i=0;i<400 && S.phase!=gk::PH_FIGHT;++i)gk::step(S,0,0);
 S.p[0].x=-35*gk::F;S.p[1].x=35*gk::F;S.p[0].face=1;S.p[1].face=-1;S.p[0].sa=3000;S.p[0].dr=6000;
 int Move=MoveClip.Find(Slot);
 if(Slot.ToString().StartsWith(TEXT("pairGrab")))Move=gk::moveByKey("SA2");
 else if(Slot.ToString().StartsWith(TEXT("pair")) || Slot.ToString().StartsWith(TEXT("throw")) || Slot==TEXT("grab"))Move=gk::moveByKey("throw");
 S.nev=0;
 if(Move>=0){gk::debugStart(S,0,Move);if(gk::move(Move).isAir){S.p[0].y=90*gk::F;S.p[1].y=90*gk::F;S.p[1].st=gk::JUMP;}for(int e=0;e<S.nev;++e)OnEvent(S.ev[e]);}
 if(Slot.ToString().StartsWith(TEXT("pairForward")) || Slot.ToString().StartsWith(TEXT("pairBack"))){
  const int Direction=Slot.ToString().StartsWith(TEXT("pairBack"))?gk::IN_L:gk::IN_R;
  for(int Tick=0;Tick<10;++Tick){gk::step(S,Direction,0);for(int e=0;e<S.nev;++e)OnEvent(S.ev[e]);}
 }
 else if(Slot==TEXT("run")){S.p[0].st=gk::RUN;S.p[0].sf=0;}
 else if(Slot==TEXT("parry") || Slot.ToString().StartsWith(TEXT("parry"))){S.p[0].st=gk::PARRY;gk::debugStart(S,1,gk::moveByKey(Slot==TEXT("parryLow")?"2LK":"5HP"));}
 // State assignments can be tried with normal controls in the same sandbox.
 if(auto* PC=GetWorld()->GetFirstPlayerController()){PC->bShowMouseCursor=false;PC->SetInputMode(FInputModeGameOnly());}
 CamD.bSnap=true;
}
void AGkMatch::SetupReturn(){
 if(LabSlots.IsValidIndex(LabSlot))Clips[LabSet].Add(LabSlots[LabSlot].Slot,SetupOriginal);
 bSetupTesting=false;Mode=SetupOldMode;Dummy=SetupOldDummy;Screen=4;bMenu=false;Director.Reset();LabClock=0;
 if(auto* PC=GetWorld()->GetFirstPlayerController()){PC->bShowMouseCursor=true;FInputModeGameAndUI In;In.SetHideCursorDuringCapture(false);In.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(In);}
}
void AGkMatch::SetupAction(int32 Id){
 if(Id==1){LabClose();return;}if(Id==2){SetupApply();return;}if(Id==3){SetupTest();return;}
 if(Id==4){SetupDrafts.Remove(LabSlots[LabSlot].Slot);LabTryAt=-2;SetupSelect(LabSlot);return;}
 if(Id==5){bSetupPaused=!bSetupPaused;return;}if(Id==6){SetupSpeed=SetupSpeed==1.f?.5f:SetupSpeed==.5f?.25f:1.f;return;}
 if(Id==7){bSetupSearch=true;return;}if(Id==8){SetupSearch.Reset();LabScroll=0;return;}
 if(Id==9){SetupPack=(SetupPack+1)%UE_ARRAY_COUNT(Packs);LabScroll=0;return;}
 if(Id==10){SetupFilter=(SetupFilter+2)%(UE_ARRAY_COUNT(Filters))-1;LabScroll=0;return;}
 if(Id==17){bSetupVariants=!bSetupVariants;LabScroll=0;return;}
 if(Id==11){LabTry.bLoop=!LabTry.bLoop;return;}
 if(Id==12){LabTry.Impact=-1;return;}
 if(Id==13){bSetupPaused=true;LabClock=FMath::Max(0.f,LabClock-1.f/60);return;}
 if(Id==14){bSetupPaused=true;LabClock+=1.f/60;return;}
 if(Id==15){SetupSlotScroll=FMath::Max(0,SetupSlotScroll-5);return;}if(Id==16){SetupSlotScroll+=5;return;}
 if(Id>=20 && Id<26){const float Step=(Id&1)?.025f:-.025f;switch((Id-20)/2){
  case 0:LabTry.From=FMath::Clamp(LabTry.From+Step,0.f,LabTry.To-.025f);break;
  case 1:LabTry.To=FMath::Clamp(LabTry.To+Step,LabTry.From+.025f,1.f);break;
  case 2:LabTry.Impact=FMath::Clamp((LabTry.Impact<0?.5f:LabTry.Impact)+Step,LabTry.From,LabTry.To);break;
 }LabClock=0;return;}
 if(Id>=100 && Id<109){LabCat=Id-100;SetupSlotScroll=0;for(int i=0;i<LabSlots.Num();++i)if(LabSlots[i].Cat==LabCat){SetupSelect(i);break;}return;}
 if(Id>=1000 && Id<9999){SetupSelect(Id-1000);return;}
 if(Id>=10000 && LabList.IsValidIndex(Id-10000)){
  const auto& Clip=LabList[Id-10000];auto* A=LoadObject<UAnimSequence>(nullptr,*Clip.Path);if(!A)return;
  ClipKeep.AddUnique(A);const bool Loop=LabTry.bLoop;LabTry=FGkClip{};LabTry.Anim=A;LabTry.bAuthored=true;LabTry.bLoop=Loop;LabTryAt=Id-10000;bLabDrop=true;LabClock=0;bSetupPaused=false;
  LabNote=TEXT("PREVIEW ONLY — Apply to save this move");LabNoteTimer=180;
 }
}
void AGkMatch::SetupInput(){
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 auto Hit=[&](FKey K){return ScriptMenuKey.IsValid()?K==ScriptMenuKey:PC->WasInputKeyJustPressed(K);};
 if(Hit(EKeys::LeftMouseButton)){bSetupSearch=false;SetupAction(LabHot);return;}
 if(bSetupSearch){
  if(Hit(EKeys::Escape)||Hit(EKeys::Enter)){bSetupSearch=false;return;}
  if(Hit(EKeys::BackSpace) && SetupSearch.Len())SetupSearch.LeftChopInline(1);
  for(TCHAR C='A';C<='Z';++C)if(Hit(FKey(FName(*FString::Chr(C)))))SetupSearch+=FString::Chr(C).ToLower();
  if(Hit(EKeys::SpaceBar))SetupSearch+=TEXT(" ");
  const FKey DigitKeys[]={EKeys::Zero,EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine};
  for(int i=0;i<10;++i)if(Hit(DigitKeys[i]))SetupSearch+=FString::FromInt(i);
  LabScroll=0;return;
 }
 if(Hit(EKeys::Escape)){LabClose();return;}if(Hit(EKeys::F5)){SetupTest();return;}if(Hit(EKeys::F6)){SetupApply();return;}
 if(Hit(EKeys::SpaceBar))bSetupPaused=!bSetupPaused;
 if(Hit(EKeys::MouseScrollDown))LabScroll+=3;if(Hit(EKeys::MouseScrollUp))LabScroll=FMath::Max(0,LabScroll-3);
 if(Hit(EKeys::Tab))SetupAction(100+(LabCat+1)%9);
 TArray<int> Show;for(int i=0;i<LabList.Num();++i)if(SetupMatches(LabList[i]))Show.Add(i);
 if(Hit(EKeys::Down)||Hit(EKeys::Up)){int At=Show.Find(LabTryAt);At=FMath::Clamp(At+(Hit(EKeys::Down)?1:-1),0,FMath::Max(0,Show.Num()-1));if(Show.IsValidIndex(At)){SetupAction(10000+Show[At]);LabScroll=FMath::Clamp(At-4,0,FMath::Max(0,Show.Num()-10));}}
}
void AGkMatch::DrawCharacterSetup(UCanvas* C){
 const float Scale=C->SizeX/1280.f,Height=C->SizeY/Scale;float MX=-1,MY=-1;if(auto* PC=GetWorld()->GetFirstPlayerController())PC->GetMousePosition(MX,MY);MX/=Scale;MY/=Scale;LabHot=-1;
 const FLinearColor Ink(.018,.02,.028,.97),Paper(.92,.86,.72,1),Red(.85,.045,.025,1),Mute(.48,.51,.58,1),Panel(.045,.05,.065,.94);
 auto Box=[&](float X,float Y,float W,float H,FLinearColor Col){FCanvasTileItem I(FVector2D(X*Scale,Y*Scale),GWhiteTexture,FVector2D(W*Scale,H*Scale),Col);I.BlendMode=SE_BLEND_Translucent;C->DrawItem(I);};
 auto Text=[&](FString Label,float X,float Y,float Size,FLinearColor Col){
  if(!HudFont)HudFont=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto"));
  FCanvasTextItem I(FVector2D(X*Scale,Y*Scale),FText::FromString(Label),HudFont.Get(),Col);
  static auto Face=MakeShared<FCompositeFont>(FName(TEXT("Setup")),FPaths::ProjectContentDir()/TEXT("GkUI/Fonts/BarlowCondensed/BarlowCondensed-SemiBold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  I.SlateFontInfo=FSlateFontInfo(Face,Size*Scale);C->DrawItem(I);
 };
 auto Button=[&](int Id,float X,float Y,float W,float H,FString Label,bool On=false){bool Hover=MX>=X&&MX<X+W&&MY>=Y&&MY<Y+H;if(Hover)LabHot=Id;Box(X,Y,W,H,On?Red:Hover?FLinearColor(.2,.23,.3,1):Panel);Text(Label,X+9,Y+6,16,Paper);};
 Box(0,0,1280,76,Ink);Text(TEXT("ROBOT 1 / CHARACTER WORKSHOP"),24,12,28,Paper);Text(TEXT("CHOOSE A MOVE  >  AUDITION  >  TEST  >  APPLY"),24,48,14,Mute);
 Button(1,1170,20,90,36,TEXT("EXIT"));
 Box(0,76,282,Height-76,Ink);Text(TEXT("01 / MOVE SLOT"),18,86,18,Paper);
 for(int i=0;i<9;++i)Button(100+i,16,116+i*30,250,27,Categories[i],LabCat==i);
 TArray<int> Slots;for(int i=0;i<LabSlots.Num();++i)if(LabSlots[i].Cat==LabCat)Slots.Add(i);
 int Rows=FMath::Max(1,int((Height-440)/30));
 SetupSlotScroll=FMath::Clamp(SetupSlotScroll,0,FMath::Max(0,Slots.Num()-Rows));
 for(int j=0;j<Rows && j+SetupSlotScroll<Slots.Num();++j){int Id=Slots[j+SetupSlotScroll];Button(1000+Id,16,400+j*30,250,28,LabSlots[Id].Label.Left(33),LabSlot==Id);}
 // Explicit paging for categories containing many moves.
 if(Slots.Num()>Rows){Button(15,16,Height-32,120,28,TEXT("PREVIOUS"));Button(16,146,Height-32,120,28,TEXT("NEXT"));}
 Box(850,76,430,Height-76,Ink);Text(TEXT("02 / ANIMATION LIBRARY"),866,86,18,Paper);
 Button(7,866,116,346,34,bSetupSearch?SetupSearch+TEXT("|"):SetupSearch.IsEmpty()?TEXT("SEARCH NAME OR PACK..."):SetupSearch);Button(8,1218,116,44,34,TEXT("X"));
 Button(9,866,158,396,32,FString(TEXT("PACK  /  "))+Packs[SetupPack]);Button(10,866,197,256,32,Filters[SetupFilter+1]);Button(17,1128,197,134,32,bSetupVariants?TEXT("ALL VARIANTS"):TEXT("BASE CLIPS"));
 TArray<int> Show;for(int i=0;i<LabList.Num();++i)if(SetupMatches(LabList[i]))Show.Add(i);
 const int ListRows=FMath::Max(1,int((Height-280)/43));LabScroll=FMath::Clamp(LabScroll,0,FMath::Max(0,Show.Num()-ListRows));
 for(int r=0;r<ListRows && r+LabScroll<Show.Num();++r){int Id=Show[r+LabScroll];const auto& Clip=LabList[Id];bool Chosen=LabTry.Anim && Clip.Path==LabTry.Anim->GetPathName();
  Button(10000+Id,866,240+r*43,396,40,Clip.Label.Left(47),Chosen);Text(Clip.Pack,876,264+r*43,11,Chosen?Paper:Mute);
 }
 Text(FString::Printf(TEXT("%d CLIPS  /  SCROLL OR ARROW KEYS TO BROWSE"),Show.Num()),866,Height-27,13,Mute);
 if(Show.IsEmpty())Text(TEXT("No matches. Clear search or change filters."),876,260,16,Paper);
 if(!LabSlots.IsValidIndex(LabSlot))return;
 const FName Slot=LabSlots[LabSlot].Slot;const auto Saved=Clips[LabSet].FindRef(Slot);const bool Dirty=!Same(LabTry,Saved);
 Text(LabSlots[LabSlot].Label,304,94,25,Paper);Text(Dirty?TEXT("UNSAVED PREVIEW"):TEXT("SAVED ASSIGNMENT"),304,128,14,Dirty?Red:Mute);
 const float Bottom=Height-206;Box(294,Bottom,544,198,Ink);
 FString Name=LabTry.Anim?LabTry.Anim->GetName():TEXT("No animation / fallback pose");Text(Name.Left(58),306,Bottom+8,16,Paper);
 Text(FString::Printf(TEXT("%.2fs  /  %.2fs     %.2fx"),LabClock,LabTry.Anim?LabTry.Anim->GetPlayLength():0,SetupSpeed),306,Bottom+32,14,Mute);
 Button(5,600,Bottom+28,98,30,bSetupPaused?TEXT("PLAY"):TEXT("PAUSE"));Button(6,704,Bottom+28,120,30,TEXT("SPEED"));
 const TCHAR* Labels[]={TEXT("IN"),TEXT("OUT"),TEXT("CONTACT")};const float Values[]={LabTry.From,LabTry.To,LabTry.Impact};
 for(int i=0;i<3;++i){float X=306+i*174;Text(FString::Printf(TEXT("%s %s"),Labels[i],i==2&&Values[i]<0?TEXT("AUTO"):*FString::Printf(TEXT("%.3f"),Values[i])),X,Bottom+67,14,Paper);Button(20+i*2,X,Bottom+89,48,28,TEXT("-"));Button(21+i*2,X+54,Bottom+89,48,28,TEXT("+"));}
 Button(11,306,Bottom+125,104,28,LabTry.bLoop?TEXT("LOOP ON"):TEXT("LOOP OFF"));Button(12,416,Bottom+125,120,28,TEXT("AUTO CONTACT"));Button(13,600,Bottom+125,108,28,TEXT("FRAME -"));Button(14,714,Bottom+125,110,28,TEXT("FRAME +"));
 Button(3,306,Bottom+160,168,32,TEXT("F5  TEST MOVE"),true);Button(2,480,Bottom+160,168,32,TEXT("F6  APPLY"),Dirty);Button(4,654,Bottom+160,170,32,TEXT("REVERT PREVIEW"));
 if(LabNoteTimer>0)Text(LabNote.Left(62),304,160,14,Paper);
}
