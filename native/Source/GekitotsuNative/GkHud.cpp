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
	Item.SlateFontInfo = FCoreStyle::GetDefaultFontStyle(FName(TEXT("BoldItalic")), FMath::Max(4.f, Size), Line);
	Item.bCentreX = bCentre; Cv->DrawItem(Item);
}
static FLinearColor GkFade(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * FMath::Clamp(A, 0.f, 1.f)); }

void AGkMatch::DrawHud(UCanvas* Cv)
{
	if (!Cv) { return; }
	const float W = Cv->SizeX, H = Cv->SizeY, U = H / 1080.f, Mid = W * 0.5f;
	const FLinearColor Ink(0.02f, 0.02f, 0.03f, 0.85f), Bone(0.96f, 0.94f, 0.9f, 1.f), Gold(1.f, 0.78f, 0.2f, 1.f), Red(0.9f, 0.16f, 0.12f, 1.f), Blue(0.35f, 0.65f, 1.f, 1.f), Ash(0.3f, 0.3f, 0.34f, 0.9f), Jade(0.25f, 0.9f, 0.5f, 1.f), Slate(0.55f, 0.55f, 0.6f, 1.f);
	const FLinearColor Tint[2] = { bForestLook ? FLinearColor(1.f,.18f,.30f) : Red, Blue }; const FString Names[2] = { TEXT("PLAYER 1"), Mode == 1 ? TEXT("DUMMY") : TEXT("CPU") };
	if (Screen == 4 && (!bScript || bLabUi))
	{
		// the animation lab: tabs by kind of move, a grid of the moves, and a drop-down of clips on the one that is clicked
		float Mx = -1.f, My = -1.f; if (APlayerController* Ptr = GetWorld()->GetFirstPlayerController()) { Ptr->GetMousePosition(Mx, My); }
		int32 Hot = 0;
		auto Btn = [&](float X, float Y, float Bw, float Bh, int32 Id, const FString& Text, float Size, bool bOn, const FString& Sub = FString(), bool bSubGold = true)
		{
			const bool bOver = Mx >= X && Mx < X + Bw && My >= Y && My < Y + Bh; if (bOver) { Hot = Id; }
			GkRect(Cv, X, Y, Bw, Bh, bOn ? FLinearColor(0.6f, 0.38f, 0.04f, 0.97f) : (bOver ? FLinearColor(0.24f, 0.24f, 0.3f, 0.97f) : FLinearColor(0.09f, 0.09f, 0.12f, 0.95f)));
			GkText(Cv, Text, X + 10.f * U, Sub.IsEmpty() ? Y + (Bh - Size * 1.5f) * 0.5f : Y + 6.f * U, Size, Bone, false, 1);
			if (!Sub.IsEmpty()) { GkText(Cv, Sub, X + 10.f * U, Y + Bh - 24.f * U, 12.f * U, bSubGold ? Gold : Slate, false, 1); }
		};
		auto ClipLabel = [&](FName Slot) -> FString
		{
			const FGkClip* Has = Clips[LabSet].Find(Slot); if (!Has || !Has->Anim) { return FString(); }
			const FString Path = Has->Anim->GetPathName(); for (const FLabClip& L : LabList) { if (L.Path == Path) { return L.Label; } } return Has->Anim->GetName();
		};
		static const TCHAR* Who[3] = { TEXT("KAGU"), TEXT("NOA"), TEXT("ROBOT") };
		static const TCHAR* Tabs[9] = { TEXT("MOVEMENT"), TEXT("GUARD"), TEXT("HIT REACTIONS"), TEXT("PUNCHES"), TEXT("KICKS"), TEXT("AIR + FLYING"), TEXT("SPECIALS"), TEXT("SUPERS"), TEXT("THROWS") };
		static const TCHAR* Kinds[9] = { TEXT("MOVEMENT"), TEXT("BLOCKS"), TEXT("HITS"), TEXT("FALL / GET UP"), TEXT("PUNCHES"), TEXT("KICKS"), TEXT("COMBOS"), TEXT("ENERGY"), TEXT("FLYING") };
		const float Pw = W * 0.6f, Lx = 24.f * U, Gw = Pw - 48.f * U;
		GkRect(Cv, 0.f, 0.f, Pw, H, FLinearColor(0.02f, 0.02f, 0.03f, 0.88f));
		GkText(Cv, TEXT("ANIMATIONS"), Lx, 18.f * U, 30.f * U, Bone);
		Btn(Lx + 260.f * U, 18.f * U, 250.f * U, 44.f * U, 7, FString::Printf(TEXT("CHARACTER:  %s"), Who[FMath::Clamp(LabSet, 0, 2)]), 16.f * U, false);
		Btn(Pw - 24.f * U - 130.f * U, 18.f * U, 130.f * U, 44.f * U, 1, TEXT("BACK"), 16.f * U, false);
		const float Tw = Gw / 9.f;
		for (int32 k = 0; k < 9; k++) { Btn(Lx + k * Tw, 78.f * U, Tw - 4.f * U, 42.f * U, 100 + k, Tabs[k], 12.f * U, LabCat == k); }
		const float Sp = 8.f * U, Cw = (Gw - 2.f * Sp) / 3.f, Ch = 58.f * U, Gy = 134.f * U;
		int32 Cell = 0; float DropX = 0.f, DropY = 0.f;
		for (int32 k = 0; k < LabSlots.Num(); k++)
		{
			if (LabSlots[k].Cat != LabCat) { continue; }
			const float X = Lx + (Cell % 3) * (Cw + Sp), Y = Gy + (Cell / 3) * (Ch + Sp); Cell++;
			const FString Has = ClipLabel(LabSlots[k].Slot);
			Btn(X, Y, Cw, Ch, 1000 + k, LabSlots[k].Label, 15.f * U, k == LabSlot, Has.IsEmpty() ? FString(TEXT("hand pose (no clip)")) : Has, !Has.IsEmpty());
			if (k == LabSlot) { DropX = X; DropY = Y + Ch; }
		}
		// the fighter's side: what is selected, and its trim
		if (LabSlots.IsValidIndex(LabSlot))
		{
			const float Rx = Pw + 30.f * U, Ry = H - 230.f * U; const FGkClip* Has = Clips[LabSet].Find(LabSlots[LabSlot].Slot); const FString Name = ClipLabel(LabSlots[LabSlot].Slot);
			GkText(Cv, LabSlots[LabSlot].Label, Rx, Ry, 26.f * U, Bone);
			GkText(Cv, bLabDrop && LabList.IsValidIndex(LabTryAt) ? FString::Printf(TEXT("trying:  %s"), *LabList[LabTryAt].Label) : (Name.IsEmpty() ? FString(TEXT("hand pose (no clip)")) : Name), Rx, Ry + 40.f * U, 18.f * U, Gold);
			if (Has && Has->Anim && !bLabDrop)
			{
				GkText(Cv, FString::Printf(TEXT("START  %.2f"), Has->From), Rx, Ry + 86.f * U, 15.f * U, Bone); Btn(Rx + 130.f * U, Ry + 78.f * U, 40.f * U, 36.f * U, 2, TEXT("-"), 16.f * U, false); Btn(Rx + 176.f * U, Ry + 78.f * U, 40.f * U, 36.f * U, 3, TEXT("+"), 16.f * U, false);
				GkText(Cv, FString::Printf(TEXT("END  %.2f"), Has->To), Rx + 250.f * U, Ry + 86.f * U, 15.f * U, Bone); Btn(Rx + 360.f * U, Ry + 78.f * U, 40.f * U, 36.f * U, 4, TEXT("-"), 16.f * U, false); Btn(Rx + 406.f * U, Ry + 78.f * U, 40.f * U, 36.f * U, 5, TEXT("+"), 16.f * U, false);
				Btn(Rx + 480.f * U, Ry + 78.f * U, 150.f * U, 36.f * U, 6, Has->bLoop ? TEXT("LOOP: ON") : TEXT("LOOP: OFF"), 14.f * U, Has->bLoop);
			}
			GkText(Cv, TEXT("Click a move, then pick a clip from the list."), Rx, H - 62.f * U, 14.f * U, GkFade(Bone, 0.75f)); GkText(Cv, TEXT("Pointing at a clip plays it. Keys work too: arrows, ENTER, TAB. ESC goes back."), Rx, H - 40.f * U, 14.f * U, GkFade(Bone, 0.75f));
		}
		if (bLabDrop && LabSlots.IsValidIndex(LabSlot))
		{
			const int32 Rows = 12; const float Rh = 30.f * U, Dw = FMath::Max(Cw, 520.f * U), Dh = 3.f * 34.f * U + (Rows + 1) * Rh + 34.f * U;
			float X = FMath::Min(DropX, Pw - Dw - 10.f * U), Y = DropY; if (Y + Dh > H - 10.f * U) { Y = FMath::Max(10.f * U, DropY - Ch - Dh); }
			if (Mx >= X && Mx < X + Dw && My >= Y && My < Y + Dh) { Hot = -1; }
			GkRect(Cv, X - 3.f * U, Y - 3.f * U, Dw + 6.f * U, Dh + 6.f * U, Gold); GkRect(Cv, X, Y, Dw, Dh, FLinearColor(0.03f, 0.03f, 0.05f, 1.f));
			for (int32 k = 0; k < 9; k++) { Btn(X + (k % 3) * (Dw / 3.f), Y + (k / 3) * 34.f * U, Dw / 3.f - 3.f * U, 31.f * U, 200 + k, Kinds[k], 12.f * U, LabDropCat == k); }
			TArray<int32> Show; for (int32 k = 0; k < LabList.Num(); k++) { if (LabList[k].Cat == LabDropCat) { Show.Add(k); } }
			LabScroll = FMath::Clamp(LabScroll, 0, FMath::Max(0, Show.Num() - Rows));
			const FGkClip* Has = Clips[LabSet].Find(LabSlots[LabSlot].Slot); const FString Now = (Has && Has->Anim) ? Has->Anim->GetPathName() : FString();
			float Ry = Y + 3.f * 34.f * U;
			Btn(X, Ry, Dw, Rh - 2.f * U, 9999, TEXT("NONE  (hand pose)"), 13.f * U, Now.IsEmpty()); if (LabKey == -1) { GkRect(Cv, X, Ry, 5.f * U, Rh - 2.f * U, Gold); } Ry += Rh;
			for (int32 r = 0; r < Rows && LabScroll + r < Show.Num(); r++) { const int32 k = Show[LabScroll + r]; Btn(X, Ry + r * Rh, Dw, Rh - 2.f * U, 10000 + k, LabList[k].Label, 13.f * U, LabList[k].Path == Now); if (LabKey == LabScroll + r) { GkRect(Cv, X, Ry + r * Rh, 5.f * U, Rh - 2.f * U, Gold); } }
			GkText(Cv, FString::Printf(TEXT("%d - %d of %d      scroll for more"), Show.Num() ? LabScroll + 1 : 0, FMath::Min(Show.Num(), LabScroll + Rows), Show.Num()), X + 10.f * U, Y + Dh - 28.f * U, 12.f * U, Slate, false, 1);
		}
		LabHot = Hot;
		if (LabNoteTimer > 0) { GkText(Cv, LabNote, Pw + 30.f * U, 24.f * U, 18.f * U, Jade, false, 2); }
		return;
	}

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
			GkHudPanel(Cv,X0-20.f*U,Top-5.f*U,BarW+40.f*U,BarH+11.f*U,12.f*U,FLinearColor(.36,.39,.42));
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
   GkHudPanel(Cv,X-104*U,295*U,205*U,94*U,26*U,GkFade(Ink,.7f*Fade));
   GkText(Cv,FString::FromInt(Combo[i]),X-20*U,260*U,94*U*Pop,GkFade(Bone,Fade),true,4);
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
  const float X=W*.56f,Y=H*.57f;
  GkHudPanel(Cv,X,Y,W*.42f,H*.31f,30*U,Ink);
  GkText(Cv,TEXT("VICTORY"),X+42*U,Y+10*U,68*U,Gold,false,4);
  GkText(Cv,S.winner>=0?Names[S.winner]+TEXT(" / ROBOT 1"):TEXT("DRAW GAME"),X+42*U,Y+100*U,23*U,Bone);
  GkText(Cv,TEXT("LET'S DO THAT AGAIN."),X+42*U,Y+144*U,20*U,Tint[FMath::Clamp(S.winner,0,1)]);
  if(OverTimer>50)GkText(Cv,TEXT("ATTACK  rematch     TAB  menu"),X+42*U,Y+208*U,16*U,Bone);
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
 // A fast diagonal ink wipe bridges pages without flashing white over the match.
 if(PageAge<.32f && UiPageKey>=0) {
  const float P=PageAge/.32f;
  for(int32 k=0;k<5;++k){float X=FMath::Lerp(-W*.8f,W*1.3f,P)+k*W*.055f;
   GkHudPanel(Cv,X,-H*.1f,W*.34f,H*1.2f,H*.22f,k==4?Red:Ink);
  }
 }
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
 if(CallHold<=0)return;
 const float W=Cv->SizeX,H=Cv->SizeY,U=H/1080.f,Mid=W*.5f;
 const bool KO=CallWord.Contains(TEXT("K.O.")),Fight=CallWord==TEXT("FIGHT!");
 const float Out=FMath::Clamp((CallHold-CallAge)/10.f,0.f,1.f),In=FMath::Clamp(CallAge/8.f,0.f,1.f);
 const float X=Mid+(1-In)*(1-In)*W*.55f-(1-Out)*W*.6f,Y=H*.35f;
 const float Size=(KO?142.f:Fight?130.f:85.f)*U*(1.f+.17f*(1-In));
 const FLinearColor Ink(.012,.018,.032,.92*Out),Red(1,.09,.23,Out),White(.98,.96,.91,Out);
 // Asymmetric knife strokes and offset ink outlines, with the live match visible around them.
 GkHudPanel(Cv,X-W*.41f,Y+30*U,W*.82f,125*U,60*U,Ink);
 GkHudPanel(Cv,X-W*.35f,Y+149*U,W*.67f,8*U,4*U,Red);
 for(int32 k=0;k<7;++k){const float Dx=(k*41+CallAge*16)%170;
  GkHudPanel(Cv,X-W*.45f-Dx*U,Y+(k*21)*U,(72+k*7)*U,3*U,2*U,White);
  GkHudPanel(Cv,X+W*.31f+Dx*U,Y+(k*21)*U,(100-k*5)*U,3*U,2*U,Red);
 }
 GkText(Cv,CallWord,X+5*U,Y-25*U+5*U,Size,Red,true,7);
 GkText(Cv,CallWord,X,Y-25*U,Size,White,true,5);
 GkJapanese(Cv,KO?TEXT("決着"):Fight?TEXT("勝負開始"):TEXT("準備はいいか"),X-65*U,Y+166*U,26*U,White);
}

void AGkMatch::DrawArcadeMenu(UCanvas* Cv)
{
 const float W=Cv->SizeX,H=Cv->SizeY,U=H/1080.f;
 const FLinearColor Ink(.014,.021,.037,.94),White(.97,.96,.91),Red(1,.09,.23),Gold(1,.72,.18),Muted(.54,.62,.7);
 const bool Pause=Screen==3;
 const float Enter=1-FMath::Pow(1-FMath::Clamp(PageAge/.30f,0.f,1.f),3.f);
 const float X=66*U-(1-Enter)*100*U,Y=210*U,PanelW=650*U;
 if(Pause)GkRect(Cv,0,0,W,H,FLinearColor(0,.01,.025,.58));
 // Scene remains fully live to the right; only the controls get an ink backing.
 if((Screen==0 || Screen==6) || MenuPage || Pause)GkHudPanel(Cv,-100*U,0,870*U,H,180*U,Ink);
 GkRect(Cv,0,0,W,5*U,Red);
 if(Screen!=5){
 GkText(Cv,TEXT("GEKITOTSU"),X,45*U,62*U,White,false,3);
 GkJapanese(Cv,TEXT("激突"),X+430*U,55*U,40*U,Red);
 GkText(Cv,TEXT("SMALL FUSE. BIG PERSONALITY."),X,133*U,15*U,Gold,false,0);
 }
 const FString Footer=Screen==5?TEXT("START / ENTER   begin"):PadNames.IsEmpty()?TEXT("ARROWS / WASD   choose       ENTER / J   confirm       ESC / L   back"):TEXT("D-PAD   choose       LIGHT   confirm       HEAVY   back       START   pause");
 GkRect(Cv,0,H-64*U,W,64*U,Ink);
 GkText(Cv,Footer,66*U,H-43*U,17*U,White,false,0);
 GkText(Cv,TEXT("ROBOT 01 / DREADMARSH"),W-400*U,H-43*U,14*U,Muted,false,0);
 auto List=[&](const FString& Title,const TArray<FString>& Rows,int32 Selected,float RowHeight=70.f){
  GkText(Cv,Title,X,Y,23*U,Gold,false,0);
  for(int32 k=0;k<Rows.Num();++k){const bool On=k==Selected;const float Ry=Y+60*U+k*RowHeight*U;
   const float Kick=On?10*FMath::Exp(-CursorAge*14):0;
   if(On){GkHudPanel(Cv,X-20*U+Kick*U,Ry-4*U,PanelW,(RowHeight-9)*U,16*U,Red);
    GkRect(Cv,X-20*U,Ry-4*U,5*U,(RowHeight-9)*U,White);
    // Slow highlight travels along a narrow rail, never obscuring the label.
    const float Progress=FMath::Fmod(UiTime*.4f,1.f);
    GkRect(Cv,X+Progress*(PanelW-80*U),Ry+(RowHeight-16)*U,60*U,2*U,White);
   }
   GkText(Cv,FString::Printf(TEXT("%02d"),k+1),X+4*U,Ry+12*U,14*U,On?White:Muted,false,0);
   GkText(Cv,Rows[k],X+50*U+(On?9*U:0),Ry+5*U,FMath::Min(On?28.f:25.f,560.f/FMath::Max(1.f,Rows[k].Len()*.57f))*U,On?White:Muted,false,On?2:0);
  }
 };
 if(!bPlaced){List(TEXT("GETTING THE STAGE READY"),{TEXT("DREADMARSH")},0);return;}
 if(MenuPage==2){
  List(TEXT("SOUND & PRESENTATION"),{
   FString::Printf(TEXT("MUSIC     < %d%% >"),FMath::RoundToInt(MusicGain*100)),
   FString::Printf(TEXT("SOUND FX     < %d%% >"),FMath::RoundToInt(EffectsGain*100)),
   FString::Printf(TEXT("VOICES     < %d%% >"),FMath::RoundToInt(VoiceGain*100)),
   FString::Printf(TEXT("CAMERA SHAKE     < %d%% >"),FMath::RoundToInt(ShakeGain*100)),
   FString::Printf(TEXT("LIGHT FLASHES     < %d%% >"),FMath::RoundToInt(FlashGain*100)),TEXT("BACK")},MenuAt);
  GkText(Cv,TEXT("LEFT / RIGHT adjusts. Changes save automatically."),X,H-126*U,16*U,Gold,false,0);return;
 }
 if(MenuPage==3){
  List(TEXT("MAKE SOME TROUBLE / MOVE LIST"),{
   TEXT("L / M / H    normal attacks"),TEXT("ASSIST + L / M / H    kicks"),TEXT("SPECIAL    energy shot / air beam"),TEXT("FORWARD + SPECIAL    anti-air"),TEXT("HOMING    chase through the air"),TEXT("L + M    throw / M + H    run"),TEXT("H + SP    super / BACK    level 2"),TEXT("DOWN + H + SP    level 3"),TEXT("PARRY    read them / IMPACT    break in")},-1,61);
  GkText(Cv,TEXT("Close the distance. Confirm your hit. Spend your meter."),X,H-123*U,16*U,Gold);return;
 }
 if(MenuPage==1){
  static const TCHAR* Actions[]={TEXT("LIGHT"),TEXT("MEDIUM"),TEXT("HEAVY"),TEXT("SPECIAL"),TEXT("ASSIST"),TEXT("PARRY"),TEXT("IMPACT"),TEXT("HOMING")};
  TArray<FString> Rows;for(int32 k=0;k<8;++k){FString Key=PadCount>0?FString::Printf(TEXT("BUTTON %d"),PadBind[k]+1):Bind[k].GetDisplayName().ToString();
   Rows.Add(FString(Actions[k])+TEXT("    ")+(k==BindAt?TEXT("PRESS NOW"):k<BindAt?Key:TEXT("--")));}
  List(TEXT("YOUR BUTTONS / YOUR STYLE"),Rows,BindAt,61.f);
  GkText(Cv,TEXT("TAB / ESC cancels. Directions stay on the stick."),X,H-126*U,16*U,Gold,false,0);return;
 }
 if(Screen==5){
  GkHudPanel(Cv,0,H*.65f,W,H*.26f,110*U,Ink);
  GkText(Cv,TEXT("GEKITOTSU"),66*U,H*.67f,112*U,White,false,5);
  GkJapanese(Cv,TEXT("激突"),W*.68f,H*.67f,82*U,Red);
  GkText(Cv,TEXT("PRESS START"),W*.5f,H*.85f,26*U,GkFade(Gold,.65f+.35f*FMath::Sin(UiTime*3.f)),true,2);
  return;
 }
 if(Screen==7){
  GkHudPanel(Cv,0,H*.63f,W,H*.28f,90*U,Ink);
  GkText(Cv,TEXT("ROBOT 1"),W*.22f,H*.68f,58*U,Red,true,3);
  GkText(Cv,TEXT("VS"),W*.5f,H*.65f,90*U,Gold,true,4);
  GkText(Cv,TEXT("ROBOT 1"),W*.78f,H*.68f,58*U,FLinearColor(.3,.7,1),true,3);
  GkText(Cv,TEXT("DREADMARSH / LET'S MAKE SOME NOISE"),W*.5f,H*.84f,18*U,White,true,1);return;
 }
 if(Screen==6){
  List(TEXT("THAT'S A WRAP."),{TEXT("RUN IT BACK"),TEXT("CHARACTER SELECT"),TEXT("MAIN MENU"),TEXT("QUIT GAME")},MenuAt,82);
  GkText(Cv,TEXT("VICTORY"),W*.57f,H*.74f,78*U,Gold,false,4);
  GkText(Cv,S.winner<0?TEXT("DRAW GAME"):S.winner==0?TEXT("P1 / ROBOT 1"):TEXT("P2 / ROBOT 1"),W*.58f,H*.85f,26*U,White);return;
 }
 if(Screen==0){
  List(TEXT("LET'S MAKE SOME NOISE."),{TEXT("VERSUS CPU"),TEXT("TRAINING"),TEXT("WATCH"),FString::Printf(TEXT("CPU LEVEL    < %d >"),Level),TEXT("SET UP BUTTONS"),TEXT("HOW TO PLAY"),TEXT("SOUND & PRESENTATION"),TEXT("QUIT GAME")},MenuAt,72);
  GkText(Cv,TEXT("01"),W-330*U,H*.64f,145*U,White,false,4);
  GkText(Cv,TEXT("ROBOT"),W-340*U,H*.82f,44*U,Red,false,2);
 }
 else if(Screen==1){
  GkHudPanel(Cv,0,H*.69f,W,H*.22f,90*U,Ink);
  GkJapanese(Cv,TEXT("挑戦者"),66*U,H*.72f,34*U,Gold);
  GkText(Cv,TEXT("ROBOT 1"),66*U,H*.77f,72*U,White,false,3);
  GkText(Cv,TEXT("RUSH DOWN / HIGH ENERGY / ZERO HUMILITY"),W*.43f,H*.73f,23*U,Gold);
  GkText(Cv,FString::Printf(TEXT("P%d / %s"),SelWho+1,CostumeName(Setup.Costume[SelWho])),W*.43f,H*.79f,28*U,White);
  for(int32 Outfit=0;Outfit<8;++Outfit){const float Sx=W*.43f+Outfit*66*U,Sy=H*.85f;const bool Selected=Setup.Costume[SelWho]==Outfit;
   GkHudPanel(Cv,Sx-3*U,Sy-3*U,59*U,33*U,6*U,Selected?White:Ink);
   GkHudPanel(Cv,Sx,Sy,53*U,27*U,5*U,CostumeColor(Outfit));
   GkRect(Cv,Sx+34*U,Sy+3*U,12*U,21*U,CostumeColor(Outfit,true));
  }
  GkText(Cv,TEXT("LEFT / RIGHT  colour     LIGHT  lock in"),66*U,H*.88f,15*U,Gold);
 }
 else if(Screen==2){
  GkHudPanel(Cv,0,H*.57f,W,H*.34f,80*U,Ink);
  GkText(Cv,TEXT("CHOOSE YOUR STAGE"),66*U,H*.61f,21*U,Gold);
  const FString Stage=TEXT("DREADMARSH");
  GkText(Cv,Stage.ToUpper(),66*U,H*.66f,66*U,White,false,3);
  GkText(Cv,FString::Printf(TEXT("<   %s   >"),MenuPalette==0?TEXT("DYNAMIC WEATHER"):MenuPalette==1?TEXT("DAYLIGHT"):MenuPalette==2?TEXT("MOONLIGHT"):MenuPalette==3?TEXT("STORM"):MenuPalette==4?TEXT("WARM HAZE"):MenuPalette==5?TEXT("TOXIC GREEN"):TEXT("TWILIGHT")),66*U,H*.80f,23*U,Red);
  GkText(Cv,TEXT("LIVE STAGE / DYNAMIC WEATHER"),W*.62f,H*.64f,19*U,Gold);
  GkText(Cv,TEXT("DAY / NIGHT / STORM"),W*.62f,H*.71f,24*U,White);
  GkText(Cv,TEXT("HAZE / TOXIC / TWILIGHT"),W*.62f,H*.76f,24*U,White);
 }
 else {
  static const TCHAR* DummyNames[]={TEXT("STAND"),TEXT("CROUCH"),TEXT("JUMP"),TEXT("GUARD"),TEXT("FIGHT BACK")};
  if(!bDebugMenu) {
   List(TEXT("PAUSED / TAKE A BREATHER."),{TEXT("RESUME"),TEXT("RESTART MATCH"),TEXT("MOVE LIST"),TEXT("SOUND & PRESENTATION"),TEXT("CHARACTER SELECT"),TEXT("MAIN MENU"),TEXT("QUIT GAME")},MenuAt,72);return;
  }
  List(TEXT("DEBUG / QUICK ACCESS"),{TEXT("RESUME"),TEXT("RESTART MATCH"),TEXT("CHARACTER SELECT"),TEXT("STAGE SELECT"),TEXT("MAIN MENU"),
   FString::Printf(TEXT("DUMMY    < %s >"),DummyNames[FMath::Clamp(Dummy,0,4)]),FString::Printf(TEXT("HIT BOXES    < %s >"),bBoxes?TEXT("ON"):TEXT("OFF")),TEXT("SOUND & PRESENTATION"),TEXT("SET UP BUTTONS"),TEXT("ANIMATION LAB")},MenuAt,61);
 }
}
