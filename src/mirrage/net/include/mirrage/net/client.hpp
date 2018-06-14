#pragma once

#include <mirrage/net/channel.hpp>
#include <mirrage/net/common.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/reflection.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <gsl/gsl>
#include <string>
#include <unordered_set>


namespace mirrage::net {

	class Client;

	class Client_builder {
	  public:
		Client_builder(std::string hostname, std::uint16_t port, const Channel_definitions&);

		auto on_connect(Connected_callback) -> Client_builder&;
		auto on_disconnect(Disconnected_callback) -> Client_builder&;

		auto connect() -> Client;

	  private:
		std::string                _hostname;
		std::uint16_t              _port;
		const Channel_definitions& _channels;
		Connected_callback         _on_connected_callback;
		Disconnected_callback      _on_disconnected_callback;
	};


	/**
	 * @brief A network peer that is connected to exacly one server
	 *
	 * Example:
	 *  auto channels = Channel_def_builder{}
	 *                          .channel("chat"_strid, Channel_type::reliable)
	 *                          .channel("world_update"_strid, Channel_type::unreliable)
	 *                          .build();
	 *
	 *  auto c = Client_builder("example.org", 4242, channels)
	 *                   .on_connect([](auto peer) { LOG(plog::debug) << "connected to server"; })
	 *                   .on_disconnect(
	 *                           [](auto peer, auto arg) { LOG(plog::debug) << "disconnected from server"; })
	 *                   .connect();
	 */
	class Client final : public detail::Connection {
	  public:
		auto channel(util::Str_id channel) -> Channel;

		void disconnect(std::uint32_t reason);

	  private:
		friend auto Client_builder::connect() -> Client;

		std::unique_ptr<ENetPeer, void (*)(ENetPeer*)> _peer;

		Client(const std::string&         hostname,
		       std::uint16_t              port,
		       const Channel_definitions& channels,
		       Connected_callback         on_connected,
		       Disconnected_callback      on_disconnected);
	};

} // namespace mirrage::net
