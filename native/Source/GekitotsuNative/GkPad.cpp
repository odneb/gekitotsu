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
THIRD_PARTY_INCLUDES_END

struct FGkPadDev { IOHIDDeviceRef Ref = nullptr; FString Name; int32 Vid = 0, Pid = 0; uint32 Buttons = 0; int32 Hat = 0; float X = 0.f, Y = 0.f; };
struct FGkPadImpl { IOHIDManagerRef Mgr = nullptr; TArray<FGkPadDev> Devs; };

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

static void GkPadAdded(void* Ctx, IOReturn, void*, IOHIDDeviceRef Dev)
{
	FScopeLock Lock(&GGkPadLock); FGkPadImpl* I = (FGkPadImpl*)Ctx; if (!I || I != GGkPadLive || !Dev) { return; }
	for (const FGkPadDev& D : I->Devs) { if (D.Ref == Dev) { return; } }
	FGkPadDev D; D.Ref = Dev; D.Vid = GkPadNumber(Dev, CFSTR(kIOHIDVendorIDKey)); D.Pid = GkPadNumber(Dev, CFSTR(kIOHIDProductIDKey));
	char Buf[256] = { 0 }; CFTypeRef Nm = IOHIDDeviceGetProperty(Dev, CFSTR(kIOHIDProductKey));
	if (Nm && CFGetTypeID(Nm) == CFStringGetTypeID() && CFStringGetCString((CFStringRef)Nm, Buf, sizeof(Buf), kCFStringEncodingUTF8)) { D.Name = UTF8_TO_TCHAR(Buf); }
	if (D.Name.IsEmpty()) { D.Name = TEXT("Controller"); }
	I->Devs.Add(D); GkPadList(I);
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
	const bool bButton = Page == kHIDPage_Button, bDesk = Page == kHIDPage_GenericDesktop && (Usage == kHIDUsage_GD_Hatswitch || Usage == kHIDUsage_GD_X || Usage == kHIDUsage_GD_Y);
	if ((!bButton && !bDesk) || IOHIDValueGetLength(Val) > 4) { return; }
	const long V = (long)IOHIDValueGetIntegerValue(Val), Lo = (long)IOHIDElementGetLogicalMin(El), Hi = (long)IOHIDElementGetLogicalMax(El);
	IOHIDDeviceRef Dev = IOHIDElementGetDevice(El);
	FScopeLock Lock(&GGkPadLock); FGkPadImpl* I = (FGkPadImpl*)Ctx; if (!I || I != GGkPadLive) { return; }
	for (FGkPadDev& D : I->Devs)
	{
		if (D.Ref != Dev) { continue; }
		if (bButton) { if (Usage >= 1 && Usage <= 32) { const uint32 Bit = 1u << (Usage - 1); if (V != 0) { D.Buttons |= Bit; } else { D.Buttons &= ~Bit; } } }
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
	IOHIDManagerOpen(Mgr, kIOHIDOptionsTypeNone);
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
	return Impl->Devs.Num();
}

#else

struct FGkPadImpl {};
FGkPads::FGkPads() {}
FGkPads::~FGkPads() {}
int32 FGkPads::Read(uint32& OutButtons, int32& OutDirs, FString* OutNames) const { OutButtons = 0; OutDirs = 0; return 0; }

#endif
