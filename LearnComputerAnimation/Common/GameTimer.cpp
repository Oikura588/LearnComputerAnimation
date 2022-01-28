#include "GameTimer.h"

#include "d3dApp.h"

GameTimer::GameTimer():mSecondsPerCount(0.0f)
                       ,mDeltaTime(-1.0)
                       ,mBaseTime(0)
                       ,mPausedTime(0)
                       ,mPrevTime(0)
                       ,mCurrTime(0)
                       ,mStopped(false)
                       ,mStopTime(0)
{
    __int64 countsPerSecond;
    // 性能计时器，使用的时间度量单位为count，
    // 使用QueryPerformanceCounter可以拿到当前Count值，count*(1/Frequency)就是当前时间.
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSecond);
    mSecondsPerCount = 1.0/(double)countsPerSecond;
}

float GameTimer::TotalTime() const
{
    if(mStopped)
    {
        return (float)((mStopTime-mPausedTime-mBaseTime)*mSecondsPerCount);
    }
    else
    {
        return (float)((mCurrTime-mPausedTime-mBaseTime)*mSecondsPerCount);
    }
}

float GameTimer::DeltaTime() const
{
    return (float)mDeltaTime;
}

void GameTimer::Reset()
{
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    mBaseTime = currTime;
    mPrevTime = currTime;
    mStopTime = 0;
    mStopped = false;
}

void GameTimer::Start()
{
    __int64 startTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

    if(mStopped)
    {
        mPausedTime +=(startTime-mStopTime);
        mStopTime = 0;
        mStopped = false;
    }
}

void GameTimer::Stop()
{
    if(!mStopped)
    {
        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
        mStopTime = currTime;
        mStopped = true;
    }
}

void GameTimer::Tick()
{
    if(mStopped)
    {
        mDeltaTime = 0.0f;
        return ;
    }
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    mCurrTime = currTime;
    mDeltaTime = (mCurrTime - mPrevTime)*mSecondsPerCount;
    mPrevTime = mCurrTime;

    // Force non negative.
    // The DXSDK's CDXUTTimer mentions that if the processor goes into
    // a power save mode or we get shuffled to another processor, then mDeltaTime can be negative.
    if(mDeltaTime<0.f)
    {
        mDeltaTime = 0.f;
    }
}
