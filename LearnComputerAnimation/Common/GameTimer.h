#pragma once

class GameTimer
{
public:
    GameTimer();

    float TotalTime() const;
    float DeltaTime() const;

    void Reset();   //重置
    void Start();   //开始计时
    void Stop();    //暂停计时
    void Tick();    //每帧调用.

private:
    double mSecondsPerCount;
    double mDeltaTime;

    // __int64 是Windows下的64位
    __int64 mBaseTime;
    __int64 mPausedTime;
    __int64 mStopTime;
    __int64 mPrevTime;
    __int64 mCurrTime;

    bool mStopped;
};
