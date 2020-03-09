/** A generic bus that redirects messages to all subscribers *****************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/
#pragma once

#include <mirrage/utils/container_utils.hpp>
#include <mirrage/utils/func_traits.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/ranges.hpp>
#include <mirrage/utils/reflection.hpp>
#include <mirrage/utils/small_vector.hpp>
#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/uuid.hpp>

#include <concurrentqueue.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>


namespace mirrage::util {

	class Message_bus;

	using Request_id = uuid;

	extern auto create_request_id() -> Request_id;

	namespace detail {
		constexpr auto default_mailbox_size = 16;

		class Mailbox_base;

		template <typename T>
		class Mailbox;

		template <typename T>
		using has_response_type = typename T::response_type;

		template <typename T, typename = void>
		struct is_request_message : std::false_type {
		};
		// clang-format off
		template <typename T>
		struct is_request_message<
		        T,
		        std::enable_if_t<
		                util::is_detected_v<has_response_type, T> &&
		                std::is_same_v<Request_id, std::decay_t<decltype(std::declval<T>().request_id)>>
		        > >
		  : std::true_type {
		};
		// clang-format on

		template <typename T, typename = void>
		struct message_result {
			using type = void;
		};
		template <typename T>
		struct message_result<T, std::enable_if_t<is_request_message<T>::value>> {
			using type = std::future<typename T::response_type>;
		};
	} // namespace detail

	template <typename T>
	constexpr bool is_request_message = detail::is_request_message<T>::value;

	template <typename T>
	using message_result_t = typename detail::message_result<T>::type;

	/// A collection of messages handlers registered on a central Message_bus.
	/// Messages for the subscriptions are processed synchronously when update_subscriptions() is called.
	///
	/// All methods in this class are thread-safe when executed on different instances using the same Message_bus
	///   but access to the same instance needs to be externally synchronized.
	class Mailbox_collection {
	  public:
		Mailbox_collection(Message_bus& bus) : _bus(bus) {}

		auto& bus() noexcept { return _bus; }

		/// Subscribe to messages of type T, calling the given handler for each of them.
		/// The queue_size is used as a hint for the temporary storage of unprocessed messages.
		template <class T, std::size_t queue_size = detail::default_mailbox_size, typename Func>
		void subscribe(Func&& handler);

		/// Subscribe to a list of messages deduced from the argument types of the given handlers.
		template <std::size_t queue_size = detail::default_mailbox_size, typename... Func>
		void subscribe_to(Func&&... handler);

		template <class T>
		void unsubscribe();

		void update_subscriptions();

		template <typename Msg, typename... Arg>
		auto send(Arg&&... arg) -> message_result_t<Msg>
		{
			return send_msg(Msg{std::forward<Arg>(arg)...});
		}
		template <typename Msg>
		auto send_msg(const Msg& msg) -> message_result_t<Msg>;

		void enable();
		void disable();

	  private:
		Message_bus&                                                          _bus;
		std::unordered_map<type_uid_t, std::shared_ptr<detail::Mailbox_base>> _boxes;

		template <typename Msg>
		auto _subscribe_to_response(Request_id) -> std::future<typename Msg::response_type>;
	};


	class Message_bus {
	  public:
		/// thread-safe
		template <typename Msg, typename... Arg>
		void send(Arg&&... arg)
		{
			send_msg(Msg{std::forward<Arg>(arg)...});
		}

		/// thread-safe
		template <typename Msg>
		void send_msg(const Msg& msg, const void* self = nullptr);

	  private:
		template <typename T>
		friend class detail::Mailbox;

		struct Mailbox_ref {
			Mailbox_ref() = default;
			template <typename T>
			Mailbox_ref(std::shared_ptr<detail::Mailbox<T>> mailbox);

			template <typename T>
			void exec_send(const T& m, const void* src) const;

			std::weak_ptr<void>                     _mailbox;
			std::function<void(void*, const void*)> _send;
		};

		struct Mailbox_array {
			Mailbox_array() : data(), size(0) {}
			Mailbox_array(std::size_t size)
			  : data(new Mailbox_ref[size], std::default_delete<Mailbox_ref[]>()), size(size)
			{
			}

			template <typename... Ts>
			auto with(Ts&&... args) -> Mailbox_array;

			template <typename T>
			auto without(std::weak_ptr<detail::Mailbox<T>> e) -> Mailbox_array;

			std::shared_ptr<Mailbox_ref> data;
			std::size_t                  size;
		};

		std::shared_mutex          _mutex;
		std::vector<Mailbox_array> _groups;


		/// thread-safe
		template <typename T>
		void register_mailbox(std::shared_ptr<detail::Mailbox<T>> mailbox);

		/// thread-safe
		template <typename T>
		void unregister_mailbox(std::weak_ptr<detail::Mailbox<T>> mailbox);
	};
} // namespace mirrage::util

#define MESSAGEBUS_HPP_INCLUDED
#include "messagebus.hxx"
