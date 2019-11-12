#include <mirrage/net/server.hpp>

#include <mirrage/net/error.hpp>

#include <mirrage/utils/container_utils.hpp>

#include <enet/enet.h>


namespace mirrage::net {

	Server_builder::Server_builder(Host_type                  type,
	                               std::string                hostname,
	                               std::uint16_t              port,
	                               const Channel_definitions& channels)
	  : _type(type), _hostname(std::move(hostname)), _port(port), _channels(channels)
	{
	}

	auto Server_builder::create() -> Server
	{
		return {_type,
		        _hostname,
		        _port,
		        _channels,
		        _max_clients,
		        _max_in_bandwidth,
		        _max_out_bandwidth,
		        _on_connect,
		        _on_disconnect};
	}


	namespace {

		auto open_server_host(Server_builder::Host_type type,
		                      const std::string&        hostname,
		                      std::uint16_t             port,
		                      std::size_t               channel_count,
		                      int                       max_clients,
		                      int                       max_in_bandwidth,
		                      int                       max_out_bandwidth)
		{

			ENetAddress address;
			address.port = port;
			switch(type) {
				case Server_builder::Host_type::any: address.host = ENET_HOST_ANY; break;
				case Server_builder::Host_type::broadcast:
#ifdef WIN32
					address.host = ENET_HOST_ANY;
#else
					address.host = ENET_HOST_BROADCAST;
#endif
					break;

				case Server_builder::Host_type::named:
					auto ec = enet_address_set_host(&address, hostname.c_str());
					if(ec != 0) {
						const auto msg = "Couldn't resolve host \"" + hostname + "\".";
						LOG(plog::warning) << msg;
						throw std::system_error(Net_error::unknown_host, msg);
					}
					break;
			}

			auto host = enet_host_create(&address,
			                             gsl::narrow<size_t>(max_clients),
			                             channel_count,
			                             gsl::narrow<enet_uint32>(max_in_bandwidth),
			                             gsl::narrow<enet_uint32>(max_out_bandwidth));

			if(!host) {
				constexpr auto msg = "Couldn't create the host data structure (out of memory?)";
				LOG(plog::warning) << msg;
				throw std::system_error(Net_error::unspecified_network_error, msg);
			}

			return std::unique_ptr<ENetHost, void (*)(ENetHost*)>(host, &enet_host_destroy);
		}

	} // namespace

	Server::Server(Server_builder::Host_type  type,
	               const std::string&         hostname,
	               std::uint16_t              port,
	               const Channel_definitions& channels,
	               int                        max_clients,
	               int                        max_in_bandwidth,
	               int                        max_out_bandwidth,
	               Connected_callback         on_connected,
	               Disconnected_callback      on_disconnected)
	  : Connection(
	          open_server_host(
	                  type, hostname, port, channels.size(), max_clients, max_in_bandwidth, max_out_bandwidth),
	          channels,
	          std::move(on_connected),
	          std::move(on_disconnected))
	{
	}

	auto Server::broadcast_channel(util::Str_id channel) -> Channel
	{
		auto&& c = _channels.by_name(channel);
		if(c.is_nothing()) {
			const auto msg = "Unknown channel \"" + channel.str() + "\".";
			LOG(plog::warning) << msg;
			throw std::system_error(Net_error::unknown_channel, msg);
		}

		return Channel(_host.get(), c.get_or_throw());
	}
	auto Server::client_channel(util::Str_id channel, Client_handle client) -> Channel
	{
		auto&& c = _channels.by_name(channel);
		if(c.is_nothing()) {
			const auto msg = "Unknown channel \"" + channel.str() + "\".";
			LOG(plog::warning) << msg;
			throw std::system_error(Net_error::unknown_channel, msg);
		}

		return Channel(client, c.get_or_throw());
	}

	void Server::_on_connected(Client_handle client) { _clients.emplace_back(client); }
	void Server::_on_disconnected(Client_handle client, std::uint32_t) { util::erase_fast(_clients, client); }

} // namespace mirrage::net
