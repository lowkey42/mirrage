#pragma once

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/str_id.hpp>

#include <gsl/gsl>
#include <unordered_map>
#include <vector>


extern "C" {
typedef struct _ENetPeer   ENetPeer;
typedef struct _ENetHost   ENetHost;
typedef struct _ENetPacket ENetPacket;
}

namespace mirrage::net {

	enum class Channel_type : std::uint8_t { reliable, unreliable };

	struct Channel_definition {
		std::uint8_t id;
		Channel_type type;
		util::Str_id name;
	};

	/**
	 * @brief Defines a set of network channels used for communication between client and server.
	 * The Channel_definitions passed to a client and server that should communicate have to be identical!
	 *
	 * Example:
	 *  auto channels = Channel_def_builder{}
	 *                          .channel("chat"_strid, Channel_type::reliable)
	 *                          .channel("world_update"_strid, Channel_type::unreliable)
	 *                          .build();
	*/
	class Channel_definitions {
	  public:
		auto by_id(std::uint8_t i) -> util::maybe<Channel_definition>
		{
			return i < _by_id.size() ? util::just(_by_id[i]) : util::nothing;
		}
		auto by_name(util::Str_id name) -> util::maybe<Channel_definition>
		{
			auto iter = _by_name.find(name);
			return iter != _by_name.end() ? util::just(iter->second) : util::nothing;
		}

		auto size() const noexcept { return _by_id.size(); }

	  private:
		friend class Channel_def_builder;

		std::unordered_map<util::Str_id, Channel_definition> _by_name;
		std::vector<Channel_definition>                      _by_id;

		void _add(util::Str_id id, Channel_type type);
	};

	class Channel_def_builder {
	  public:
		auto channel(util::Str_id id, Channel_type type) -> Channel_def_builder&;

		auto build() { return _channels; }

	  private:
		Channel_definitions _channels;
	};


	/**
	 * @brief Allows sending packages to a single channel and peer (or all of them)
	 */
	class Channel {
	  public:
		Channel(ENetPeer* peer, Channel_definition definition);
		Channel(ENetHost* host, Channel_definition definition);

		void send(gsl::span<gsl::byte> data);

		template <typename F>
		void send(std::size_t size, F&& f)
		{
			auto p = _create_empty_packet(size);

			f(_packet_data(*p));

			_send_packet(std::move(p));
		}

	  private:
		using Packet = std::unique_ptr<ENetPacket, void (*)(ENetPacket*)>;

		ENetPeer*          _peer;
		ENetHost*          _host;
		Channel_definition _definition;

		auto _create_empty_packet(std::size_t) -> Packet;
		auto _packet_data(ENetPacket&) -> gsl::span<gsl::byte>;
		void _send_packet(Packet);
	};

} // namespace mirrage::net
