#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "Core/gk_sim.h"
#include "Core/gk_ai.h"
#include "GkBody.h"
#include "GkFx.h"
#include "GkPresentationDirector.h"
#include "GkMatch.generated.h"

class UPoseableMeshComponent; class USkeletalMeshComponent; class UAnimSequence; class UNiagaraSystem; class USoundBase; class UStaticMeshComponent; class USceneComponent; class ACameraActor; class AExponentialHeightFog; class UCanvas; class FGkPads; class UMaterialInstanceDynamic;

/** A place to fight. Kind 0 is the measured training room and 1 the canyon, both built by the game on its own bare map; 2 is a dressed map from an environment pack, opened as it was shipped, with the fight set down in it. */
struct FGkStage { FString Name, Map, Sky; int32 Kind = 2; double Time = 500.0; bool bFixed = false; FVector At = FVector::ZeroVector; double Yaw = 0.0; FString Opt;
	/** One of the stage's options, written key=value;key=value at the end of its line: light=map, rim=r,g,b, air=petals, ninja=0, lamp=4000, ev=0.5, lens=1.8 */
	FString Get(const TCHAR* Key, const TCHAR* Def = TEXT("")) const { TArray<FString> Ps; Opt.ParseIntoArray(Ps, TEXT(";")); for (const FString& P : Ps) { FString K, V; if (P.Split(TEXT("="), &K, &V) && K.TrimStartAndEnd() == Key) { return V.TrimStartAndEnd(); } } return FString(Def); } };
/** What has been chosen in the menus. It outlives a change of map. Char: 0 is Kagu, 1 Noa, 2 and 3 the mannequins. */
struct FGkSetup { int32 Costume[2]={0,1}; int32 Palette = -1; int32 Stage = 1, Mode = 0, Level = 2, Dummy = 0, Char[2] = { 2, 2 }; bool bGo = false; };

UCLASS()
class GEKITOTSUNATIVE_API AGkGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGkGameMode();
	virtual void StartPlay() override;
};

UCLASS()
class GEKITOTSUNATIVE_API AGkHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};

/**
 * One match. The rules live in Core/ (gk::step): a fixed 60 Hz simulation that knows nothing of Unreal. This actor
 * feeds it the buttons, steps it in real time whatever the picture's frame rate, and shows the result: two skeletal
 * fighters whose animation is set to the exact frame the rules are on, the camera, and the HUD.
 */
UCLASS()
class GEKITOTSUNATIVE_API AGkMatch : public AActor
{
	GENERATED_BODY()
public:
	AGkMatch();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	void DrawHud(UCanvas* Cv);
	static AGkMatch* Current;
	static FGkSetup Setup; static TArray<FGkStage> Stages; static void LoadStages();
	// where the fight stands in this map, and whether that has been settled yet (a pack's map streams in around the camera first)
	FTransform Anchor = FTransform::Identity; int32 Kind = 1; bool bPlaced = true; float PlaceWait = 0.f; FString PlaceNote;
	FVector Wd(const FVector& Local) const { return Anchor.TransformPosition(Local); }
	// the menus: 0 the front menu, 1 choosing fighters, 2 choosing a stage, 3 the fight (bMenu is then its pause menu)
	int32 Screen = 0, SelWho = 0, StageAt = 1, LightsClock = 0;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FloorSlab;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ShrineWater;
	void DressFighter(int32 i); void StartStage(int32 Index); bool TryPlace(); void QuietMapLights(); FString MapName() const;

	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Body[2];
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ShotMesh[4];
	// Noa moves by animation clips where there is one for what she is doing (bought packs, retargeted to her skeleton by
	// tools/retarget_noa.py) and by the solved poses where there is not. Each clip is shown at the exact time the rules are on.
	struct FGkClip { TObjectPtr<UAnimSequence> Anim; bool bLoop = false; float From = 0.f, To = 1.f, Impact = -1.f; bool bAuthored=false, bVertical=false; };
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Anim[2];
	UPROPERTY() TArray<TObjectPtr<UAnimSequence>> ClipKeep;
	TMap<FName, FGkClip> Clips[3]; int32 ClipSet[2] = { -1, -1 }; TObjectPtr<UAnimSequence> Playing[2];   // clip sets: 0 Kagu's, 1 Noa's
	void LoadClips(); const FGkClip* ClipFor(int32 Set, FName Pose, int32 MoveId) const;
	// What is shown: one pose per fighter, built each frame from the clip (or the hand pose) and cross-faded from the last
	// when it changes. An attack's clip is timed so its furthest reach falls on the move's first active frame.
	TArray<FTransform> PoseShown[2], PoseFrom[2], RefCS[2]; TArray<int32> SkelIdx[2], Twin[2]; const void* PoseMesh[2] = { nullptr, nullptr }; const void* SkelFor[2] = { nullptr, nullptr };
	const void* ShownAnim[2] = { nullptr, nullptr }; FName ShownClip[2]; int32 BlendAt[2] = { 0, 0 }, BlendLen[2] = { 0, 0 }, LastT[2] = { 0, 0 }, AnimClock = 0, KeyBone[2][5];
	TMap<const UAnimSequence*, float> HitAt, ThrustAt; TSet<FString> Picked;
	void EvalClip(int32 i, UAnimSequence* A, float Time, TArray<FTransform>& Out, bool bIncludeRoot=false); float ImpactOf(int32 i, const FGkClip& C, bool bThrust = false);
	// How a fighter's state is told at a glance: his colours and outline change with it (guard blue, parry cyan, a perfect
	// parry gold, burnout grey, a blow landing white), with a word over his head and particles to match.
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	UMaterialInstanceDynamic* BodyMid[2][4] = { { nullptr, nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr, nullptr } }; UMaterialInstanceDynamic* InkMid[2] = { nullptr, nullptr }; FLinearColor BaseCol[2][4], ShadeCol[2][4];
	int32 PerfectT[2] = { 0, 0 }, ImpactT[2] = { 0, 0 }; float Lean[2] = { 0.f, 0.f }, Squash[2] = { 0.f, 0.f }, Flash = 0.f, RainDue = 0.f, BoltDue = 3.f;
	void LookFighter(int32 i); void StateFx(int32 i); void Beam(const FVector& From, float Dir, float Len, float R, const FLinearColor& Col, int32 Who); float BeamDue = 0.f;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FilmBeam[2];
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FilmBeamMid[2];
	float FilmBeamLife[2] = {0,0};
 float FilmWetness=-1.f;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FilmOrb[2];
	UPROPERTY() TObjectPtr<class UPointLightComponent> FilmEnergyLight[2];
	void EnergyOrb(int32 Who, float Radius);
	// scripted pictures (-gkscript): a list of commands in Saved/script.txt that pose the game and save screenshots
	TArray<FString> Script; int32 ScriptAt = 0, ScriptWait = 0; bool bScript = false; FName ScriptSlot; float ScriptStorm = -1.f; FString ScriptNote; bool bLabUi = false, bNoHud = false; FString ScriptCaptureName; int32 ScriptCaptureLeft = 0, ScriptCaptureFrame = 0; int32 ScriptRunLeft = 0, ScriptIn[2] = { 0, 0 }; void RunScript(); FKey ScriptMenuKey;
	// The animation lab (Screen 4): one fighter alone, every move and state in turn, and every clip he has to try on it.
	// What is chosen is kept in Content/Gk/anims.txt (set | slot | clip | from | to | loop) and used by the game from then on.
	struct FLabSlot { FName Slot; FString Label; int32 Cat = 0; };
	struct FLabClip { FString Path, Label, Pack; int32 Cat = 0; };
	TArray<FLabSlot> LabSlots; TArray<FLabClip> LabList; int32 LabSlot = 0, LabFrame = 0, LabSet = 2, LabCat = 0, LabDropCat = 0, LabScroll = 0, LabHot = 0, LabNoteTimer = 0, LabTryAt = -2; bool bLabDrop = false, bLabMouse = false; int32 LabKey = -2; float LabClock = 0.f; FString LabNote; FGkClip LabTry; FGkClip LabPartner; bool bLabPair=false; float LabPairSpacing=200.f;
	TMap<FName,FGkClip> SetupDrafts;
 FString SetupSearch; int32 SetupPack=0,SetupFilter=-1,SetupSlotScroll=0,SetupDropdown=0;
 bool bSetupAdvanced=false;
 bool bSetupVariants=false;
 bool bSetupSearch=false,bSetupPaused=false,bSetupTesting=false;float SetupSpeed=1.f;
 FGkClip SetupOriginal;int32 SetupOldMode=0,SetupOldDummy=0;
 void DrawSetupTest(UCanvas* Cv);void DrawCharacterSetup(UCanvas* Cv);void SetupInput();void SetupAction(int32 Id);void SetupSelect(int32 Slot);void SetupApply();void SetupTest();void SetupReturn();
 bool SetupMatches(const FLabClip& C) const;
 void LabOpen(); void LabClose(); void LabClick(int32 Id); void LabPick(int32 ClipIndex); void LabSave(); void LoadPicks(); FString PicksFile(bool bProject) const;
	// hit, guard and dust effects and their sounds (Vefects' Combat Flipbook VFX)
	UPROPERTY() TMap<FName, TObjectPtr<UNiagaraSystem>> Vfx;
	UPROPERTY() TObjectPtr<class UNiagaraComponent> ShotFx[4]; int32 ShotKind[4] = { 0, 0, 0, 0 };
	UPROPERTY() TMap<FName, TObjectPtr<USoundBase>> Sfx;
	UPROPERTY() TObjectPtr<class UAudioComponent> RainLoop; UPROPERTY() TObjectPtr<class UAudioComponent> WindLoop; UPROPERTY() TObjectPtr<class UAudioComponent> GaleLoop; UPROPERTY() TObjectPtr<class UAudioComponent> PourLoop; UPROPERTY() TObjectPtr<class UAudioComponent> RoomLoop; UPROPERTY() TObjectPtr<class UAudioComponent> Music; UPROPERTY() TArray<TObjectPtr<USoundBase>> Songs; int32 LastSong = -1; float MusicVol = 0.5f; int32 StepT[2] = { 0, 0 };
	float SongGain = 1.f;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> SndKeep; TMap<FName, TArray<USoundBase*>> SndSets; void Snd(const TCHAR* Set, float Volume = 1.f, float Pitch = 1.f); void EventSound(const gk::Event& E);
	void LoadVoices(); void VoiceEvent(const gk::Event& E); void Announce(const TCHAR* Name,bool Interrupt); void DumpFilmFog(); void FighterVoice(int32 Who,const TCHAR* Name,int32 Priority);
	UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> Voices;
	UPROPERTY() TObjectPtr<class UAudioComponent> Announcer;
 UPROPERTY() TObjectPtr<class UAudioComponent> FighterVoices[2];
 int32 VoicePriority[2]={0,0}; double VoiceNext[2]={0,0};
 UPROPERTY() TObjectPtr<class UNiagaraComponent> FilmNinja;
 UPROPERTY() TObjectPtr<class UNiagaraComponent> FilmRain;
 UPROPERTY() TObjectPtr<class UTexture2D> FilmPortraits[2];
 UPROPERTY() TObjectPtr<class UFont> HudFont;
 // Native presentation: live scene captures, page choreography, and separately mixed UI audio.
 UPROPERTY() TObjectPtr<class USceneCaptureComponent2D> PortraitCamera[2];
 UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> PortraitTarget[2];
 float UiTime=0.f, PageAge=0.f, CursorAge=0.f, PortraitDue=0.f;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> TitleMesh[2];
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TitleRed;
 float MenuIdleAge=0,MenuCutAge=99,MenuStampAge=99; int32 AttractShot=0,MenuSoundStep=0; FString MenuStamp;
 void DrawMenuEffects(UCanvas* Canvas);
 UPROPERTY() TObjectPtr<class UText3DComponent> SpatialType[8];
 UPROPERTY() TObjectPtr<UFont> SpatialFont;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SpatialFront;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SpatialSide;
 void UpdateSpatialType(float Delta);
 void UpdateMenuSet();
 UPROPERTY() TObjectPtr<UPoseableMeshComponent> MenuCostumes[8];
 UPROPERTY() TObjectPtr<UStaticMeshComponent> MenuSetPlanes[7];
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MenuPaper;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MenuBlack;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PrintFinish;
 int32 MenuReturnPage=0,MenuReturnAt=0;
 int32 UiPageKey=-1, UiCursorKey=-1, MenuPalette=0; bool bDebugMenu=false;
 float MusicGain=1.f, EffectsGain=1.f, VoiceGain=1.f, ShakeGain=1.f, FlashGain=1.f;
 FGkPresentationDirector Director;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MangaMid;
 bool bDirectorLoaded=false, bDirectorEnabled=true;
 float DirectorReloadDue=0.f; FDateTime DirectorFileTime;
 FVector DirectorContact=FVector::ZeroVector, PressureAt=FVector::ZeroVector;
 float PressureAge=10.f,PressureStrength=0;
 int DirectorBeamFrame[2]={-1,-1};
 struct FSurfaceResponse {FLinearColor Tint;float Dust=0.2f,Speed=1.f,Wake=1.f;};TMap<FString,FSurfaceResponse> SurfaceProfiles;
 void LoadDirector(); void DirectEvent(const gk::Event& E); void UpdateDirector(float Delta); void ApplyDirector();
 void SurfaceResponse(const FVector& Feet,float Strength,float Direction);
 void UpdatePresentation(float Delta); void MenuSound(int32 Kind); void LoadPresentation(); void SavePresentation();
 static FLinearColor CostumeColor(int32 Outfit, bool Accent=false); static const TCHAR* CostumeName(int32 Outfit);
 int32 ParryBeat[2]={-100,-100},ParryVariant[2]={0,0},ParryCount[2]={0,0};
 TMap<const UAnimSequence*,float> CrouchSample;
 void DrawFireball(int32 Index,bool Active);
 UPROPERTY() TObjectPtr<UStaticMeshComponent> FireballTail[4];
 void ApplyPersonality(int32 Who, FName Slot, float Frame, TArray<FTransform>& Pose);
 void DrawArcadeMenu(UCanvas* Canvas); void DrawArcadeCall(UCanvas* Canvas);
	void LoadVfx(); bool PlayVfx(const TCHAR* Name, const FVector& Local, float Scale, float Dir, const TCHAR* Sound, float Volume = 1.f);
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	// the two places to fight: the measured training room, and the wasteland stage; and the air between
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> GridParts;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> StageParts;
	UPROPERTY() TObjectPtr<AExponentialHeightFog> Fog;
	FGkFx Fx; int32 ShownPlace = -1;
	// The sky and weather over the wasteland (SoStylized's Stylized Dynamic Sky & Weather): it owns the sun, sky light and
	// fog there. The plain lights below are the training room's.
	UPROPERTY() TObjectPtr<AActor> SkyActor;
	UPROPERTY() TArray<TObjectPtr<UObject>> Weathers;
	UPROPERTY() TArray<TObjectPtr<AActor>> RoomLights;
	UPROPERTY() TObjectPtr<USceneComponent> RoomAir;
	float Storm = 0.f, Dim = 0.f; int32 WeatherAt = -1;
	void MakeSky(); void SetWeather(int32 Index, double BlendSeconds);
	// The look of the place. A rim of light on the fighters from the lamp behind them; the lens, which holds the fighters
	// sharp and lets the far ground go soft; the lamp itself, which the mist catches; and FluidNinja LIVE's ground fog,
	// a fluid that lies over the floor of the fight and is stirred by the fighters as they move through it.
	FLinearColor RimCol = FLinearColor(1.f, 0.85f, 0.65f); float RimTune[3] = { 2.f, 2.f, 0.6f }, BoltT = 0.f, LensF = 1.8f, LampI = 5000.f, LampScatter = 3.f, WindDue = 0.f, SplashDue = 0.f; int32 AirKind = 0; bool bOwnLight = false, bNinjaTried = false, bNinjaQuiet = false;
	UPROPERTY() TObjectPtr<class USpotLightComponent> BackLamp; UPROPERTY() TObjectPtr<class ULevelStreamingDynamic> NinjaLevel; UPROPERTY() TArray<TObjectPtr<AActor>> Stirrers;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> EdgeMid; bool bEdgeAdd = false; bool bFly = false; bool bScout = false; FVector ScoutPos = FVector::ZeroVector; FRotator ScoutRot = FRotator::ZeroRotator; float ScoutFov = 60.f;
	void StageLook(); void NinjaFog(); void Mist();
	void UpdateForestLook(float DeltaSeconds);
	FVector FloorPoint(const FVector& Local) const;
	bool bForestLook = false;
	double ForestCameraGround = 0.0;
	int32 PaletteOverride = INDEX_NONE;
	float PaletteWeights[6] = { 1.f };
	UPROPERTY() TObjectPtr<UMaterialInterface> ForestCel;
	UPROPERTY() TObjectPtr<UMaterialInterface> FilmEnergyMaterial;
 UPROPERTY() TObjectPtr<UMaterialInterface> FilmProjectileMaterial;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FilmSky;
	UPROPERTY() TObjectPtr<UMaterialInterface> ForestFighterCel;
	TWeakObjectPtr<class UDirectionalLightComponent> ForestSun;
	TWeakObjectPtr<class UExponentialHeightFogComponent> ForestFog;
	FRotator ForestSunRotation;
	float ForestSunIntensity = 0.f;
	float ForestFogDensity = 0.f;
	FLinearColor ForestSunColor = FLinearColor::White;
	FLinearColor ForestFogColor = FLinearColor::White;
	FLinearColor ForestRim = FLinearColor::White;
	FLinearColor ForestGrade = FLinearColor::White;
	// afterimages: when a fighter moves fast (a dash, a rush, a drive impact, a super, being knocked flying) he leaves
	// bright copies of himself behind for a few frames, each held in the pose he had there
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Ghost[6]; UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GhostMid[6]; int32 GhostAge[6] = { 99, 99, 99, 99, 99, 99 }, GhostNext[2] = { 0, 0 }, GhostClock[2] = { 0, 0 };
	void Afterimage(int32 i);
	// the browser game's animation and the two skeletons it is solved onto
	FGkPoses Poses; FGkBody Rig[2]; TArray<FName> MoveClip; int32 HitKind[2] = { 1, 1 };

	// the match
	gk::State S; gk::Ai Cpu[2];
	int32 Mode = 0, Level = 2, Dummy = 0;            // mode: 0 versus CPU, 1 training, 2 watch. dummy: stand, crouch, jump, block all, CPU
	double Acc = 0.0, LastReal = 0.0; float FrameMs = 16.7f, StepMs = 0.f; int32 OverTimer = 0, Ticks = 0;
	// what the picture adds on top of the rules
	float Shake = 0.f; FVector CamPos = FVector(-900.0, 0.0, 120.0);
	// The camera's director, as in the browser game: it sits side-on while the fight is being read and swings round the
	// action for the moments that deserve it. Yaw orbits the point looked at, Pit is elevation, Roll the dutch tilt, Zm
	// the distance, Fov a widening; the K values are one-shot shoves from impacts that die away on their own.
	struct FCamDir { float Yaw = 0.f, Pit = 4.f, Roll = 0.f, Zm = 1.f, Fov = 0.f, KRoll = 0.f, KZm = 0.f, KFov = 0.f, KYaw = 0.f; bool bSnap = true; } CamD; int32 FrzMax = 0;
	void CamKick(float Roll, float Zm, float Fov, float Yaw = 0.f);
	FString CallWord; int32 CallAge = 0, CallHold = 0;
	int32 Combo[2] = { 0, 0 }, ComboDmg[2] = { 0, 0 }, ComboTimer[2] = { 0, 0 }; FString Note[2]; int32 NoteTimer[2] = { 0, 0 };
	struct FSpark { FVector At; int32 Age; int32 Kind; }; TArray<FSpark> Sparks;
	// training aids
	struct FHist { int32 In, Frames; }; TArray<FHist> Hist; int32 LastIn = -1, Adv = -999, ActFrame[2] = { 0, 0 }; bool bWasIdle[2] = { true, true }, bExchange = false; bool bBoxes = false;
	// controls
	FGkPads* Pads = nullptr; uint32 PadNow = 0, PadPrev = 0; int32 PadDirs = 0, PadDirPrev = 0, PadCount = 0; FString PadNames;
	int32 PadBind[8] = { 0, 1, 2, 6, 7, 5, 4, 3 }; FKey Bind[8]; int32 InputNow = 0;
	bool bMenu = true; int32 MenuAt = 0, MenuPage = 0, BindAt = 0;
	float ScreenPct = 0.f, SlowSeconds = 0.f; FString StatusPath; int32 StatusClock = 0;

	static constexpr double Cm = (100.0 / 51.0) / 100.0;   // centimetres in one of the rules' sub-units: the fighter is 89 units, about 1.75 m
	FVector At(int32 X, int32 Y, double Depth = 0.0) const { return FVector(Depth, double(X) * Cm, double(Y) * Cm); }
	bool bEntrance=false,bEntrancePlayed=false; int32 EntranceFrame=0,EntrancePoseFrames=120;
 int32 EntranceA() const {return 144;} int32 EntranceBWalk() const {return EntranceA()+EntrancePoseFrames;} int32 EntranceB() const {return EntranceBWalk()+72;} int32 EntranceDolly() const {return EntranceB()+EntrancePoseFrames;} int32 EntranceEnd() const {return EntranceDolly()+60;}
 bool StepEntrance(); void FinishEntrance(); void EntranceCamera(); float EntranceX(int32 Who) const; FName EntranceClip(int32 Who) const; float EntranceSample(int32 Who,float Length) const;
 void Restart(); int32 ReadPlayer() const; int32 DummyInput(); void MenuInput(); void StepOnce(int32 Force0 = -1, int32 Force1 = -1); void OnEvent(const gk::Event& E); void Track(int32 In);
	void Present(float Real); void ShowFighter(int32 i); void SaveBinds() const; void LoadBinds(); void Say(const TCHAR* Word, int32 Hold); void Remark(int32 Who, const TCHAR* Text);
};
