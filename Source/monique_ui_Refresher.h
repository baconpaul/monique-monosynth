#ifndef MONO_UI_REFRESHER_H_INCLUDED
#define MONO_UI_REFRESHER_H_INCLUDED

#include "App_h_includer.h"

//==============================================================================
class Monique_Ui_Refreshable
{
public:
    virtual void refresh() noexcept = 0;

protected:
    Monique_Ui_Refreshable() noexcept;
    virtual ~Monique_Ui_Refreshable() noexcept;
};

//==============================================================================
class Monique_Ui_Refresher : public Timer 
{
    CriticalSection lock;
    Array<Monique_Ui_Refreshable*> refreshables;

    void timerCallback() override;

private:
    friend class Monique_Ui_Refreshable;
    NOINLINE void add(Monique_Ui_Refreshable*const) noexcept;
    NOINLINE void remove(Monique_Ui_Refreshable*const) noexcept;

public:
    NOINLINE void remove_all() noexcept;

private:
    Monique_Ui_Refresher() noexcept;
    ~Monique_Ui_Refresher() noexcept;

public:
    juce_DeclareSingleton (Monique_Ui_Refresher,false)
};

#endif
