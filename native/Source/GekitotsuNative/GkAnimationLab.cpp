#include "GkMatch.h"
#include "Animation/AnimSequence.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace
{
using FRow = TSharedPtr<int32>;
const TCHAR* Groups[] = {TEXT("Movement"), TEXT("Ground normals"), TEXT("Aerials"), TEXT("Specials"), TEXT("Defense & Drive"), TEXT("Throws"), TEXT("Supers"), TEXT("Reactions")};
const FLinearColor Colors[] = {{.35f,.7f,1.f},{.9f,.69f,.3f},{.42f,.83f,.76f},{.73f,.57f,1.f},{1.f,.52f,.42f},{.93f,.56f,.76f},{1.f,.72f,.36f},{.70f,.76f,.86f}};

struct FMotionFilter
{
 const TCHAR* Name;
 const TCHAR* Keywords;
};
const FMotionFilter MotionFilters[] = {
 {TEXT("Idle"), TEXT("idle|stance|breathing|ready pose")},
 {TEXT("Walk"), TEXT("walk|stroll|strafe")},
 {TEXT("Run"), TEXT("run|sprint|jog")},
 {TEXT("Dash"), TEXT("dash|dodge|evade|step forward|step back")},
 {TEXT("Move"), TEXT("walk|stroll|strafe|run|sprint|jog|dash|dodge|evade|locomotion|move_|move forward|move back|fly|flight|jump|hop|hover|float")},
 {TEXT("Jump"), TEXT("jump|hop|leap")},
 {TEXT("Fly"), TEXT("fly|flight|air dash|homing")},
 {TEXT("Float / Hover"), TEXT("float|hover|levitat|air idle|fly idle|flying idle")},
 {TEXT("Rise"), TEXT("rise|rising|takeoff|lift off|lift up|ascend|uppercut|shoryuken|kick_up|kick up")},
 {TEXT("Fall"), TEXT("fall|descend|knockdown|knock down|tumble|drop|spike|stomp")},
 {TEXT("Land"), TEXT("land|touchdown")},
 {TEXT("Get up"), TEXT("getup|get up|standup|stand up|recover")},
 {TEXT("Punch"), TEXT("punch|jab|hook|uppercut|shoryuken|fist|elbow")},
 {TEXT("Kick / Knee"), TEXT("kick|knee|stomp|axe kick")},
 {TEXT("Combo"), TEXT("combo|multi hit|multihit|flurry|barrage")},
 {TEXT("Aerial"), TEXT("aerial|airborne|air |jump|fly|flight|hover|float")},
 {TEXT("Reaction"), TEXT("reaction|stagger|recoil|hurt|injured|receiver|hit_stun|hitstun|knockout|knockdown|splat")},
 {TEXT("Guard / Parry"), TEXT("block|guard|parry|defense|defence")},
 {TEXT("Grab / Throw"), TEXT("grab|throw|grapple|hold victim|takedown")},
 {TEXT("Energy / Charge"), TEXT("energy|beam|fireball|charge|power up|powerup|release")},
 {TEXT("Taunt / Pose"), TEXT("taunt|emote|victory|showcase|celebrat|pose")}
};

bool HasMotion(const FString& Text, const FMotionFilter& Filter)
{
 TArray<FString> Keywords;
 FString(Filter.Keywords).ParseIntoArray(Keywords,TEXT("|"));
 return Keywords.ContainsByPredicate([&Text](const FString& Word){return Text.Contains(Word);});
}

FString TagsFile() {return FPaths::ProjectSavedDir()/TEXT("AnimationLabTags.json");}

int32 GroupFor(const AGkMatch::FLabSlot& Slot)
{
 if (Slot.Slot == TEXT("mv_DI") || Slot.Slot == TEXT("mv_DREV") || Slot.Slot == TEXT("drush") || Slot.Cat == 1) return 4;
 switch (Slot.Cat)
 {
  case 0: return 0;
  case 2: return 7;
  case 3: case 4: return 1;
  case 5: return 2;
  case 7: return 6;
  case 8: return 5;
  default: return 3;
 }
}

FString MoveTitle(const AGkMatch::FLabSlot& Slot)
{
 if (Slot.Slot == TEXT("mv_DI")) return TEXT("Drive Impact");
 if (Slot.Slot == TEXT("mv_SRK_M")) return TEXT("Shoryuken  ·  Forward + Special");
 if (Slot.Slot == TEXT("mv_SRK_OD")) return TEXT("Shoryuken  ·  Overdrive");
 if (Slot.Slot == TEXT("mv_SRK_L")) return TEXT("Shoryuken  ·  Light");
 if (Slot.Slot == TEXT("mv_SRK_H")) return TEXT("Shoryuken  ·  Heavy");
 if (Slot.Slot == TEXT("mv_AUP")) return TEXT("Riptide  ·  Air Forward + Special");
 if (Slot.Slot == TEXT("mv_ABEAM")) return TEXT("Hot Shot  ·  Air Special");
 return Slot.Label;
}

bool Search(const FString& Text, const FString& Query)
{
 TArray<FString> Terms;
 Query.ParseIntoArrayWS(Terms);
 for (const FString& Term : Terms) if (!Text.Contains(Term)) return false;
 return true;
}

TSharedRef<STextBlock> Label(const FString& Text, int32 Size=10, FLinearColor Tint=FLinearColor(.8f,.84f,.9f))
{
 FSlateFontInfo Font=FAppStyle::GetFontStyle("NormalFont");
 Font.Size=Size;
 return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font).ColorAndOpacity(Tint);
}
FSlateFontInfo LabFont(float Size)
{
 FSlateFontInfo Font=FAppStyle::GetFontStyle("NormalFont");
 Font.Size=Size;
 return Font;
}

TSharedRef<SButton> Button(const TCHAR* Text, FOnClicked Click)
{
 return SNew(SButton).Text(FText::FromString(Text)).OnClicked(Click);
}

class SGkThumb : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SGkThumb) {}
  SLATE_ARGUMENT(TWeakObjectPtr<AGkMatch>, Match)
  SLATE_ARGUMENT(FString, Path)
  SLATE_ARGUMENT(float, Position)
 SLATE_END_ARGS()

 void Construct(const FArguments& Args)
 {
  Match = Args._Match; Path = Args._Path; Position = Args._Position;
  Brush.ImageSize = FVector2D(70, 56);
  ChildSlot [SNew(SBox).WidthOverride(70).HeightOverride(56)[SNew(SImage).Image(&Brush)]];
 }

 void Tick(const FGeometry&, double Time, float) override
 {
  if (!Match.IsValid() || Path.IsEmpty()) return;
  const bool bHover = IsHovered();
  if (Target.IsValid() && !bHover && !bWasHovered) return;
  if (Time < NextCapture || Time < SharedCaptureTime) return;
  SharedCaptureTime = Time + .045;
  NextCapture = Time + .14;
  const float Sample = bHover ? float(FMath::Fmod(Time, 1.3) / 1.3) : Position;
  Target = Match->CaptureSetupThumb(Path, Sample, Target.Get());
  Brush.SetResourceObject(Target.Get());
  bWasHovered = bHover;
 }
 ~SGkThumb()
 {
  if(Match.IsValid() && Target.IsValid())Match->SetupThumbTargets.RemoveSingleSwap(Target.Get());
 }
private:
 inline static double SharedCaptureTime = 0;
 TWeakObjectPtr<AGkMatch> Match;
 TWeakObjectPtr<UTextureRenderTarget2D> Target;
 FString Path;
 FSlateBrush Brush;
 float Position = .48f;
 double NextCapture = 0;
 bool bWasHovered = false;
};

class SGkAnimationLab : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SGkAnimationLab) {}
  SLATE_ARGUMENT(AGkMatch*, Match)
 SLATE_END_ARGS()

 void Construct(const FArguments& Args)
 {
  Match = Args._Match;
  TArray<FString> Packs;
  for(const auto& Clip:Args._Match->LabList) Packs.AddUnique(Clip.Pack);
  Packs.Sort();
  PackOptions.Add(MakeShared<FString>(TEXT("All packs")));
  for(const FString& Pack:Packs) PackOptions.Add(MakeShared<FString>(Pack));
  MotionOptions.Add(MakeShared<FString>(TEXT("All motions")));
  MotionOptions.Add(MakeShared<FString>(TEXT("Match selected move")));
  MotionOptions.Add(MakeShared<FString>(TEXT("Match left category")));
  for(const auto& Motion:MotionFilters) MotionOptions.Add(MakeShared<FString>(Motion.Name));
  for (const TCHAR* Outcome : {TEXT("Hit"), TEXT("Block"), TEXT("Whiff"), TEXT("Wall splat")}) Outcomes.Add(MakeShared<FString>(Outcome));
  const float MoveOffset=Args._Match->SetupMoveOffset,ClipOffset=Args._Match->SetupClipOffset,MapOffset=Args._Match->SetupMapOffset;
  SAssignNew(MoveList, SListView<FRow>)
   .ListItemsSource(&Moves).SelectionMode(ESelectionMode::Single)
   .OnGenerateRow(this, &SGkAnimationLab::MoveRow).OnListViewScrolled_Lambda([this](double Offset){if(Match.IsValid())Match->SetupMoveOffset=float(Offset);})
   .OnSelectionChanged_Lambda([this](FRow Row, ESelectInfo::Type Type){ if (Row && Type != ESelectInfo::Direct) Select(*Row); });
  SAssignNew(ClipList, SListView<FRow>)
   .ListItemsSource(&Clips).SelectionMode(ESelectionMode::Single)
   .OnGenerateRow(this, &SGkAnimationLab::ClipRow).OnListViewScrolled_Lambda([this](double Offset){if(Match.IsValid())Match->SetupClipOffset=float(Offset);})
   .OnSelectionChanged_Lambda([this](FRow Row,ESelectInfo::Type Type){ if(Row && Type!=ESelectInfo::Direct && Match.IsValid()){Match->SetupAction(10000+*Row);RefreshTags();} });
  auto Categories = SNew(SUniformGridPanel).SlotPadding(FMargin(1));
  Categories->AddSlot(0,0)[Button(TEXT("All"), FOnClicked::CreateLambda([this]{ return FilterGroup(-1); }))];
  for (int32 Index=0; Index<UE_ARRAY_COUNT(Groups); ++Index)
   Categories->AddSlot((Index+1)%3,(Index+1)/3)[Button(Groups[Index], FOnClicked::CreateLambda([this,Index]{return FilterGroup(Index);}))];

  ChildSlot
  [
   SNew(SBorder).Padding(FMargin(8,8,8,45)).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.033f,.048f,.35f))
   [
    SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight().Padding(4,3)
    [
     SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,16,0)[Label(TEXT("Animation Lab"),18)]
     +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(TEXT("Robot 1"))]
     +SHorizontalBox::Slot().AutoWidth().Padding(3,0)[Button(TEXT("Move map"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get()){M->bSetupMap=!M->bSetupMap; RefreshMap();}return FReply::Handled();}))]
     +SHorizontalBox::Slot().AutoWidth().Padding(3,0)[Button(TEXT("Done  F6"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->LabClose();return FReply::Handled();}))]
    ]
    +SVerticalBox::Slot().FillHeight(1)
    [
     SNew(SSplitter)
     +SSplitter::Slot().Value(.32f)
     [
      SNew(SBorder).Padding(8)
      [
       SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[Label(TEXT("Moves & states"),13)]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)
       [
        SAssignNew(MoveSearch, SSearchBox).InitialText(FText::FromString(Args._Match->SetupMoveSearch))
        .HintText(FText::FromString(TEXT("Search moves, inputs, or aliases")))
        .OnTextChanged_Lambda([this](const FText& T){
         if(auto* M=Match.Get())
         {
          M->SetupMoveSearch=T.ToString();
          if(!M->SetupMoveSearch.IsEmpty())M->SetupGroup=-1;
          RefreshMoves();
          if(M->SetupLibraryMotion==TEXT("Match left category"))RefreshClips();
         }
        })
        .OnTextCommitted_Lambda([this](const FText&,ETextCommit::Type Type){if(Type==ETextCommit::OnEnter && Moves.Num()){Select(*Moves[0]);FocusLab();}})
       ]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)[Label(TEXT("Categories"),9)]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
       [
        Categories
       ]
       +SVerticalBox::Slot().AutoHeight().Padding(0,1,0,5)
       [
        SNew(STextBlock).Text_Lambda([this]{const auto* M=Match.Get();return FText::FromString(M&&M->SetupGroup>=0?FString(Groups[M->SetupGroup]):TEXT("All categories"));})
       ]
       +SVerticalBox::Slot().FillHeight(1)[MoveList.ToSharedRef()]
      ]
     ]
     +SSplitter::Slot().Value(.40f)
     [
      SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[BuildInspector()]
      +SVerticalBox::Slot().FillHeight(1)
      [
       SNew(SOverlay)
       +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(5)
       [SNew(STextBlock).Text(FText::FromString(TEXT("Robot 1  ·  Live preview"))).Font(LabFont(10))]
       +SOverlay::Slot()
       [
        SAssignNew(MapScroll,SScrollBox)
       .Visibility_Lambda([this]{return Match.IsValid()&&Match->bSetupMap?EVisibility::Visible:EVisibility::Collapsed;})
       .OnUserScrolled_Lambda([this](float Offset){if(Match.IsValid())Match->SetupMapOffset=Offset;})
       ]
      ]
      +SVerticalBox::Slot().AutoHeight()[BuildPlayback()]
     ]
     +SSplitter::Slot().Value(.28f)
     [
      SNew(SBorder).Padding(8)
      [
       SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[Label(TEXT("Clip library"),13)]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)
       [
        SAssignNew(ClipSearch,SSearchBox).InitialText(FText::FromString(Args._Match->SetupSearch))
        .HintText(FText::FromString(TEXT("Try: air knee, stagger, uppercut")))
        .OnTextChanged_Lambda([this](const FText& T){if(auto* M=Match.Get()){M->SetupSearch=T.ToString(); RefreshClips();}})
       ]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,3)[Label(TEXT("Filter by"),9)]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
       [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[Label(TEXT("Pack"),9)]
        +SHorizontalBox::Slot().FillWidth(1)[Choice(PackOptions,
         OptionIndex(PackOptions,Args._Match->SetupLibraryPack),
         [this](int32 I){if(auto* M=Match.Get()){M->SetupPack=0;M->SetupLibraryPack=I?*PackOptions[I]:FString();RefreshClips();}},
         [this]{return Match.IsValid()?OptionIndex(PackOptions,Match->SetupLibraryPack):0;})]
       ]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
       [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[Label(TEXT("Motion"),9)]
        +SHorizontalBox::Slot().FillWidth(1)[Choice(MotionOptions,
         OptionIndex(MotionOptions,Args._Match->SetupLibraryMotion.ToString()),
         [this](int32 I){if(auto* M=Match.Get()){M->SetupLibraryMotion=I?FName(**MotionOptions[I]):NAME_None;RefreshClips();}},
         [this]{return Match.IsValid()?OptionIndex(MotionOptions,Match->SetupLibraryMotion.ToString()):0;})]
       ]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
       [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)
        [SNew(SCheckBox).IsChecked_Lambda([this]{return Match.IsValid()&&Match->bSetupAssignedOnly?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
         .OnCheckStateChanged_Lambda([this](ECheckBoxState State){if(auto* M=Match.Get()){M->bSetupAssignedOnly=State==ECheckBoxState::Checked;RefreshClips();}})
         [Label(TEXT("Assigned only"),9)]]
        +SHorizontalBox::Slot().AutoWidth()
        [SNew(SCheckBox).IsChecked_Lambda([this]{return Match.IsValid()&&Match->bSetupVariants?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
         .OnCheckStateChanged_Lambda([this](ECheckBoxState State){if(auto* M=Match.Get()){M->bSetupVariants=State==ECheckBoxState::Checked;RefreshClips();}})
         [Label(TEXT("Include variants"),9)]]
       ]
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)
       [
        SNew(STextBlock).Text_Lambda([this]{
         const auto* M=Match.Get();
         const bool bFollow=M&&(M->SetupLibraryMotion==TEXT("Match selected move") || M->SetupLibraryMotion==TEXT("Match left category"));
         const FName Motion=bFollow?M->SetupSuggestedMotion():NAME_None;
         return FText::FromString(Motion.IsNone()?FString::Printf(TEXT("%d matching clips"),Clips.Num()):
          FString::Printf(TEXT("%d matching clips  ·  %s"),Clips.Num(),*Motion.ToString()));
        })
       ]
       +SVerticalBox::Slot().FillHeight(1)[ClipList.ToSharedRef()]
       +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
       [SAssignNew(TagsField,SEditableTextBox).HintText(FText::FromString(TEXT("Add tags for the selected clip")))
        .OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type Type){
         if(auto* M=Match.Get();M && M->LabTry.Anim && (Type==ETextCommit::OnEnter || Type==ETextCommit::OnUserMovedFocus))
         {
          M->SetSetupTags(M->LabTry.Anim->GetPathName(),T.ToString());
          RefreshClips();
         }
        })]
      ]
     ]
    ]
   ]
  ];
  RefreshMoves();
  RefreshClips();
  MoveList->SetScrollOffset(MoveOffset); ClipList->SetScrollOffset(ClipOffset); MapScroll->SetScrollOffset(MapOffset);
  RefreshTags();
 }

 bool SupportsKeyboardFocus() const override {return true;}

 FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
 {
  auto* M=Match.Get();
  if(!M) return FReply::Unhandled();
  const FKey Key=Event.GetKey();
  if(Key==EKeys::Escape)
  {
   if(MoveSearch->HasKeyboardFocus() || MoveSearch->HasFocusedDescendants()
    || ClipSearch->HasKeyboardFocus() || ClipSearch->HasFocusedDescendants()
    || (TagsField && (TagsField->HasKeyboardFocus() || TagsField->HasFocusedDescendants())))
   {
    FocusLab();return FReply::Handled();
   }
   M->LabClose();return FReply::Handled();
  }
  if(Key==EKeys::F6){M->LabClose();return FReply::Handled();}
  if(Key==EKeys::F5){M->SetupTest();return FReply::Handled();}
  if(Key==EKeys::F && (Event.IsCommandDown() || Event.IsControlDown()))
   return FReply::Handled().SetUserFocus(MoveSearch.ToSharedRef(), EFocusCause::SetDirectly);
  return FReply::Unhandled();
 }
 FReply OnKeyDown(const FGeometry&,const FKeyEvent& Event) override
 {
  if(Event.GetKey()==EKeys::Enter && Match.IsValid()){Match->SetupTest();return FReply::Handled();}
  return FReply::Unhandled();
 }
private:
 void FocusLab() { FSlateApplication::Get().SetKeyboardFocus(SharedThis(this)); }

 static int32 OptionIndex(const TArray<TSharedPtr<FString>>& Options,const FString& Value)
 {
  return FMath::Max(0,Options.IndexOfByPredicate([&Value](const auto& Item){return *Item==Value;}));
 }
 TSharedRef<SWidget> Choice(TArray<TSharedPtr<FString>>& Options, int32 Index, TFunction<void(int32)> Changed,TFunction<int32()> Current)
 {
  auto* Source=&Options;
  const auto Selected=Options[FMath::Clamp(Index,0,Options.Num()-1)];
  return SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(Source).InitiallySelectedItem(Selected)
   .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){return Label(Item?*Item:FString());})
   .OnSelectionChanged_Lambda([Source,Changed](TSharedPtr<FString> Item,ESelectInfo::Type){const int32 Found=Source->Find(Item);if(Found>=0)Changed(Found);})
   [SNew(STextBlock).Text_Lambda([Source,Current]{return FText::FromString(*(*Source)[FMath::Clamp(Current(),0,Source->Num()-1)]);})];
 }
 FReply FilterGroup(int32 Group)
 {
  if(auto* M=Match.Get())
  {
   M->SetupGroup=Group;
   RefreshMoves();
   if(M->SetupLibraryMotion==TEXT("Match left category"))RefreshClips();
  }
  return FReply::Handled();
 }
 void Select(int32 Index)
 {
  if(auto* M=Match.Get()){
   if(M->LabSlot!=Index) M->SetupSelect(Index);
   for(const FRow& Row:Moves) if(*Row==Index){MoveList->SetSelection(Row,ESelectInfo::Direct);MoveList->RequestScrollIntoView(Row);break;}
   RefreshInspector();
   RefreshTags();
   if(M->SetupLibraryMotion==TEXT("Match selected move"))RefreshClips();
  }
 }
 void RefreshTags()
 {
  if(auto* M=Match.Get();TagsField && M)
   TagsField->SetText(FText::FromString(M->LabTry.Anim?M->SetupCustomTags.FindRef(M->LabTry.Anim->GetPathName()):FString()));
 }
 void RefreshMoves()
 {
  auto* M=Match.Get();if(!M)return;
  Moves.Reset();
  for(int32 Index=0;Index<M->LabSlots.Num();++Index)
  {
   const auto& Slot=M->LabSlots[Index];
   const int32 Group=GroupFor(Slot);
   if(M->SetupGroup>=0 && Group!=M->SetupGroup)continue;
   FString Aliases=MoveTitle(Slot)+TEXT(" ")+Slot.Label+TEXT(" ")+Groups[Group];
   if(Slot.Slot.ToString().StartsWith(TEXT("mv_SRK"))) Aliases+=TEXT(" shoryuken uppercut 623 dragon punch side special forward special anti air");
   if(Slot.Slot==TEXT("mv_AUP")) Aliases+=TEXT(" shoryuken rising uppercut air side special anti air");
   if(Slot.Slot==TEXT("mv_DI")) Aliases+=TEXT(" breach di armor paint impact hit block whiff");
   if(Search(Aliases,M->SetupMoveSearch))Moves.Add(MakeShared<int32>(Index));
  }
  Moves.StableSort([M](const FRow& A,const FRow& B){
   const auto& X=M->LabSlots[*A];const auto& Y=M->LabSlots[*B];
   auto Rank=[](const AGkMatch::FLabSlot& Slot){return Slot.Slot==TEXT("mv_SRK_M")?0:1;};
   return GroupFor(X)==GroupFor(Y)?Rank(X)<Rank(Y):GroupFor(X)<GroupFor(Y);
  });
  MoveList->RequestListRefresh();
  if(!M->SetupMoveSearch.IsEmpty())MoveList->ScrollToTop();
  for(const FRow& Row:Moves)if(*Row==M->LabSlot){MoveList->SetSelection(Row,ESelectInfo::Direct);break;}
  RefreshMap();
  RefreshInspector();
 }
 void RefreshClips()
 {
  auto* M=Match.Get();if(!M)return;
  Clips.Reset();
  for(int32 Index=0;Index<M->LabList.Num();++Index) if(M->SetupMatches(M->LabList[Index])) Clips.Add(MakeShared<int32>(Index));
  ClipList->RequestListRefresh();
  if(Clips.Num()) ClipList->ScrollToTop();
  if(M->LabTry.Anim) for(const FRow& Row:Clips) if(M->LabList[*Row].Path==M->LabTry.Anim->GetPathName()){ClipList->SetSelection(Row,ESelectInfo::Direct);break;}
 }
 TSharedRef<ITableRow> MoveRow(FRow Row,const TSharedRef<STableViewBase>& Owner)
 {
  const auto& Slot=Match->LabSlots[*Row];
  return SNew(STableRow<FRow>,Owner).Padding(FMargin(4,3))
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,7,0)[SNew(SBox).WidthOverride(3)[SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(Colors[GroupFor(Slot)])]]
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[Label(MoveTitle(Slot),11)]
     +SVerticalBox::Slot().AutoHeight()[Label(Groups[GroupFor(Slot)],8,FLinearColor(.57f,.64f,.73f))]]];
 }
 TSharedRef<ITableRow> ClipRow(FRow Row,const TSharedRef<STableViewBase>& Owner)
 {
  const auto& Clip=Match->LabList[*Row];
  return SNew(STableRow<FRow>,Owner).Padding(FMargin(3,3)).ToolTipText(FText::FromString(Match->SetupClipSearchText(Clip)))
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth()[SNew(SGkThumb).Match(Match).Path(Clip.Path).Position(.48f)]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(7,0)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Clip.Label)).AutoWrapText(true)]
   +SVerticalBox::Slot().AutoHeight()[Label(Clip.Pack+(Match->SetupClipIsAssigned(Clip)?TEXT("  ·  Assigned"):TEXT("")),8,FLinearColor(.57f,.64f,.73f))]]];
 }
 void RefreshMap()
 {
  if(!MapScroll||!Match.IsValid())return;
  MapScroll->ClearChildren();
  if(!Match->bSetupMap)return;
  for(int32 Group=0;Group<UE_ARRAY_COUNT(Groups);++Group)
  {
   auto Tiles=SNew(SUniformGridPanel).SlotPadding(FMargin(2));
   int32 Count=0;
   for(const FRow& Row:Moves)
   {
    const int32 Id=*Row;
    const auto& Slot=Match->LabSlots[Id];
    if(GroupFor(Slot)!=Group)continue;
    const auto* Saved=Match->Clips[Match->LabSet].Find(Slot.Slot);
    const FString Path=Saved&&Saved->Anim?Saved->Anim->GetPathName():FString();
    Tiles->AddSlot(Count%3,Count/3)
     [SNew(SButton).OnClicked_Lambda([this,Id]{Select(Id);return FReply::Handled();})
      [SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SGkThumb).Match(Match).Path(Path).Position(Saved&&Saved->Impact>=0?Saved->Impact:.48f)]
       +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(MoveTitle(Slot))).WrapTextAt(95).Font(LabFont(8)).ColorAndOpacity(Colors[Group])]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
       [Label(Match->SetupDrafts.Contains(Slot.Slot)?TEXT("Draft"):Path.IsEmpty()?TEXT("Default pose"):TEXT("Assigned"),7)]]];
    ++Count;
   }
   if(Count){MapScroll->AddSlot().Padding(5,4)[Label(Groups[Group],12,Colors[Group])];MapScroll->AddSlot()[Tiles];}
  }
 }
 TSharedRef<SWidget> BuildInspector()
 {
  return SNew(SBorder).Padding(8)
   [SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()
   [SNew(STextBlock).Font(LabFont(9)).Text_Lambda([this]{
    auto* M=Match.Get();
    return FText::FromString(M && M->LabNoteTimer>0?M->LabNote:FString());
   })]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(LabFont(14)).Text_Lambda([this]{
     const auto* M=Match.Get();return FText::FromString(M&&M->LabSlots.IsValidIndex(M->LabSlot)?MoveTitle(M->LabSlots[M->LabSlot]):TEXT("Select a move"));
    })]
    +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[SAssignNew(Links,SVerticalBox)]];
 }
 void RefreshInspector()
 {
  auto* M=Match.Get();if(!M||!Links||!M->LabSlots.IsValidIndex(M->LabSlot))return;
  Links->ClearChildren();
  const FName Slot=M->LabSlots[M->LabSlot].Slot;
  const FName Context=M->SetupTestContext;
  const FName Move=Context.IsNone()?Slot:Context;
  if(Move==TEXT("mv_DI"))
  {
   Links->AddSlot().AutoHeight()[Label(TEXT("Shared reactions  ·  edits apply wherever the reaction is used"),9)];
   auto Row=SNew(SHorizontalBox);
   const FName Targets[]={TEXT("mv_DI"),TEXT("ahit"),TEXT("blockS"),TEXT("splat")};
   const TCHAR* Actions[]={TEXT("Swing"),TEXT("Hit: launch"),TEXT("Block"),TEXT("Wall splat")};
   for(int32 K=0;K<4;++K)
   {
    Row->AddSlot().AutoWidth().Padding(1)
     [Button(Actions[K],FOnClicked::CreateLambda([this,Target=Targets[K],K]{
      if(auto* Owner=Match.Get())for(int32 I=0;I<Owner->LabSlots.Num();++I)if(Owner->LabSlots[I].Slot==Target)
      {
       Select(I); Owner->SetupTestContext=TEXT("mv_DI"); Owner->SetupOutcome=K==2?1:K==3?3:0;
       RefreshInspector(); break;
      }
      return FReply::Handled();
     }))];
   }
   Links->AddSlot().AutoHeight().Padding(0,4,0,0)[Row];
  }
  if(Slot.ToString().StartsWith(TEXT("pair")))
  {
   FString Partner=Slot.ToString();
   if(Partner.EndsWith(TEXT("Victim")))Partner.LeftChopInline(6);else Partner+=TEXT("Victim");
   Links->AddSlot().AutoHeight()[Button(TEXT("Edit linked performer / receiver"),FOnClicked::CreateLambda([this,Partner]{
    if(auto* Owner=Match.Get())for(int32 I=0;I<Owner->LabSlots.Num();++I)if(Owner->LabSlots[I].Slot==FName(*Partner)){Select(I);break;}
    return FReply::Handled();
   }))];
  }
 }
 TSharedRef<SWidget> BuildPlayback()
 {
  auto Controls=SNew(SHorizontalBox);
  Controls->AddSlot().AutoWidth().Padding(2)[Button(TEXT("Preview  ▶ / Ⅱ"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->bSetupPaused=!M->bSetupPaused;return FReply::Handled();}))];
  Controls->AddSlot().AutoWidth().Padding(2)[Button(TEXT("−1"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->SetupAction(13);return FReply::Handled();}))];
  Controls->AddSlot().AutoWidth().Padding(2)[Button(TEXT("+1"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->SetupAction(14);return FReply::Handled();}))];
  Controls->AddSlot().AutoWidth().Padding(2)[Button(TEXT("Speed"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->SetupAction(6);return FReply::Handled();}))];
  Controls->AddSlot().AutoWidth().Padding(2)[Choice(Outcomes,Match->SetupOutcome,[this](int32 I){if(auto* M=Match.Get())M->SetupOutcome=I;},[this]{return Match.IsValid()?Match->SetupOutcome:0;})];
  return SNew(SBorder).Padding(8)
   [SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{
    auto* M=Match.Get();if(!M)return FText::GetEmpty();
    const FString Name=M->LabTry.Anim?M->LabTry.Anim->GetName():TEXT("Default pose");
    const float Duration=M->LabTry.Anim?M->LabTry.Anim->GetPlayLength()*(M->LabTry.To-M->LabTry.From):0;
    return FText::FromString(FString::Printf(TEXT("%s  ·  %.2fs  ·  %.2fx"),*Name,Duration,M->SetupSpeed));
   })]
   +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,4)
   [SNew(SSlider).Value_Lambda([this]{
    auto* M=Match.Get();if(!M||!M->LabTry.Anim)return 0.f;
    const float Span=FMath::Max(.01f,M->LabTry.Anim->GetPlayLength()*(M->LabTry.To-M->LabTry.From));
    return FMath::Min(1.f,FMath::Fmod(M->LabClock,Span+.4f)/Span);
   }).OnValueChanged_Lambda([this](float V){
    auto* M=Match.Get();if(!M||!M->LabTry.Anim)return;
    M->bSetupPaused=true;M->LabClock=V*M->LabTry.Anim->GetPlayLength()*(M->LabTry.To-M->LabTry.From);
   })]
   +SVerticalBox::Slot().AutoHeight()[Controls]
   +SVerticalBox::Slot().AutoHeight().Padding(2,4)
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Timing(TEXT("Trim in"),0)]
    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Timing(TEXT("Contact"),1)]
    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Timing(TEXT("Trim out"),2)]
    +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("Auto contact"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->SetupAction(12);return FReply::Handled();}))]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Button(TEXT("Test  Enter / F5"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->SetupTest();return FReply::Handled();}))]
    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Button(TEXT("Play"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->SetupTest(true);return FReply::Handled();}))]
    +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Button(TEXT("Save animation"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get()){M->SetupApply();RefreshMap();}return FReply::Handled();}))]
    +SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("Revert"),FOnClicked::CreateLambda([this]{if(auto* M=Match.Get())M->SetupAction(4);return FReply::Handled();}))]]];
 }
 TSharedRef<SWidget> Timing(const TCHAR* Name,int32 Field)
 {
  return SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[Label(Name,8)]
   +SVerticalBox::Slot().AutoHeight()[SNew(SSlider)
    .Value_Lambda([this,Field]{
     auto* M=Match.Get();if(!M)return 0.f;
     const auto& C=M->LabTry;
     return Field==0?C.From:Field==2?C.To:C.Impact>=0?C.Impact:(C.From+C.To)/2;
    })
    .OnValueChanged_Lambda([this,Field](float Value){
     auto* M=Match.Get();if(!M || !M->LabTry.Anim)return;
     auto& C=M->LabTry;
     if(Field==0)C.From=FMath::Min(Value,C.To-.01f);
     else if(Field==2)C.To=FMath::Max(Value,C.From+.01f);
     else C.Impact=FMath::Clamp(Value,C.From,C.To);
     if(C.Impact>=0)C.Impact=FMath::Clamp(C.Impact,C.From,C.To);
     M->bSetupPaused=true;
     const float Sample=Field==0?C.From:Field==2?C.To:C.Impact;
     M->LabClock=(Sample-C.From)*C.Anim->GetPlayLength();
    })];
 }

 TWeakObjectPtr<AGkMatch> Match;
 TArray<FRow> Moves,Clips;
 TArray<TSharedPtr<FString>> PackOptions,MotionOptions,Outcomes;
 TSharedPtr<SListView<FRow>> MoveList,ClipList;
 TSharedPtr<SSearchBox> MoveSearch,ClipSearch;
 TSharedPtr<SEditableTextBox> TagsField;
 TSharedPtr<SScrollBox> MapScroll;
 TSharedPtr<SVerticalBox> Links;
};
}

bool AGkMatch::SetupClipIsAssigned(const FLabClip& Clip) const
{
 if(LabSet<0 || LabSet>=UE_ARRAY_COUNT(Clips))return false;
 for(const auto& Entry:Clips[LabSet])
 {
  if(Entry.Value.Anim && Entry.Value.Anim->GetPathName()==Clip.Path)return true;
 }
 return false;
}

FName AGkMatch::SetupSuggestedMotion() const
{
 if(SetupLibraryMotion==TEXT("Match left category"))
 {
  static const FName ByGroup[]={TEXT("Move"),TEXT("Ground attack"),TEXT("Aerial"),TEXT("Special motion"),
   TEXT("Guard / Parry"),TEXT("Grab / Throw"),TEXT("Energy / Charge"),TEXT("Reaction")};
  return SetupGroup>=0 && SetupGroup<UE_ARRAY_COUNT(ByGroup)?ByGroup[SetupGroup]:NAME_None;
 }
 if(!LabSlots.IsValidIndex(LabSlot))return NAME_None;
 const auto& Slot=LabSlots[LabSlot];
 const FString Key=Slot.Slot.ToString();
 if(Key==TEXT("idle"))return TEXT("Idle");
 if(Key.StartsWith(TEXT("walk")) || Key==TEXT("introWalk"))return TEXT("Walk");
 if(Key==TEXT("run"))return TEXT("Run");
 if(Key.StartsWith(TEXT("dash")) || Key==TEXT("drush"))return TEXT("Dash");
 if(Key==TEXT("prejump"))return TEXT("Rise");
 if(Key==TEXT("jump"))return TEXT("Jump");
 if(Key==TEXT("land"))return TEXT("Land");
 if(Key==TEXT("kd"))return TEXT("Fall");
 if(Key==TEXT("getup"))return TEXT("Get up");
 if(Key==TEXT("mv_DI"))return TEXT("Punch");
 if(Key.StartsWith(TEXT("mv_SRK")) || Key==TEXT("mv_AUP"))return TEXT("Rise");
 if(Key.Contains(TEXT("HOME")))return TEXT("Fly");
 if(Key.Contains(TEXT("jKnee")) || Key.Contains(TEXT("jStomp")) || Key.Contains(TEXT("jUpAir")))return TEXT("Kick / Knee");
 switch(Slot.Cat)
 {
  case 0: return Key==TEXT("win") || Key.StartsWith(TEXT("showcase")) ? FName(TEXT("Taunt / Pose")) : FName(TEXT("Move"));
  case 1: return TEXT("Guard / Parry");
  case 2: return TEXT("Reaction");
  case 3: return TEXT("Punch");
  case 4: return TEXT("Kick / Knee");
  case 5: return TEXT("Aerial");
  case 8: return Key.EndsWith(TEXT("Victim")) ? FName(TEXT("Reaction")) : FName(TEXT("Grab / Throw"));
  default: return TEXT("Energy / Charge");
 }
}

bool AGkMatch::SetupHasMotion(const FLabClip& Clip,FName Motion) const
{
 if(Motion.IsNone())return true;
 // A clip already assigned to the selected move belongs in its suggestions.
 if(SetupLibraryMotion==TEXT("Match selected move") && LabSlots.IsValidIndex(LabSlot))
 {
  const auto* Assigned=Clips[LabSet].Find(LabSlots[LabSlot].Slot);
  if(Assigned && Assigned->Anim && Assigned->Anim->GetPathName()==Clip.Path)return true;
 }
 const FString Text=Clip.Label+TEXT(" ")+Clip.Pack+TEXT(" ")+Clip.Tags+TEXT(" ")+SetupCustomTags.FindRef(Clip.Path);
 if(Motion==TEXT("Ground attack"))return SetupHasMotion(Clip,TEXT("Punch")) || SetupHasMotion(Clip,TEXT("Kick / Knee")) || SetupHasMotion(Clip,TEXT("Combo"));
 if(Motion==TEXT("Special motion"))return SetupHasMotion(Clip,TEXT("Energy / Charge")) || SetupHasMotion(Clip,TEXT("Rise")) || SetupHasMotion(Clip,TEXT("Fly"));
 for(const auto& Filter:MotionFilters)
 {
  if(Motion==FName(Filter.Name))return HasMotion(Text,Filter);
 }
 return false;
}

FString AGkMatch::SetupClipSearchText(const FLabClip& Clip) const
{
 const FString Metadata=Clip.Label+TEXT(" ")+Clip.Pack+TEXT(" ")+Clip.Tags+TEXT(" ")+SetupCustomTags.FindRef(Clip.Path);
 FString Result=Metadata;
 for(const auto& Filter:MotionFilters)
 {
  if(HasMotion(Metadata,Filter))Result+=FString(TEXT(" "))+Filter.Name;
 }
 return Result;
}

void AGkMatch::OpenSetupWidget()
{
 if(SetupWidget || bScript || !GetWorld() || !GetWorld()->GetGameViewport()) return;
 FString Text;
 TSharedPtr<FJsonObject> Tags;
 if(FFileHelper::LoadFileToString(Text,*TagsFile()) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Tags) && Tags)
 {
  SetupCustomTags.Reset();
  for(const auto& Entry:Tags->Values)if(Entry.Value->Type==EJson::String)SetupCustomTags.Add(FString(Entry.Key.ToView()),Entry.Value->AsString());
 }
 SetupWidget=SNew(SGkAnimationLab).Match(this);
 GetWorld()->GetGameViewport()->AddViewportWidgetContent(SetupWidget.ToSharedRef(),10);
 if(auto* PC=GetWorld()->GetFirstPlayerController())
 {
  FInputModeGameAndUI Input;
  Input.SetWidgetToFocus(SetupWidget);
  Input.SetHideCursorDuringCapture(false);
  Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
  PC->SetInputMode(Input);
 }
}

void AGkMatch::SetSetupTags(const FString& Path,const FString& Tags)
{
 const FString Clean=Tags.TrimStartAndEnd();
 if(Clean==SetupCustomTags.FindRef(Path))return;
 if(Clean.IsEmpty())SetupCustomTags.Remove(Path);
 else SetupCustomTags.Add(Path,Clean);
 auto Json=MakeShared<FJsonObject>();
 for(const auto& Entry:SetupCustomTags)Json->SetStringField(Entry.Key,Entry.Value);
 FString Text;
 if(FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text)) && !FFileHelper::SaveStringToFile(Text,*TagsFile()))
  UE_LOG(LogTemp,Warning,TEXT("Could not save Animation Lab tags"));
}

void AGkMatch::CloseSetupWidget()
{
 if(!SetupWidget)return;
 if(GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(SetupWidget.ToSharedRef());
 SetupWidget.Reset();
 if(SetupThumbCamera)SetupThumbCamera->TextureTarget=nullptr;
}

UTextureRenderTarget2D* AGkMatch::CaptureSetupThumb(const FString& Path,float Position,UTextureRenderTarget2D* Reuse)
{
 if(Screen!=4 || !Body[0] || !Body[0]->GetSkinnedAsset() || !bPlaced)return nullptr;
 auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Path);
 if(!Sequence)return nullptr;
 if(!SetupThumbBody)
 {
  SetupThumbBody=NewObject<UPoseableMeshComponent>(this);
  SetupThumbBody->SetSkinnedAssetAndUpdate(Body[0]->GetSkinnedAsset());
  SetupThumbBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  SetupThumbBody->SetCastShadow(false);
  SetupThumbBody->SetVisibleInSceneCaptureOnly(true);
  SetupThumbBody->RegisterComponent();
  SetupThumbCamera=NewObject<USceneCaptureComponent2D>(this);
  SetupThumbCamera->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
  SetupThumbCamera->CaptureSource=SCS_FinalColorLDR;
  SetupThumbCamera->bCaptureEveryFrame=false;
  SetupThumbCamera->bCaptureOnMovement=false;
  SetupThumbCamera->ShowFlags.SetAtmosphere(false);
  SetupThumbCamera->ShowFlags.SetFog(false);
  SetupThumbCamera->ShowFlags.SetVolumetricFog(false);
  SetupThumbCamera->ShowFlags.SetDynamicShadows(false);
  SetupThumbCamera->FOVAngle=36;
  SetupThumbCamera->RegisterComponent();
  SetupThumbCamera->ShowOnlyComponent(SetupThumbBody);
 }
 if(!Reuse)
 {
  Reuse=NewObject<UTextureRenderTarget2D>(this);
  Reuse->InitCustomFormat(112,90,PF_B8G8R8A8,false);
  SetupThumbTargets.Add(Reuse);
 }
 for(int32 Index=0;Index<Body[0]->GetNumMaterials();++Index)SetupThumbBody->SetMaterial(Index,Body[0]->GetMaterial(Index));
 EvalClip(0,Sequence,Position*Sequence->GetPlayLength(),SetupThumbBody->BoneSpaceTransforms);
 SetupThumbBody->MarkRefreshTransformDirty();
 SetupThumbBody->RefreshBoneTransforms();
 const FVector Origin=Wd(FVector(-120,0,800));
 SetupThumbBody->SetWorldLocationAndRotation(Origin,Anchor.GetRotation());
 const FVector Eye=Origin+Anchor.TransformVectorNoScale(FVector(-345,70,100));
 SetupThumbCamera->SetWorldLocationAndRotation(Eye,(Origin+FVector(0,0,92)-Eye).Rotation());
 SetupThumbCamera->TextureTarget=Reuse;
 SetupThumbCamera->CaptureScene();
 return Reuse;
}
