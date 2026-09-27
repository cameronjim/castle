// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/HawkeyeShots.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Hawkeye.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

namespace HawkeyeShots
{
	/** GFrameCounter of the last frame whose draw took one of these captures; 0 before any. */
	static uint64 LastCaptureFrame = 0;

	enum class EState : uint8
	{
		Queued,
		Capturing,
		Writing,
		Done,
		Failed,
	};

	struct FShot
	{
		FAutomationTestBase* Test = nullptr;
		FString Path;
		bool bShowUI = true;
		FDateTime RequestedUtc;
		double IssuedSeconds = 0.0;
		EState State = EState::Queued;
		/** Set by the writer thread: 0 running, 1 written, -1 failed. */
		TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> WriteResult;
	};

	/**
	 * The one queue every test's captures go through, ticked by the core ticker so the latent
	 * commands that asked for the shots do not have to.
	 */
	class FQueue
	{
	public:
		static FQueue& Get()
		{
			static FQueue Queue;
			return Queue;
		}

		void Add(FAutomationTestBase* Test, const FString& Path, bool bShowUI)
		{
			const int32 Busy = Current;
			FShot& Shot = Shots.AddDefaulted_GetRef();
			Shot.Test = Test;
			Shot.Path = Path;
			Shot.bShowUI = bShowUI;
			Shot.RequestedUtc = FDateTime::UtcNow();
			if (Busy != INDEX_NONE || FScreenshotRequest::IsScreenshotRequested())
			{
				UE_LOG(LogHawkeye, Log, TEXT("Shots: %s waits for %s."), *FPaths::GetCleanFilename(Path),
					Busy != INDEX_NONE ? *FPaths::GetCleanFilename(Shots[Busy].Path) : TEXT("another capture"));
			}
			if (!TickHandle.IsValid())
			{
				// The PNG writer lives in a module: load it here on the game thread, not on the worker.
				FModuleManager::Get().LoadModule(TEXT("ImageWrapper"));
				TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FQueue::Tick));
			}
			// With nothing in flight it goes out now, so the frame the test asked for is the one captured.
			Tick(0.f);
		}

		int32 CountOutstanding(const FAutomationTestBase* Test) const
		{
			int32 N = 0;
			for (const FShot& Shot : Shots)
			{
				N += Shot.Test == Test && (Shot.State == EState::Queued || Shot.State == EState::Capturing
					|| Shot.State == EState::Writing) ? 1 : 0;
			}
			return N;
		}

		int32 CountFailed(const FAutomationTestBase* Test) const
		{
			int32 N = 0;
			for (const FShot& Shot : Shots)
			{
				N += Shot.Test == Test && Shot.State == EState::Failed ? 1 : 0;
			}
			return N;
		}

		void Forget(const FAutomationTestBase* Test)
		{
			// Indices into Shots stay valid while one is in flight.
			if (Current != INDEX_NONE)
			{
				return;
			}
			Shots.RemoveAll([Test](const FShot& Shot)
			{
				return Shot.Test == Test && (Shot.State == EState::Done || Shot.State == EState::Failed);
			});
		}

	private:
		bool Tick(float /*DeltaSeconds*/)
		{
			const double Now = FPlatformTime::Seconds();
			if (Current != INDEX_NONE)
			{
				FShot& Shot = Shots[Current];
				if (Shot.State == EState::Capturing && Now - Shot.IssuedSeconds > CaptureTimeoutSeconds)
				{
					Unbind();
					// Take the stale request back, or it would capture the next test's frame under this name.
					FScreenshotRequest::Reset();
					Fail(Shot, FString::Printf(TEXT("the viewport never drew it in %.0f s (%s)"), CaptureTimeoutSeconds,
						*DescribeViewport()));
					Current = INDEX_NONE;
				}
				else if (Shot.State == EState::Writing && Shot.WriteResult.IsValid() && Shot.WriteResult->Load() != 0)
				{
					Check(Shot, Shot.WriteResult->Load() > 0);
					Current = INDEX_NONE;
				}
				else if (Shot.State == EState::Writing && Now - Shot.IssuedSeconds > CaptureTimeoutSeconds * 3.0)
				{
					Fail(Shot, TEXT("the PNG writer never finished"));
					Current = INDEX_NONE;
				}
			}
			if (Current == INDEX_NONE)
			{
				Issue(Now);
			}
			return true;
		}

		void Issue(double Now)
		{
			// Someone else's capture (a console HighResShot) owns the single request slot: wait for it.
			if (FScreenshotRequest::IsScreenshotRequested())
			{
				return;
			}
			const int32 Next = Shots.IndexOfByPredicate([](const FShot& Shot) { return Shot.State == EState::Queued; });
			if (Next == INDEX_NONE)
			{
				return;
			}
			FShot& Shot = Shots[Next];
			Current = Next;
			Shot.IssuedSeconds = Now;
			Shot.State = EState::Capturing;
			CapturedHandle = UGameViewportClient::OnScreenshotCaptured().AddRaw(this, &FQueue::HandleCaptured);
			FScreenshotRequest::RequestScreenshot(Shot.Path, Shot.bShowUI, /*bAddUniqueSuffix=*/false);
		}

		/** Inside the viewport's draw: copy the pixels and hand the PNG to a worker. */
		void HandleCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors)
		{
			Unbind();
			LastCaptureFrame = GFrameCounter;
			if (Current == INDEX_NONE || Shots[Current].State != EState::Capturing)
			{
				return;
			}
			FShot& Shot = Shots[Current];
			Shot.State = EState::Writing;
			Shot.WriteResult = MakeShared<TAtomic<int32>, ESPMode::ThreadSafe>(0);
			TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> Result = Shot.WriteResult;
			const FString Path = Shot.Path;
			Async(EAsyncExecution::ThreadPool, [Width, Height, Pixels = Colors, Path, Result]()
			{
				const FImageView Image(Pixels.GetData(), Width, Height);
				const bool bSaved = FImageUtils::SaveImageByExtension(*Path, Image);
				Result->Store(bSaved ? 1 : -1);
			});
		}

		void Check(FShot& Shot, bool bSaved)
		{
			IFileManager& Files = IFileManager::Get();
			const bool bExists = Files.FileExists(*Shot.Path);
			const int64 Size = bExists ? Files.FileSize(*Shot.Path) : -1;
			const FDateTime Stamp = bExists ? Files.GetTimeStamp(*Shot.Path) : FDateTime::MinValue();
			if (bSaved && IsFreshCapture(bExists, Size, Stamp, Shot.RequestedUtc))
			{
				Shot.State = EState::Done;
				return;
			}
			FString Why;
			if (!bSaved)
			{
				Why = TEXT("the PNG could not be written");
			}
			else if (!bExists)
			{
				Why = TEXT("there is no file after the capture");
			}
			else if (Size <= 0)
			{
				Why = TEXT("the file is empty");
			}
			else
			{
				Why = FString::Printf(TEXT("the file is from %s, before the request at %s"), *Stamp.ToString(),
					*Shot.RequestedUtc.ToString());
			}
			Fail(Shot, Why);
		}

		void Fail(FShot& Shot, const FString& Why)
		{
			Shot.State = EState::Failed;
			const FString Message = FString::Printf(TEXT("Screenshot %s was not captured: %s."), *Shot.Path, *Why);
			UE_LOG(LogHawkeye, Error, TEXT("Shots: %s"), *Message);
			if (Shot.Test)
			{
				Shot.Test->AddError(Message);
			}
		}

		void Unbind()
		{
			if (CapturedHandle.IsValid())
			{
				UGameViewportClient::OnScreenshotCaptured().Remove(CapturedHandle);
				CapturedHandle.Reset();
			}
		}

		static FString DescribeViewport()
		{
			UGameViewportClient* Client = GEngine ? GEngine->GameViewport.Get() : nullptr;
			const FViewport* Viewport = Client ? Client->Viewport : nullptr;
			const TSharedPtr<SWindow> Window = Client ? Client->GetWindow() : nullptr;
			const FIntPoint Size = Viewport ? Viewport->GetSizeXY() : FIntPoint::ZeroValue;
			const bool bActive = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive();
			return FString::Printf(TEXT("viewport %dx%d, window %s%s, application %s"), Size.X, Size.Y,
				Window.IsValid() ? TEXT("open") : TEXT("missing"),
				Window.IsValid() && Window->IsWindowMinimized() ? TEXT(" and minimised") : TEXT(""),
				bActive ? TEXT("in front") : TEXT("in the background"));
		}

		TArray<FShot> Shots;
		int32 Current = INDEX_NONE;
		FDelegateHandle CapturedHandle;
		FTSTicker::FDelegateHandle TickHandle;
	};

	void Request(FAutomationTestBase* Test, const FString& FullPath, bool bShowUI)
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FullPath));
		if (Test)
		{
			Test->AddInfo(FString::Printf(TEXT("Requested %s"), *FullPath));
		}
		FQueue::Get().Add(Test, FullPath, bShowUI);
	}

	int32 GetOutstanding(const FAutomationTestBase* Test)
	{
		return FQueue::Get().CountOutstanding(Test);
	}

	int32 GetFailures(const FAutomationTestBase* Test)
	{
		return FQueue::Get().CountFailed(Test);
	}

	void ForgetFinished(const FAutomationTestBase* Test)
	{
		FQueue::Get().Forget(Test);
	}

	bool DidCaptureLastFrame()
	{
		return LastCaptureFrame != 0 && GFrameCounter - LastCaptureFrame <= 1;
	}

	bool IsFreshCapture(bool bExists, int64 FileSize, const FDateTime& FileTimeUtc, const FDateTime& RequestedUtc,
		double SlackSeconds)
	{
		return bExists && FileSize > 0 && FileTimeUtc >= RequestedUtc - FTimespan::FromSeconds(SlackSeconds);
	}
}

bool FHawkeyeWaitForShots::Update()
{
	const double Now = FPlatformTime::Seconds();
	if (StartSeconds < 0.0)
	{
		StartSeconds = Now;
	}
	const int32 Outstanding = HawkeyeShots::GetOutstanding(Test);
	if (Outstanding > 0 && Now - StartSeconds < TimeoutSeconds)
	{
		return false;
	}
	const int32 Failures = HawkeyeShots::GetFailures(Test);
	if (Test)
	{
		if (Outstanding > 0)
		{
			Test->AddError(FString::Printf(TEXT("%d screenshot(s) still not written after %.0f s."), Outstanding,
				TimeoutSeconds));
		}
		Test->AddInfo(FString::Printf(TEXT("Screenshots: %d failed, %d outstanding."), Failures, Outstanding));
	}
	HawkeyeShots::ForgetFinished(Test);
	return true;
}

#endif
