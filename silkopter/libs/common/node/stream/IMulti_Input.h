#pragma once

#include "Stream_Base.h"

namespace silk
{
namespace node
{
namespace stream
{


template<class T> struct Input_Value
{
    Input_Value() = default;
    Input_Value(T value) : value(value) {}
    uint32_t version = 0;
    T value = T();
};
template<> struct Input_Value<bool>
{
    Input_Value() : version(0), value(0) {}
    Input_Value(bool value) : version(0), value(value ? 1 : 0) {}
    uint32_t version : 31;
    uint32_t value : 1;
};


class IMulti_Input : public IScalar_Stream<Type::MULTI_INPUT>
{
public:
    typedef std::false_type can_be_filtered_t;

    struct Toggles
    {
        Input_Value<bool> take_off;
        Input_Value<bool> land;
        Input_Value<bool> return_home;
    };

    enum class Mode : uint8_t
    {
        IDLE,
        ARMED,
    };

    struct Vertical_Mode
    {
        Vertical_Mode() : thrust_rate() {}

        enum Mode : uint8_t
        {
            THRUST_RATE,
            THRUST_OFFSET,
            CLIMB_RATE,
        };
        Input_Value<Mode> mode = THRUST_OFFSET;

        union
        {
            Input_Value<float> thrust_rate;
            Input_Value<float> thrust_offset;
            Input_Value<float> climb_rate;
        };
    };

    struct Horizontal_Mode
    {
        Horizontal_Mode() : angle_rate() {}

        enum Mode : uint8_t
        {
            ANGLE_RATE,
            ANGLE,
            VELOCITY,
        };
        Input_Value<Mode> mode = ANGLE_RATE;

        union
        {
            Input_Value<math::vec2f> angle_rate;   //angle rate of change - radians per second
            Input_Value<math::vec2f> angle;        //angle from horizontal. zero means horizontal
            Input_Value<math::vec2f> velocity;     //speed, meters per second
        };
    };

    struct Yaw_Mode
    {
        enum Mode : uint8_t
        {
            ANGLE_RATE,
        };
        Input_Value<Mode> mode = ANGLE_RATE;

        Input_Value<math::vec2f> angle_rate;   //angle rate of change - radians per second
    };

    //the reference frame for the user controls
    enum class Reference_Frame : uint8_t
    {
        LOCAL, 	//normal mode - back means back of uav
        USER,	//simple mode - back means towards the user, front away from her.
    };

    struct Helpers
    {
        Input_Value<bool> stay_in_range; //avoid out of range situations.
        Input_Value<bool> stay_in_battery_range; //avoid going too far considering current battery.
        Input_Value<bool> stay_in_perimeter; //stay in a configured perimeter.
        Input_Value<bool> avoid_altitude_drop; //avoid dropping too much altitude too fast.
        Input_Value<bool> avoid_the_user; //avoid getting too close to the launch position (the user).
        Input_Value<bool> avoid_proximity; //maintains a min distance from all objects around.
    };

    ///////////////////////////////
    /// Data


    struct Value
    {
        Toggles toggles;
        Vertical_Mode vertical_mode;
        Horizontal_Mode horizontal_mode;
        Yaw_Mode yaw_mode;

        Input_Value<Mode> mode = Mode::IDLE;
        Input_Value<Reference_Frame> reference_frame = Reference_Frame::LOCAL;
        Helpers assists;
    };

    typedef stream::Sample<Value>     Sample;

    virtual ~IMulti_Input() {}

    virtual auto get_samples() const -> std::vector<Sample> const& = 0;
};
DECLARE_CLASS_PTR(IMulti_Input);


}
}
}

