#include "GkGame.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Engine/Font.h"

// The HUD is drawn straight onto the canvas: slanted bars, heavy italic type, and kanji stamped behind the calls.

static void GkTri(UCanvas* Cv, const FVector2D& A, const FVector2D& B, const FVector2D& D, const FLinearColor& Col)
{
	FCanvasTriangleItem Item(A, B, D, GWhiteTexture); Item.SetColor(Col); Item.BlendMode = SE_BLEND_Translucent; Cv->DrawItem(Item);
}

/** A parallelogram: the top edge is pushed sideways by Skew. */
static void GkBar(UCanvas* Cv, float X, float Y, float W, float H, float Skew, const FLinearColor& Col)
{
	if (W <= 0.f || H <= 0.f || Col.A <= 0.f) { return; }
	const FVector2D A(X + Skew, Y), B(X + W + Skew, Y), C(X + W, Y + H), D(X, Y + H);
	GkTri(Cv, A, B, C, Col); GkTri(Cv, A, C, D, Col);
}

static void GkText(UCanvas* Cv, const FString& Str, float X, float Y, float Size, const FLinearColor& Col, bool bCentre = false, const TCHAR* Face = TEXT("BlackItalic"), int32 Outline = 0)
{
	if (Col.A <= 0.f) { return; }
	// the canvas wants a font asset to hang the text on; the face actually drawn is the engine's heavy italic, which also carries kanji
	static TWeakObjectPtr<UFont> Carrier;
	if (!Carrier.IsValid()) { Carrier = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")); }
	if (!Carrier.IsValid()) { return; }
	const FFontOutlineSettings Line = Outline > 0 ? FFontOutlineSettings(Outline, FLinearColor(0.012f, 0.006f, 0.012f, Col.A)) : FFontOutlineSettings();
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), Carrier.Get(), Col);
	Item.SlateFontInfo = FCoreStyle::GetDefaultFontStyle(FName(Face), FMath::Max(4.f, Size), Line);
	Item.bCentreX = bCentre; Cv->DrawItem(Item);
}

static FLinearColor GkFade(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * FMath::Clamp(A, 0.f, 1.f)); }

/** A call across the middle of the screen: an ink band that whips in, a kanji stamp behind, the word in front. */
static void GkCall(UCanvas* Cv, float W, float H, float U, const FString& Word, const FString& Kanji, int32 Age, const FLinearColor& Col, float Hold)
{
	const float In = FMath::Clamp(Age / 7.f, 0.f, 1.f), Out = FMath::Clamp((Hold - Age) / 8.f, 0.f, 1.f), Vis = FMath::Min(In, Out);
	if (Vis <= 0.f) { return; }
	const float Pop = 1.f + 0.7f * (1.f - In) * (1.f - In), Mid = W * 0.5f, Cy = H * 0.4f;
	const float BandW = W * (0.25f + 0.75f * In), BandH = 150.f * U * Vis;
	GkBar(Cv, Mid - BandW * 0.5f, Cy - BandH * 0.5f + 20.f * U, BandW, BandH, 60.f * U, FLinearColor(0.012f, 0.006f, 0.014f, 0.86f * Vis));
	GkBar(Cv, Mid - BandW * 0.5f, Cy + BandH * 0.5f + 24.f * U, BandW, 5.f * U, 0.f, GkFade(Col, Vis));
	GkText(Cv, Kanji, Mid, Cy - 190.f * U, 190.f * U, GkFade(Col, 0.34f * Vis), true, TEXT("Bold"));
	GkText(Cv, Word, Mid, Cy - 62.f * U * Pop, 98.f * U * Pop, FLinearColor(0.97f, 0.94f, 0.88f, Vis), true, TEXT("BlackItalic"), 3);
}

void FGkGame::DrawHud(UCanvas* Cv)
{
	if (!Cv || !bHud) { return; }
	const float W = Cv->SizeX, H = Cv->SizeY, U = H / 1080.f, Mid = W * 0.5f;
	const FLinearColor Ink(0.014f, 0.007f, 0.016f, 0.9f), Bone(0.97f, 0.94f, 0.88f, 1.f), Gold(1.f, 0.76f, 0.2f, 1.f), Blood(0.93f, 0.13f, 0.09f, 1.f), Frost(0.6f, 0.83f, 1.f, 1.f), Ash(0.3f, 0.26f, 0.3f, 0.9f);
	const FLinearColor Tint[2] = { Blood, Frost };
	const FString Names[2] = { TEXT("KAGU"), TEXT("SHIRO") }; const FString Glyph[2] = { TEXT("篝"), TEXT("白") };

	// a super closes the world down to a letterbox
	if (Dim > 0.01f) { GkBar(Cv, 0.f, 0.f, W, 96.f * U * Dim, 0.f, FLinearColor(0.f, 0.f, 0.f, 1.f)); GkBar(Cv, 0.f, H - 96.f * U * Dim, W, 96.f * U * Dim, 0.f, FLinearColor(0.f, 0.f, 0.f, 1.f)); }

	const float BarW = 720.f * U, BarH = 28.f * U, Top = 62.f * U, Gap = 92.f * U, Skew = 16.f * U;
	for (int32 i = 0; i < 2; i++)
	{
		const gk::Player& P = S.p[i]; const float Sg = i ? 1.f : -1.f;
		const float K = FMath::Clamp(P.hp / float(gk::MAX_HP), 0.f, 1.f), Lag = FMath::Clamp(V[i].HpLag, K, 1.f);
		const float Near = Mid + Sg * Gap;                       // the end of the bar beside the clock
		const float X0 = i ? Near : Near - BarW, Sk = -Sg * Skew; // both bars lean toward the clock
		const bool bLow = K < 0.25f; const float Pulse = 0.75f + 0.25f * FMath::Sin(Tick60 * 0.3f);
		GkBar(Cv, X0 - 7.f * U, Top - 6.f * U, BarW + 14.f * U, BarH + 12.f * U, Sk * 1.4f, Ink);
		GkBar(Cv, X0, Top, BarW, BarH, Sk, Ash);
		// health empties away from the clock; the pale edge behind it is the damage of the combo in progress
		if (i) { GkBar(Cv, X0, Top, BarW * Lag, BarH, Sk, Bone); GkBar(Cv, X0, Top, BarW * K, BarH, Sk, bLow ? GkFade(Gold, Pulse) : Tint[i]); GkBar(Cv, X0, Top, BarW * K, BarH * 0.3f, Sk * 0.3f, FLinearColor(1.f, 1.f, 1.f, 0.28f)); }
		else { GkBar(Cv, X0 + BarW * (1.f - Lag), Top, BarW * Lag, BarH, Sk, Bone); GkBar(Cv, X0 + BarW * (1.f - K), Top, BarW * K, BarH, Sk, bLow ? GkFade(Gold, Pulse) : Tint[i]); GkBar(Cv, X0 + BarW * (1.f - K) + Sk * 0.7f, Top, BarW * K, BarH * 0.3f, Sk * 0.3f, FLinearColor(1.f, 1.f, 1.f, 0.28f)); }
		// name, with his kanji large and faint behind it
		const float NameX = i ? X0 + BarW - 250.f * U : X0 + 40.f * U;
		GkText(Cv, Glyph[i], NameX + (i ? 196.f : -34.f) * U, Top + BarH + 2.f * U, 46.f * U, GkFade(Tint[i], 0.85f), false, TEXT("Bold"));
		GkText(Cv, Names[i], NameX + (i ? 0.f : 30.f) * U, Top + BarH + 10.f * U, 30.f * U, Bone, false, TEXT("BlackItalic"), 2);
		// rounds won
		for (int32 r = 0; r < 2; r++)
		{
			const float Rx = Near + Sg * (22.f + r * 34.f) * U - 11.f * U;
			GkBar(Cv, Rx - 3.f * U, Top - 40.f * U, 28.f * U, 18.f * U, Sk * 0.6f, Ink);
			GkBar(Cv, Rx, Top - 37.f * U, 22.f * U, 12.f * U, Sk * 0.5f, r < P.rounds ? Gold : Ash);
		}
		// the Tide gauge: three bars along the bottom, and a count of the full ones
		const float SegW = 150.f * U, SegH = 16.f * U, My = H - 84.f * U; const int32 Full = P.meter / gk::BAR;
		for (int32 b = 0; b < 3; b++)
		{
			const float Part = FMath::Clamp((P.meter - b * gk::BAR) / float(gk::BAR), 0.f, 1.f);
			const float X = i ? W - 150.f * U - (b + 1) * (SegW + 12.f * U) : 150.f * U + b * (SegW + 12.f * U);
			GkBar(Cv, X - 4.f * U, My - 4.f * U, SegW + 8.f * U, SegH + 8.f * U, Sk * 0.7f, Ink);
			GkBar(Cv, X, My, SegW, SegH, Sk * 0.5f, Ash);
			if (i) { GkBar(Cv, X + SegW * (1.f - Part), My, SegW * Part, SegH, Sk * 0.5f, Part >= 1.f ? GkFade(Gold, Pulse + 0.25f) : Tint[i]); }
			else { GkBar(Cv, X, My, SegW * Part, SegH, Sk * 0.5f, Part >= 1.f ? GkFade(Gold, Pulse + 0.25f) : Tint[i]); }
		}
		GkText(Cv, FString::Printf(TEXT("%d"), Full), i ? W - 104.f * U : 104.f * U, My - 44.f * U, 58.f * U, Full > 0 ? Gold : Ash, true, TEXT("BlackItalic"), 3);
		GkText(Cv, TEXT("TIDE"), i ? W - 104.f * U : 104.f * U, My + 26.f * U, 15.f * U, Bone, true, TEXT("Bold"));
		// the combo count, on the side of whoever is landing it
		if (ComboTimer[i] > 0 && Combo[i] >= 2)
		{
			const float Pop = 1.f + 0.45f * (ComboPop[i] / 8.f), A = FMath::Clamp(ComboTimer[i] / 12.f, 0.f, 1.f), Cx = i ? W - 250.f * U : 250.f * U, Cy = 300.f * U;
			GkText(Cv, TEXT("連撃"), Cx, Cy - 50.f * U, 150.f * U, GkFade(Tint[i], 0.15f * A), true, TEXT("Bold"));
			GkText(Cv, FString::Printf(TEXT("%d"), Combo[i]), Cx, Cy - 20.f * U * Pop, 120.f * U * Pop, GkFade(Combo[i] >= 8 ? Gold : Bone, A), true, TEXT("BlackItalic"), 4);
			GkText(Cv, TEXT("HITS"), Cx, Cy + 176.f * U, 30.f * U, GkFade(Tint[i], A), true, TEXT("BlackItalic"), 2);
		}
	}
	// the clock
	GkBar(Cv, Mid - 66.f * U, Top - 30.f * U, 132.f * U, 96.f * U, 0.f, Ink);
	GkBar(Cv, Mid - 66.f * U, Top + 62.f * U, 132.f * U, 4.f * U, 0.f, Gold);
	const int32 Secs = FMath::Clamp((S.time + 59) / 60, 0, 99);
	GkText(Cv, FString::Printf(TEXT("%02d"), Secs), Mid, Top - 30.f * U, 62.f * U, Secs <= 10 ? Gold : Bone, true, TEXT("Black"));

	// the calls
	if (S.phase == gk::PH_INTRO)
	{
		static const TCHAR* RoundKanji[] = { TEXT("第一戦"), TEXT("第二戦"), TEXT("最終戦") };
		const int32 Rn = FMath::Clamp<int32>(S.round, 1, 3);
		if (S.pt < 55) { GkCall(Cv, W, H, U, Rn >= 3 ? FString(TEXT("FINAL ROUND")) : FString::Printf(TEXT("ROUND %d"), Rn), RoundKanji[Rn - 1], S.pt, Bone, 55.f); }
		else { GkCall(Cv, W, H, U, TEXT("GEKITOTSU!"), TEXT("激突"), S.pt - 55, Blood, 60.f); }
	}
	const int32 Since = gk::ROUND_TIME - S.time;
	if (S.phase == gk::PH_FIGHT && Since < 25) { GkCall(Cv, W, H, U, TEXT("GEKITOTSU!"), TEXT("\u6FC0\u7A81"), 35 + Since, Blood, 60.f); }
	if (S.phase == gk::PH_KO && S.pt < 150) { GkCall(Cv, W, H, U, TEXT("K.O."), TEXT("決着"), S.pt, Blood, 150.f); }
	if (S.phase == gk::PH_OVER) { const int32 Wn = S.winner == 1 ? 1 : 0; GkCall(Cv, W, H, U, Names[Wn] + TEXT(" WINS"), TEXT("勝者"), OverTimer, Tint[Wn], 300.f); }
	// a super announces itself
	if (SuperTimer > 0 && SuperWho >= 0)
	{
		static const TCHAR* SuperName[] = { TEXT("MOONWAKE"), TEXT("STORM SURGE"), TEXT("KING TIDE"), TEXT("ABYSSAL TIDE") };
		static const TCHAR* SuperKanji[] = { TEXT("月痕"), TEXT("高潮"), TEXT("大潮"), TEXT("深淵") };
		const int32 Lv = FMath::Clamp(SuperLevel, 1, 4) - 1, Age = 100 - SuperTimer; const int32 Wh = SuperWho & 1;
		const float In = FMath::Clamp(Age / 6.f, 0.f, 1.f), Out = FMath::Clamp(SuperTimer / 10.f, 0.f, 1.f), Vis = FMath::Min(In, Out);
		const float Yb = H * 0.72f, Slide = (1.f - In) * 500.f * U * (Wh ? 1.f : -1.f);
		GkBar(Cv, 0.f, Yb, W, 112.f * U * Vis, 0.f, FLinearColor(0.012f, 0.006f, 0.014f, 0.9f * Vis));
		GkBar(Cv, 0.f, Yb + 112.f * U * Vis, W, 5.f * U, 0.f, GkFade(Tint[Wh], Vis));
		GkText(Cv, SuperKanji[Lv], Mid + Slide * 0.4f, Yb - 70.f * U, 170.f * U, GkFade(Tint[Wh], 0.4f * Vis), true, TEXT("Bold"));
		GkText(Cv, SuperName[Lv], Mid + Slide, Yb + 6.f * U, 72.f * U, GkFade(Bone, Vis), true, TEXT("BlackItalic"), 3);
	}
	// which mode this is, and how to reach the menu
	static const TCHAR* ModeName[] = { TEXT("WATCH"), TEXT("VS CPU"), TEXT("TRAINING") };
	GkText(Cv, ModeName[FMath::Clamp(Mode, 0, 2)], Mid, Top + 72.f * U, 17.f * U, Gold, true, TEXT("Bold"));
	GkText(Cv, TEXT("TAB / START  MENU      1  VS CPU      2  TRAINING      3  WATCH      R  RESTART"), Mid, H - 30.f * U, 12.f * U, GkFade(Bone, 0.6f), true, TEXT("Bold"));
	// what the controls are sending right now: the way to check a controller
	{
		static const TCHAR* Lab[] = { TEXT("L"), TEXT("M"), TEXT("H"), TEXT("S"), TEXT("P") }; static const uint16 Bit[] = { gk::BL, gk::BM, gk::BH, gk::BS, gk::BP };
		const float Sz = 34.f * U, Gp = 5.f * U, Bx = Mid - 4.5f * (Sz + Gp), By = H - 112.f * U;
		auto Key = [&](float X, float Y, bool bOn, const TCHAR* Txt)
		{
			GkBar(Cv, X, Y, Sz, Sz, 0.f, bOn ? Gold : Ink);
			if (Txt) { GkText(Cv, Txt, X + Sz * 0.5f, Y + 4.f * U, 17.f * U, bOn ? Ink : Bone, true, TEXT("Bold")); }
		};
		Key(Bx, By, (LastIn & gk::LEFT) != 0, TEXT("<")); Key(Bx + Sz + Gp, By, (LastIn & gk::DOWN) != 0, TEXT("v")); Key(Bx + 2 * (Sz + Gp), By, (LastIn & gk::RIGHT) != 0, TEXT(">"));
		Key(Bx + Sz + Gp, By - Sz - Gp, (LastIn & gk::UP) != 0, TEXT("^"));
		for (int32 k = 0; k < 5; k++) { Key(Bx + (4 + k) * (Sz + Gp), By, (LastIn & Bit[k]) != 0, Lab[k]); }
	}
	if (bMenu)
	{
		GkBar(Cv, 0.f, 0.f, W, H, 0.f, FLinearColor(0.f, 0.f, 0.f, 0.74f));
		const float Px = Mid - 390.f * U, Py = H * 0.2f;
		GkBar(Cv, Px, Py, 780.f * U, 540.f * U, 0.f, Ink); GkBar(Cv, Px, Py + 540.f * U, 780.f * U, 5.f * U, 0.f, Blood);
		if (MenuPage == 1)
		{
			static const TCHAR* Act[] = { TEXT("LIGHT"), TEXT("MEDIUM"), TEXT("HEAVY"), TEXT("SPECIAL"), TEXT("PARRY"), TEXT("THROW"), TEXT("SLIPSTREAM"), TEXT("SUPER") };
			GkText(Cv, TEXT("SET UP BUTTONS"), Mid, Py + 30.f * U, 40.f * U, Bone, true);
			GkText(Cv, FString::Printf(TEXT("Press the button for  %s   (%d of 8)"), Act[FMath::Clamp(BindAt, 0, 7)], BindAt + 1), Mid, Py + 210.f * U, 30.f * U, Gold, true, TEXT("Bold"));
			GkText(Cv, TEXT("TAB or START cancels.  Directions use the stick or d-pad."), Mid, Py + 440.f * U, 16.f * U, GkFade(Bone, 0.75f), true, TEXT("Bold"));
		}
		else
		{
			static const TCHAR* DummyName[] = { TEXT("STANDS"), TEXT("CROUCHES"), TEXT("JUMPS"), TEXT("BLOCKS"), TEXT("FIGHTS BACK") };
			const FString Items[5] = { FString(TEXT("RESUME")), FString::Printf(TEXT("MODE    <  %s  >"), ModeName[FMath::Clamp(Mode, 0, 2)]),
				FString::Printf(TEXT("TRAINING DUMMY    <  %s  >"), DummyName[FMath::Clamp(Dummy, 0, 4)]), FString(TEXT("SET UP BUTTONS")), FString(TEXT("RESTART MATCH")) };
			GkText(Cv, TEXT("GEKITOTSU"), Mid, Py + 24.f * U, 44.f * U, Bone, true);
			for (int32 k = 0; k < 5; k++)
			{
				const bool bSel = k == MenuAt; const float Iy = Py + (120.f + k * 74.f) * U;
				if (bSel) { GkBar(Cv, Px + 40.f * U, Iy, 700.f * U, 56.f * U, 0.f, Blood); }
				GkText(Cv, Items[k], Mid, Iy + 8.f * U, 26.f * U, bSel ? Bone : GkFade(Bone, 0.7f), true, TEXT("Bold"));
			}
			GkText(Cv, TEXT("UP / DOWN choose     LEFT / RIGHT change     LIGHT or ENTER confirm"), Mid, Py + 500.f * U, 14.f * U, GkFade(Bone, 0.7f), true, TEXT("Bold"));
		}
	}
	// the white of a decisive blow
	if (White > 0.02f) { GkBar(Cv, 0.f, 0.f, W, H, 0.f, FLinearColor(1.f, 1.f, 1.f, FMath::Min(1.f, White))); }
}
