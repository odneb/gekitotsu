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
 LabClock=0;LabFrame=0;LabScroll=0;bSetupSearch=false;SetupDropdown=0;
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
 if(auto* PC=GetWorld()->GetFirstPlayerController()){PC->bShowMouseCursor=true;FInputModeGameAndUI In;In.SetHideCursorDuringCapture(false);In.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(In);}
 if(Slot.ToString().StartsWith(TEXT("intro"))){Mode=0;Restart();}
 CamD.bSnap=true;
}
void AGkMatch::SetupReturn(){
 if(LabSlots.IsValidIndex(LabSlot))Clips[LabSet].Add(LabSlots[LabSlot].Slot,SetupOriginal);
 bEntrance=false;bSetupTesting=false;Mode=SetupOldMode;Dummy=SetupOldDummy;Screen=4;bMenu=false;Director.Reset();LabClock=0;
 if(auto* PC=GetWorld()->GetFirstPlayerController()){PC->bShowMouseCursor=true;FInputModeGameAndUI In;In.SetHideCursorDuringCapture(false);In.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(In);}
}
void AGkMatch::SetupAction(int32 Id){
 if(Id==30){SetupDropdown=SetupDropdown==1?0:1;SetupSlotScroll=FMath::Max(0,LabSlot-3);return;}
 if(Id==31){bSetupAdvanced=!bSetupAdvanced;return;}
 if(Id==32){SetupDropdown=0;SetupReturn();return;}
 if(Id==33){SetupReturn();SetupTest();return;}
 if(Id>=200 && Id<210){SetupPack=Id-200;SetupDropdown=0;LabScroll=0;return;}
 if(Id==1){LabClose();return;}if(Id==2){SetupApply();return;}if(Id==3){SetupTest();return;}
 if(Id==4){SetupDrafts.Remove(LabSlots[LabSlot].Slot);LabTryAt=-2;SetupSelect(LabSlot);return;}
 if(Id==5){bSetupPaused=!bSetupPaused;return;}if(Id==6){SetupSpeed=SetupSpeed==1.f?.5f:SetupSpeed==.5f?.25f:1.f;return;}
 if(Id==7){bSetupSearch=true;return;}if(Id==8){SetupSearch.Reset();LabScroll=0;return;}
 if(Id==9){SetupDropdown=SetupDropdown==2?0:2;return;}
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
 if(Hit(EKeys::Escape)){if(SetupDropdown){SetupDropdown=0;return;}LabClose();return;}if(Hit(EKeys::F5)){SetupTest();return;}if(Hit(EKeys::F6)){LabClose();return;}
 if(Hit(EKeys::SpaceBar))bSetupPaused=!bSetupPaused;
 if(SetupDropdown==1){if(Hit(EKeys::MouseScrollDown))SetupSlotScroll+=3;if(Hit(EKeys::MouseScrollUp))SetupSlotScroll=FMath::Max(0,SetupSlotScroll-3);return;}
 if(Hit(EKeys::MouseScrollDown))LabScroll+=3;if(Hit(EKeys::MouseScrollUp))LabScroll=FMath::Max(0,LabScroll-3);
 if(Hit(EKeys::Tab))SetupAction(100+(LabCat+1)%9);
 TArray<int> Show;for(int i=0;i<LabList.Num();++i)if(SetupMatches(LabList[i]))Show.Add(i);
 if(Hit(EKeys::Down)||Hit(EKeys::Up)){int At=Show.Find(LabTryAt);At=FMath::Clamp(At+(Hit(EKeys::Down)?1:-1),0,FMath::Max(0,Show.Num()-1));if(Show.IsValidIndex(At)){SetupAction(10000+Show[At]);LabScroll=FMath::Clamp(At-4,0,FMath::Max(0,Show.Num()-10));}}
}
// The editor is deliberately quieter than the game: one assignment, one preview, one test.
void AGkMatch::DrawCharacterSetup(UCanvas* C){
 const float Scale=C->SizeX/1280.f,Height=C->SizeY/Scale;float MX=-1,MY=-1;if(auto* PC=GetWorld()->GetFirstPlayerController())PC->GetMousePosition(MX,MY);MX/=Scale;MY/=Scale;LabHot=-1;
 const FLinearColor Ink(.025,.029,.038,1),Paper(.94,.95,.98,1),Blue(.08,.36,.88,1),Mute(.57,.62,.7,1),Panel(.075,.085,.105,1);
 auto Box=[&](float X,float Y,float W,float H,FLinearColor Col){FCanvasTileItem I(FVector2D(X*Scale,Y*Scale),GWhiteTexture,FVector2D(W*Scale,H*Scale),Col);I.BlendMode=SE_BLEND_Translucent;C->DrawItem(I);};
 auto Text=[&](FString Label,float X,float Y,float Size,FLinearColor Col){
  if(!HudFont)HudFont=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto"));
  FCanvasTextItem I(FVector2D(X*Scale,Y*Scale),FText::FromString(Label),HudFont.Get(),Col);
  static auto Face=MakeShared<FCompositeFont>(FName(TEXT("SetupClean")),FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);I.SlateFontInfo=FSlateFontInfo(Face,Size*Scale);C->DrawItem(I);
 };
 auto Button=[&](int Id,float X,float Y,float W,float H,FString Label,bool On=false){bool Hover=MX>=X&&MX<X+W&&MY>=Y&&MY<Y+H;if(Hover)LabHot=Id;Box(X,Y,W,H,On?Blue:Hover?FLinearColor(.17,.2,.26,1):Panel);Text(Label,X+12,Y+9,15,Paper);};
 Box(0,0,1280,148,Ink);Text(TEXT("Animation setup"),24,16,25,Paper);Text(TEXT("Choose a move, preview an animation, then try it in the game."),24,51,14,Mute);
 Button(1,1120,20,136,38,TEXT("Done   F6"));
 Text(TEXT("Move or state"),24,84,13,Mute);
 Button(30,24,106,700,36,LabSlots.IsValidIndex(LabSlot)?LabSlots[LabSlot].Label+TEXT("   v"):TEXT("Choose a move   v"));
 Box(752,76,528,Height-76,Ink);Text(TEXT("Choose animation"),772,85,19,Paper);
 Button(7,772,119,428,36,bSetupSearch?SetupSearch+TEXT("|"):SetupSearch.IsEmpty()?TEXT("Search animations..."):SetupSearch);Button(8,1208,119,48,36,TEXT("X"));
 Button(9,772,165,484,36,FString(TEXT("Pack: "))+Packs[SetupPack]+TEXT("   v"));
 TArray<int> Show;for(int i=0;i<LabList.Num();++i)if(SetupMatches(LabList[i]))Show.Add(i);
 const int ListRows=FMath::Max(1,int((Height-270)/51));LabScroll=FMath::Clamp(LabScroll,0,FMath::Max(0,Show.Num()-ListRows));
 for(int r=0;r<ListRows && r+LabScroll<Show.Num();++r){int Id=Show[r+LabScroll];const auto& Clip=LabList[Id];bool Chosen=LabTry.Anim && Clip.Path==LabTry.Anim->GetPathName();
  Button(10000+Id,772,212+r*51,484,47,Clip.Label.Left(44),Chosen);Text(Clip.Pack,784,241+r*51,11,Chosen?Paper:Mute);
 }
 Text(FString::Printf(TEXT("%d animations  •  Scroll to browse"),Show.Num()),772,Height-43,13,Mute);
 if(Show.IsEmpty())Text(TEXT("No matches. Try another search or pack."),784,245,14,Paper);
 if(!LabSlots.IsValidIndex(LabSlot))return;
 const auto Saved=Clips[LabSet].FindRef(LabSlots[LabSlot].Slot);const bool Dirty=!Same(LabTry,Saved);
 const float Bottom=Height-(bSetupAdvanced?290:156);Box(0,Bottom,752,Height-Bottom,Ink);
 FString Name=LabTry.Anim?LabTry.Anim->GetName():TEXT("Default pose");for(const auto& Clip:LabList)if(LabTry.Anim && Clip.Path==LabTry.Anim->GetPathName()){Name=Clip.Label;break;}
 Text(Name.Left(48),24,Bottom+13,17,Paper);Text(Dirty?TEXT("Preview — not saved yet"):TEXT("Saved for this move"),24,Bottom+40,13,Dirty?FLinearColor(1,.7,.3):Mute);
 Button(5,536,Bottom+12,88,36,bSetupPaused?TEXT("Play"):TEXT("Pause"));Button(6,634,Bottom+12,90,36,FString::Printf(TEXT("%.2fx"),SetupSpeed));
 Button(31,24,Bottom+67,210,32,bSetupAdvanced?TEXT("Hide timing options ^"):TEXT("Timing options v"));
 if(bSetupAdvanced){
  const TCHAR* Labels[]={TEXT("Start"),TEXT("End"),TEXT("Contact")};const float Values[]={LabTry.From,LabTry.To,LabTry.Impact};
  for(int i=0;i<3;++i){float X=24+i*236;Text(FString::Printf(TEXT("%s: %s"),Labels[i],i==2&&Values[i]<0?TEXT("automatic"):*FString::Printf(TEXT("%.0f%%"),Values[i]*100)),X,Bottom+110,14,Paper);Button(20+i*2,X,Bottom+135,60,32,TEXT("−"));Button(21+i*2,X+70,Bottom+135,60,32,TEXT("+"));}
  Button(11,24,Bottom+178,125,32,LabTry.bLoop?TEXT("Loop: on"):TEXT("Loop: off"));Button(12,159,Bottom+178,157,32,TEXT("Auto contact"));Button(13,326,Bottom+178,120,32,TEXT("Frame −"));Button(14,456,Bottom+178,120,32,TEXT("Frame +"));Button(17,586,Bottom+178,138,32,bSetupVariants?TEXT("All variants"):TEXT("Base clips"));
 }
 Button(3,24,Height-48,220,38,TEXT("Test move   F5"),true);Button(2,254,Height-48,220,38,TEXT("Save animation"),Dirty);Button(4,484,Height-48,240,38,TEXT("Undo preview"));
 // Popovers render last, so they capture clicks over the list underneath.
 if(SetupDropdown==1){
  const int Rows=FMath::Max(1,FMath::Min(10,int((Height-190)/45)));SetupSlotScroll=FMath::Clamp(SetupSlotScroll,0,FMath::Max(0,LabSlots.Num()-Rows));
  Box(24,146,700,Rows*45+33,Ink);
  for(int j=0;j<Rows && j+SetupSlotScroll<LabSlots.Num();++j){int Id=j+SetupSlotScroll;Button(1000+Id,28,150+j*45,692,43,LabSlots[Id].Label,LabSlot==Id);Text(Categories[LabSlots[Id].Cat],40,178+j*45,10,Mute);}
  Text(TEXT("Scroll to see every move and state"),40,154+Rows*45,12,Mute);
 }else if(SetupDropdown==2){Box(768,203,492,UE_ARRAY_COUNT(Packs)*35+8,Ink);for(int i=0;i<UE_ARRAY_COUNT(Packs);++i)Button(200+i,772,207+i*35,484,33,Packs[i],SetupPack==i);}
}
void AGkMatch::DrawSetupTest(UCanvas* C){
 const float Scale=C->SizeX/1280.f;float MX=-1,MY=-1;if(auto* PC=GetWorld()->GetFirstPlayerController())PC->GetMousePosition(MX,MY);MX/=Scale;MY/=Scale;LabHot=-1;
 auto Button=[&](int Id,float X,float W,const FString& Label){const bool Hover=MX>=X&&MX<X+W&&MY>=86&&MY<132;if(Hover)LabHot=Id;
  FCanvasTileItem B(FVector2D(X*Scale,86*Scale),GWhiteTexture,FVector2D(W*Scale,46*Scale),Hover?FLinearColor(.16,.4,.8,1):FLinearColor(.03,.045,.07,.97));B.BlendMode=SE_BLEND_Translucent;C->DrawItem(B);
  FCanvasTextItem T(FVector2D((X+14)*Scale,99*Scale),FText::FromString(Label),HudFont.Get(),FLinearColor::White);static auto Face=MakeShared<FCompositeFont>(FName(TEXT("SetupTest")),FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);T.SlateFontInfo=FSlateFontInfo(Face,16*Scale);C->DrawItem(T);
 };
 Button(32,20,350,TEXT("Back to animations   Esc"));Button(33,382,190,TEXT("Test again   F5"));
}
