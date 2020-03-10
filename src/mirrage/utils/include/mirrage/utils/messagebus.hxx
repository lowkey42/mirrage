#pragma once

#ifndef MESSAGEBUS_HPP_INCLUDED
#include "messagebus.hpp"
#endif


namespace mirrage::util {

	namespace detail {
		constexpr auto default_msg_batch_size = 2;

		class Mailbox_base {
		  public:
			Mailbox_base()          = default;
			virtual ~Mailbox_base() = default;

			/// two-phase init because of shared_from_this
			virtual void register_mailbox() = 0;
			virtual void process()          = 0;
			virtual void enable()           = 0;
			virtual void disable()          = 0;
		};

		template <typename T>
		class Mailbox : public Mailbox_base, public std::enable_shared_from_this<Mailbox<T>> {
		  public:
			template <typename Handler>
			Mailbox(Message_bus& bus, Handler&& handler, std::size_t size = default_mailbox_size)
			  : _queue(size, 0, 4)
			  , _bus(bus)
			  , _processor([h = std::forward<Handler>(handler)](Mailbox<T>& self) {
				  T    msg[default_msg_batch_size];
				  auto count = std::size_t(0);
				  do {
					  count = self._queue.try_dequeue_bulk(msg, default_msg_batch_size);
					  std::for_each(std::begin(msg), std::begin(msg) + count, h);
				  } while(count > 0);
			  })
			{
			}
			~Mailbox() { _bus.unregister_mailbox(this->weak_from_this()); }

			void send(const T& v)
			{
				if(_active.load())
					_queue.enqueue(v);
			}

			void register_mailbox() override { _bus.register_mailbox(this->shared_from_this()); }

			void enable() override { _active.store(true); }
			void disable() override { _active.store(false); }

			void process() override { _processor(*this); }

		  private:
			moodycamel::ConcurrentQueue<T>   _queue;
			Message_bus&                     _bus;
			std::atomic<bool>                _active{true};
			std::function<void(Mailbox<T>&)> _processor;
		};

		template <typename T>
		class Response_mailbox : public Mailbox<T> {
		  public:
			Response_mailbox(Message_bus& bus, std::size_t size = default_mailbox_size)
			  : Mailbox<T>(
			          bus,
			          [this](const T& msg) {
				          if(auto iter = _mapping.find(msg.request_id); iter != _mapping.end()) {
					          iter->second.set_value(msg);
					          _mapping.erase(iter);
				          }
			          },
			          size)
			{
			}

			auto get_response(Request_id id)
			{
				auto [iter, success] =
				        _mapping.emplace(id, Entry{{}, std::chrono::steady_clock::now() + timeout});
				MIRRAGE_INVARIANT(success,
				                  "The response mapping already contains an entry for this UUID: " << id);
				return iter->second.promise.get_future();
			}

			void process() override
			{
				Mailbox<T>::process();

				// remove mapping that reached their timeout without getting a response
				auto now = std::chrono::steady_clock::now();
				util::erase_if(_mapping, [&](auto& e) { return e.second.timeout_timepoint <= now; });
			}

		  private:
			using Time = std::chrono::time_point<std::chrono::steady_clock>;
			struct Entry {
				std::promise<T> promise;
				Time            timeout_timepoint;
			};

			static constexpr auto                 timeout = std::chrono::seconds(30);
			std::unordered_map<Request_id, Entry> _mapping;
		};
	} // namespace detail

	template <typename T, std::size_t queue_size, typename Func>
	void Mailbox_collection::subscribe(Func&& handler)
	{
		MIRRAGE_INVARIANT(_boxes.find(type_uid_of<T>()) == _boxes.end(), "Listener already registered!");

		using result_t = std::remove_reference_t<decltype(handler(std::declval<T&>()))>;

		if constexpr(is_request_message<T>) {
			static_assert(std::is_same_v<result_t, typename T::response_type>,
			              "Message handlers for request messages have to return the type specified in "
			              "Request::response_type");
		}

		if constexpr(std::is_void_v<result_t>) {
			auto box = std::make_shared<detail::Mailbox<T>>(_bus, std::forward<Func>(handler), queue_size);
			box->register_mailbox();
			_boxes.emplace(type_uid_of<T>(), std::move(box));

		} else {
			auto handler_wrapper = [handler = std::forward<Func>(handler), this](T& msg) {
				if constexpr(is_request_message<T>) {
					auto resp       = handler(msg);
					resp.request_id = msg.request_id;
					_bus.send_msg(std::move(resp));
				} else {
					send_msg(handler(msg));
				}
			};

			auto box = std::make_shared<detail::Mailbox<T>>(_bus, std::move(handler_wrapper), queue_size);
			box->register_mailbox();
			_boxes.emplace(type_uid_of<T>(), std::move(box));
		}
	}

	template <std::size_t queue_size, typename... Func>
	void Mailbox_collection::subscribe_to(Func&&... handler)
	{
		apply(
		        [&](auto&& h) {
			        using T = std::decay_t<nth_func_arg_t<std::remove_reference_t<decltype(h)>, 0>>;
			        this->subscribe<T, queue_size>(std::forward<decltype(h)>(h));
		        },
		        std::forward<Func>(handler)...);
	}

	template <typename Msg>
	auto Mailbox_collection::_subscribe_to_response(Request_id id) -> std::future<typename Msg::response_type>
	{
		using T = typename Msg::response_type;

		auto& box = _boxes[type_uid_of<T>()];
		if(!box) {
			box = std::make_shared<detail::Response_mailbox<T>>(_bus, detail::default_mailbox_size);
			box->register_mailbox();
		}

		auto resp_box = dynamic_cast<detail::Response_mailbox<T>*>(box.get());
		MIRRAGE_INVARIANT(resp_box, "A response message has been used in a normal subscription!");
		return resp_box->get_response(id);
	}

	template <typename T>
	void Mailbox_collection::unsubscribe()
	{
		_boxes.erase(type_uid_of<T>());
	}

	inline void Mailbox_collection::update_subscriptions()
	{
		for(auto&& [_, box] : _boxes)
			box->process();
	}

	template <typename Msg>
	auto Mailbox_collection::send_msg(const Msg& msg) -> message_result_t<Msg>
	{
		const auto self     = _boxes.find(type_uid_of<Msg>());
		const auto self_ptr = self != _boxes.end() ? self->second.get() : nullptr;

		if constexpr(is_request_message<Msg>) {
			auto response = _subscribe_to_response<Msg>(msg.request_id);
			_bus.send_msg(msg, self_ptr);
			return response;

		} else {
			_bus.send_msg(msg, self_ptr);
		}
	}

	inline void Mailbox_collection::enable()
	{
		for(auto&& [_, box] : _boxes) {
			box->enable();
		}
	}

	inline void Mailbox_collection::disable()
	{
		for(auto&& [_, box] : _boxes) {
			box->disable();
		}
	}


	template <typename T>
	Message_bus::Mailbox_ref::Mailbox_ref(std::shared_ptr<detail::Mailbox<T>> mailbox)
	  : _mailbox(std::static_pointer_cast<void>(std::move(mailbox))), _send(+[](void* mb, const void* m) {
		  static_cast<detail::Mailbox<T>*>(mb)->send(*static_cast<const T*>(m));
	  })
	{
	}

	template <typename T>
	void Message_bus::Mailbox_ref::exec_send(const T& m, const void* src) const
	{
		if(auto dst = _mailbox.lock(); dst && (src == nullptr || src != dst.get())) {
			_send(dst.get(), static_cast<const void*>(&m));
		}
	}

	template <typename... Ts>
	auto Message_bus::Mailbox_array::with(Ts&&... args) -> Mailbox_array
	{
		auto r    = Mailbox_array(size + 1);
		auto back = std::copy(data.get(), data.get() + size, r.data.get());
		*back     = Mailbox_ref{std::forward<Ts>(args)...};
		return r;
	}

	template <typename T>
	auto Message_bus::Mailbox_array::without(std::weak_ptr<detail::Mailbox<T>> e) -> Mailbox_array
	{
		auto begin = data.get();
		auto end   = data.get() + size;
		auto iter  = std::find_if(begin, end, [&](auto& mb) {
            return !mb._mailbox.owner_before(e) && !e.owner_before(mb._mailbox);
        });

		if(iter != end) {
			auto r = Mailbox_array(size);
			auto i = std::copy(begin, iter, r.data.get());
			std::copy(iter + 1, end, i);
			return r;

		} else {
			return *this;
		}
	}

	template <typename T>
	void Message_bus::register_mailbox(std::shared_ptr<detail::Mailbox<T>> mailbox)
	{
		auto _ = std::unique_lock(_mutex);

		const auto id = type_uid_of<T>();
		if(std::size_t(id) >= _groups.size()) {
			_groups.resize(id + 1);
		}

		_groups[id] = _groups[id].with(std::move(mailbox));
	}

	template <typename T>
	void Message_bus::unregister_mailbox(std::weak_ptr<detail::Mailbox<T>> mailbox)
	{
		auto _ = std::unique_lock(_mutex);

		const auto id = type_uid_of<T>();
		if(std::size_t(id) < _groups.size()) {
			_groups[id] = _groups[id].without(mailbox);
		}
	}

	template <typename Msg>
	void Message_bus::send_msg(const Msg& msg, const void* self)
	{
		const auto id = type_uid_of<Msg>();

		auto lock = std::shared_lock(_mutex);
		if(std::size_t(id) < _groups.size()) {
			// copy because the callbacks might register/unregister listeners
			auto receivers = _groups[id];
			lock.unlock();

			for(auto& mb : util::range(receivers.data.get(), receivers.data.get() + receivers.size)) {
				mb.exec_send(msg, self);
			}
		}
	}

} // namespace mirrage::util
