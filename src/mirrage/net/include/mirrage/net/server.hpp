#pragma once

#include <mirrage/net/channel.hpp>
#include <mirrage/net/common.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/reflection.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <gsl/gsl>
#include <string_view>
#include <unordered_set>


namespace mirrage::net {

	class Server;

	class Server_builder {
	  public:
		enum class Host_type { named, any, broadcast };

		auto max_clients(int limit) -> auto& {
			_max_clients = limit;
			return *this;
		}
		auto max_bandwidth(int in, int out) -> auto& {
			_max_in_bandwidth  = in;
			_max_out_bandwidth = out;
			return *this;
		}

		auto on_connect(Connected_callback handler) -> auto& {
			_on_connect = std::move(handler);
			return *this;
		}
		auto on_disconnect(Disconnected_callback&& handler) -> auto& {
			_on_disconnect = std::move(handler);
			return *this;
		}

		auto create() -> Server;

	  private:
		friend class Server;

		Server_builder(Host_type type, std::string hostname, std::uint16_t port, const Channel_definitions&);

		Host_type             _type;
		std::string           _hostname;
		std::uint16_t         _port;
		Channel_definitions   _channels;
		int                   _max_clients       = 128;
		int                   _max_in_bandwidth  = 0;
		int                   _max_out_bandwidth = 0;
		Connected_callback    _on_connect;
		Disconnected_callback _on_disconnect;
	};


	/**
	 * @brief A network peer that other peers (clients) can connect to.
	 *
	 * Example:
	 *  auto channels = Channel_def_builder{}
	 *                          .channel("chat"_strid, Channel_type::reliable)
	 *                          .channel("world_update"_strid, Channel_type::unreliable)
	 *                          .build();
	 *
	 *  auto s = Server::on_any_interface(4242, channels)
	 *                   .max_clients(5)
	 *                   .on_connect([](auto peer) { LOG(plog::debug) << "new client connected"; })
	 *                   .on_disconnect([](auto peer, auto arg) { LOG(plog::debug) << "client disconnected"; })
	 *                   .create();
	 */
	class Server final : public detail::Connection {
	  public:
		static Server_builder on_named_interface(std::string                hostname,
		                                         std::uint16_t              port,
		                                         const Channel_definitions& channels) {
			return {Server_builder::Host_type::named, std::move(hostname), port, channels};
		}
		static Server_builder on_any_interface(std::uint16_t port, const Channel_definitions& channels) {
			return {Server_builder::Host_type::any, {}, port, channels};
		}
		static Server_builder on_broadcast_interface(std::uint16_t              port,
		                                             const Channel_definitions& channels) {
			return {Server_builder::Host_type::broadcast, {}, port, channels};
		}


		auto broadcast_channel(util::Str_id channel) -> Channel;
		auto client_channel(util::Str_id channel, Client_handle client) -> Channel;

		auto clients() const -> auto& { return _clients; }

	  protected:
		void _on_connected(Client_handle) override;
		void _on_disconnected(Client_handle, std::uint32_t) override;

	  private:
		friend class Server_builder;

		std::vector<Client_handle> _clients;

		Server(Server_builder::Host_type  type,
		       const std::string&         hostname,
		       std::uint16_t              port,
		       const Channel_definitions& channels,
		       int                        max_clients,
		       int                        max_in_bandwidth,
		       int                        max_out_bandwidth,
		       Connected_callback         on_connected,
		       Disconnected_callback      on_disconnected);
	};

} // namespace mirrage::net
