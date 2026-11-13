#include "GkMatch.h"
#include "GkPad.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/CompositeFont.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Paths.h"
#include "TextureResource.h"

// Match gauges share the same angular frame language; training diagnostics remain optional.

static void GkRect(UCanvas* Cv, float X, float Y, float W, float H, const FLinearColor& Col)
{
	if (W <= 0.f || H <= 0.f || Col.A <= 0.f) { return; }
	FCanvasTileItem Item(FVector2D(X, Y), GWhiteTexture, FVector2D(W, H), Col); Item.BlendMode = SE_BLEND_Translucent; Cv->DrawItem(Item);
}
static void GkHudPanel(UCanvas* Canvas, float X, float Y, float Width, float Height, float Slant,
	const FLinearColor& Color)
{
	const FVector2D TL(X + Slant, Y), TR(X + Width, Y), BL(X, Y + Height), BR(X + Width - Slant, Y + Height);
	auto DrawTriangle = [&](const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		FCanvasUVTri Triangle;
		Triangle.V0_Pos = A;
		Triangle.V1_Pos = B;
		Triangle.V2_Pos = C;
		Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = Color;
		FCanvasTriangleItem Item(Triangle, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	};
	DrawTriangle(TL, TR, BL);
	DrawTriangle(TR, BR, BL);
}

static void GkRoundToken(UCanvas* Cv,float X,float Y,float Radius,const FLinearColor& Color)
{
 for(int k=0;k<16;++k){const float A=k*2*PI/16,B=(k+1)*2*PI/16;FCanvasUVTri T;
 T.V0_Pos=FVector2D(X,Y);T.V1_Pos=FVector2D(X+FMath::Cos(A)*Radius,Y+FMath::Sin(A)*Radius);T.V2_Pos=FVector2D(X+FMath::Cos(B)*Radius,Y+FMath::Sin(B)*Radius);
 T.V0_Color=T.V1_Color=T.V2_Color=Color;FCanvasTriangleItem Item(T,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Cv->DrawItem(Item);}
}

static void GkPortrait(UCanvas* Cv,UTexture* Texture,float X,float Y,float Width,float Height,bool Mirror)
{
 const float Cut=Width*.22f;
 FVector2D P[4]={{X,Y},{X+Width,Y},{X+Width-(Mirror?0:Cut),Y+Height},{X+(Mirror?Cut:0),Y+Height}};
 FVector2D UV[4]={{0,.04},{1,.04},{1,.94},{0,.94}};
 for(int k=0;k<2;++k){int a=0,b=k+1,c=k+2;FCanvasUVTri T;
 T.V0_Pos=P[a];T.V1_Pos=P[b];T.V2_Pos=P[c];T.V0_UV=UV[a];T.V1_UV=UV[b];T.V2_UV=UV[c];
 T.V0_Color=T.V1_Color=T.V2_Color=FLinearColor::White;FCanvasTriangleItem Item(T,Texture->GetResource());Item.BlendMode=SE_BLEND_Translucent;Cv->DrawItem(Item);}
}

static void GkText(UCanvas* Cv, const FString& Str, float X, float Y, float Size, const FLinearColor& Col, bool bCentre = false, int32 Outline = 2)
{
	if (!AGkMatch::Current || Col.A <= 0.f) return;
    auto& Carrier=AGkMatch::Current->HudFont;
    if (!Carrier) Carrier=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    if (!Carrier) return;
	const FFontOutlineSettings Line = Outline > 0 ? FFontOutlineSettings(Outline, FLinearColor(0.f, 0.f, 0.f, Col.A)) : FFontOutlineSettings();
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), Carrier.Get(), Col);
	static TSharedPtr<const FCompositeFont> Face=MakeShared<FCompositeFont>(FName(TEXT("Arcade")),FPaths::ProjectContentDir()/TEXT("GkUI/Fonts/BarlowCondensed/BarlowCondensed-SemiBold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
 Item.SlateFontInfo=FSlateFontInfo(Face,FMath::Max(4.f,Size));Item.SlateFontInfo->OutlineSettings=Line;
	Item.bCentreX = bCentre; Cv->DrawItem(Item);
}
static FLinearColor GkFade(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * FMath::Clamp(A, 0.f, 1.f)); }

void AGkMatch::DrawHud(UCanvas* Cv)
{
	if (!Cv) { return; }
	const float W = Cv->SizeX, H = Cv->SizeY, U = H / 1080.f, Mid = W * 0.5f;
	const FLinearColor Ink(0.008f, 0.006f, 0.009f, 1.f), Bone(0.98f, 0.88f, 0.68f, 1.f), Gold(1.f, 0.78f, 0.2f, 1.f), Red(0.9f, 0.16f, 0.12f, 1.f), Blue(0.35f, 0.65f, 1.f, 1.f), Ash(0.3f, 0.3f, 0.34f, 0.9f), Jade(0.25f, 0.9f, 0.5f, 1.f), Slate(0.55f, 0.55f, 0.6f, 1.f);
	const FLinearColor Tint[2] = { bForestLook ? FLinearColor(.88f,.022f,.018f) : Red, Blue }; const FString Names[2] = { TEXT("PLAYER 1"), Mode == 1 ? TEXT("DUMMY") : TEXT("CPU") };
	if (Screen == 4 && (!bScript || bLabUi)) { DrawCharacterSetup(Cv); return; }
 if(bEntrance && Screen==3 && !bMenu && !bNoHud){
  const float Bars=H*.055f*(1-FMath::Clamp((EntranceFrame-EntranceDolly())/60.f,0.f,1.f));GkRect(Cv,0,0,W,Bars,Ink);GkRect(Cv,0,H-Bars,W,Bars,Ink);
  if(EntranceFrame<72)GkText(Cv,Stages.IsValidIndex(Setup.Stage)?Stages[Setup.Stage].Name:TEXT("GEKITOTSU"),W*.06f,H*.78f,38*U,Bone);
  else if(EntranceFrame<EntranceDolly())GkText(Cv,TEXT("ROBOT 1"),W*.06f,H*.80f,30*U,Bone);
  GkText(Cv,TEXT("START / ENTER  SKIP"),W*.82f,H*.94f,15*U,Bone);
  return;
 }

	if(bMenu && !bNoHud){DrawArcadeMenu(Cv);DrawMenuEffects(Cv);return;}

	// hit and hurt boxes, straight from the rules: green where a fighter can be hit, red where a blow lands
	if (bBoxes && Mode == 1)   // the boxes and move read-outs are training aids
	{
		auto Draw = [&](const gk::Box& B, const FLinearColor& C)
		{
			const FVector A0 = Cv->Project(Wd(At(B.x0, B.y1))), A1 = Cv->Project(Wd(At(B.x1, B.y0)));
			const float X0 = FMath::Min(A0.X, A1.X), Y0 = FMath::Min(A0.Y, A1.Y), Bw = FMath::Abs(A1.X - A0.X), Bh = FMath::Abs(A1.Y - A0.Y);
			GkRect(Cv, X0, Y0, Bw, Bh, GkFade(C, 0.22f)); GkRect(Cv, X0, Y0, Bw, 2.f, C); GkRect(Cv, X0, Y0 + Bh - 2.f, Bw, 2.f, C); GkRect(Cv, X0, Y0, 2.f, Bh, C); GkRect(Cv, X0 + Bw - 2.f, Y0, 2.f, Bh, C);
		};
		for (int32 i = 0; i < 2; i++)
		{
			gk::Box Hu[12], Hi[9]; const int32 Nu = gk::hurtboxes(S.p[i], Hu), Nh = gk::hitboxes(S.p[i], Hi);
			for (int32 k = 0; k < Nu; k++) { Draw(Hu[k], Jade); } for (int32 k = 0; k < Nh; k++) { Draw(Hi[k], Red); }
		}
	}
	// what each fighter is doing, over his head: a coloured word for the states that matter (and in training the move with its frame)
	for (int32 i = 0; i < 2 && Screen == 3; i++)
	{
		const gk::Fighter& P = S.p[i]; const FVector Head = Cv->Project(Wd(At(P.x, P.y + 100 * gk::F))); if (Head.Z <= 0.0) { continue; }
		const TCHAR* Word = nullptr; FLinearColor Chip = Ash;
		if (PerfectT[i] > 0) { Word = TEXT("PERFECT!"); Chip = Gold; }
		else if (P.st == gk::ATK && P.mv >= 0 && gk::move(P.mv).di) { Word = TEXT("DRIVE IMPACT!"); Chip = FLinearColor(0.85f, 0.05f, 0.5f, 1.f); }
		else if (P.st == gk::PARRY) { Word = TEXT("PARRY"); Chip = FLinearColor(0.f, 0.75f, 0.85f, 1.f); }
		else if (P.st == gk::PREC) { Word = TEXT("OPEN"); Chip = FLinearColor(0.25f, 0.32f, 0.34f, 1.f); }
		else if (P.st == gk::BLOCK) { Word = TEXT("GUARD"); Chip = FLinearColor(0.15f, 0.4f, 1.f, 1.f); }
		else if (P.st == gk::KDOWN) { Word = TEXT("DOWN"); Chip = FLinearColor(0.5f, 0.1f, 0.08f, 1.f); }
		else if (P.arm > 0) { Word = TEXT("ARMOR"); Chip = FLinearColor(0.95f, 0.45f, 0.05f, 1.f); }
		else if (P.st == gk::DRUSH) { Word = TEXT("RUSH"); Chip = FLinearColor(0.1f, 0.7f, 0.25f, 1.f); }
		float Yy = Head.Y - 64.f * U;
		if (Word) { const float Ww = (FCString::Strlen(Word) * 15.f + 26.f) * U; if(!bForestLook)GkRect(Cv, Head.X - Ww * 0.5f, Yy, Ww, 30.f * U, Chip); GkText(Cv, Word, Head.X, Yy + 2.f * U, 19.f * U, Bone, true, 2); Yy -= 34.f * U; }
		if (P.bo) { const float Ww = 136.f * U, Bl = 0.65f + 0.35f * FMath::Sin(Ticks * 0.3f); GkRect(Cv, Head.X - Ww * 0.5f, Yy, Ww, 30.f * U, FLinearColor(0.3f, 0.32f, 0.3f, Bl)); GkText(Cv, TEXT("BURNOUT"), Head.X, Yy + 2.f * U, 19.f * U, Bone, true, 2); }
		if (Mode == 1 && P.st == gk::ATK) { GkText(Cv, FString::Printf(TEXT("%s  %d"), UTF8_TO_TCHAR(gk::moveName(P.mv)), P.mf), Head.X, Head.Y + 6.f * U, 15.f * U, Gold, true); }
	}
	for (const FSpark& Sp : Sparks)
	{
		const FVector P = Cv->Project(Sp.At); const float K = Sp.Age / 12.f, R = (20.f + 70.f * K) * U; const FLinearColor C = Sp.Kind == 0 ? Gold : Sp.Kind == 1 ? Bone : Blue;
		GkRect(Cv, P.X - R, P.Y - 3.f * U, R * 2.f, 6.f * U, GkFade(C, 1.f - K)); GkRect(Cv, P.X - 3.f * U, P.Y - R, 6.f * U, R * 2.f, GkFade(C, 1.f - K));
	}

	// a marker over each fighter, so two of the same character can be told apart
	if (Screen == 3 && bPlaced && !bForestLook)
	{
		for (int32 i = 0; i < 2; i++)
		{
			FVector Mark = At(S.p[i].x, S.p[i].y + 104 * gk::F);
			if (bForestLook && bPlaced) { Mark.Z += FloorPoint(At(S.p[i].x, 0, i ? 8.0 : -8.0)).Z; }
			const FVector Hd = Cv->Project(Wd(Mark)); if (Hd.Z <= 0.0) { continue; }
			const FLinearColor Mk = i ? Blue : Red; const float Mw = 34.f * U;
			GkRect(Cv, Hd.X - Mw * 0.5f, Hd.Y - 30.f * U, Mw, 20.f * U, Mk); GkText(Cv, i ? (Mode == 0 ? TEXT("CPU") : TEXT("2P")) : TEXT("1P"), Hd.X, Hd.Y - 29.f * U, 13.f * U, Bone, true, 0);
			GkRect(Cv, Hd.X - 3.f * U, Hd.Y - 10.f * U, 6.f * U, 6.f * U, Mk);
		}
	}
	if (bScript || bFly) { if (!ScriptNote.IsEmpty()) { GkText(Cv, ScriptNote, 20.f * U, H - 60.f * U, 30.f * U, Gold); } if (Screen == 4 && !bLabUi) { return; } }
	if (bNoHud) { return; }
	static const TCHAR* ModeName[] = { TEXT("VS CPU"), TEXT("TRAINING"), TEXT("WATCH") };
	static const TCHAR* DummyName[] = { TEXT("STANDS"), TEXT("CROUCHES"), TEXT("JUMPS"), TEXT("BLOCKS ALL"), TEXT("FIGHTS BACK") };
	if (Screen == 3)
	{
	const float Gap = (bForestLook ? 62.f : 80.f) * U;
	const float BarW = FMath::Min(760.f * U, Mid - Gap - 170.f * U), BarH = 26.f * U, Top = (bForestLook ? 34.f : 56.f) * U;
	for (int32 i = 0; i < 2; i++)
	{
		const gk::Fighter& P = S.p[i]; const float K = FMath::Clamp(P.hp / 1000.f, 0.f, 1.f), Lag = FMath::Clamp(P.red / 1000.f, K, 1.f);
		const float X0 = i ? Mid + Gap : Mid - Gap - BarW;
		if (bForestLook)
		{
			GkHudPanel(Cv, X0 - 24.f * U, Top - 7.f * U, BarW + 48.f * U, BarH + 15.f * U, 16.f * U, Ink);
			GkHudPanel(Cv,X0-20.f*U,Top-5.f*U,BarW+40.f*U,BarH+11.f*U,12.f*U,FLinearColor(.75,.62,.42));
			GkHudPanel(Cv,X0-17.f*U,Top-3.f*U,BarW+34.f*U,BarH+7.f*U,10.f*U,Ink);
			GkRect(Cv, X0, Top, BarW, BarH, Ash);
			if (Setup.Char[i] >= 2)
			{

    if(PortraitTarget[i] && PortraitTarget[i]->GetResource()) {
     const float Size=134.f*U,X=i?W-Size-8.f*U:8.f*U;
     GkHudPanel(Cv,X-4*U,5*U,Size+8*U,Size*.78f+8*U,15*U,Tint[i]);
     GkPortrait(Cv,PortraitTarget[i],X,5*U,Size,Size*.78f,i!=0);
    }
			}
		}
		else
		{
			GkRect(Cv, X0 - 4.f * U, Top - 4.f * U, BarW + 8.f * U, BarH + 8.f * U, Ink);
			GkRect(Cv, X0, Top, BarW, BarH, Ash);
		}
		// health empties away from the clock; the pale part is damage still being taken
		if (i) { GkRect(Cv, X0, Top, BarW * Lag, BarH, Bone); GkRect(Cv, X0, Top, BarW * K, BarH, bForestLook ? FLinearColor(0.98f, 0.52f, 0.09f) : K <= 0.25f ? Gold : Tint[i]); }
		else { GkRect(Cv, X0 + BarW * (1.f - Lag), Top, BarW * Lag, BarH, Bone); GkRect(Cv, X0 + BarW * (1.f - K), Top, BarW * K, BarH, bForestLook ? FLinearColor(0.98f, 0.52f, 0.09f) : K <= 0.25f ? Gold : Tint[i]); }
		if (bForestLook)
		{
			const float Shine = BarW * K;
			GkRect(Cv, i ? X0 : X0 + BarW - Shine, Top, Shine, 5.f * U, FLinearColor(1.f, 0.85f, 0.40f));
		}
		GkText(Cv, bForestLook ? (i ? TEXT("ROBOT 1 / P2") : TEXT("P1 / ROBOT 1")) : Names[i], i ? X0 + BarW - 150.f * U : X0, Top - 25.f * U, 16.f * U, Bone);
		// the Drive gauge: six bars
		for (int32 b = 0; b < 6; b++)
		{
			const float Part = FMath::Clamp((P.dr - b * 1000) / 1000.f, 0.f, 1.f), Dw = 38.f * U, Dh = 8.f * U, Dx = i ? Mid + Gap + b * (Dw + 6.f * U) : Mid - Gap - (b + 1) * (Dw + 6.f * U) + 6.f * U, Dy = Top + BarH + 10.f * U;
			GkRect(Cv, Dx, Dy, Dw, Dh, Ash); if (i) { GkRect(Cv, Dx, Dy, Dw * Part, Dh, P.bo ? Slate : Jade); } else { GkRect(Cv, Dx + Dw * (1.f - Part), Dy, Dw * Part, Dh, P.bo ? Slate : Jade); }
		}
		if (P.bo) { const float Bx = i ? Mid + Gap : Mid - Gap - 6.f * 62.f * U + 6.f * U, Bl = 0.5f + 0.5f * FMath::Sin(Ticks * 0.3f); GkRect(Cv, Bx - 3.f * U, Top + BarH + 7.f * U, 6.f * 62.f * U, 18.f * U, FLinearColor(0.75f, 0.1f, 0.08f, 0.35f + 0.4f * Bl)); GkText(Cv, TEXT("BURNOUT"), Bx + 130.f * U, Top + BarH + 5.f * U, 17.f * U, Bone, false, 2); }
		for (int32 r = 0; r < FMath::Clamp(S.cfg.rounds, 1, 3); r++) {
   const float Rx=i?Mid+Gap+(r*23.f+7.f)*U:Mid-Gap-(r*23.f+7.f)*U,Ry=Top+BarH+29.f*U;
   if(bForestLook){GkRoundToken(Cv,Rx,Ry,6.f*U,FLinearColor(.65,.55,.38));GkRoundToken(Cv,Rx,Ry,4.4f*U,r<P.wins?Gold:FLinearColor(.045,.055,.075));}
   else GkRect(Cv,Rx,Ry,18.f*U,8.f*U,r<P.wins?Gold:Ash);
  }
		// the super gauge: three levels
		for (int32 b = 0; b < 3; b++)
		{
			const float Part = FMath::Clamp((P.sa - b * 1000) / 1000.f, 0.f, 1.f), Sw = 150.f * U, Sx = i ? W - 60.f * U - (b + 1) * (Sw + 10.f * U) : 60.f * U + b * (Sw + 10.f * U), Sy = H - 70.f * U;
			if (bForestLook)
			{
				GkHudPanel(Cv,Sx-5.f*U,Sy-6.f*U,Sw+10.f*U,30.f*U,9.f*U,Ink);
				GkHudPanel(Cv,Sx,Sy-2.f*U,Sw,22.f*U,6.f*U,GkFade(Tint[i],.27f));
			}
			GkRect(Cv, Sx, Sy, Sw, 16.f * U, Ash); if (i) { GkRect(Cv, Sx + Sw * (1.f - Part), Sy, Sw * Part, 16.f * U, Part >= 1.f && !bForestLook ? Gold : Tint[i]); } else { GkRect(Cv, Sx, Sy, Sw * Part, 16.f * U, Part >= 1.f && !bForestLook ? Gold : Tint[i]); }
		}
		GkText(Cv, (bForestLook ? FString::FromInt(P.sa / 1000) : FString::Printf(TEXT("SUPER %d"), P.sa / 1000)), i ? W - 180.f * U : 60.f * U, H - 128.f * U, (bForestLook ? 48.f : 18.f) * U, bForestLook ? Tint[i] : P.sa >= 1000 ? Gold : Bone);
		const float Cx = i ? W - 240.f * U : 240.f * U;
		if (ComboTimer[i] > 0 && Combo[i] >= 2) {
   const float Pop=1.f+.23f*FMath::Exp(-float(80-ComboTimer[i])*.22f),Fade=FMath::Min(1.f,ComboTimer[i]/12.f);
   const float X=Cx+(i?1:-1)*FMath::Max(0,ComboTimer[i]-74)*6.f*U;
   if(!SpatialType[i+1])GkHudPanel(Cv,X-104*U,295*U,205*U,94*U,26*U,GkFade(Ink,.7f*Fade));
   if(!SpatialType[i+1])GkText(Cv,FString::FromInt(Combo[i]),X-20*U,260*U,94*U*Pop,GkFade(Bone,Fade),true,4);
   GkText(Cv,TEXT("HIT"),X+67*U,325*U,24*U,GkFade(Tint[i],Fade),true,2);
   GkText(Cv,Combo[i]>=10?TEXT("ABSURD!"):Combo[i]>=6?TEXT("SHOW OFF!"):TEXT("NICE!"),X,392*U,22*U,GkFade(Gold,Fade),true,2);
   GkText(Cv,FString::Printf(TEXT("%d DAMAGE"),ComboDmg[i]),X,426*U,14*U,GkFade(Bone,Fade),true,1);
  }
		if (NoteTimer[i] > 0) { GkText(Cv, Note[i], Cx, 420.f * U, 28.f * U, GkFade(Gold, NoteTimer[i] / 12.f), true); }
	}
	if (bForestLook) { GkHudPanel(Cv, Mid - 66.f * U, Top - 22.f * U, 132.f * U, 76.f * U, 20.f * U, Ink); }
	else { GkRect(Cv, Mid - 56.f * U, Top - 22.f * U, 112.f * U, 76.f * U, Ink); }
	GkText(Cv, FString::Printf(TEXT("%02d"), FMath::Clamp((S.time + 59) / 60, 0, 99)), Mid, Top - 20.f * U, 50.f * U, Bone, true);
	if (Mode == 1 || !bForestLook) { GkText(Cv, ModeName[FMath::Clamp(Mode, 0, 2)], Mid, Top + 60.f * U, 13.f * U, Bone, true); }
	if (bForestLook && Mode != 1)
	{
		GkText(Cv,TEXT("D R E A D M A R S H"),Mid,H-42.f*U,12.f*U,GkFade(Bone,.6f),true,1);
	}
	// Name the actual super during its held anticipation; it never covers the active fight.
	if (S.freeze>0 && S.frzWho>=0 && S.frzWho<2)
	{
		const gk::Fighter& F=S.p[S.frzWho];
		if (F.st==gk::ATK && F.mv>=0 && gk::move(F.mv).sup)
		{
			const float Y=H*.76f;
			GkHudPanel(Cv,Mid-300.f*U,Y,600.f*U,72.f*U,28.f*U,GkFade(Ink,.94f));
			GkRect(Cv,Mid-255.f*U,Y,510.f*U,3.f*U,Tint[S.frzWho]);
			GkText(Cv,UTF8_TO_TCHAR(gk::moveName(F.mv)),Mid,Y+10.f*U,34.f*U,Bone,true,2);
		}
	}

 DrawArcadeCall(Cv);
 if(S.phase==gk::PH_OVER) {
  GkHudPanel(Cv,W*.56f,H*.79f,W*.40f,H*.13f,25*U,Ink);
  GkText(Cv,S.winner>=0?Names[S.winner]+TEXT(" / ROBOT 1"):TEXT("DRAW GAME"),W*.59f,H*.81f,28*U,Bone);
  GkText(Cv,TEXT("RESULTS UP NEXT"),W*.59f,H*.87f,18*U,Gold);
 }

	// Keep the input readout available in training.
	if (!bForestLook || Mode == 1)
	{
		static const TCHAR* Lab[] = { TEXT("L"), TEXT("M"), TEXT("H"), TEXT("SP"), TEXT("AS"), TEXT("PA"), TEXT("DI"), TEXT("HM") };
		static const int32 Bit[] = { gk::IN_LT, gk::IN_MD, gk::IN_HV, gk::IN_SP, gk::IN_AS, gk::IN_PA, gk::IN_DI, gk::IN_HM };
		const float Sz = 34.f * U, Gp = 5.f * U, Bx = Mid - 6.f * (Sz + Gp), By = H - 100.f * U;
		auto Key = [&](float X, float Y, bool bOn, const TCHAR* Txt) { GkRect(Cv, X, Y, Sz, Sz, bOn ? Gold : Ink); GkText(Cv, Txt, X + Sz * 0.5f, Y + 7.f * U, 13.f * U, bOn ? FLinearColor::Black : Bone, true, 0); };
		Key(Bx, By, (InputNow & gk::IN_L) != 0, TEXT("<")); Key(Bx + Sz + Gp, By, (InputNow & gk::IN_D) != 0, TEXT("v")); Key(Bx + 2 * (Sz + Gp), By, (InputNow & gk::IN_R) != 0, TEXT(">")); Key(Bx + Sz + Gp, By - Sz - Gp, (InputNow & gk::IN_U) != 0, TEXT("^"));
		for (int32 k = 0; k < 8; k++) { Key(Bx + (4 + k) * (Sz + Gp), By, (InputNow & Bit[k]) != 0, Lab[k]); }
	}
	if (Mode == 1)
	{
		static const TCHAR* Arrow[] = { TEXT(""), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("-"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9") };
		static const TCHAR* Lab[] = { TEXT("L"), TEXT("M"), TEXT("H"), TEXT("SP"), TEXT("AS"), TEXT("DI"), TEXT("PA"), TEXT("HM") };
		for (int32 k = 0; k < Hist.Num(); k++)
		{
			const int32 In = Hist[k].In, Dir = 5 + (((In & 8) ? 1 : 0) - ((In & 4) ? 1 : 0)) + 3 * (((In & 1) ? 1 : 0) - ((In & 2) ? 1 : 0));
			FString Line = FString::Printf(TEXT("%2d   %s   "), Hist[k].Frames, Arrow[FMath::Clamp(Dir, 0, 9)]); for (int32 b = 0; b < 8; b++) { if (In & (16 << b)) { Line += Lab[b]; Line += TEXT(" "); } }
			GkText(Cv, Line, 30.f * U, (240.f + k * 28.f) * U, 17.f * U, Bone);
		}
		if (Adv > -900) { GkText(Cv, FString::Printf(TEXT("FRAME ADVANTAGE  %+d"), Adv), Mid, Top + 120.f * U, 22.f * U, Adv >= 0 ? Jade : FLinearColor(1.f, 0.55f, 0.5f, 1.f), true); }
		GkText(Cv, FString::Printf(TEXT("DUMMY  %s"), DummyName[FMath::Clamp(Dummy, 0, 4)]), W - 300.f * U, 240.f * U, 17.f * U, Bone);
	}
	if (!bForestLook || Mode == 1) { GkText(Cv, FString::Printf(TEXT("%d fps"), FMath::RoundToInt(1000.f / FMath::Max(1.f, FrameMs))), W - 90.f * U, 8.f * U, 15.f * U, FrameMs < 18.f ? Jade : Gold); }
	if (Mode == 1 || bMenu) { GkText(Cv, TEXT("TAB / START  menu      R / SELECT  restart      B  boxes"), Mid, H - 28.f * U, 13.f * U, GkFade(Bone, 0.7f), true); }
	// exactly what is arriving from the stick, so a controller that is seen but not heard can be told apart from one that is mapped wrongly
	if (Pads && Mode == 1 && bBoxes) { GkText(Cv, FString::Printf(TEXT("%s   buttons %08x  dirs %d"), *Pads->Debug(), PadNow, PadDirs), Mid, H - 48.f * U, 12.f * U, Gold, true); }

	}
 if(bMenu || Screen!=3 || !bPlaced)DrawArcadeMenu(Cv);
 DrawMenuEffects(Cv);
}

static void GkJapanese(UCanvas* Cv,const FString& Text,float X,float Y,float Size,FLinearColor Color)
{
 if(!AGkMatch::Current || !AGkMatch::Current->HudFont)return;
 FCanvasTextItem Item(FVector2D(X,Y),FText::FromString(Text),AGkMatch::Current->HudFont.Get(),Color);
 static TSharedPtr<const FCompositeFont> Font=MakeShared<FCompositeFont>(FName(TEXT("Japanese")),FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
 Item.SlateFontInfo=FSlateFontInfo(Font,FMath::Max(8.f,Size));
 Cv->DrawItem(Item);
}
void AGkMatch::DrawArcadeCall(UCanvas* Cv)
{
 if(CallHold<=0 || bMenu || CallAge>12)return;
 const float W=Cv->SizeX,H=Cv->SizeY,U=H/1080.f,Mid=W*.5f;
 const bool KO=CallWord.Contains(TEXT("K.O.")),Fight=CallWord==TEXT("FIGHT!");
 const float Out=FMath::Clamp((CallHold-CallAge)/10.f,0.f,1.f),In=FMath::Clamp(CallAge/8.f,0.f,1.f);
 const float X=Mid+(1-In)*(1-In)*W*.55f-(1-Out)*W*.6f,Y=H*.35f;
 const float Size=(KO?142.f:Fight?130.f:85.f)*U*(1.f+.17f*(1-In));
 const FLinearColor Ink(.008,.006,.009,.92*Out),Red(.88,.022,.018,Out),White(.98,.88,.68,Out);
 // Asymmetric knife strokes and offset ink outlines, with the live match visible around them.
 if(!SpatialType[0])GkHudPanel(Cv,X-W*.41f,Y+30*U,W*.82f,125*U,60*U,Ink);
 GkHudPanel(Cv,X-W*.35f,Y+149*U,W*.67f,8*U,4*U,Red);
 for(int32 k=0;k<7;++k){const float Dx=(k*41+CallAge*16)%170;
  GkHudPanel(Cv,X-W*.45f-Dx*U,Y+(k*21)*U,(72+k*7)*U,3*U,2*U,White);
  GkHudPanel(Cv,X+W*.31f+Dx*U,Y+(k*21)*U,(100-k*5)*U,3*U,2*U,Red);
 }
 if(!SpatialType[0])GkText(Cv,CallWord,X+5*U,Y-25*U+5*U,Size,Red,true,7);
 if(!SpatialType[0])GkText(Cv,CallWord,X,Y-25*U,Size,White,true,5);
 GkJapanese(Cv,KO?TEXT("決着"):Fight?TEXT("勝負開始"):TEXT("準備はいいか"),X-65*U,Y+166*U,26*U,White);
}

// Display lettering and functional text use separate licensed faces. The title is world geometry.
static void GkDisplay(UCanvas* Cv,const FString& Str,float X,float Y,float Size,FLinearColor Color,float Width=1.f,bool Center=false)
{
 if(!AGkMatch::Current || !AGkMatch::Current->HudFont)return;
 static TSharedPtr<const FCompositeFont> Face=MakeShared<FCompositeFont>(FName(TEXT("Impact")),FPaths::ProjectContentDir()/TEXT("GkUI/Fonts/Machina/GekitotsuMachina.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
 FCanvasTextItem Item(FVector2D(X,Y),FText::FromString(Str),AGkMatch::Current->HudFont.Get(),Color);
 Item.SlateFontInfo=FSlateFontInfo(Face,FMath::Max(8.f,Size));Item.Scale=FVector2D(Width,1);Item.bCentreX=Center;Cv->DrawItem(Item);
}
static void GkSlash(UCanvas* Cv,float X,float Y,float Width,float Height,FLinearColor Color)
{
 FCanvasUVTri T;T.V0_Pos=FVector2D(X,Y+Height);T.V1_Pos=FVector2D(X+Width,Y);T.V2_Pos=FVector2D(X+Width*.72f,Y+Height*.9f);
 T.V0_Color=T.V1_Color=T.V2_Color=Color;FCanvasTriangleItem Item(T,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Cv->DrawItem(Item);
}

// Poster compositions: the model, physical lettering and print shapes share one native set.
void AGkMatch::DrawArcadeMenu(UCanvas* Cv)
{
 const float W=Cv->SizeX,H=Cv->SizeY,U=H/1080.f;
 const FLinearColor Paper(.95,.81,.59),Ink(.009,.006,.004),Red(.76,.008,.004);
 auto Label=[&](const FString& T,float X,float Y,float S,FLinearColor C){GkText(Cv,T,X*W,Y*H,S*U,C,false,0);};
 auto Word=[&](const FString& T,float X,float Y,float S,FLinearColor C){GkDisplay(Cv,T,X*W,Y*H,S*U,C);};
 auto Stroke=[&](float X,float Y,float Width,float Height,FLinearColor C){
  GkHudPanel(Cv,X*W,Y*H,Width*W,Height*H,Height*H*.35f,C);
  for(int q=0;q<5;++q)GkSlash(Cv,(X+Width-.025f+q*.004f)*W,(Y+.01f+q*.012f)*H,W*.038f,H*.012f,C);
 };
 auto Wear=[&](){
  GkRect(Cv,3*U,3*U,W-6*U,1*U,Paper);GkRect(Cv,3*U,H-4*U,W-6*U,1*U,Paper);
  GkRect(Cv,3*U,3*U,1*U,H-6*U,Paper);GkRect(Cv,W-4*U,3*U,1*U,H-6*U,Paper);
  for(int k=0;k<120;++k){const uint32 Hash=uint32(k*747796405u+2891336453u);float X=(Hash%10000)/10000.f*W;float L=(2+(Hash/13)%13)*U;
   GkRect(Cv,X,(k&1)?H-6*U:2*U,L,(1+(Hash/37)%3)*U,Paper);
   if(k%4==0)GkSlash(Cv,k&1?W-10*U:0,(Hash%997)/997.f*H,12*U,4*U,Paper);
  }
 };
 Wear();
 if(Screen==5 && MenuPage==0){
  Stroke(.035,.865,.28,.082,Ink);Word(TEXT("PRESS START"),.052,.876,44,Paper);return;
 }
 // Controls stay out of the artwork until they are useful.
 if(Screen!=7)Label(MenuPage==1?TEXT("PRESS A BUTTON TO ASSIGN  /  ESC CANCELS"):Screen==1?TEXT("LEFT / RIGHT  COLOUR     ENTER  LOCK IN     ESC  BACK"):Screen==2?TEXT("LEFT / RIGHT  WEATHER     ENTER  READY     ESC  BACK"):TEXT("UP / DOWN  SELECT     ENTER  CONFIRM     ESC  BACK"),.045,.959,19,Paper);
 auto Rows=[&](const TArray<FString>& Entries,int Selected,float Y=.34f,float Step=.09f){
  for(int k=0;k<Entries.Num();++k){const bool On=k==Selected;const float At=Y+k*Step;
   if(On)Stroke(.035,At-.007f,.59f,Step-.012f,Red);
   Word(Entries[k],.06f,At,38,Paper);
  }
 };
 if(MenuPage==4){
  Rows({FString::Printf(TEXT("CPU LEVEL     < %d >"),Level),TEXT("WATCH A MATCH"),TEXT("BUTTON SETTINGS"),TEXT("MOVE LIST"),TEXT("SOUND & EFFECTS"),TEXT("BACK")},MenuAt,.34f,.088f);return;
 }
 if(MenuPage==2){
  const TCHAR* Names[]={TEXT("MUSIC"),TEXT("SOUND FX"),TEXT("VOICES"),TEXT("CAMERA SHAKE"),TEXT("LIGHT FLASHES")};float Values[]={MusicGain,EffectsGain,VoiceGain,ShakeGain,FlashGain};
  for(int k=0;k<5;++k){float Y=.34f+k*.09f;bool On=MenuAt==k;if(On)Stroke(.035,Y-.01f,.87f,.078f,Red);
   Word(Names[k],.06,Y,38,Paper);
   for(int j=0;j<10;++j)GkHudPanel(Cv,W*(.46f+j*.032f),H*(Y+.015f),W*.024f,H*.027f,6*U,j<FMath::RoundToInt(Values[k]*10)?Paper:FLinearColor(.16,.025,.018));
   Label(FString::Printf(TEXT("%d"),FMath::RoundToInt(Values[k]*100)),.81,Y,34,Paper);
  }
  if(MenuAt==5)Stroke(.035,.79f,.29f,.072f,Red);Word(TEXT("BACK"),.06,.80,38,Paper);Label(TEXT("LEFT / RIGHT TO ADJUST"),.46,.835,23,Paper);return;
 }
 if(MenuPage==1){
  const TCHAR* Actions[]={TEXT("LIGHT"),TEXT("MEDIUM"),TEXT("HEAVY"),TEXT("SPECIAL"),TEXT("ASSIST"),TEXT("PARRY"),TEXT("IMPACT"),TEXT("HOMING")};
  for(int k=0;k<8;++k){float X=.055f+(k/4)*.46f,Y=.35f+(k%4)*.13f;bool On=k==BindAt;
   if(On)Stroke(X-.02f,Y-.01f,.42f,.10f,Red);
   Word(Actions[k],X,Y,42,Paper);
   FString Key=PadCount>0?FString::Printf(TEXT("BUTTON %d"),PadBind[k]+1):Bind[k].GetDisplayName().ToString();
   Label(On?TEXT("PRESS NOW"):k<BindAt?Key:TEXT("—"),X+.22f,Y+.025f,26,Paper);GkRect(Cv,W*X,H*(Y+.09f),W*.38f,1*U,Paper);
  }return;
 }
 if(MenuPage==3){
  const TCHAR* Moves[]={TEXT("L / M / H     NORMALS"),TEXT("ASSIST + L / M / H     KICKS"),TEXT("SPECIAL     SHOT / AIR BEAM"),TEXT("FORWARD + SPECIAL     ANTI-AIR"),TEXT("HOMING     AIR CHASE"),TEXT("L + M     THROW       M + H     RUN"),TEXT("H + SP     SUPER       BACK     LEVEL 2"),TEXT("DOWN + H + SP     LEVEL 3"),TEXT("PARRY     READ       IMPACT     BREAK")};
  for(int k=0;k<9;++k){float X=.055f+(k/5)*.47f,Y=.35f+(k%5)*.106f;Label(Moves[k],X,Y,28,Paper);GkRect(Cv,W*X,H*(Y+.066f),W*.40f,1*U,Red);}return;
 }
 if(Screen==0){
  // The choices themselves are extruded lettering on the physical set.
  Word(TEXT("01"),.84,.74,130,Paper);return;
 }
 if(Screen==1){
  Stroke(.035,.05f,.27f,.065f,Red);Word(FString::Printf(TEXT("PLAYER %d"),SelWho+1),.055,.055,34,Paper);
  for(int k=0;k<8;++k){float X=.066f+k*.12f;const bool On=Setup.Costume[SelWho]==k;
   if(On)Stroke(X-.023f,.875f,.095f,.055f,Red);
   Word(FString::Printf(TEXT("%02d"),k+1),X,.883f,28,Paper);
  }
  return;
 }
 if(Screen==2){
  Word(TEXT("01"),.055,.065,65,Paper);Label(TEXT("DREADMARSH"),.055,.59,28,Paper);
  const TCHAR* Weather[]={TEXT("DYNAMIC"),TEXT("DAYLIGHT"),TEXT("MOONLIGHT"),TEXT("STORM"),TEXT("HAZE"),TEXT("TOXIC"),TEXT("TWILIGHT")};
  Stroke(.025,.715f,.355f,.085f,Red);Word(Weather[FMath::Clamp(MenuPalette,0,6)],.055,.727,39,Paper);
  Label(MenuPalette==0?TEXT("WEATHER EVOLVES WITH THE FIGHT"):TEXT("WEATHER LOCKED"),.055,.825,19,Paper);
  for(int k=0;k<7;++k)GkRect(Cv,W*(.057f+k*.04f),H*.875f,W*.026f,5*U,k==MenuPalette?Red:Paper);
  return;
 }
 if(Screen==7){
  Stroke(.02,.79f,.38f,.12f,Ink);Stroke(.60,.79f,.38f,.12f,Ink);
  Word(TEXT("ROBOT 1"),.055,.81,54,Paper);Word(TEXT("ROBOT 1"),.66,.81,54,Paper);return;
 }
 if(Screen==6){
  Label(S.winner<0?TEXT("DRAW GAME"):S.winner==0?TEXT("PLAYER 1 / WINNER"):TEXT("PLAYER 2 / WINNER"),.055,.12,27,Paper);
  Rows({TEXT("RUN IT BACK"),TEXT("CHARACTER SELECT"),TEXT("MAIN MENU"),TEXT("QUIT")},MenuAt,.55f,.085f);return;
 }
 if(bMenu){
  if(!bDebugMenu)Rows({TEXT("RESUME"),TEXT("RESTART"),TEXT("MOVE LIST"),TEXT("SOUND & EFFECTS"),TEXT("CHARACTER SELECT"),TEXT("MAIN MENU"),TEXT("QUIT")},MenuAt,.30f,.083f);
  else {const TCHAR* DummyNames[]={TEXT("STAND"),TEXT("CROUCH"),TEXT("JUMP"),TEXT("GUARD"),TEXT("FIGHT BACK")};Rows({TEXT("RESUME"),TEXT("RESTART"),TEXT("CHARACTER SELECT"),TEXT("STAGE SELECT"),TEXT("MAIN MENU"),FString::Printf(TEXT("DUMMY  < %s >"),DummyNames[FMath::Clamp(Dummy,0,4)]),FString::Printf(TEXT("HIT BOXES  < %s >"),bBoxes?TEXT("ON"):TEXT("OFF")),TEXT("SOUND & EFFECTS"),TEXT("BUTTON SETTINGS"),TEXT("ANIMATION LAB")},MenuAt,.27f,.061f);}
 }
}

void AGkMatch::DrawMenuEffects(UCanvas* Cv)
{
 const float W=Cv->SizeX,H=Cv->SizeY,U=H/1080.f;
 const FLinearColor Ink(.009,.006,.004),Paper(.95,.81,.59),Red(.76,.008,.004);
 if(MenuCutAge<.34f && UiPageKey>=0){
  const float T=MenuCutAge/.34f;
  for(int k=0;k<3;++k){float X=FMath::Lerp(-W*1.4f,W*1.5f,T)+k*W*.10f;GkHudPanel(Cv,X,-H*.2f,W*.65f,H*1.4f,H*.5f,k==1?Red:Ink);
   if(k==1)for(int j=0;j<6;++j)GkSlash(Cv,X+W*.18f,H*(j*.17f),W*.4f,10*U,Paper);
  }
 }
 if(MenuStampAge<.28f && Screen==1 && MenuPage==0 && FlashGain>0){
  float K=1-MenuStampAge/.28f;for(int j=0;j<9;++j)GkSlash(Cv,W*(.45f+j*.06f),H*(.22f+j*.047f),W*.16f*K,H*.015f*K,FLinearColor(.95,.81,.59,FlashGain));
 }
}
