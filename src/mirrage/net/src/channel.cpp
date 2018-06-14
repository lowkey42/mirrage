#include <mirrage/net/channel.hpp>

#include <mirrage/net/error.hpp>

#include <enet/enet.h>


namespace mirrage::net {

	void Channel_definitions::_add(util::Str_id id, Channel_type type)
	{
		auto def = Channel_definition{gsl::narrow<std::uint8_t>(_by_id.size()), type, id};

		_by_name[id] = def;
		_by_id.emplace_back(def);
	}

	auto Channel_def_builder::channel(util::Str_id id, Channel_type type) -> Channel_def_builder&
	{
		_channels._add(id, type);
		return *this;
	}


	Channel::Channel(ENetPeer* peer, Channel_definition definition)
	  : _peer(peer), _host(nullptr), _definition(definition)
	{
	}
	Channel::Channel(ENetHost* host, Channel_definition definition)
	  : _peer(nullptr), _host(host), _definition(definition)
	{
	}


	auto Channel::_create_empty_packet(std::size_t size) -> Packet
	{
		auto flags = _definition.type == Channel_type::reliable ? ENET_PACKET_FLAG_RELIABLE
		                                                        : ENET_PACKET_FLAG_UNSEQUENCED;

		auto packet = std::unique_ptr<ENetPacket, void (*)(ENetPacket*)>(
		        enet_packet_create(nullptr, size, static_cast<std::uint32_t>(flags)), &enet_packet_destroy);

		if(!packet) {
			constexpr auto msg = "Couldn't create the packet data structure (out of memory?)";
			LOG(plog::warning) << msg;
			throw std::system_error(Net_error::unspecified_network_error, msg);
		}

		return packet;
	}
	auto Channel::_packet_data(ENetPacket& packet) -> gsl::span<gsl::byte>
	{
		return {reinterpret_cast<gsl::byte*>(packet.data),
		        gsl::narrow<gsl::span<gsl::byte>::index_type>(packet.dataLength)};
	}
	void Channel::_send_packet(Packet packet)
	{
		if(!_peer) {
			enet_host_broadcast(_host, _definition.id, packet.release());
		} else {
			auto rc = enet_peer_send(_peer, _definition.id, packet.get());

			if(rc != 0) {
				constexpr auto msg =
				        "Couldn't send message, because the connection has not been "
				        "established yet or packet is much to large.";
				LOG(plog::warning) << msg;
				throw std::system_error(Net_error::not_connected, msg);
			} else {
				packet.release();
			}
		}
	}

	void Channel::send(gsl::span<gsl::byte> data)
	{
		auto size   = gsl::narrow<std::size_t>(data.size_bytes());
		auto packet = _create_empty_packet(size);

		std::memcpy(packet->data, data.data(), size);

		_send_packet(std::move(packet));
	}


} // namespace mirrage::net
