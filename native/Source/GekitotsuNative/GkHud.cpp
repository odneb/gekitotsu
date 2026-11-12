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

	// hit and hurt boxes, straight from the rules: green where a fighter can be hit, red where a blow lands
	if (bBoxes)
	{
		auto Draw = [&](const gk::Box& B, const FLinearColor& C)
		{
			const FVector A0 = Cv->Project(At(B.x0, B.y1)), A1 = Cv->Project(At(B.x1, B.y0));
			const float X0 = FMath::Min(A0.X, A1.X), Y0 = FMath::Min(A0.Y, A1.Y), Bw = FMath::Abs(A1.X - A0.X), Bh = FMath::Abs(A1.Y - A0.Y);
			GkRect(Cv, X0, Y0, Bw, Bh, GkFade(C, 0.22f)); GkRect(Cv, X0, Y0, Bw, 2.f, C); GkRect(Cv, X0, Y0 + Bh - 2.f, Bw, 2.f, C); GkRect(Cv, X0, Y0, 2.f, Bh, C); GkRect(Cv, X0 + Bw - 2.f, Y0, 2.f, Bh, C);
		};
		for (int32 i = 0; i < 2; i++)
		{
			gk::Box Hu[12], Hi[9]; const int32 Nu = gk::hurtboxes(S.p[i], Hu), Nh = gk::hitboxes(S.p[i], Hi);
			for (int32 k = 0; k < Nu; k++) { Draw(Hu[k], Jade); } for (int32 k = 0; k < Nh; k++) { Draw(Hi[k], Red); }
		}
	}
	// what each fighter is doing, over his head: the state, and the move with its frame
	for (int32 i = 0; i < 2; i++)
	{
		static const TCHAR* StName[] = { TEXT(""), TEXT("PREJUMP"), TEXT("JUMP"), TEXT("LAND"), TEXT("DASH"), TEXT("BACKDASH"), TEXT("RUN"), TEXT("GHOST STEP"), TEXT(""), TEXT("BLOCK"), TEXT("HIT"), TEXT("LAUNCHED"), TEXT("DOWN"), TEXT("GET UP"), TEXT("THROWN"), TEXT("GRAB"), TEXT("WIN"), TEXT("PARRY"), TEXT("PARRY RECOVERY"), TEXT("TECH") };
		const gk::Fighter& P = S.p[i]; const FVector Head = Cv->Project(At(P.x, P.y + 100 * gk::F));
		const FString Label = P.st == gk::ATK ? FString::Printf(TEXT("%s  %d"), UTF8_TO_TCHAR(gk::moveName(P.mv)), P.mf) : FString(StName[FMath::Clamp<int32>(P.st, 0, 19)]);
		GkText(Cv, Label, Head.X, Head.Y - 26.f * U, 16.f * U, P.st == gk::ATK ? Gold : Bone, true);
	}
	for (const FSpark& Sp : Sparks)
	{
		const FVector P = Cv->Project(Sp.At); const float K = Sp.Age / 12.f, R = (20.f + 70.f * K) * U; const FLinearColor C = Sp.Kind == 0 ? Gold : Sp.Kind == 1 ? Bone : Blue;
		GkRect(Cv, P.X - R, P.Y - 3.f * U, R * 2.f, 6.f * U, GkFade(C, 1.f - K)); GkRect(Cv, P.X - 3.f * U, P.Y - R, 6.f * U, R * 2.f, GkFade(C, 1.f - K));
	}

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
		if (P.bo) { GkText(Cv, TEXT("BURNOUT"), i ? Mid + Gap + 390.f * U : Mid - Gap - 480.f * U, Top + BarH + 6.f * U, 16.f * U, Slate); }
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
	static const TCHAR* ModeName[] = { TEXT("VS CPU"), TEXT("TRAINING"), TEXT("WATCH") };
	static const TCHAR* DummyName[] = { TEXT("STANDS"), TEXT("CROUCHES"), TEXT("JUMPS"), TEXT("BLOCKS ALL"), TEXT("FIGHTS BACK") };
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
	GkText(Cv, TEXT("TAB / START  menu      R / SELECT  restart      B  boxes"), Mid, H - 28.f * U, 13.f * U, GkFade(Bone, 0.7f), true);
	// exactly what is arriving from the stick, so a controller that is seen but not heard can be told apart from one that is mapped wrongly
	if (Pads) { GkText(Cv, FString::Printf(TEXT("%s   buttons %08x  dirs %d"), *Pads->Debug(), PadNow, PadDirs), Mid, H - 48.f * U, 12.f * U, Gold, true); }

	if (bMenu)
	{
		GkRect(Cv, 0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, 0.72f));
		const float Px = Mid - 400.f * U, Py = H * 0.14f, Ph = 720.f * U;
		GkRect(Cv, Px, Py, 800.f * U, Ph, Ink); GkRect(Cv, Px, Py + Ph, 800.f * U, 4.f * U, Red);
		const FString Stick = !PadNames.IsEmpty() ? FString::Printf(TEXT("Stick / pad over USB:  %s%s"), *PadNames.Left(60), PadCount > 0 ? TEXT("") : TEXT("   (press a button on it)")) : FString(TEXT("No stick or pad seen over USB.  Keyboard, or a pad Unreal knows."));
		if (MenuPage == 1)
		{
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
		else
		{
			const FString Items[7] = { FString(TEXT("PLAY")), FString::Printf(TEXT("MODE    <  %s  >"), ModeName[FMath::Clamp(Mode, 0, 2)]), FString::Printf(TEXT("CPU LEVEL    <  %d  >"), Level),
				FString::Printf(TEXT("TRAINING DUMMY    <  %s  >"), DummyName[FMath::Clamp(Dummy, 0, 4)]), FString::Printf(TEXT("HIT BOXES    <  %s  >"), bBoxes ? TEXT("SHOWN") : TEXT("HIDDEN")), FString(TEXT("SET UP BUTTONS")), FString(TEXT("RESTART MATCH")) };
			GkText(Cv, TEXT("GEKITOTSU"), Mid, Py + 22.f * U, 44.f * U, Bone, true);
			for (int32 k = 0; k < 7; k++)
			{
				const bool bSel = k == MenuAt; const float Iy = Py + (110.f + k * 66.f) * U;
				if (bSel) { GkRect(Cv, Px + 40.f * U, Iy, 720.f * U, 54.f * U, Red); }
				GkText(Cv, Items[k], Mid, Iy + 8.f * U, 26.f * U, bSel ? Bone : GkFade(Bone, 0.7f), true);
			}
			GkText(Cv, Stick, Mid, Py + 590.f * U, 16.f * U, Gold, true);
			GkText(Cv, TEXT("UP / DOWN choose     LEFT / RIGHT change     LIGHT or ENTER confirm"), Mid, Py + 626.f * U, 14.f * U, Bone, true);
			GkText(Cv, TEXT("Keys:  WASD move   J K L attack   I special   SPACE assist   U parry   O impact   H homing"), Mid, Py + 654.f * U, 14.f * U, Bone, true);
			GkText(Cv, TEXT("Rules: native C++, checked frame by frame against the browser game.  Fighters and stage are placeholders."), Mid, Py + 684.f * U, 13.f * U, GkFade(Bone, 0.7f), true);
		}
	}
}
