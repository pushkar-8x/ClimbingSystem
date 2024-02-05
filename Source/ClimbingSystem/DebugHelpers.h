#pragma once

namespace Debug
{
	static void Print(const FString& msg, int32 key = -1 , const FColor& color = FColor::MakeRandomColor())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(key, 5.0f, color, msg);
		}

		UE_LOG(LogTemp, Warning, TEXT("%s"), *msg);
	}
}