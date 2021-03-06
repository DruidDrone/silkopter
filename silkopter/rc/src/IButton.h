#pragma once

#include "IInput_Device.h"

namespace silk
{

class IButton : public IInput_Device
{
public:
    virtual ~IButton() = default;

    virtual bool is_pressed() const = 0;

    virtual bool was_pressed() const = 0;
    virtual bool was_released() const = 0;

    virtual uint32_t get_press_count() const = 0;
    virtual uint32_t get_release_count() const = 0;
};

}
