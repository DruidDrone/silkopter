#pragma once

namespace silk
{

class HAL_Camera_Mount : q::util::Noncopyable
{
public:
    virtual ~HAL_Camera_Mount() {}

    //----------------------------------------------------------------------
    //mount

    virtual void set_rotation(math::quatf const& rot) = 0;

    //----------------------------------------------------------------------
    
    virtual void process() = 0;
};

}
