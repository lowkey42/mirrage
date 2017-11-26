#include <mirrage/net/message_bridge.hpp>


namespace mirrage::net {

	Message_bridge::Message_bridge(util::Message_bus& bus, Server& server, util::Str_id channel)
	  : _mailbox(bus), _channel(server.broadcast_channel(channel)), _channel_name(channel) {}

	Message_bridge::Message_bridge(util::Message_bus& bus, Client& client, util::Str_id channel)
	  : _mailbox(bus), _channel(client.channel(channel)), _channel_name(channel) {}

} // namespace mirrage::net
