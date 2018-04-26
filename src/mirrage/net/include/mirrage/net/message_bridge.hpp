#pragma once

#include <mirrage/net/channel.hpp>
#include <mirrage/net/client.hpp>
#include <mirrage/net/server.hpp>

#include <mirrage/utils/messagebus.hpp>

#include <boost/pfr/precise.hpp>

#include <atomic>
#include <cstdint>

namespace mirrage::net {

	// void = placeholder
	template <typename... Ts>
	struct Message_types {
	};

	/**
	 * @brief Transfers selected messages between server and clients (bidirectional).
	 * The types to be send have to registered with: register_msg_types(...)
	 * For each received packet on_packet(...) should be called with the set of messages to receive.
	 *
	 * Example:
	 *  	struct Foo {
	 *  		int x;
	 *  	};
	 *  	struct Bar {
	 *  		int y;
	 *  	};
	 *
	 *  	constexpr auto my_messages = Message_types<Foo, void, Bar>{};
	 *
	 *
	 *  	void example(util::Message_bus& bus, Server& server) {
	 *  		auto bridge = Message_bridge(bus, server, "msg_bus"_strid);
	 *
	 *  		bridge.register_msg_types(my_messages);
	 *
	 *  		// in update
	 *  		bridge.pump();
	 *
	 *  		// in packet-handler of Connection::poll
	 *  		server.poll([&](auto&&... packet) {
	 *  			if(!bridge.on_packet(my_messages, packet...)) {
	 *  				// my handlers
	 *  			}
	 *  		});
	 *  	}
	*/
	class Message_bridge {
	  public:
		Message_bridge(util::Message_bus& bus, Server&, util::Str_id channel);
		Message_bridge(util::Message_bus& bus, Client&, util::Str_id channel);

		// add message types to be received
		template <typename... Ts>
		void register_msg_types(const Message_types<Ts...>&)
		{
			auto idx = std::uint16_t(0);
			(void(_register_msg_type<Ts>(idx++)), ...);
		}

		void pump() { _mailbox.update_subscriptions(); }
		void enable() { _mailbox.enable(); }
		void disable() { _mailbox.disable(); }

		// should be called for each incoming packet
		// return: true if the packet has been processed
		template <typename... Ts>
		bool on_packet(const Message_types<Ts...>&,
		               util::Str_id channel,
		               Client_handle,
		               gsl::span<const gsl::byte>);

	  private:
		util::Mailbox_collection _mailbox;
		Channel                  _channel;
		util::Str_id             _channel_name;

		template <typename T, std::size_t bulk_size = 4>
		void _register_msg_type(std::uint16_t id, std::size_t queue_size = 16);

		template <typename T>
		bool _process_packet(std::uint16_t              t_id,
		                     std::uint16_t              msg_id,
		                     std::uint16_t              msg_hash,
		                     gsl::span<const gsl::byte> data);
	};


	// IMPLEMENTATION
	namespace detail {
		using type_hash_t = std::uint16_t;

		template <typename T, std::size_t... I, typename F>
		constexpr void for_each_struct_type(std::index_sequence<I...>, F f)
		{
			(void(f(static_cast<boost::pfr::tuple_element_t<I, T>*>(nullptr))), ...);
		}

		template <typename T>
		constexpr type_hash_t calc_type_hash()
		{
			auto hash = type_hash_t(0);

			for_each_struct_type<T>(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{}, [&](auto t) {
				hash = hash * 31 + calc_type_hash<std::remove_pointer_t<decltype(t)>>();
			});

			return hash;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<bool>()
		{
			return 1;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<float>()
		{
			return 2;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<double>()
		{
			return 3;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::int8_t>()
		{
			return 4;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::uint8_t>()
		{
			return 5;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::int16_t>()
		{
			return 6;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::uint16_t>()
		{
			return 7;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::int32_t>()
		{
			return 8;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::uint32_t>()
		{
			return 9;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::int64_t>()
		{
			return 10;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::uint64_t>()
		{
			return 11;
		}
		template <>
		constexpr inline type_hash_t calc_type_hash<std::string>()
		{
			return 12;
		}

		template <typename T>
		constexpr type_hash_t type_hash = calc_type_hash<T>();


		template <typename T>
		auto calculate_size(const T& val)
		{
			if constexpr(std::is_integral_v<T>) {
				return sizeof(T);
			} else {
				auto size = std::size_t(0);
				boost::pfr::for_each_field(val, [&](auto& field) { size += calculate_size(field); });

				return size;
			}
		}
		template <>
		inline auto calculate_size(const std::string& val)
		{
			return sizeof(std::uint32_t) + val.size();
		}

		template <typename T>
		auto read_obj(T& val, gsl::span<const gsl::byte> out) -> gsl::span<const gsl::byte>
		{
			if constexpr(std::is_integral_v<T>) {
				val = reinterpret_cast<const T&>(out[0]);
				return out.subspan<sizeof(T)>();
			} else {
				boost::pfr::for_each_field(val, [&](auto& field) { out = read_obj(field, out); });
				return out;
			}
		}
		template <>
		inline auto read_obj(std::string& val, gsl::span<const gsl::byte> out) -> gsl::span<const gsl::byte>
		{
			auto size = reinterpret_cast<const std::uint32_t&>(out[0]);
			val.resize(size);
			std::memcpy(val.data(), &out[sizeof(std::uint32_t)], size);
			return out.subspan(sizeof(std::uint32_t) + size);
		}

		template <typename T>
		auto write_obj(const T& val, gsl::span<gsl::byte> out) -> gsl::span<gsl::byte>
		{
			if constexpr(std::is_integral_v<T>) {
				reinterpret_cast<T&>(out[0]) = val;
				return out.subspan<sizeof(T)>();
			} else {
				boost::pfr::for_each_field(val, [&](auto& field) { out = write_obj(field, out); });
				return out;
			}
		}
		template <>
		inline auto write_obj(const std::string& val, gsl::span<gsl::byte> out) -> gsl::span<gsl::byte>
		{
			reinterpret_cast<std::uint32_t&>(out[0]) = gsl::narrow<std::uint32_t>(val.size());
			std::memcpy(&out[sizeof(std::uint32_t)], val.data(), val.size());
			return out.subspan(
			        gsl::narrow<gsl::span<gsl::byte>::index_type>(sizeof(std::uint32_t) + val.size()));
		}

	} // namespace detail

	template <class T, std::size_t bulk_size>
	void Message_bridge::_register_msg_type(std::uint16_t id, std::size_t queue_size)
	{
		_mailbox.subscribe<T, bulk_size>(queue_size, [&, id](const T& event) {
			auto size = detail::calculate_size(event) + sizeof(id) + sizeof(detail::type_hash_t);

			_channel.send(size, [&](auto data) {
				reinterpret_cast<std::uint16_t&>(data[0])                           = id;
				reinterpret_cast<detail::type_hash_t&>(data[sizeof(std::uint16_t)]) = detail::type_hash<T>;

				detail::write_obj(event, data.subspan(sizeof(id) + sizeof(detail::type_hash_t)));
			});
		});
	}

	template <>
	inline void Message_bridge::_register_msg_type<void>(std::uint16_t, std::size_t)
	{
	}

	template <typename T>
	bool Message_bridge::_process_packet(std::uint16_t              t_id,
	                                     std::uint16_t              msg_id,
	                                     std::uint16_t              msg_hash,
	                                     gsl::span<const gsl::byte> data)
	{
		if(t_id != msg_id) {
			return false;
		}

		constexpr auto t_hash = detail::type_hash<T>;
		if(msg_hash != t_hash) {
			LOG(plog::warning) << "Type-hash of message " << util::type_name<T>() << " (" << t_id
			                   << ") doesn't match " << t_hash << "!=" << msg_hash
			                   << " (different app version?).";
			return false;
		}

		auto event = T{};
		detail::read_obj(event, data);

		_mailbox.send_msg(event);

		return true;
	}
	template <>
	inline bool Message_bridge::_process_packet<void>(std::uint16_t,
	                                                  std::uint16_t,
	                                                  std::uint16_t,
	                                                  gsl::span<const gsl::byte>)
	{
		return false;
	}

	template <typename... Ts>
	bool Message_bridge::on_packet(const Message_types<Ts...>&,
	                               util::Str_id channel,
	                               Client_handle,
	                               gsl::span<const gsl::byte> data)
	{
		constexpr auto header_size = sizeof(std::uint16_t) + sizeof(detail::type_hash_t);

		if(channel != _channel_name || gsl::narrow<std::size_t>(data.size_bytes()) < header_size) {
			LOG(plog::info) << "Packet dropped because channel-name doesn't match or packet is too small.";
			return false;
		}

		auto msg_type_id   = *reinterpret_cast<const std::uint16_t*>(&data[0]);
		auto msg_type_hash = *reinterpret_cast<const detail::type_hash_t*>(&data[sizeof(std::uint16_t)]);
		auto rest          = data.subspan<header_size>();

		if(msg_type_id >= sizeof...(Ts)) {
			return false;
		}

		auto idx = std::uint16_t(0);
		return (_process_packet<Ts>(idx++, msg_type_id, msg_type_hash, rest) || ...);
	}

} // namespace mirrage::net
