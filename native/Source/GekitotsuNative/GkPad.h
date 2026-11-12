#pragma once
#include "CoreMinimal.h"

struct FGkPadImpl;

/**
 * Fight sticks and pads read straight from USB, the way a browser reads them. Unreal on the Mac only listens to the
 * handful of controllers Apple's own framework knows; an arcade board (a Brook, for one) is not among them, so the
 * engine never hears it. This asks the system for every joystick and gamepad and reads their buttons, hat and stick raw.
 */
class FGkPads
{
public:
	FGkPads(); ~FGkPads();
	FGkPads(const FGkPads&) = delete; FGkPads& operator=(const FGkPads&) = delete;
	/** Every controller's state merged. Buttons are numbered as the device numbers them, from 0. Returns how many controllers there are. */
	int32 Read(uint32& OutButtons, int32& OutDirs, FString* OutNames = nullptr) const;
	/** What has actually arrived from the controllers, for the on-screen read-out: how many reports, and the last one. */
	FString Debug() const;
private:
	FGkPadImpl* Impl = nullptr;
};
