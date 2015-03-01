#include "stdafx.h"
#include "HAL.h"
#include "utils/Json_Helpers.h"
#include "utils/Timed_Scope.h"

#include "rapidjson/document.h"     // rapidjson's DOM-style API
#include "rapidjson/prettywriter.h" // for stringify JSON
#include "rapidjson/stringbuffer.h"


/////////////////////////////////////////////////////////////////////////////////////

namespace silk
{

///////////////////////////////////////////////////////////////

HAL::HAL()
{
    QLOG_TOPIC("hal");
}

HAL::~HAL()
{
}

auto HAL::get_bus_factory()  -> Registry<node::bus::GS_IBus>&
{
    return m_bus_factory;
}
auto HAL::get_source_factory()  -> Registry<node::source::GS_ISource>&
{
    return m_source_factory;
}
auto HAL::get_sink_factory()    -> Registry<node::sink::GS_ISink>&
{
    return m_sink_factory;
}
auto HAL::get_processor_factory()  -> Registry<node::processor::GS_IProcessor>&
{
    return m_processor_factory;
}



auto HAL::get_buses()  -> Registry<node::bus::GS_IBus>&
{
    return m_buses;
}
auto HAL::get_sources()  -> Registry<node::source::GS_ISource>&
{
    return m_sources;
}
auto HAL::get_sinks()    -> Registry<node::sink::GS_ISink>&
{
    return m_sinks;
}
auto HAL::get_processors()  -> Registry<node::processor::GS_IProcessor>&
{
    return m_processors;
}
auto HAL::get_streams()  -> Registry<node::stream::GS_IStream>&
{
    return m_streams;
}

}

