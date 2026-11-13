#include "GkMatch.h"
#include "GkPad.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Engine/Font.h"

// A plain working HUD for the first milestone, drawn straight onto the canvas: the gauges, the calls, the training
// read-outs, the hit and hurt boxes, and the menu.

static void GkRect(UCanvas* Cv, float X, float Y, float W, float H, const FLinearColor& Col)
{
	if (W <= 0.f || H <= 0.f || Col.A <= 0.f) { return; }
	FCanvasTileItem Item(FVector2D(X, Y), GWhiteTexture, FVector2D(W, H), Col); Item.BlendMode = SE_BLEND_Translucent; Cv->DrawItem(Item);
}
static void GkText(UCanvas* Cv, const FString& Str, float X, float Y, float Size, const FLinearColor& Col, bool bCentre = false, int32 Outline = 2)
{
	static TWeakObjectPtr<UFont> Carrier;
	if (!Carrier.IsValid()) { Carrier = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")); }
	if (!Carrier.IsValid() || Col.A <= 0.f) { return; }
	const FFontOutlineSettings Line = Outline > 0 ? FFontOutlineSettings(Outline, FLinearColor(0.f, 0.f, 0.f, Col.A)) : FFontOutlineSettings();
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), Carrier.Get(), Col);
	Item.SlateFontInfo = FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), FMath::Max(4.f, Size), Line);
	Item.bCentreX = bCentre; Cv->DrawItem(Item);
}
static FLinearColor GkFade(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * FMath::Clamp(A, 0.f, 1.f)); }

void AGkMatch::DrawHud(UCanvas* Cv)
{
	if (!Cv) { return; }
	const float W = Cv->SizeX, H = Cv->SizeY, U = H / 1080.f, Mid = W * 0.5f;
	const FLinearColor Ink(0.02f, 0.02f, 0.03f, 0.85f), Bone(0.96f, 0.94f, 0.9f, 1.f), Gold(1.f, 0.78f, 0.2f, 1.f), Red(0.9f, 0.16f, 0.12f, 1.f), Blue(0.35f, 0.65f, 1.f, 1.f), Ash(0.3f, 0.3f, 0.34f, 0.9f), Jade(0.25f, 0.9f, 0.5f, 1.f), Slate(0.55f, 0.55f, 0.6f, 1.f);
	const FLinearColor Tint[2] = { Red, Blue }; const FString Names[2] = { TEXT("PLAYER 1"), Mode == 1 ? TEXT("DUMMY") : TEXT("CPU") };
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
		if (Word) { const float Ww = (FCString::Strlen(Word) * 15.f + 26.f) * U; GkRect(Cv, Head.X - Ww * 0.5f, Yy, Ww, 30.f * U, Chip); GkText(Cv, Word, Head.X, Yy + 2.f * U, 19.f * U, Bone, true, 2); Yy -= 34.f * U; }
		if (P.bo) { const float Ww = 136.f * U, Bl = 0.65f + 0.35f * FMath::Sin(Ticks * 0.3f); GkRect(Cv, Head.X - Ww * 0.5f, Yy, Ww, 30.f * U, FLinearColor(0.3f, 0.32f, 0.3f, Bl)); GkText(Cv, TEXT("BURNOUT"), Head.X, Yy + 2.f * U, 19.f * U, Bone, true, 2); }
		if (Mode == 1 && P.st == gk::ATK) { GkText(Cv, FString::Printf(TEXT("%s  %d"), UTF8_TO_TCHAR(gk::moveName(P.mv)), P.mf), Head.X, Head.Y + 6.f * U, 15.f * U, Gold, true); }
	}
	for (const FSpark& Sp : Sparks)
	{
		const FVector P = Cv->Project(Sp.At); const float K = Sp.Age / 12.f, R = (20.f + 70.f * K) * U; const FLinearColor C = Sp.Kind == 0 ? Gold : Sp.Kind == 1 ? Bone : Blue;
		GkRect(Cv, P.X - R, P.Y - 3.f * U, R * 2.f, 6.f * U, GkFade(C, 1.f - K)); GkRect(Cv, P.X - 3.f * U, P.Y - R, 6.f * U, R * 2.f, GkFade(C, 1.f - K));
	}

	// a marker over each fighter, so two of the same character can be told apart
	if (Screen == 3 && bPlaced)
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
	const float BarW = 700.f * U, BarH = 26.f * U, Top = 56.f * U, Gap = 80.f * U;
	for (int32 i = 0; i < 2; i++)
	{
		const gk::Fighter& P = S.p[i]; const float K = FMath::Clamp(P.hp / 1000.f, 0.f, 1.f), Lag = FMath::Clamp(P.red / 1000.f, K, 1.f);
		const float X0 = i ? Mid + Gap : Mid - Gap - BarW;
		GkRect(Cv, X0 - 4.f * U, Top - 4.f * U, BarW + 8.f * U, BarH + 8.f * U, Ink); GkRect(Cv, X0, Top, BarW, BarH, Ash);
		// health empties away from the clock; the pale part is damage still being taken
		if (i) { GkRect(Cv, X0, Top, BarW * Lag, BarH, Bone); GkRect(Cv, X0, Top, BarW * K, BarH, K <= 0.25f ? Gold : Tint[i]); }
		else { GkRect(Cv, X0 + BarW * (1.f - Lag), Top, BarW * Lag, BarH, Bone); GkRect(Cv, X0 + BarW * (1.f - K), Top, BarW * K, BarH, K <= 0.25f ? Gold : Tint[i]); }
		GkText(Cv, Names[i], i ? X0 + BarW - 150.f * U : X0, Top + BarH + 30.f * U, 20.f * U, Bone);
		// the Drive gauge: six bars
		for (int32 b = 0; b < 6; b++)
		{
			const float Part = FMath::Clamp((P.dr - b * 1000) / 1000.f, 0.f, 1.f), Dw = 56.f * U, Dh = 12.f * U, Dx = i ? Mid + Gap + b * (Dw + 6.f * U) : Mid - Gap - (b + 1) * (Dw + 6.f * U) + 6.f * U, Dy = Top + BarH + 10.f * U;
			GkRect(Cv, Dx, Dy, Dw, Dh, Ash); if (i) { GkRect(Cv, Dx, Dy, Dw * Part, Dh, P.bo ? Slate : Jade); } else { GkRect(Cv, Dx + Dw * (1.f - Part), Dy, Dw * Part, Dh, P.bo ? Slate : Jade); }
		}
		if (P.bo) { const float Bx = i ? Mid + Gap : Mid - Gap - 6.f * 62.f * U + 6.f * U, Bl = 0.5f + 0.5f * FMath::Sin(Ticks * 0.3f); GkRect(Cv, Bx - 3.f * U, Top + BarH + 7.f * U, 6.f * 62.f * U, 18.f * U, FLinearColor(0.75f, 0.1f, 0.08f, 0.35f + 0.4f * Bl)); GkText(Cv, TEXT("BURNOUT"), Bx + 130.f * U, Top + BarH + 5.f * U, 17.f * U, Bone, false, 2); }
		for (int32 r = 0; r < FMath::Clamp(S.cfg.rounds, 1, 3); r++) { GkRect(Cv, i ? Mid + Gap + r * 30.f * U : Mid - Gap - (r + 1) * 30.f * U + 8.f * U, Top - 26.f * U, 22.f * U, 12.f * U, r < P.wins ? Gold : Ash); }
		// the super gauge: three levels
		for (int32 b = 0; b < 3; b++)
		{
			const float Part = FMath::Clamp((P.sa - b * 1000) / 1000.f, 0.f, 1.f), Sw = 150.f * U, Sx = i ? W - 60.f * U - (b + 1) * (Sw + 10.f * U) : 60.f * U + b * (Sw + 10.f * U), Sy = H - 70.f * U;
			GkRect(Cv, Sx, Sy, Sw, 16.f * U, Ash); if (i) { GkRect(Cv, Sx + Sw * (1.f - Part), Sy, Sw * Part, 16.f * U, Part >= 1.f ? Gold : Tint[i]); } else { GkRect(Cv, Sx, Sy, Sw * Part, 16.f * U, Part >= 1.f ? Gold : Tint[i]); }
		}
		GkText(Cv, FString::Printf(TEXT("SUPER %d"), P.sa / 1000), i ? W - 180.f * U : 60.f * U, H - 104.f * U, 18.f * U, P.sa >= 1000 ? Gold : Bone);
		const float Cx = i ? W - 240.f * U : 240.f * U;
		if (ComboTimer[i] > 0 && Combo[i] >= 2) { GkText(Cv, FString::Printf(TEXT("%d HITS"), Combo[i]), Cx, 300.f * U, 54.f * U, Bone, true, 3); GkText(Cv, FString::Printf(TEXT("%d"), ComboDmg[i]), Cx, 372.f * U, 26.f * U, Tint[i], true); }
		if (NoteTimer[i] > 0) { GkText(Cv, Note[i], Cx, 420.f * U, 28.f * U, GkFade(Gold, NoteTimer[i] / 12.f), true); }
	}
	GkRect(Cv, Mid - 56.f * U, Top - 22.f * U, 112.f * U, 76.f * U, Ink);
	GkText(Cv, FString::Printf(TEXT("%02d"), FMath::Clamp((S.time + 59) / 60, 0, 99)), Mid, Top - 20.f * U, 50.f * U, Bone, true);
	GkText(Cv, ModeName[FMath::Clamp(Mode, 0, 2)], Mid, Top + 60.f * U, 16.f * U, Gold, true);

	if (CallHold > 0) { const float A = FMath::Min(1.f, FMath::Min(CallAge / 5.f, (CallHold - CallAge) / 8.f)); GkRect(Cv, 0.f, H * 0.36f, W, 120.f * U, GkFade(Ink, A)); GkText(Cv, CallWord, Mid, H * 0.36f + 14.f * U, 76.f * U, GkFade(Bone, A), true, 4); }
	if (S.phase == gk::PH_OVER)
	{
		GkRect(Cv, 0.f, H * 0.36f, W, 150.f * U, Ink);
		GkText(Cv, S.winner == 0 || S.winner == 1 ? Names[S.winner] + TEXT(" WINS") : FString(TEXT("DRAW GAME")), Mid, H * 0.36f + 14.f * U, 76.f * U, Bone, true, 4);
		if (OverTimer > 50) { GkText(Cv, TEXT("press any attack button for the next match"), Mid, H * 0.36f + 112.f * U, 20.f * U, Bone, true); }
	}
	// what the controls are sending right now: the way to check a stick
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
	GkText(Cv, FString::Printf(TEXT("%d fps"), FMath::RoundToInt(1000.f / FMath::Max(1.f, FrameMs))), W - 90.f * U, 8.f * U, 15.f * U, FrameMs < 18.f ? Jade : Gold);
	if (Mode == 1 || bMenu) { GkText(Cv, TEXT("TAB / START  menu      R / SELECT  restart      B  boxes"), Mid, H - 28.f * U, 13.f * U, GkFade(Bone, 0.7f), true); }
	// exactly what is arriving from the stick, so a controller that is seen but not heard can be told apart from one that is mapped wrongly
	if (Pads && Mode == 1 && bBoxes) { GkText(Cv, FString::Printf(TEXT("%s   buttons %08x  dirs %d"), *Pads->Debug(), PadNow, PadDirs), Mid, H - 48.f * U, 12.f * U, Gold, true); }

	}
	if (bMenu || Screen != 3 || !bPlaced)
	{
		GkRect(Cv, 0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, Screen == 3 ? 0.72f : 0.5f));
		const float Px = Mid - 400.f * U, Py = H * 0.14f, Ph = 720.f * U;
		const FString Stick = !PadNames.IsEmpty() ? FString::Printf(TEXT("Stick / pad over USB:  %s%s"), *PadNames.Left(60), PadCount > 0 ? TEXT("") : TEXT("   (press a button on it)")) : FString(TEXT("No stick or pad seen over USB.  Keyboard, or a pad Unreal knows."));
		auto List = [&](const TCHAR* Title, const TArray<FString>& Rows, int32 Sel, float Row)
		{
			GkRect(Cv, Px, Py, 800.f * U, Ph, Ink); GkRect(Cv, Px, Py + Ph, 800.f * U, 4.f * U, Red);
			GkText(Cv, Title, Mid, Py + 22.f * U, 44.f * U, Bone, true);
			for (int32 k = 0; k < Rows.Num(); k++)
			{
				const bool bSel = k == Sel; const float Iy = Py + (110.f + k * Row) * U;
				if (bSel) { GkRect(Cv, Px + 40.f * U, Iy, 720.f * U, (Row - 12.f) * U, Red); }
				GkText(Cv, Rows[k], Mid, Iy + (Row - 12.f - 30.f) * 0.5f * U, 26.f * U, bSel ? Bone : GkFade(Bone, 0.7f), true);
			}
			GkText(Cv, Stick, Mid, Py + 640.f * U, 16.f * U, Gold, true);
			GkText(Cv, TEXT("UP / DOWN choose     LEFT / RIGHT change     LIGHT or ENTER confirm     HEAVY or ESC back"), Mid, Py + 676.f * U, 14.f * U, Bone, true);
		};
		if (!bPlaced) { GkText(Cv, TEXT("LOADING THE STAGE"), Mid, H * 0.45f, 40.f * U, Bone, true); }
		else if (MenuPage == 1)
		{
			GkRect(Cv, Px, Py, 800.f * U, Ph, Ink); GkRect(Cv, Px, Py + Ph, 800.f * U, 4.f * U, Red);

			static const TCHAR* Act[] = { TEXT("LIGHT"), TEXT("MEDIUM"), TEXT("HEAVY"), TEXT("SPECIAL"), TEXT("ASSIST"), TEXT("PARRY"), TEXT("IMPACT"), TEXT("HOMING") };
			GkText(Cv, TEXT("SET UP BUTTONS"), Mid, Py + 28.f * U, 38.f * U, Bone, true);
			for (int32 a = 0; a < 8; a++)
			{
				const bool bNow = a == BindAt; const float Iy = Py + (110.f + a * 46.f) * U;
				const FString Has = PadCount > 0 ? FString::Printf(TEXT("button %d"), PadBind[a] + 1) : Bind[a].GetDisplayName().ToString();
				if (bNow) { GkRect(Cv, Px + 40.f * U, Iy - 4.f * U, 720.f * U, 40.f * U, Red); }
				GkText(Cv, Act[a], Px + 70.f * U, Iy, 22.f * U, a <= BindAt ? Bone : GkFade(Bone, 0.5f));
				GkText(Cv, bNow ? FString(TEXT("press it now")) : (a < BindAt ? Has : FString()), Px + 480.f * U, Iy, 22.f * U, bNow ? Gold : Bone);
			}
			GkText(Cv, Stick, Mid, Py + 520.f * U, 16.f * U, Gold, true);
			GkText(Cv, TEXT("TAB or ESC cancels.  Directions come from the stick or d-pad."), Mid, Py + 556.f * U, 16.f * U, Bone, true);
			GkText(Cv, TEXT("Throw: Light + Medium.   Quick Dash: Medium + Heavy.   Supers: Heavy + Special."), Mid, Py + 588.f * U, 16.f * U, Bone, true);
		
		}
		else if (Screen == 0)
		{
			GkText(Cv, TEXT("GEKITOTSU"), Mid, H * 0.035f, 64.f * U, Bone, true, 4);
			List(TEXT("MAIN MENU"), { FString(TEXT("VERSUS CPU")), FString(TEXT("TRAINING")), FString(TEXT("WATCH")), FString::Printf(TEXT("CPU LEVEL    <  %d  >"), Level), FString(TEXT("SET UP BUTTONS")), FString(TEXT("QUIT")), FString(TEXT("ANIMATIONS")) }, MenuAt, 68.f);
		}
		else if (Screen == 1)
		{
			// choosing fighters: the roster's two rows of tiles, most of them still empty
			static const TCHAR* Who[4] = { TEXT("KAGU"), TEXT("NOA"), TEXT("ROBOT"), TEXT("ROBOT 2") };
			GkText(Cv, TEXT("SELECT CHARACTER"), Mid, H * 0.06f, 56.f * U, Bone, true, 4);
			const float Tile = 104.f * U, Sp = 6.f * U, X0 = Mid - 4.f * (Tile + Sp), Y0 = H * 0.66f;
			for (int32 r = 0; r < 2; r++) { for (int32 c = 0; c < 8; c++)
			{
				const float X = X0 + c * (Tile + Sp), Y = Y0 + r * (Tile + Sp); const bool bHas = r == 0 && c < 4;
				GkRect(Cv, X, Y, Tile, Tile, bHas ? FLinearColor(0.2f, 0.21f, 0.25f, 0.95f) : FLinearColor(0.08f, 0.08f, 0.1f, 0.85f));
				if (bHas) { GkText(Cv, Who[c], X + Tile * 0.5f, Y + Tile * 0.4f, 17.f * U, Bone, true); }
				else if (r == 0 && c == 5) { GkText(Cv, TEXT("?"), X + Tile * 0.5f, Y + Tile * 0.22f, 44.f * U, GkFade(Bone, 0.5f), true); }
				for (int32 p = 0; p < 2; p++)
				{
					if (bHas && Setup.Char[p] % 4 == c && (p <= SelWho))
					{
						const FLinearColor Edge = p ? Blue : Red; const float T = 5.f * U, In = p * 7.f * U;
						GkRect(Cv, X + In, Y + In, Tile - 2 * In, T, Edge); GkRect(Cv, X + In, Y + Tile - T - In, Tile - 2 * In, T, Edge); GkRect(Cv, X + In, Y + In, T, Tile - 2 * In, Edge); GkRect(Cv, X + Tile - T - In, Y + In, T, Tile - 2 * In, Edge);
					}
				}
			} }
			GkText(Cv, FString::Printf(TEXT("PLAYER 01   %s"), Who[Setup.Char[0] % 4]), W * 0.14f, H * 0.86f, 34.f * U, SelWho == 0 ? Bone : GkFade(Bone, 0.6f), true, 3);
			GkText(Cv, FString::Printf(TEXT("%s   %s"), Mode == 0 ? TEXT("CPU") : Mode == 1 ? TEXT("DUMMY") : TEXT("CPU 02"), Who[Setup.Char[1] % 4]), W * 0.86f, H * 0.86f, 34.f * U, SelWho == 1 ? Bone : GkFade(Bone, 0.6f), true, 3);
			GkText(Cv, SelWho == 0 ? TEXT("LEFT / RIGHT  choose player one's fighter      LIGHT  confirm      HEAVY  back") : TEXT("LEFT / RIGHT  choose the opponent      LIGHT  confirm      HEAVY  back"), Mid, H * 0.945f, 16.f * U, Gold, true);
		}
		else if (Screen == 2)
		{
			TArray<FString> Places; for (const FGkStage& St : Stages) { Places.Add(St.Name); }
			List(TEXT("SELECT STAGE"), Places, StageAt, FMath::Min(70.f, 500.f / FMath::Max(1, Places.Num())));
		}
		else
		{
			List(TEXT("PAUSED"), { FString(TEXT("RESUME")), FString(TEXT("RESTART MATCH")), FString(TEXT("CHARACTER SELECT")), FString(TEXT("STAGE SELECT")), FString(TEXT("MAIN MENU")),
				FString::Printf(TEXT("TRAINING DUMMY    <  %s  >"), DummyName[FMath::Clamp(Dummy, 0, 4)]), FString::Printf(TEXT("HIT BOXES    <  %s  >"), bBoxes ? TEXT("SHOWN") : TEXT("HIDDEN")), FString(TEXT("SET UP BUTTONS")) }, MenuAt, 62.f);
		}
	}
}
