#include <mirrage/net/common.hpp>

#include <enet/enet.h>


namespace mirrage::net::detail {

	Connection::Connection(std::unique_ptr<ENetHost, void (*)(ENetHost*)> host,
	                       Channel_definitions                            channels,
	                       Connected_callback                             on_connected,
	                       Disconnected_callback                          on_disconencted)
	  : _host(std::move(host))
	  , _channels(std::move(channels))
	  , _on_connected_callback(std::move(on_connected))
	  , _on_disconnected_callback(std::move(on_disconencted))
	{
	}

	auto Connection::_poll_packet() -> util::maybe<Received_packet>
	{
		auto event = ENetEvent{};
		auto ret   = 0;

		while((ret = enet_host_service(_host.get(), &event, 0)) > 0) {
			switch(event.type) {
				case ENET_EVENT_TYPE_CONNECT:
					_connections++;
					_on_connected(event.peer);
					if(_on_connected_callback)
						_on_connected_callback(event.peer);
					break;

				case ENET_EVENT_TYPE_DISCONNECT:
					_connections--;
					_on_disconnected(event.peer, event.data);
					if(_on_disconnected_callback)
						_on_disconnected_callback(event.peer, event.data);
					break;

				case ENET_EVENT_TYPE_RECEIVE: {
					auto c = _channels.by_id(event.channelID);
					if(c.is_nothing())
						break;

					return std::make_tuple(
					        c.get_or_throw().name, event.peer, Packet(event.packet, &enet_packet_destroy));
				}
				case ENET_EVENT_TYPE_NONE: return util::nothing;
			}
		}

		if(ret < 0) {
			LOG(plog::error) << "An unknown error occured in ENet while receiving incoming packets";
		}

		return util::nothing;
	}
	auto Connection::_packet_data(const ENetPacket& packet) -> gsl::span<const gsl::byte>
	{
		return {reinterpret_cast<const gsl::byte*>(packet.data),
		        static_cast<std::ptrdiff_t>(packet.dataLength)};
	}

} // namespace mirrage::net::detail
