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

auto HAL::get_remote_clock() const -> Manual_Clock const&
{
    return m_remote_clock;
}

auto HAL::get_node_defs() const  -> Registry<node::Node_Def> const&
{
    return m_node_defs;
}
auto HAL::get_nodes() const  -> Registry<node::Node> const&
{
    return m_nodes;
}
auto HAL::get_streams() const  -> Registry<node::stream::Stream> const&
{
    return m_streams;
}

void HAL::add_node(std::string const& def_name,
                     std::string const& name,
                     rapidjson::Document&& init_params,
                     Add_Node_Callback callback)
{
    QASSERT(callback);
    if (!callback)
    {
        return;
    }
    Add_Queue_Item item;
    item.def_name = def_name;
    item.name = name;
    item.init_params = std::move(init_params);
    item.callback = callback;
    m_add_queue.push_back(std::move(item));
}

void HAL::remove_node(node::Node_ptr node,
                     Remove_Node_Callback callback)
{
    QASSERT(callback);
    if (!callback)
    {
        return;
    }
    Remove_Queue_Item item;
    item.node = std::move(node);
    item.callback = callback;
    m_remove_queue.push_back(std::move(item));
}

void HAL::connect_node_input(node::Node_ptr node, std::string const& input_name, std::string const& stream_name)
{
    auto document = jsonutil::clone_value(node->config);

    q::Path path("inputs");
    path += input_name;

    auto* value = jsonutil::find_value(document, path);
    if (!value)
    {
        QASSERT(0);
        return;
    }

    value->SetString(stream_name.c_str(), document.GetAllocator());

    Set_Config_Queue_Item item;
    item.message = silk::comms::Setup_Message::NODE_CONFIG;
    item.name = node->name;
    item.config = std::move(document);
    m_set_config_queue.push_back(std::move(item));
}

void HAL::set_node_config(node::Node_ptr node, rapidjson::Document const& config)
{
    Set_Config_Queue_Item item;
    item.message = silk::comms::Setup_Message::NODE_CONFIG;
    item.name = node->name;
    item.config = jsonutil::clone_value(config);
    m_set_config_queue.push_back(std::move(item));
}

void HAL::set_stream_telemetry_active(std::string const& stream_name, bool active, Stream_Telemetry_Callback callback)
{
    QASSERT(callback);
    if (!callback)
    {
        return;
    }
    auto stream = m_streams.find_by_name(stream_name);
    if (!stream)
    {
        QLOGE("Cannot find stream '{}'", stream_name);
        callback(Result::FAILED);
        return;
    }
    if (stream->telemetry_active_req == 0 && !active)
    {
        QASSERT(0);
        QLOGE("Trying to disable stream '{}' but it's already disabled", stream_name);
        callback(Result::FAILED);
        return;
    }

    stream->telemetry_active_req += active ? 1 : -1;

    //if the status will not change, it's ok. No need to do the request
    bool new_active = stream->telemetry_active_req > 0;
    if (new_active == stream->is_telemetry_active)
    {
        callback(Result::OK);
        return;
    }

    Stream_Telemetry_Queue_Item item;
    item.stream_name = stream_name;
    item.callback = callback;
    item.is_active = new_active;
    m_stream_telemetry_queue.push_back(std::move(item));
}




}

