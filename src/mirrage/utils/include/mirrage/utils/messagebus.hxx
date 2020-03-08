#pragma once

#ifndef MESSAGEBUS_HPP_INCLUDED
#include "messagebus.hpp"
#endif


namespace mirrage::util {

	namespace detail {
		template <typename T>
		class Mailbox : public std::enable_shared_from_this<Mailbox<T>> {
		  public:
			Mailbox(Message_bus& bus, std::size_t size = default_mailbox_size);
			~Mailbox();

			void register_mailbox();

			void send(const T& v);
			auto receive() -> maybe<T>;
			template <std::size_t Size>
			auto receive(T (&target)[Size]) -> std::size_t;

			void enable() { _active.store(true); }
			void disable() { _active.store(false); }

		  private:
			moodycamel::ConcurrentQueue<T> _queue;
			Message_bus&                   _bus;
			std::atomic<bool>              _active{true};
		};

		template <typename T>
		Mailbox<T>::Mailbox(Message_bus& bus, std::size_t size) : _queue(size, 0, 4), _bus(bus)
		{
		}

		template <typename T>
		Mailbox<T>::~Mailbox()
		{
			_bus.unregister_mailbox(this->weak_from_this());
		}

		template <typename T>
		void Mailbox<T>::register_mailbox()
		{
			_bus.register_mailbox(this->shared_from_this());
		}

		template <typename T>
		void Mailbox<T>::send(const T& v)
		{
			if(_active.load())
				_queue.enqueue(v);
		}

		template <typename T>
		auto Mailbox<T>::receive() -> maybe<T>
		{
			maybe<T> ret = T{};

			if(!_queue.try_dequeue(ret.get_or_throw()))
				ret = nothing;

			return ret;
		}

		template <typename T>
		template <std::size_t Size>
		auto Mailbox<T>::receive(T (&target)[Size]) -> std::size_t
		{
			return _queue.try_dequeue_bulk(target, Size);
		}


		template <typename T, std::size_t size, typename Func>
		void receive_bulk(void* box, Func& handler)
		{
			using namespace std;

			MIRRAGE_INVARIANT(box, "No subscription for given type");

			T     msg[size];
			auto& cbox = *static_cast<Mailbox<T>*>(box);

			auto count = std::size_t(0);
			do {
				count = cbox.receive(msg);

				for_each(begin(msg), begin(msg) + count, handler);
			} while(count > 0);
		}
	} // namespace detail

	template <typename T, std::size_t batch_size, typename Func>
	void Mailbox_collection::subscribe(std::size_t queue_size, Func handler)
	{
		using namespace std;

		MIRRAGE_INVARIANT(_boxes.find(type_uid_of<T>()) == _boxes.end(), "Listener already registered!");

		auto& box     = _boxes[type_uid_of<T>()];
		auto  mailbox = make_shared<detail::Mailbox<T>>(_bus, queue_size);
		mailbox->register_mailbox();
		box.box       = std::move(mailbox);
		box.handler   = [handler](Sub& s) { detail::receive_bulk<T, batch_size>(s.box.get(), handler); };
		box.activator = [](Sub& s, bool active) {
			auto mb = static_cast<detail::Mailbox<T>*>(s.box.get());
			if(active)
				mb->enable();
			else
				mb->disable();
		};
	}
	template <std::size_t batch_size, std::size_t queue_size, typename... Func>
	void Mailbox_collection::subscribe_to(Func&&... handler)
	{
		apply(
		        [&](auto&& h) {
			        using T = std::decay_t<nth_func_arg_t<std::remove_reference_t<decltype(h)>, 0>>;
			        this->subscribe<T, batch_size>(queue_size, std::forward<decltype(h)>(h));
		        },
		        std::forward<Func>(handler)...);
	}

	template <typename T>
	void Mailbox_collection::unsubscribe()
	{
		_boxes.erase(type_uid_of<T>());
	}

	inline void Mailbox_collection::update_subscriptions()
	{
		for(auto& b : _boxes)
			if(b.second.handler)
				b.second.handler(b.second);
	}

	template <typename Msg>
	void Mailbox_collection::send_msg(const Msg& msg)
	{
		auto self = _boxes.find(type_uid_of<Msg>());

		_bus.send_msg(msg, self != _boxes.end() ? self->second.box.get() : nullptr);
	}

	inline void Mailbox_collection::enable()
	{
		for(auto& b : _boxes) {
			b.second.activator(b.second, true);
		}
	}

	inline void Mailbox_collection::disable()
	{
		for(auto& b : _boxes) {
			b.second.activator(b.second, false);
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
