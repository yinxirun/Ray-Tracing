#include "archive.h"

void ArchiveState::SetError()
{
    ForEachState([](ArchiveState &State)
                 { State.ArIsError = true; });
}

void ArchiveState::SetIsLoading(bool bInIsLoading)
{
    ArIsLoading = bInIsLoading;
}

void ArchiveState::SetIsSaving(bool bInIsSaving)
{
    ArIsSaving = bInIsSaving;
}

void ArchiveState::SetIsPersistent(bool bInIsPersistent)
{
    ArIsPersistent = bInIsPersistent;
}

template <typename T>
__forceinline void ArchiveState::ForEachState(T Func)
{
    ArchiveState &RootState = GetInnermostState();
    Func(RootState);

    for (ArchiveState *Proxy = RootState.NextProxy; Proxy; Proxy = Proxy->NextProxy)
    {
        Func(*Proxy);
    }
}