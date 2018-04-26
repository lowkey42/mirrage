#include <mirrage/net/client.hpp>

#include <mirrage/net/error.hpp>

#include <enet/enet.h>


namespace mirrage::net {

	Client_builder::Client_builder(std::string                hostname,
	                               std::uint16_t              port,
	                               const Channel_definitions& channels)
	  : _hostname(std::move(hostname)), _port(port), _channels(channels) {}

	auto Client_builder::on_connect(Connected_callback cb) -> Client_builder& {
		_on_connected_callback = std::move(cb);
		return *this;
	}
	auto Client_builder::on_disconnect(Disconnected_callback cb) -> Client_builder& {
		_on_disconnected_callback = std::move(cb);
		return *this;
	}

	auto Client_builder::connect() -> Client {
		return {_hostname, _port, _channels, _on_connected_callback, _on_disconnected_callback};
	}

	namespace {
		auto open_client_host(std::size_t channel_count) {
			auto host = enet_host_create(nullptr, 1, channel_count, 0, 0);

			if(!host) {
				constexpr auto msg = "Couldn't create the host data structure (out of memory?)";
				LOG(plog::debug) << msg;
				throw std::system_error(Net_error::unspecified_network_error, msg);
			}

			return std::unique_ptr<ENetHost, void (*)(ENetHost*)>(host, &enet_host_destroy);
		}

		auto open_client_connection(ENetHost&          client_host,
		                            const std::string& hostname,
		                            std::uint16_t      port,
		                            std::size_t        channel_count) {
			ENetAddress address;
			address.port = port;
			auto ec      = enet_address_set_host(&address, hostname.c_str());
			if(ec != 0) {
				const auto msg = "Couldn't resolve host \"" + hostname + "\".";
				LOG(plog::warning) << msg;
				throw std::system_error(Net_error::unknown_host, msg);
			}


			auto peer = enet_host_connect(&client_host, &address, channel_count, 0);
			if(!peer) {
				const auto msg =
				        "Couldn't connect to host \"" + hostname + "\" at port " + std::to_string(port) + ".";
				LOG(plog::warning) << msg;
				throw std::system_error(Net_error::connection_error, msg);
			}

			return std::unique_ptr<ENetPeer, void (*)(ENetPeer*)>(peer,
			                                                      [](auto p) { enet_peer_disconnect(p, 0); });
		}
	} // namespace

	Client::Client(const std::string&         hostname,
	               std::uint16_t              port,
	               const Channel_definitions& channels,
	               Connected_callback         on_connected,
	               Disconnected_callback      on_disconnected)
	  : Connection(open_client_host(channels.size()),
	               channels,
	               std::move(on_connected),
	               std::move(on_disconnected))
	  , _peer(open_client_connection(*_host, hostname, port, _channels.size())) {}

	auto Client::channel(util::Str_id channel) -> Channel {
		auto&& c = _channels.by_name(channel);
		if(c.is_nothing()) {
			const auto msg = "Unknown channel \"" + channel.str() + "\".";
			LOG(plog::warning) << msg;
			throw std::system_error(Net_error::unknown_channel, msg);
		}

		return Channel(_peer.get(), c.get_or_throw());
	}

	void Client::disconnect(std::uint32_t reason) { enet_peer_disconnect(_peer.get(), reason); }

} // namespace mirrage::net
