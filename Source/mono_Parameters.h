/*
  ==============================================================================

    parameters.h
    Author:  monotomy

  ==============================================================================
*/

#ifndef MONO_PARAMETER_H_INCLUDED
#define MONO_PARAMETER_H_INCLUDED

#include "App_h_includer.h"
#include "monique_ui_LookAndFeel.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

#define DEFAULT_MODULATION 0.2f

// ==============================================================================
// ==============================================================================
// ==============================================================================
enum
{
    HAS_NO_MODULATION = -9999
};

// ==============================================================================
enum TYPES_DEF {
    IS_FLOAT = 0,
    IS_BOOL = 1,
    IS_INT = 2,
    UNKNOWN_TYPE = 3,
};

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================

// TODO
/*
    constexpr arguments for parameter info
*/

template<class T>
struct FastArray : public Array<T>
{
    // THIS FUNCTION DOES NOT USE A LOCK LIKE THE ORIGINAL ARRAY getUnchecked
    inline T getUnchecked( int index_ ) const noexcept
    {
        return Array<T>::data.elements[index_];
    }

    NOINLINE FastArray();
    NOINLINE ~FastArray();
};

//==============================================================================
template<class T> NOINLINE FastArray<T>::FastArray() {}
template<class T> NOINLINE FastArray<T>::~FastArray() {}

//==============================================================================
//==============================================================================
//==============================================================================
class Parameter;
class ParameterListener
{
    friend class Parameter;
    virtual void parameter_value_changed( Parameter* ) noexcept {}
    virtual void parameter_value_changed_always_notification( Parameter* ) noexcept {}
    virtual void parameter_value_on_load_changed( Parameter* ) noexcept {}
    virtual void parameter_modulation_value_changed( Parameter* ) noexcept {}

protected:
    NOINLINE ParameterListener() noexcept;
    NOINLINE ~ParameterListener() noexcept;
};

// ==============================================================================
// ==============================================================================
// ==============================================================================
class ChangeParamOverTime;
struct ParameterInfo
{
    TYPES_DEF type;

    const float min_value;
    const float max_value;
    const float init_value;

    const int num_steps;

    const String name;
    const String short_name;

private:
    // ==============================================================================
    friend class Parameter;
#define MIN_MAX(min_,max_) min_,max_ /* HELPER MACRO TO MAKE CTOR ARGUMENTS MORE READABLE */
    NOINLINE ParameterInfo( TYPES_DEF type_,
                            const float min_value_, const float max_value_, const float init_value_,
                            const int num_steps_,
                            const String& name_, const String& short_name_ ) noexcept;
    NOINLINE ~ParameterInfo() noexcept;

private:
    // ==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( ParameterInfo )
};

// ==============================================================================
// ==============================================================================
// ==============================================================================
class ParameterRuntimeInfo
{
public:
    // ==============================================================================
    inline void set_last_modulation_amount( float current_modulation_amount_ ) noexcept
    {
        current_modulation_amount = current_modulation_amount_;
    }
    inline float get_last_modulation_amount() const noexcept
    {
        return current_modulation_amount;
    }
private:
    float current_modulation_amount;

private:
    // ==============================================================================
    friend class Parameter;
    NOINLINE ParameterRuntimeInfo() noexcept;
    NOINLINE ~ParameterRuntimeInfo() noexcept;

private:
    // ==============================================================================
    friend class ChangeParamOverTime;
    ChangeParamOverTime* timeChanger;
public:
    inline void stop_time_change() const noexcept; // see below ChangeParamOverTime

private:
    // ==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( ParameterRuntimeInfo )
};

// ==============================================================================
// ==============================================================================
// ==============================================================================
// TODO is it more performant to have the listerner functions not inlined in the param class?
// -> e.g. move to info?
class MIDIControl;
static inline void write_parameter_to_file( XmlElement& xml_, const Parameter* param_ ) noexcept;
static inline void read_parameter_from_file( const XmlElement& xml_, Parameter* param_ ) noexcept;
class Parameter
{
public:
    // ==============================================================================
    // GETTER
    inline operator float() const noexcept
    {
        return value;
    }
    inline float get_value() const noexcept
    {
        return value;
    }
protected:
    // ASSUME THE MEMORY GOES LINEAR FORWARD AND THE GETTER IS THE MOST USED FUNCTION
    float value;

private:
    // IF YOU GET AN COMPILE ERROR YOU SHOULD TAKE A LOOK AT BoolParameter or IntParameter
    //inline operator bool() const noexcept = delete;
    //inline operator int() const noexcept = delete;

public:
    // ==============================================================================
    // HELPER
    inline bool operator== ( float value_ ) const noexcept
    {
        return value == value_;
    }
    inline bool operator== ( int value_ ) const noexcept
    {
        return value == float(value_);
    }
    inline const Parameter* ptr() const noexcept
    {
        return this;
    }
    inline Parameter* ptr() noexcept
    {
        return this;
    }

public:
    // ==============================================================================
    // SETTER
#if DEBUG
#define DBG_CHECK_RANGE( x ) \
    if( x > info->max_value ) { \
      std::cout << "ERROR: value is bigger as max: " << info->short_name << "->" << x << " max:"<<  info->max_value << std::endl; \
    } \
    else if( x < info->min_value ) { \
      std::cout << "ERROR: value is smaller as min: " << info->short_name << "->" << x << " max:"<<  info->min_value << std::endl; \
    }
#else
#define DBG_CHECK_RANGE( x )
#endif

    inline void set_value( float value_ ) noexcept
    {
        DBG_CHECK_RANGE( value_ )
        if( value != value_ )
        {
            value = value_;
            notify_value_listeners();
        }
    }
    inline float operator= ( float value_ ) noexcept
    {
        DBG_CHECK_RANGE( value_ )
        if( value != value_ )
        {
            value = value_;
            notify_value_listeners();
        }
        return value_;
    }
    inline float operator= ( const Parameter& other_ ) noexcept
    {
        const float value_( other_.value );
        DBG_CHECK_RANGE( value_ )
        if( value != value_ )
        {
            value = value_;
            notify_value_listeners();
        }
        return value_;
    }
    inline void set_value_without_notification( float value_ ) noexcept
    {
        DBG_CHECK_RANGE( value_ )
        if( value != value_ )
        {
            value = value_;
            notify_always_value_listeners();
        }
    }

public:
    // ==============================================================================
    // MODULATOR (OPTIONAL PARAMETER)
#if DEBUG
#define DBG_CHECK_MODULATOR_RANGE( x ) \
    if( x > 1 ) { \
      std::cout << "ERROR: modulator is bigger 1: " << info->short_name << "->" << x << std::endl; \
    } \
    else if( x < -1 ) { \
      std::cout << "ERROR: modulator is smaller -1: " << info->short_name << "->" << x << std::endl; \
    }
#else
#define DBG_CHECK_MODULATOR_RANGE( x )
#endif
    inline float get_modulation_amount() const noexcept
    {
        return modulation_amount;
    }
protected:
    // MODULATION AMOUNT GOES MIN AND MAX FROM -1 to 1
    float modulation_amount;

public:
    // ==============================================================================
    // MODULATOR (OPTIONAL PARAMETER)
    inline void set_modulation_amount( float modulation_amount_ ) noexcept
    {
        DBG_CHECK_MODULATOR_RANGE( modulation_amount_ )
        if( modulation_amount != modulation_amount_ )
        {
            modulation_amount = modulation_amount_;
            notify_modulation_value_listeners();
        }// TODO remove the fucking templates!

// ==============================================================================
// ==============================================================================
// ==============================================================================
        /*
        template<typename T>
        class mono_ParameterBase;
        */

        /*
        // TODO listener for modulators
        template<typename T>
        class mono_ParameterListener {
            friend class mono_ParameterBase< T >;
            virtual void parameter_value_changed( mono_ParameterBase< T >* param_ ) noexcept = 0;
            virtual void parameter_value_changed_always_notification( mono_ParameterBase< T >* param_ ) noexcept {};
            virtual void parameter_modulation_value_changed( mono_ParameterBase< T >* param_ ) noexcept {};
            virtual void parameter_value_on_load_changed( mono_ParameterBase< T >* param_ ) noexcept {};

        protected:
            NOINLINE mono_ParameterListener();
            NOINLINE ~mono_ParameterListener();
        };

        template<typename T>
        NOINLINE mono_ParameterListener<T>::mono_ParameterListener() {};
        template<typename T>
        NOINLINE mono_ParameterListener<T>::~mono_ParameterListener() {};
        */
    }
    inline void set_modulation_amount_without_notification( float modulation_amount_ ) noexcept
    {
        DBG_CHECK_MODULATOR_RANGE( modulation_amount_ )
        modulation_amount = modulation_amount_;
    }

public:
    // ==============================================================================
    // INFO
    inline const ParameterInfo& get_info() const noexcept
    {
        return *info;
    }
protected:
    const ParameterInfo*const info;

public:
    // ==============================================================================
    // RUNTIME INFO
    inline const ParameterRuntimeInfo& get_runtime_info() const noexcept
    {
        return *runtime_info;
    }
    inline ParameterRuntimeInfo& get_runtime_info() noexcept
    {
        return *runtime_info;
    }

protected:
    ParameterRuntimeInfo*const runtime_info;

protected:
    // ==============================================================================
    // OBSERVABLE
    FastArray< ParameterListener* > value_listeners;
    FastArray< ParameterListener* > always_value_listeners;

public:
    // REGISTRATIONS
    void register_listener( ParameterListener* listener_ ) noexcept;
    void register_always_listener( ParameterListener* listener_ ) noexcept;
    void remove_listener( const ParameterListener* listener_ ) noexcept;

protected:
    // NOT THREAD SAVE IF YOU ADD LISTENERS AT RUNTIME
    // NOTIFICATIONS
    inline void notify_value_listeners() noexcept;
    inline void notify_always_value_listeners() noexcept;
    friend void read_parameter_from_file( const XmlElement&, Parameter* ) noexcept;
    inline void notify_on_load_value_listeners() noexcept;
    inline void notify_modulation_value_listeners() noexcept;

public:
    // ==============================================================================
    // MIFI
    // TODO OWN THREAD NOTIFICATION HANDLER E.G. FOR MIDI
    MIDIControl*const midi_control;

public:
    // ==============================================================================
    // NOTE: the parameter is designed to live for a long time (app start to end and ctors may need some more power as usual)
    NOINLINE Parameter(const float min_value_, const float max_value_, const float init_value_,
                       const int num_steps_,
                       const String& name_, const String& short_name_,
                       const float init_modulation_amount_ = HAS_NO_MODULATION,
                       TYPES_DEF = IS_FLOAT
                      ) noexcept;
    NOINLINE ~Parameter() noexcept;

private:
    // ==============================================================================
    MONO_NOT_CTOR_COPYABLE( Parameter )
    MONO_NOT_MOVE_COPY_OPERATOR( Parameter )
};

// ==============================================================================
inline void Parameter::notify_value_listeners() noexcept
{
    for( int i = 0 ; i != value_listeners.size() ; ++i )
    {
        value_listeners.getUnchecked(i)->parameter_value_changed( this );
    }
};
inline void Parameter::notify_always_value_listeners() noexcept
{
    for( int i = 0 ; i != always_value_listeners.size() ; ++i )
    {
        always_value_listeners.getUnchecked(i)->parameter_value_changed_always_notification( this );
    }
};
inline void Parameter::notify_on_load_value_listeners() noexcept
{
    for( int i = 0 ; i != value_listeners.size() ; ++i )
    {
        value_listeners.getUnchecked(i)->parameter_value_on_load_changed( this );
    }
}
inline void Parameter::notify_modulation_value_listeners() noexcept
{
    for( int i = 0 ; i != value_listeners.size() ; ++i )
    {
        value_listeners.getUnchecked(i)->parameter_modulation_value_changed( this );
    }
}

// ==============================================================================
// ==============================================================================
// ==============================================================================
// TODO notifications
class BoolParameter : public Parameter
{
public:
    // ==============================================================================
    // GETTER
    inline bool operator^=( bool ) noexcept
    {
        value = !value;
        return value;
    }
    inline operator bool() const noexcept
    {
        return bool(value);
    }

public:
    // ==============================================================================
    // HELPER
    inline bool operator== ( bool value_ ) const noexcept
    {
        return bool(value) == value_;
    }
    inline const BoolParameter* bool_ptr() const noexcept
    {
        return this;
    }
    inline BoolParameter* bool_ptr() noexcept
    {
        return this;
    }

public:
    // ==============================================================================
    // SETTER
    inline bool operator= ( const bool value_ ) noexcept
    {
        DBG_CHECK_RANGE( value_ )
        return bool(value = value_);
    }
    inline bool operator= ( const BoolParameter& other_ ) noexcept
    {
        DBG_CHECK_RANGE( other_.value )
        return bool(value = other_.value);
    }

private:
    // IF YOU GET AN COMPILE ERROR, YOU HAVE USED THE WRONG PARAM
    inline operator float() const noexcept = delete;
    inline operator int() const noexcept = delete;

public:
    NOINLINE BoolParameter( const bool init_value_,
                            const String& name_, const String& short_name_ ) noexcept;
    NOINLINE ~BoolParameter() noexcept;

private:
    // ==============================================================================
    MONO_NOT_CTOR_COPYABLE( BoolParameter )
    MONO_NOT_MOVE_COPY_OPERATOR( BoolParameter )
};
// ==============================================================================
// ==============================================================================
// ==============================================================================
// TODO notifications
class IntParameter : public Parameter
{
public:
    // ==============================================================================
    // GETTER
    inline operator int() const noexcept
    {
        return int(value);
    }

public:
    // ==============================================================================
    // HELPER
    inline int operator== ( int value_ ) const noexcept
    {
        return int(value) == value_;
    }
    inline const IntParameter* int_ptr() const noexcept
    {
        return this;
    }
    inline IntParameter* int_ptr() noexcept
    {
        return this;
    }

public:
    // ==============================================================================
    // SETTER
    inline int operator= ( const int value_ ) noexcept
    {
        DBG_CHECK_RANGE( value_ )
        return int(value = value_);
    }
    inline int operator= ( const IntParameter& other_ ) noexcept
    {
        DBG_CHECK_RANGE( other_.value )
        return int(value = other_.value);
    }

private:
    // IF YOU GET AN COMPILE ERROR, YOU HAVE USED THE WRONG PARAM
    inline operator float() const noexcept = delete;
    //inline operator bool() const noexcept = delete;

public:
    NOINLINE IntParameter( const int min_value_, const int max_value_, const int init_value_,
                           const String& name_, const String& short_name_ ) noexcept;
    NOINLINE ~IntParameter() noexcept;

private:
    // ==============================================================================
    MONO_NOT_CTOR_COPYABLE( IntParameter )
    MONO_NOT_MOVE_COPY_OPERATOR( IntParameter )
};

// ==============================================================================
// ==============================================================================
// ==============================================================================
class ModulatedParameter : public Parameter
{
public:
    NOINLINE ModulatedParameter(const float min_value_, const float max_value_, const float init_value_,
                                const int num_steps_,
                                const String& name_, const String& short_name_,
                                const float init_modulation_amount_ ) noexcept;
    NOINLINE ~ModulatedParameter() noexcept;
};

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
class ArrayOfParameters
{
    FastArray<Parameter*> parameters;

public:
#if DEBUG
#define DEBUG_CHECK_ARRAY_RANGE( x ) if( x >= parameters.size() ) { std::cout << "ERROR: ARRAY ACCESS OUT OF RANGE" << std::endl; }
#else
#define DEBUG_CHECK_ARRAY_RANGE( x )
#endif
    inline Parameter& operator[]( int index_ ) noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }
    inline Parameter& get( int index_ ) const noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }
    inline const Parameter& operator[]( int index_ ) const noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }
    inline const Parameter& get( int index_ ) noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }

public:
    NOINLINE ArrayOfParameters( const int num_parameters_,

                                const float min_value_, const float max_value_, const float init_value_,
                                const int num_steps_,

                                const String& owner_class_name_,
                                const int owner_id_,

                                const String& param_name_,
                                const String& param_name_short_,
                                bool create_human_id_ = true
                              ) noexcept;
    NOINLINE ~ArrayOfParameters() noexcept;

private:
    // ==============================================================================
    MONO_NOT_CTOR_COPYABLE( ArrayOfParameters )
    MONO_NOT_MOVE_COPY_OPERATOR( ArrayOfParameters )
};

//==============================================================================
//==============================================================================
//==============================================================================
class ArrayOfBoolParameters
{
    FastArray<BoolParameter*> parameters;

public:
    inline BoolParameter& operator[]( int index_ ) noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }
    inline BoolParameter& get( int index_ ) const noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }
    inline const BoolParameter& operator[]( int index_ ) const noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }
    inline const BoolParameter& get( int index_ ) noexcept
    {
        DEBUG_CHECK_ARRAY_RANGE( index_ );
        return *parameters.getUnchecked( index_ );
    }

public:
    NOINLINE ArrayOfBoolParameters( const int num_parameters_,

                                    const bool init_value_,

                                    const String& owner_class_name_,
                                    const int owner_id_,

                                    const String& param_name_,
                                    const String& param_name_short_,
                                    bool create_human_id_ = true
                                  ) noexcept;
    NOINLINE ~ArrayOfBoolParameters() noexcept;

private:
    // ==============================================================================
    MONO_NOT_CTOR_COPYABLE( ArrayOfBoolParameters )
    MONO_NOT_MOVE_COPY_OPERATOR( ArrayOfBoolParameters )
};

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
#define PARAM_CHANGE_INTERVAL_IN_MS 20
class ChangeParamOverTime : public Timer // TODO TIME SLICED THREAD ( DONT START HUNDREDS OF TIMERS!)
{
    Parameter& param;

    int sum_callbacks;

    float current_value;
    const float target_value;
    const float value_delta;
    const float min;
    const float max;

    // ==============================================================================
    ChangeParamOverTime( Parameter& param_, float target_value_, int time_in_ms_ ) noexcept;
    ~ChangeParamOverTime() noexcept;

    // ==============================================================================
    void timerCallback() override;
    inline void change() noexcept;

    friend class ParameterRuntimeInfo;
    inline void forceStopAndKill() noexcept;

public:
    // ==============================================================================
    inline static void execute( Parameter& param_, float target_value_, int time_in_ms_ ) noexcept
    {
        new ChangeParamOverTime(param_,target_value_,time_in_ms_);
    }

private:
    // ==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( ChangeParamOverTime )
};

inline void ChangeParamOverTime::forceStopAndKill() noexcept
{
    stopTimer();
    if( param.get_runtime_info().timeChanger == this )
        param.get_runtime_info().timeChanger = nullptr;

    delete this;
    return;
}

//==============================================================================
inline void ParameterRuntimeInfo::stop_time_change() const noexcept
{
    if( timeChanger )
        timeChanger->forceStopAndKill();
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
// PLUGIN PARAMETER SUPPORT
static inline float get_percent_value( const Parameter* param_ ) noexcept
{
    const ParameterInfo& info = param_->get_info();
    const float min = info.min_value;
    const float max = info.max_value;
    const float value = param_->get_value();

    return 1.0f/(max-min)*(value-min);
}

//==============================================================================
static inline void set_percent_value( Parameter* param_, float value_in_percent_ ) noexcept
{
    const ParameterInfo& info = param_->get_info();
    const float min = info.min_value;
    const float max = info.max_value;

    param_->set_value( (max-min)*value_in_percent_ + min );
}

//==============================================================================
static inline float get_percent_default_value( const Parameter* param_ ) noexcept
{
    const ParameterInfo& info = param_->get_info();
    return 1.0f/(info.max_value-info.min_value)*(info.init_value-info.min_value);
}

//==============================================================================
static inline int get_num_steps( const Parameter* param_ ) noexcept
{
    return param_->get_info().num_steps;
}

//==============================================================================
static inline bool has_modulation( const Parameter* param_ ) noexcept
{
    return param_->get_modulation_amount() != HAS_NO_MODULATION;
}

//==============================================================================
static inline float get_last_modulation_amount( const ModulatedParameter* param_ ) noexcept
{
    return param_->get_runtime_info().get_last_modulation_amount();
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
// operator*
static inline float operator* ( const Parameter& param_, const int value_ ) noexcept
{
    return param_.get_value()*value_;
}
static inline float operator* ( const Parameter& param_, const float value_ ) noexcept
{
    return param_.get_value()*value_;
}
static inline float operator* ( const Parameter& param_, const double value_ ) noexcept
{
    return param_.get_value()*value_;
}
static inline float operator* ( const int value_, const Parameter& param_ ) noexcept
{
    return param_.get_value()*value_;
}
static inline float operator* ( const float value_, const Parameter& param_ ) noexcept
{
    return param_.get_value()*value_;
}
static inline float operator* ( const double value_, const Parameter& param_ ) noexcept
{
    return param_.get_value()*value_;
}
static inline float operator* ( const Parameter& param_lh_, const Parameter& param_rh_ ) noexcept
{
    return param_lh_.get_value()*param_rh_.get_value();
}

// operator+
static inline float operator+ ( const Parameter& param_, const int value_ ) noexcept
{
    return param_.get_value()+value_;
}
static inline float operator+ ( const Parameter& param_, const float value_ ) noexcept
{
    return param_.get_value()+value_;
}
static inline float operator+ ( const Parameter& param_, const double value_ ) noexcept
{
    return param_.get_value()+value_;
}
static inline float operator+ ( const int value_, const Parameter& param_ ) noexcept
{
    return param_.get_value()+value_;
}
static inline float operator+ ( const float value_, const Parameter& param_ ) noexcept
{
    return param_.get_value()+value_;
}
static inline float operator+ ( const double value_, const Parameter& param_ ) noexcept
{
    return param_.get_value()+value_;
}
static inline float operator+ ( const Parameter& param_lh_, const Parameter& param_rh_ ) noexcept
{
    return param_lh_.get_value()+param_rh_.get_value();
}

// operator!=
static inline bool operator!= ( const bool value_, const BoolParameter& param_rh_ ) noexcept
{
    return value_ != bool(param_rh_.get_value());
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
// BUILD NAME HELPERS
NOINLINE static inline String generate_param_name( const String& owner_class, int owner_id_, const String& param_name_, int param_id_ ) noexcept
{
    return owner_class + String("_") + String(owner_id_) + String("_") + param_name_ + String("_") + String(param_id_);
}
NOINLINE static inline String generate_param_name( const String& owner_class, int owner_id_, const String& param_name_ ) noexcept
{
    return owner_class + String("_") + String(owner_id_) + String("_") + param_name_;
}
NOINLINE static inline String generate_short_human_name( const String& owner_class, int owner_id_, const String& param_name_ ) noexcept
{
    return owner_class + String("_") + String(owner_id_+1) + String("_") + param_name_;
}
NOINLINE static inline String generate_short_human_name( const String& owner_class, const String& param_name_ ) noexcept
{
    return owner_class + String("_") + String("_") + param_name_;
}
NOINLINE static inline String generate_short_human_name( const String& param_name_ ) noexcept
{
    return param_name_;
}
NOINLINE static inline String generate_short_human_name( const String& owner_class, const String& param_name_, int param_id_ ) noexcept
{
    return owner_class + String("_") + param_name_ + String("_") + String(param_id_+1);
}
NOINLINE static inline String generate_short_human_name( const String& owner_class, int owner_id_, const String& param_name_, int param_id_ ) noexcept
{
    return owner_class + String("_") + String(owner_id_+1) + String("_") + param_name_ + String("_") + String(param_id_+1);
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
// FILE IO
static inline void write_parameter_to_file( XmlElement& xml_, const Parameter* param_ ) noexcept
{
    const float value = param_->get_value();
    const ParameterInfo& info = param_->get_info();
    if( value != info.init_value )
    {
        xml_.setAttribute( info.name, value );
    }
}
static inline void read_parameter_from_file( const XmlElement& xml_, Parameter* param_ ) noexcept
{
    const ParameterInfo& info = param_->get_info();
    const float old_value = param_->get_value();
    float new_value = xml_.getDoubleAttribute( info.name, info.init_value );
    if( new_value != old_value )
    {
        // TODO dont read the value two times
        const float max_value = info.max_value;
        if( new_value > max_value )
        {
            new_value = max_value;
        }
        else if( new_value < info.min_value )
        {
            new_value = info.min_value;
        }

        param_->set_value_without_notification( new_value );
        param_->notify_on_load_value_listeners();
    }
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
// TYPIFICATION
static inline TYPES_DEF type_of( const Parameter* param_ ) noexcept
{
    return param_->get_info().type;
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
// TOGGLE / SWITCH / INVERT
// IF YOU HAVE AN FLOT OR INT YOU WILL GET MAX OR MIN - DEPENDING ON THE CURRENT STATE
// IF YOU HAVE AN BOOL::FALSE YOO WILL GET BOOL::TRUE
static inline void toggle ( Parameter* param_ ) noexcept
{
    if( type_of( param_ ) == TYPES_DEF::IS_BOOL )
        param_->set_value( not bool(param_->get_value()) );
}
static inline void min_max_switch( Parameter* param_ ) noexcept
{
    const ParameterInfo& info = param_->get_info();
    const float min( info.min_value );
    param_->set_value( param_->get_value() != min ? min : info.max_value );
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
class MIDIControl : ParameterListener
{
public:
    // TODO use from mono_audiodevicemanager
    enum TYPES
    {
        NOT_SET = -1,
        NOTE = 1,
        CC = 0
    };

private:
    friend class Parameter;
    int listen_type;
    int channel;
    int8 midi_number; // NOTES OR CC
    String is_ctrl_version_of_name;
    bool is_in_ctrl_mode;
    Parameter*const owner;

public:
    int get_listen_type() const noexcept {
        return listen_type;
    }
    int8 get_midi_number() const noexcept {
        return midi_number;
    }
    int get_chnanel() const noexcept {
        return channel;
    }
    const String& get_is_ctrl_version_of_name() const noexcept {
        return is_ctrl_version_of_name;
    }

    bool is_listen_to( MidiMessage& input_message_ ) const noexcept;
    bool read_from_if_you_listen( const MidiMessage& input_message_ ) noexcept;
    bool train( const MidiMessage& input_message_, Parameter*const is_ctrl_version_of_ ) noexcept;
    bool train( int listen_type_, int8 midi_number, int channel_, String is_ctrl_version_of_name_ ) noexcept;
    bool is_valid_trained() const noexcept;
    void send_feedback_only() const noexcept;
    void send_clear_feedback_only() const noexcept;

    void set_ctrl_mode( bool mode_ ) noexcept;
    inline bool get_ctrl_mode() const noexcept {
        return is_in_ctrl_mode;
    }

private:
    // FEEDBACK
    void start_listen_for_feedback() noexcept;
    void stop_listen_for_feedback() noexcept;
    void parameter_value_changed( Parameter* param_ ) noexcept override;
    void parameter_value_on_load_changed( Parameter* param_ ) noexcept override;
    void parameter_modulation_value_changed( Parameter* param_ ) noexcept override;


    void generate_feedback_message( MidiMessage& ) const noexcept;
    void generate_modulation_feedback_message( MidiMessage& ) const noexcept;
    void send_standard_feedback() const noexcept;
    void send_modulation_feedback() const noexcept;

public:
    NOINLINE MIDIControl( Parameter*const owner_ );
    NOINLINE ~MIDIControl();

    void clear();
};
class MIDIControlHandler
{
    bool is_activated_and_waiting_for_param;
    Parameter* learning_param;
    Parameter* learning_ctrl_param;

    Array< Component* > learning_comps;

public:
    void toggle_midi_learn() noexcept;
    bool is_waiting_for_param() const noexcept;
    void set_learn_param( Parameter* param_ ) noexcept;
    void set_learn_width_ctrl_param( Parameter* param_, Parameter* ctrl_param_, Component* comp_ ) noexcept;
    void set_learn_param( Parameter* param_, Component* comp_ ) noexcept;
    void set_learn_param( Parameter* param_, Array< Component* > comps_ ) noexcept;
    Parameter* is_learning() const noexcept;
    bool handle_incoming_message( const MidiMessage& input_message_ ) noexcept;
    void clear() noexcept;

    MIDIControlHandler() noexcept;
    ~MIDIControlHandler() noexcept;

    juce_DeclareSingleton (MIDIControlHandler,false)
};
#define IF_MIDI_LEARN__HANDLE( param ) \
        if( MIDIControlHandler::getInstance()->is_waiting_for_param() || MIDIControlHandler::getInstance()->is_learning() ) \
        { \
            MIDIControlHandler::getInstance()->set_learn_param( param ); \
        }
#define IF_MIDI_LEARN__HANDLE__AND_UPDATE_COMPONENT( param, component_ ) \
        if( MIDIControlHandler::getInstance()->is_waiting_for_param() || MIDIControlHandler::getInstance()->is_learning() ) \
        { \
            MIDIControlHandler::getInstance()->set_learn_param( param, component_ ); \
        }
#define IF_MIDI_LEARN__HANDLE_TWO_PARAMS__AND_UPDATE_COMPONENT( param, param_ctrl, component_ ) \
        if( MIDIControlHandler::getInstance()->is_waiting_for_param() || MIDIControlHandler::getInstance()->is_learning() ) \
        { \
            MIDIControlHandler::getInstance()->set_learn_width_ctrl_param( param, param_ctrl, component_ ); \
        }
#define SET_COMPONENT_TO_MIDI_LEARN( comp ) \
        { \
	  UiLookAndFeel::getInstance()->midi_learn_comp = comp; \
	  comp->repaint(); \
	}
#define UNSET_COMPONENT_MIDI_LEARN( comp ) \
        { \
	  UiLookAndFeel::getInstance()->midi_learn_comp = nullptr; \
	  comp->repaint(); \
	}

//==============================================================================
//==============================================================================
//==============================================================================
// FILE IO (MIDI)
static inline void write_midi_to( XmlElement& xml_, const Parameter* param_ ) noexcept
{
    const ParameterInfo& info = param_->get_info();
    if( param_->midi_control->get_listen_type() != MIDIControl::NOT_SET )
        xml_.setAttribute( info.name + "_MIDI_T", param_->midi_control->get_listen_type() );
    if( param_->midi_control->get_midi_number() != -1 )
        xml_.setAttribute( info.name + "_MIDI_NR", param_->midi_control->get_midi_number() );
    if( param_->midi_control->get_chnanel() != -1 )
        xml_.setAttribute( info.name + "_MIDI_CH", param_->midi_control->get_chnanel() );
    if( param_->midi_control->get_is_ctrl_version_of_name() != "" )
        xml_.setAttribute( info.name + "_MIDI_CTRL", param_->midi_control->get_is_ctrl_version_of_name() );
}
static inline void read_midi_from( const XmlElement& xml_, Parameter* param_ ) noexcept
{
    const ParameterInfo& info = param_->get_info();
    param_->midi_control->train
    (
        xml_.getIntAttribute( info.name + "_MIDI_T", MIDIControl::NOT_SET ),
        xml_.getIntAttribute( info.name + "_MIDI_NR", -1 ),
        xml_.getIntAttribute( info.name + "_MIDI_CH", -1 ),
        xml_.getStringAttribute( info.name + "_MIDI_CTRL", "" )
    );
}

































/*
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
template<typename T>
class mono_ParameterListener;

// TODO add the values to the base class and dont overwrite the expensive virtual functions
struct mono_ParameterCompatibilityBase {
    virtual float get_scaled_value() const noexcept = 0;
    virtual void set_scaled_value( float ) noexcept = 0;
    virtual void set_scaled_without_notification( float ) noexcept = 0;
    virtual float get_scaled_init_value() const noexcept = 0;
    virtual void toggle_value() noexcept = 0;

    virtual void notify_on_load_value_listeners() noexcept {};

    // NEEDED FOR SET BY USER
    NOINLINE virtual float slider_interval() const noexcept = 0;
    NOINLINE virtual int min_unscaled() const noexcept = 0;
    NOINLINE virtual int max_unscaled() const noexcept = 0;
    virtual float get_min() noexcept { return 0; }
    virtual float get_max() noexcept { return 0; }
    NOINLINE virtual int reset_unscaled() const noexcept = 0;
    NOINLINE virtual int scale() const noexcept = 0;

    // RETURNS HAS_NO_MODULATION (-1) if the function isn't overwritten
    NOINLINE virtual float get_modulation_amount() const noexcept;
    virtual void set_modulation_amount(float) noexcept {};
    virtual void set_modulation_amount_without_notification(float) noexcept {}

    float last_modulation_amount;
    float get_last_modulation_amount() const noexcept;
    virtual bool has_modulation() const noexcept {
        return false;
    }

    // SAVE AND RESTORE
    //NOINLINE virtual void write_to( XmlElement& xml_ ) const noexcept {};
    //NOINLINE virtual void read_from( const XmlElement& xml_ ) noexcept {};
    NOINLINE virtual void write_midi_to( XmlElement& xml_ ) const noexcept {};
    NOINLINE virtual void read_midi_from( const XmlElement& xml_ ) noexcept {};

    // PLUGINPROCESSOR AUTOMATIONS
    NOINLINE virtual const char* get_name() const noexcept = 0;
    NOINLINE virtual const char* get_short_name() const noexcept = 0;
    virtual float get_float_percent_value() const noexcept = 0;
    virtual void set_float_percent_value( float percent_ ) noexcept = 0;
    NOINLINE virtual float get_float_percent_dafault_value() const noexcept = 0;
    NOINLINE virtual int get_num_steps() const noexcept = 0;

    // MIDI CONTROLL
    ScopedPointer<MIDIControl> midi_control;
    virtual int get_type() const noexcept = 0;

protected:
    NOINLINE mono_ParameterCompatibilityBase() noexcept;
    NOINLINE ~mono_ParameterCompatibilityBase() noexcept;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( mono_ParameterCompatibilityBase )
};
*/
// ==============================================================================
/*
template<typename T>
class mono_ParameterBase : public mono_ParameterCompatibilityBase, public simple_type_info<T> {
protected:
    T value;

public:
    inline operator T() const noexcept {
        return value;
    }
    // TODO, we dont need a return here.
    inline T operator^=( bool ) noexcept {
        value = !value;
        notify_value_listeners();

        return value;
    }
    inline const T& get_reference() const noexcept {
        return value;
    }
    // TODO, we dont need a return here.
    inline void set_scaled( T value_ ) noexcept {
        value = value_;
        notify_value_listeners();
    }

    inline void set_scaled_without_notification( float value_ ) noexcept override {
        value = value_;
        notify_always_value_listeners();
    }

    int get_type() const noexcept override {
        return simple_type_info< T >::type();
    }

    // ---------------------------------------
private:
    Array< mono_ParameterListener< T >* > listeners;
    Array< mono_ParameterListener< T >* > always_listeners;
    inline void notify_value_listeners() {
        for( int i = 0 ; i != listeners.size() ; ++i ) {
            listeners.getUnchecked(i)->parameter_value_changed( this );
        }
    }
protected:
    inline void notify_modulation_value_listeners() {
        for( int i = 0 ; i != listeners.size() ; ++i ) {
            listeners.getUnchecked(i)->parameter_modulation_value_changed( this );
        }
    }
    inline void notify_on_load_value_listeners() noexcept override {
        for( int i = 0 ; i != listeners.size() ; ++i ) {
            listeners.getUnchecked(i)->parameter_value_on_load_changed( this );
        }
    }
    inline void notify_always_value_listeners() {
        for( int i = 0 ; i != always_listeners.size() ; ++i ) {
            always_listeners.getUnchecked(i)->parameter_value_changed_always_notification( this );
        }
    }
public:
    NOINLINE void register_listener( mono_ParameterListener< T >*const listener_ );
    NOINLINE void register_always_listener( mono_ParameterListener< T >*const listener_ );
    NOINLINE void remove_listener( mono_ParameterListener< T >*const listener_ );

protected:
    NOINLINE explicit mono_ParameterBase( T init_value_ );
    NOINLINE ~mono_ParameterBase();

private:
    NOINLINE mono_ParameterBase();

    friend class ChangeParamOverTime;
    ChangeParamOverTime* timeChanger;
public:
    void stopTimeChanger();
};

template<typename T>
NOINLINE mono_ParameterBase<T>::mono_ParameterBase( T init_value_ ) : value( init_value_ ), timeChanger( nullptr ) {}
template<typename T>
NOINLINE mono_ParameterBase<T>::~mono_ParameterBase() {
    stopTimeChanger();
}

template<typename T>
NOINLINE void mono_ParameterBase<T>::register_listener( mono_ParameterListener< T >*const listener_ ) {
    if( ! listeners.contains( listener_ ) ) {
        listeners.add( listener_ );
    }
}
template<typename T>
NOINLINE void mono_ParameterBase<T>::register_always_listener( mono_ParameterListener< T >*const listener_ ) {
    if( ! always_listeners.contains( listener_ ) ) {
        always_listeners.add( listener_ );
        listeners.add( listener_ );
    }
}
template<typename T>
NOINLINE void mono_ParameterBase<T>::remove_listener( mono_ParameterListener< T >*const listener_ ) {
    listeners.removeFirstMatchingValue( listener_ );
    always_listeners.removeFirstMatchingValue( listener_ );
}

template<typename T>
void mono_ParameterBase<T>::stopTimeChanger() {
    if( timeChanger )
        timeChanger->forceStopAndKill();
}
*/
// ==============================================================================
// ==============================================================================
// ==============================================================================
/*
#define MONO_PARAMETER_TEMPLATE_DECLARATION_INIT \
typename T,					\
         int init_value_ = false,		\
         int min_ = false,			\
         int max_ = true,			\
         int scale_factor = 1,			\
         int interval = 1000

#define MONO_PARAMETER_TEMPLATE_DECLARATION 	\
typename T, 					\
int init_value_, 				\
int min_, int max_, 				\
int scale_factor, 				\
int interval

#define MONO_PARAMETER_TEMPLATE_DEFINITION	\
T, 						\
init_value_, 					\
min_, 						\
max_, 						\
scale_factor, 					\
interval

// ==============================================================================
template<MONO_PARAMETER_TEMPLATE_DECLARATION_INIT>
class mono_Parameter : public mono_ParameterBase< T > {
public:
    typedef mono_ParameterBase< T > parameter_base_t;
    typedef T type;

    static constexpr int MIN_UNSCALED = min_;
    static constexpr int MAX_UNSCALED = max_;
    static constexpr int SCALE = scale_factor;
#ifdef CONSTEXPR_SUPPORT
    static constexpr T MIN_SCALED = T(MIN_UNSCALED)/SCALE;
    static constexpr T MAX_SCALED = T(MAX_UNSCALED)/SCALE;
    static constexpr T INIT_SCALED = T(init_value_)/SCALE;
    static constexpr T NUM_STEPS = T(T(MAX_UNSCALED)-T(MIN_UNSCALED))*SCALE;
#else
    static const T MIN_SCALED;
    static const T MAX_SCALED;
    static const T INIT_SCALED;
    static const T NUM_STEPS;
#endif

    using parameter_base_t::operator T;
    using parameter_base_t::operator=;
    using parameter_base_t::operator^=;
    using parameter_base_t::get_reference;

    inline T operator=( T value_ ) noexcept {
        // TODO, only check for to big values in DEBUG MODE?
        if( value_ > MAX_SCALED ) {
            DBG( "ERROR: set to big value: " << get_short_name() << "->" << value_ << " max:"<<  MAX_SCALED );
            value_ = MAX_SCALED;
        }
        else if( value_ < MIN_SCALED ) {
            DBG( "ERROR: set to small value: " << get_short_name() << "->" << value_ << " min:"<<  MIN_SCALED  );
            value_ = MIN_SCALED;
        }

        parameter_base_t::set_scaled( value_ );

        return value_;
    }
    inline const mono_Parameter& operator=( const mono_Parameter& other_ ) noexcept {
        parameter_base_t::set_scaled( other_.value );

        return *this;
    }

    parameter_base_t* get_base() noexcept {
        return static_cast< parameter_base_t* >( this );
    }

    float get_min() noexcept override {
        return MIN_SCALED;
    }
    float get_max() noexcept override {
        return MAX_SCALED;
    }

    void toggle_value() noexcept override {
        if( parameter_base_t::value == MIN_SCALED )
            parameter_base_t::set_scaled( MAX_SCALED );
        else
            parameter_base_t::set_scaled( MIN_SCALED );
    }

    // PLUGINPROCESSOR AUTOMATIONS
    float get_float_percent_value() const noexcept override;
    void set_float_percent_value( float percent_ ) noexcept override;
    NOINLINE float get_float_percent_dafault_value() const noexcept override;
    NOINLINE int get_num_steps() const noexcept override;
    NOINLINE const char* get_name() const noexcept override;
    NOINLINE const char* get_short_name() const noexcept override;

    // TODO SET FROM NORMALIZED

private:
    NOINLINE float slider_interval() const noexcept override;
    NOINLINE int min_unscaled() const noexcept override;
    NOINLINE int max_unscaled() const noexcept override;
    NOINLINE int reset_unscaled() const noexcept override;
    NOINLINE int scale() const noexcept override;

    NOINLINE float get_scaled_value() const noexcept override;
    NOINLINE float get_scaled_init_value() const noexcept override;
    NOINLINE void set_scaled_value( float v_ ) noexcept override;
public:
    const String name;
    const String short_name;

    NOINLINE mono_Parameter( const String& name_, const String& short_name_ ) noexcept;
    NOINLINE ~mono_Parameter() noexcept;

    NOINLINE void write_midi_to( XmlElement& xml_ ) const noexcept override;
    NOINLINE void read_midi_from( const XmlElement& xml_ ) noexcept override;

private:
    MONO_NOT_CTOR_COPYABLE( mono_Parameter )
    MONO_NOT_MOVE_COPY_OPERATOR( mono_Parameter )
    NOINLINE mono_Parameter();
};
#ifndef CONSTEXPR_SUPPORT
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
const T mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::MIN_SCALED = T(MIN_UNSCALED)/SCALE;

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
const T mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::MAX_SCALED = T(MAX_UNSCALED)/SCALE;

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
const T mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::INIT_SCALED = T(init_value_)/SCALE;

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
const T mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::NUM_STEPS = T(T(MAX_UNSCALED)-T(MIN_UNSCALED))*SCALE;
#endif
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE int mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::min_unscaled() const noexcept {
    return min_;
}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE int mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::max_unscaled() const noexcept {
    return max_;
}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE int mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::reset_unscaled() const noexcept {
    return init_value_;
}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE int mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::scale() const noexcept {
    return scale_factor;
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE float mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::slider_interval() const noexcept {
    return float(interval)/1000;
}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE float mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::get_scaled_init_value() const noexcept {
    return INIT_SCALED;
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE float mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::get_scaled_value() const noexcept {
    return parameter_base_t::value;
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::set_scaled_value( float v_ ) noexcept {
    operator=( v_ );
}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
float mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::get_float_percent_value() const noexcept {
    return 1.0f/(MAX_SCALED-MIN_SCALED)*(parameter_base_t::value-MIN_SCALED);
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
void mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::set_float_percent_value( float percent_ ) noexcept {
    operator=( (MAX_SCALED-MIN_SCALED)*percent_ + MIN_SCALED );
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE float mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::get_float_percent_dafault_value() const noexcept {
    return INIT_SCALED;
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE int mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::get_num_steps() const noexcept {
    return NUM_STEPS;
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE const char* mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::get_name() const noexcept {
    return name.toRawUTF8();
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE const char* mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::get_short_name() const noexcept {
    return short_name.toRawUTF8();
}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::mono_Parameter( const String& name_, const String& short_name_ ) noexcept
:
parameter_base_t( T(init_value_)/scale_factor ), name( name_ ), short_name(short_name_)
{}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::~mono_Parameter() noexcept {}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::write_midi_to( XmlElement& xml_ ) const noexcept {
    if( parameter_base_t::midi_control->get_listen_type() != MIDIControl::NOT_SET )
        xml_.setAttribute( name + "_MIDI_T", parameter_base_t::midi_control->get_listen_type() );
    if( parameter_base_t::midi_control->get_midi_number() != -1 )
        xml_.setAttribute( name + "_MIDI_NR", parameter_base_t::midi_control->get_midi_number() );
    if( parameter_base_t::midi_control->get_chnanel() != -1 )
        xml_.setAttribute( name + "_MIDI_CH", parameter_base_t::midi_control->get_chnanel() );
    if( parameter_base_t::midi_control->get_is_ctrl_version_of_name() != "" )
        xml_.setAttribute( name + "_MIDI_CTRL", parameter_base_t::midi_control->get_is_ctrl_version_of_name() );
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_Parameter<MONO_PARAMETER_TEMPLATE_DEFINITION>::read_midi_from( const XmlElement& xml_ ) noexcept {
    parameter_base_t::midi_control->train
    (
        xml_.getIntAttribute( name + "_MIDI_T", MIDIControl::NOT_SET ),
        xml_.getIntAttribute( name + "_MIDI_NR", -1 ),
        xml_.getIntAttribute( name + "_MIDI_CH", -1 ),
        xml_.getStringAttribute( name + "_MIDI_CTRL", "" )
    );
}
*/
// ==============================================================================
// ==============================================================================
// ==============================================================================

// ==============================================================================
// ==============================================================================
// ==============================================================================

// TODO remove
template<int min_, int max_>
class mono_GlideValue {
    int counter;
    float current_value;
    float value;
    float delta;

public:
    inline float glide_tick() noexcept {
        if( counter )
        {
            --counter;

            if( !counter )
                current_value = value;
            else
            {
                current_value+=delta;
                if( current_value > max_ || current_value < min_ )
                {
                    current_value = value;
                    counter = 0;
                }
            }
        }

        return current_value;
    }

    inline void update( int n_ ) noexcept {
        if( value != current_value )
        {
            counter = n_;
            delta = (value-current_value) / n_;
        }
        else
            counter = 0;
    }
    inline bool is_up_to_date() const {
        return counter == 0;
    }

    inline void override_current_value( float current_value_ ) noexcept {
        current_value = current_value_;
    }
    inline float get_last_tick() noexcept {
        return current_value;
    }

    inline const mono_GlideValue& operator=( const mono_GlideValue& other_ ) noexcept {
        value = other_.value;

        return *this;
    }
    inline float operator=( float amount_ ) noexcept {
        value = amount_;

        return amount_;
    }

    inline operator float() const noexcept {
        return value;
    }

    NOINLINE mono_GlideValue(float init_value_=0) noexcept;
    NOINLINE ~mono_GlideValue() noexcept;

private:
    MONO_NOT_CTOR_COPYABLE( mono_GlideValue )
    MONO_NOT_MOVE_COPY_OPERATOR( mono_GlideValue )
};

template<int min_, int max_>
NOINLINE mono_GlideValue<min_,max_>
::mono_GlideValue(float init_value_) noexcept
:
counter(0),
        current_value(init_value_),
        value(init_value_),
        delta(0)
{}
template<int min_, int max_>
NOINLINE mono_GlideValue<min_,max_>::~mono_GlideValue() noexcept {}

/*
// ==============================================================================
template<MONO_PARAMETER_TEMPLATE_DECLARATION_INIT>
class mono_ParameterGlideModulated : public mono_Parameter< MONO_PARAMETER_TEMPLATE_DEFINITION >
{
    typedef mono_Parameter< MONO_PARAMETER_TEMPLATE_DEFINITION > parameter_t;
    typedef typename parameter_t::parameter_base_t parameter_base_t;

    float modulation;

public:
    virtual inline float get_modulation_amount() const noexcept override {
        return modulation;
    }

private:
    virtual void set_modulation_amount( float modulation_amount_ ) noexcept override;
    virtual void set_modulation_amount_without_notification( float modulation_amount_ ) noexcept override;

    bool has_modulation() const noexcept override {
        return true;
    }

public:
    inline const mono_ParameterGlideModulated& operator=( const mono_ParameterGlideModulated& other_ ) noexcept {
        parameter_t::set_scaled( other_.parameter_t::value );
        modulation = other_.modulation;

        return *this;
    }
    inline const mono_ParameterGlideModulated& operator=( const T v_ ) noexcept {
        parameter_t::operator=( v_ );

        return *this;
    }
    using parameter_t::operator T;
    using parameter_t::operator ^=;
    using parameter_t::get_reference;
    using parameter_t::get_base;

public:
    NOINLINE mono_ParameterGlideModulated( const String& name_, const String& short_name_ );
    NOINLINE ~mono_ParameterGlideModulated();

private:
    // TODO make this ti a function which takes the param as arg?
    NOINLINE void write_to( XmlElement& xml_ ) const noexcept;
    NOINLINE void read_from( const XmlElement& xml_ ) noexcept;
    NOINLINE void write_midi_to( XmlElement& xml_ ) const noexcept override;
    NOINLINE void read_midi_from( const XmlElement& xml_ ) noexcept override;

    MONO_NOT_CTOR_COPYABLE( mono_ParameterGlideModulated )
    MONO_NOT_MOVE_COPY_OPERATOR( mono_ParameterGlideModulated )
    mono_ParameterGlideModulated();
};
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::mono_ParameterGlideModulated( const String& name_, const String& short_name_ )
    :
    parameter_t( name_, short_name_ ),
    modulation( DEFAULT_MODULATION )
{}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::~mono_ParameterGlideModulated() {}

template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::set_modulation_amount( float modulation_amount_ ) noexcept {
    modulation = modulation_amount_;
    parameter_base_t::notify_modulation_value_listeners();
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::set_modulation_amount_without_notification( float modulation_amount_ ) noexcept {
    modulation = modulation_amount_;
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::write_to( XmlElement& xml_ ) const noexcept {
    if( parameter_base_t::value != parameter_t::INIT_SCALED )
        xml_.setAttribute( parameter_t::name, parameter_base_t::value );
    if( modulation != DEFAULT_MODULATION )
        xml_.setAttribute( parameter_t::name + String("_mod"), modulation );
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::read_from( const XmlElement& xml_ ) noexcept {
    T new_value = xml_.getDoubleAttribute( parameter_t::name, T(init_value_)/scale_factor );
    if( new_value > parameter_t::MAX_SCALED ) {
        new_value = parameter_t::MAX_SCALED;
    }
    else if( new_value < parameter_t::MIN_SCALED ) {
        new_value = parameter_t::MIN_SCALED;
    }
    parameter_base_t::set_scaled_without_notification ( new_value );

    T new_modulation = xml_.getDoubleAttribute( parameter_t::name + String("_mod"), DEFAULT_MODULATION );
    if( new_modulation > 1 ) {
        new_modulation = 1;
    }
    else if( new_modulation < -1 ) {
        new_modulation = -1;
    }
    modulation = new_modulation;

    parameter_base_t::notify_on_load_value_listeners();
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::write_midi_to( XmlElement& xml_ ) const noexcept {
    if( parameter_base_t::midi_control->get_listen_type() != MIDIControl::NOT_SET )
        xml_.setAttribute( parameter_t::name + "_MIDI_T", parameter_base_t::midi_control->get_listen_type() );
    if( parameter_base_t::midi_control->get_midi_number() != -1 )
        xml_.setAttribute( parameter_t::name + "_MIDI_NR", parameter_base_t::midi_control->get_midi_number() );
    if( parameter_base_t::midi_control->get_chnanel() != -1 )
        xml_.setAttribute( parameter_t::name + "_MIDI_CH", parameter_base_t::midi_control->get_chnanel() );
    if( parameter_base_t::midi_control->get_is_ctrl_version_of_name() != "" )
        xml_.setAttribute( parameter_t::name + "_MIDI_CTRL", parameter_base_t::midi_control->get_is_ctrl_version_of_name() );
}
template<MONO_PARAMETER_TEMPLATE_DECLARATION>
NOINLINE void mono_ParameterGlideModulated<MONO_PARAMETER_TEMPLATE_DEFINITION>::read_midi_from( const XmlElement& xml_ ) noexcept {
    parameter_base_t::midi_control->train
    (
        xml_.getIntAttribute( parameter_t::name + "_MIDI_T", MIDIControl::NOT_SET ),
        xml_.getIntAttribute( parameter_t::name + "_MIDI_NR", -1 ),
        xml_.getIntAttribute( parameter_t::name + "_MIDI_CH", -1 ),
        xml_.getStringAttribute( parameter_t::name + "_MIDI_CTRL", "" )
    );
}


template< class mono_Parameter_t, int size_ >
class mono_ParameterArray {
    OwnedArray< mono_Parameter_t > array;

public:
    typedef mono_Parameter_t mono_Parameter_type;
    typedef typename mono_Parameter_t::type type;
    typedef typename mono_Parameter_t::type array_type[size_];
    enum { SIZE = size_ };

    inline mono_Parameter_t& operator[](int i) noexcept {
        return *array.getUnchecked(i);
    }
    inline const mono_Parameter_t& operator[](int i) const noexcept {
        return *array.getUnchecked(i);
    }
    inline mono_Parameter_t& get(int i) noexcept {
        return *array.getUnchecked(i);
    }
    inline const mono_Parameter_t& get(int i) const noexcept {
        return *array.getUnchecked(i);
    }

    NOINLINE mono_ParameterArray( const String& owner_class_name_,
                                  int owner_id_,
                                  const String& param_name_,
                                  const String& param_name_short_,
                                  bool create_human_id_ = true
                                ) noexcept;
    NOINLINE ~mono_ParameterArray();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (mono_ParameterArray)
    NOINLINE mono_ParameterArray();
};

template< class mono_Parameter_t, int size_ >
mono_ParameterArray< mono_Parameter_t,size_ >::mono_ParameterArray(
    const String& owner_class_name_,
    int owner_id_,
    const String& param_name_,
    const String& param_name_short_,
    bool create_human_id_ ) noexcept
{
    for( int i = 0 ; i != SIZE ; ++ i )
        array.add( new mono_Parameter_t(generate_param_name(owner_class_name_,owner_id_,param_name_,i),
        create_human_id_ ? generate_short_human_name(owner_class_name_,owner_id_,param_name_short_,i) : generate_short_human_name(owner_class_name_,param_name_short_,i) ) );

    array.minimiseStorageOverheads();
}

template< class mono_Parameter_t, int size_ >
mono_ParameterArray< mono_Parameter_t,size_ >::~mono_ParameterArray() {}

// ==============================================================================
// ==============================================================================
// ==============================================================================
template< class mono_ParameterArray_t >
class mono_ParameterReferenceArray
{
    Array< typename mono_ParameterArray_t::mono_Parameter_type* > array;

public:
    typedef typename mono_ParameterArray_t::mono_Parameter_type mono_Parameter_type;
    typedef typename mono_ParameterArray_t::type type;
    enum { SIZE = mono_ParameterArray_t::SIZE };

    inline mono_Parameter_type& operator[](int i) noexcept {
        return *array.getUnchecked(i);
    }
    inline const mono_Parameter_type& operator[](int i) const noexcept {
        return *array.getUnchecked(i);
    }

    void init( mono_ParameterArray_t& source_array_ ) noexcept {
        for( int i = 0 ; i != SIZE ; ++i ) {
            array.add( &source_array_[i] );
        }
    }

    NOINLINE mono_ParameterReferenceArray() {}
    NOINLINE ~mono_ParameterReferenceArray() {}

private:
    //JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (mono_ParameterReferenceArray)

};

// TODO thee to store the static values (init, min max... )
*/



// TODO remove the fucking templates!

// ==============================================================================
// ==============================================================================
// ==============================================================================
/*
template<typename T>
class mono_ParameterBase;
*/

/*
// TODO listener for modulators
template<typename T>
class mono_ParameterListener {
    friend class mono_ParameterBase< T >;
    virtual void parameter_value_changed( mono_ParameterBase< T >* param_ ) noexcept = 0;
    virtual void parameter_value_changed_always_notification( mono_ParameterBase< T >* param_ ) noexcept {};
    virtual void parameter_modulation_value_changed( mono_ParameterBase< T >* param_ ) noexcept {};
    virtual void parameter_value_on_load_changed( mono_ParameterBase< T >* param_ ) noexcept {};

protected:
    NOINLINE mono_ParameterListener();
    NOINLINE ~mono_ParameterListener();
};

template<typename T>
NOINLINE mono_ParameterListener<T>::mono_ParameterListener() {};
template<typename T>
NOINLINE mono_ParameterListener<T>::~mono_ParameterListener() {};
*/





#pragma GCC diagnostic pop

#endif

