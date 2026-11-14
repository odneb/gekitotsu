#include "GkGameEngine.h"
#include "GkMatch.h"

bool UGkGameEngine::Exec(UWorld* World, const TCHAR* Command, FOutputDevice& Output)
{
	// macOS sends a deferred EXIT command for Command-Q and the app's Quit item.
	const FString Action = FString(Command).TrimStartAndEnd();
	if (AGkMatch::Current && (Action.Equals(TEXT("EXIT"), ESearchCase::IgnoreCase)
		|| Action.Equals(TEXT("QUIT"), ESearchCase::IgnoreCase)))
	{
		AGkMatch::Current->ConfirmQuit();
		return true;
	}
	return Super::Exec(World, Command, Output);
}
