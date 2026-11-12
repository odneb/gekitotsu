#include "GkPad.h"
#include "Misc/ScopeLock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_MAC
THIRD_PARTY_INCLUDES_START
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hidsystem/IOHIDLib.h>
THIRD_PARTY_INCLUDES_END

struct FGkPadDev { IOHIDDeviceRef Ref = nullptr; FString Name; int32 Vid = 0, Pid = 0; uint32 Buttons = 0; int32 Hat = 0; float X = 0.f, Y = 0.f; };
struct FGkPadImpl { IOHIDManagerRef Mgr = nullptr; TArray<FGkPadDev> Devs; int32 Access = -1, Keyboardish = 0, Events = 0, Used = 0, LastPage = 0, LastUsage = 0, LastValue = 0, OpenResult = 0, Reports = 0, LastReportLen = 0; uint8 LastReport[16] = { 0 }; uint8 Now[16] = { 0 }, Prev[16] = { 0 }; int32 Ids[4] = { -1, -1, -1, -1 }, Changes = 0, NowLen = 0; };

// The system calls back on the main thread; the game reads on its own. One lock guards both, and a callback that
// arrives for a reader that has gone away finds this pointer changed and does nothing.
static FCriticalSection GGkPadLock;
static FGkPadImpl* GGkPadLive = nullptr;

static int32 GkPadNumber(IOHIDDeviceRef Dev, CFStringRef Key)
{
	int32 Out = 0; CFTypeRef V = IOHIDDeviceGetProperty(Dev, Key);
	if (V && CFGetTypeID(V) == CFNumberGetTypeID()) { CFNumberGetValue((CFNumberRef)V, kCFNumberSInt32Type, &Out); }
	return Out;
}

static void GkPadList(const FGkPadImpl* I)
{
	// a note of what was found, for when a controller does not answer
	FString Out;
	for (const FGkPadDev& D : I->Devs) { Out += FString::Printf(TEXT("%s  vendor %04x product %04x\n"), *D.Name, D.Vid, D.Pid); }
	if (Out.IsEmpty()) { Out = TEXT("no joystick or gamepad is visible over USB or Bluetooth\n"); }
	FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("gk_pads.txt")));
}

static uint8 GGkPadReportBuf[256];
static void GkPadReport(void* Ctx, IOReturn, void* Sender, IOHIDReportType, uint32_t, uint8_t* Report, CFIndex Len)
{
	FScopeLock Lock(&GGkPadLock); FGkPadImpl* I = (FGkPadImpl*)Ctx; if (!I || I != GGkPadLive || !Report) { return; }
	I->Reports++;
	// keep the newest report, note which kinds arrive, and write down every one whose first ten bytes differ from the last (a press, on any layout)
	for (int32 k = 0; k < 4; k++) { if (I->Ids[k] == Report[0]) { break; } if (I->Ids[k] < 0) { I->Ids[k] = Report[0]; break; } }
	bool bDiff = false; for (int32 k = 0; k < 10 && k < Len; k++) { if (Report[k] != I->Prev[k]) { bDiff = true; } }
	I->NowLen = int32(Len); for (int32 k = 0; k < 16; k++) { I->Now[k] = k < Len ? Report[k] : 0; I->Prev[k] = I->Now[k]; }
	if (bDiff && I->Changes < 400)
	{
		I->Changes++; FString Hex; for (int32 k = 0; k < 16 && k < Len; k++) { Hex += FString::Printf(TEXT("%02x "), Report[k]); }
		UE_LOG(LogTemp, Display, TEXT("GkPad report changed (len %d): %s"), int32(Len), *Hex);
	}
	// A PS4-style board (which is what a Brook is in PS4 mode) sends report 1 in a fixed, well-known layout. It is read
	// directly here: sticks at bytes 1-2, the d-pad in the low half of byte 5, the buttons in the rest of bytes 5-7.
	if (Len < 8 || Report[0] != 0x01) { return; }
	for (FGkPadDev& D : I->Devs)
	{
		if ((void*)D.Ref != Sender) { continue; }
		static const int32 Ways[8] = { 1, 1 | 8, 8, 2 | 8, 2, 2 | 4, 4, 1 | 4 };
		const int32 Hat = Report[5] & 0x0f; D.Hat = Hat < 8 ? Ways[Hat] : 0;
		D.X = (Report[1] - 128) / 127.f; D.Y = (Report[2] - 128) / 127.f;
		D.Buttons = uint32(Report[5] >> 4) | (uint32(Report[6]) << 4) | (uint32(Report[7] & 0x03) << 12);
		const bool bBusy = D.Buttons != 0 || D.Hat != 0 || FMath::Abs(D.X) > 0.5f || FMath::Abs(D.Y) > 0.5f;
		if (bBusy) { I->Used++; I->LastReportLen = int32(Len); for (int32 k = 0; k < 16; k++) { I->LastReport[k] = k < Len ? Report[k] : 0; } }
		else if (I->LastReportLen == 0) { for (int32 k = 0; k < 16; k++) { I->LastReport[k] = k < Len ? Report[k] : 0; } I->LastReportLen = -int32(Len); }
		break;
	}
}

static void GkPadAdded(void* Ctx, IOReturn, void*, IOHIDDeviceRef Dev)
{
	FScopeLock Lock(&GGkPadLock); FGkPadImpl* I = (FGkPadImpl*)Ctx; if (!I || I != GGkPadLive || !Dev) { return; }
	for (const FGkPadDev& D : I->Devs) { if (D.Ref == Dev) { return; } }
	FGkPadDev D; D.Ref = Dev; D.Vid = GkPadNumber(Dev, CFSTR(kIOHIDVendorIDKey)); D.Pid = GkPadNumber(Dev, CFSTR(kIOHIDProductIDKey));
	char Buf[256] = { 0 }; CFTypeRef Nm = IOHIDDeviceGetProperty(Dev, CFSTR(kIOHIDProductKey));
	if (Nm && CFGetTypeID(Nm) == CFStringGetTypeID() && CFStringGetCString((CFStringRef)Nm, Buf, sizeof(Buf), kCFStringEncodingUTF8)) { D.Name = UTF8_TO_TCHAR(Buf); }
	if (D.Name.IsEmpty()) { D.Name = TEXT("Controller"); }
	I->Devs.Add(D); GkPadList(I);
	const IOReturn Opened = IOHIDDeviceOpen(Dev, kIOHIDOptionsTypeNone);
	IOHIDDeviceRegisterInputReportCallback(Dev, GGkPadReportBuf, sizeof(GGkPadReportBuf), GkPadReport, Ctx);
	const bool bKeys = IOHIDDeviceConformsTo(Dev, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) || IOHIDDeviceConformsTo(Dev, kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad) || IOHIDDeviceConformsTo(Dev, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse);
	if (bKeys) { I->Keyboardish = 1; }
	UE_LOG(LogTemp, Display, TEXT("GkPad found: %s vendor %04x product %04x, open result %08x, primary usage %d/%d, also a keyboard or mouse: %d"), *D.Name, D.Vid, D.Pid, uint32(Opened),
		GkPadNumber(Dev, CFSTR(kIOHIDPrimaryUsagePageKey)), GkPadNumber(Dev, CFSTR(kIOHIDPrimaryUsageKey)), bKeys ? 1 : 0);
}

static void GkPadRemoved(void* Ctx, IOReturn, void*, IOHIDDeviceRef Dev)
{
	FScopeLock Lock(&GGkPadLock); FGkPadImpl* I = (FGkPadImpl*)Ctx; if (!I || I != GGkPadLive) { return; }
	I->Devs.RemoveAll([Dev](const FGkPadDev& D) { return D.Ref == Dev; }); GkPadList(I);
}

static void GkPadValue(void* Ctx, IOReturn, void*, IOHIDValueRef Val)
{
	if (!Val) { return; }
	IOHIDElementRef El = IOHIDValueGetElement(Val); if (!El) { return; }
	const uint32 Page = IOHIDElementGetUsagePage(El), Usage = IOHIDElementGetUsage(El);
	const bool bPadKey = Page == kHIDPage_GenericDesktop && Usage >= 0x90 && Usage <= 0x93;   // some sticks send the d-pad as four buttons
	const bool bButton = Page == kHIDPage_Button, bDesk = Page == kHIDPage_GenericDesktop && (Usage == kHIDUsage_GD_Hatswitch || Usage == kHIDUsage_GD_X || Usage == kHIDUsage_GD_Y || bPadKey);
	const bool bLong = IOHIDValueGetLength(Val) > 4;
	const long V = bLong ? 0 : (long)IOHIDValueGetIntegerValue(Val), Lo = (long)IOHIDElementGetLogicalMin(El), Hi = (long)IOHIDElementGetLogicalMax(El);
	IOHIDDeviceRef Dev = IOHIDElementGetDevice(El);
	FScopeLock Lock(&GGkPadLock); FGkPadImpl* I = (FGkPadImpl*)Ctx; if (!I || I != GGkPadLive) { return; }
	I->Events++;
	if (bButton || (bDesk && Usage != kHIDUsage_GD_X && Usage != kHIDUsage_GD_Y)) { I->LastPage = int32(Page); I->LastUsage = int32(Usage); I->LastValue = int32(V); }
	if (I->Events <= 300 && (bButton || bDesk)) { UE_LOG(LogTemp, Display, TEXT("GkPad value: page %u usage %u value %ld (range %ld..%ld)"), Page, Usage, V, Lo, Hi); }
	if ((!bButton && !bDesk) || bLong) { return; }
	I->Used++;
	for (FGkPadDev& D : I->Devs)
	{
		if (D.Ref != Dev) { continue; }
		if (bButton) { if (Usage >= 1 && Usage <= 32) { const uint32 Bit = 1u << (Usage - 1); if (V != 0) { D.Buttons |= Bit; } else { D.Buttons &= ~Bit; } } }
		else if (bPadKey) { static const int32 Bits[4] = { 1, 2, 8, 4 }; const int32 Bit = Bits[Usage - 0x90]; if (V != 0) { D.Hat |= Bit; } else { D.Hat &= ~Bit; } }
		else if (Usage == kHIDUsage_GD_Hatswitch)
		{
			// eight ways clockwise from up (some hats have four); anything outside the range is the stick at rest
			static const int32 Ways[8] = { 1, 1 | 8, 8, 2 | 8, 2, 2 | 4, 4, 1 | 4 };
			const long N = Hi - Lo + 1; long K = V - Lo; if (N == 4) { K *= 2; }
			D.Hat = (V >= Lo && V <= Hi && K >= 0 && K < 8) ? Ways[K] : 0;
		}
		else if (Hi > Lo) { const float K = float(V - Lo) / float(Hi - Lo) * 2.f - 1.f; if (Usage == kHIDUsage_GD_X) { D.X = K; } else { D.Y = K; } }
		break;
	}
}

static CFDictionaryRef GkPadMatch(int32 Usage)
{
	CFMutableDictionaryRef Dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	int32 Page = kHIDPage_GenericDesktop;
	CFNumberRef P = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &Page), U = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &Usage);
	CFDictionarySetValue(Dict, CFSTR(kIOHIDDeviceUsagePageKey), P); CFDictionarySetValue(Dict, CFSTR(kIOHIDDeviceUsageKey), U);
	CFRelease(P); CFRelease(U);
	return Dict;
}

FGkPads::FGkPads()
{
	Impl = new FGkPadImpl();
	{ FScopeLock Lock(&GGkPadLock); GGkPadLive = Impl; }
	IOHIDManagerRef Mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone); if (!Mgr) { return; }
	CFDictionaryRef Kinds[3] = { GkPadMatch(kHIDUsage_GD_Joystick), GkPadMatch(kHIDUsage_GD_GamePad), GkPadMatch(kHIDUsage_GD_MultiAxisController) };
	CFArrayRef Want = CFArrayCreate(kCFAllocatorDefault, (const void**)Kinds, 3, &kCFTypeArrayCallBacks);
	IOHIDManagerSetDeviceMatchingMultiple(Mgr, Want);
	CFRelease(Want); for (CFDictionaryRef K : Kinds) { CFRelease(K); }
	IOHIDManagerRegisterDeviceMatchingCallback(Mgr, GkPadAdded, Impl);
	IOHIDManagerRegisterDeviceRemovalCallback(Mgr, GkPadRemoved, Impl);
	IOHIDManagerRegisterInputValueCallback(Mgr, GkPadValue, Impl);
	IOHIDManagerScheduleWithRunLoop(Mgr, CFRunLoopGetMain(), kCFRunLoopCommonModes);
	const IOReturn Opened = IOHIDManagerOpen(Mgr, kIOHIDOptionsTypeNone);
	{ FScopeLock Lock(&GGkPadLock); Impl->OpenResult = int32(Opened); }
	// macOS will not hand over a device it counts as a keyboard without the user's say-so (Privacy & Security > Input Monitoring).
	// If that is what stopped us, ask: the system shows its own prompt, once.
	const int32 Access = int32(IOHIDCheckAccess(kIOHIDRequestTypeListenEvent));
	{ FScopeLock Lock(&GGkPadLock); Impl->Access = Access; }
	if (Opened != kIOReturnSuccess && Access != int32(kIOHIDAccessTypeGranted)) { IOHIDRequestAccess(kIOHIDRequestTypeListenEvent); }
	UE_LOG(LogTemp, Display, TEXT("GkPad manager open result %08x, input monitoring access %d"), uint32(Opened), Access);
	{ FScopeLock Lock(&GGkPadLock); Impl->Mgr = Mgr; GkPadList(Impl); }
}

FGkPads::~FGkPads()
{
	IOHIDManagerRef Mgr = nullptr;
	{ FScopeLock Lock(&GGkPadLock); if (GGkPadLive == Impl) { GGkPadLive = nullptr; } Mgr = Impl->Mgr; Impl->Mgr = nullptr; }
	if (Mgr)
	{
		IOHIDManagerUnscheduleFromRunLoop(Mgr, CFRunLoopGetMain(), kCFRunLoopCommonModes);
		IOHIDManagerClose(Mgr, kIOHIDOptionsTypeNone); CFRelease(Mgr);
	}
	FScopeLock Lock(&GGkPadLock); delete Impl; Impl = nullptr;
}

int32 FGkPads::Read(uint32& OutButtons, int32& OutDirs, FString* OutNames) const
{
	FScopeLock Lock(&GGkPadLock); OutButtons = 0; OutDirs = 0; if (!Impl) { return 0; }
	for (const FGkPadDev& D : Impl->Devs)
	{
		OutButtons |= D.Buttons; OutDirs |= D.Hat;
		if (D.X < -0.5f) { OutDirs |= 4; } if (D.X > 0.5f) { OutDirs |= 8; } if (D.Y < -0.5f) { OutDirs |= 1; } if (D.Y > 0.5f) { OutDirs |= 2; }
		if (OutNames) { if (!OutNames->IsEmpty()) { *OutNames += TEXT(", "); } *OutNames += D.Name; }
	}
	return Impl->Used > 0 ? Impl->Devs.Num() : 0;
}

FString FGkPads::Debug() const
{
	FScopeLock Lock(&GGkPadLock); if (!Impl) { return FString(); }
	FString Hex; for (int32 k = 0; k < FMath::Min(12, Impl->NowLen); k++) { Hex += FString::Printf(TEXT("%02x "), Impl->Now[k]); }
	Hex += FString::Printf(TEXT("| kinds %d %d %d | changes %d"), Impl->Ids[0], Impl->Ids[1], Impl->Ids[2], Impl->Changes);
	if (Impl->OpenResult != 0 && Impl->Devs.Num() > 0) { return FString::Printf(TEXT("macOS is blocking the stick (%08x, access %d). Allow GekitotsuNative in System Settings > Privacy & Security > Input Monitoring, then reopen the game."), uint32(Impl->OpenResult), Impl->Access); }
	return FString::Printf(TEXT("USB: open %08x   values %d   presses heard %d   last value: page %d usage %d value %d   reports %d, now [%s]"), uint32(Impl->OpenResult), Impl->Events, Impl->Used, Impl->LastPage, Impl->LastUsage, Impl->LastValue, Impl->Reports, *Hex);
}

#else

struct FGkPadImpl {};
FString FGkPads::Debug() const { return FString(); }
FGkPads::FGkPads() {}
FGkPads::~FGkPads() {}
int32 FGkPads::Read(uint32& OutButtons, int32& OutDirs, FString* OutNames) const { OutButtons = 0; OutDirs = 0; return 0; }

#endif
