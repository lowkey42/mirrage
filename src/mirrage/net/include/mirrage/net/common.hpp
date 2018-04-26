#pragma once

#include <mirrage/net/channel.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/reflection.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <gsl/gsl>
#include <string_view>
#include <unordered_set>

extern "C" {
typedef struct _ENetPeer   ENetPeer;
typedef struct _ENetHost   ENetHost;
typedef struct _ENetEvent  ENetEvent;
typedef struct _ENetPacket ENetPacket;
}

namespace mirrage::net {

	using Client_handle = ENetPeer*;

	using Connected_callback    = std::function<void(Client_handle)>;
	using Disconnected_callback = std::function<void(Client_handle, std::uint32_t)>;


	namespace detail {

		class Connection {
		  public:
			template <typename F> // F = void(util::Str_id channel, Client_handle, gsl::span<const gsl::byte>)
			void poll(F&& on_packet)
			{
				auto packet = util::maybe<Received_packet>::nothing();
				while((packet = _poll_packet()).is_some()) {
					on_packet(std::get<0>(packet.get_or_throw()),
					          std::get<1>(packet.get_or_throw()),
					          _packet_data(*std::get<2>(packet.get_or_throw())));
				}
			}

			auto connected() const noexcept { return _connections != 0; }

		  protected:
			using Packet          = std::unique_ptr<ENetPacket, void (*)(ENetPacket*)>;
			using Received_packet = std::tuple<util::Str_id, Client_handle, Packet>;

			std::unique_ptr<ENetHost, void (*)(ENetHost*)> _host;
			Channel_definitions                            _channels;

			Connection(std::unique_ptr<ENetHost, void (*)(ENetHost*)> host,
			           Channel_definitions                            channels,
			           Connected_callback                             on_connected,
			           Disconnected_callback                          on_disconencted);
			Connection(Connection&&) = default;
			Connection& operator=(Connection&&) = default;
			~Connection()                       = default;

			virtual void _on_connected(Client_handle) {}
			virtual void _on_disconnected(Client_handle, std::uint32_t) {}

		  private:
			Connected_callback    _on_connected_callback;
			Disconnected_callback _on_disconnected_callback;
			int                   _connections;

			auto        _poll_packet() -> util::maybe<Received_packet>;
			static auto _packet_data(const ENetPacket&) -> gsl::span<const gsl::byte>;
		};

	} // namespace detail

} // namespace mirrage::net
