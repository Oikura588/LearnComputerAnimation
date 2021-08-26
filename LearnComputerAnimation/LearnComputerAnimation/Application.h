#pragma once
class Application
{
private:
    // 复制构造私有
    Application(const Application&);
    Application& operator=(const Application&);

public:
    inline Application(){}
    inline virtual ~Application(){}
    inline virtual void Initialize(){}
    inline virtual void Update(float inDeltaTime){}
    inline virtual void Render(float inAspectRatio){}
    inline virtual void Shutdown(){}

    
};