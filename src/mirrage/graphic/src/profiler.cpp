#include <mirrage/graphic/profiler.hpp>

#include <mirrage/graphic/device.hpp>


namespace mirrage::graphic {

	Profiler_result::Profiler_result(std::string                  name,
	                                 std::uint32_t                qid_begin,
	                                 std::uint32_t                qid_end,
	                                 std::vector<Profiler_result> sub_elements)
	  : _name(std::move(name))
	  , _query_id_begin(qid_begin)
	  , _query_id_end(qid_end)
	  , _sub_results(std::move(sub_elements)) {

		_sub_results_lookup.reserve(_sub_results.size());
		for(auto i = 0u; i < _sub_results.size(); i++) {
			_sub_results_lookup.emplace(_sub_results[i].name(), i);
		}
	}

	void Profiler_result::update_time(double new_time) {
		if(_time_ms_index < 0) {
			_time_ms_index = 0;
			_time_ms.fill(new_time);
			_time_avg_ms = new_time;
			_time_min_ms = new_time;
			_time_max_ms = new_time;
			return;
		}

		_time_ms_index = (_time_ms_index + 1) % history_size;

		_time_avg_ms += new_time / history_size - _time_ms[_time_ms_index] / history_size;

		_time_ms[_time_ms_index] = new_time;
		_time_min_ms             = std::min(_time_min_ms, new_time);
		_time_max_ms             = std::max(_time_max_ms, new_time);
	}

	void Profiler_result::reset() noexcept {
		_time_ms_index = -1;

		for(auto& sub : _sub_results) {
			sub.reset();
		}
	}


	Profiler::Push_raii::Push_raii(Push_raii&& rhs) noexcept : _profiler(rhs._profiler) {
		rhs._profiler = nullptr;
	}
	auto Profiler::Push_raii::operator=(Push_raii&& rhs) noexcept -> Push_raii& {
		if(&rhs != this) {
			_profiler     = rhs._profiler;
			rhs._profiler = nullptr;
		}
		return *this;
	}

	Profiler::Push_raii::~Push_raii() {
		if(_profiler) {
			_profiler->_pop();
		}
	}

	Profiler::Push_raii::Push_raii() : _profiler(nullptr) {}
	Profiler::Push_raii::Push_raii(Profiler& profiler) : _profiler(&profiler) {}


	Profiler::Profiler(Device& device, std::size_t max_elements)
	  : _device(*device.vk_device())
	  , _ns_per_tick(device.physical_device_properties().limits.timestampPeriod)
	  , _query_ids(max_elements)
	  , _last_results("All", _generate_query_id(), _generate_query_id())
	  , _query_pools(device.max_frames_in_flight(),
	                 [&] {
		                 return _device.createQueryPoolUnique(vk::QueryPoolCreateInfo{
		                         vk::QueryPoolCreateFlags{}, vk::QueryType::eTimestamp, _query_ids});
		             })
	  , _query_used(max_elements, false) {}

	Profiler& Profiler::operator=(Profiler&& rhs) noexcept {
		_wait_done();

		_device                 = std::move(rhs._device);
		_ns_per_tick            = std::move(rhs._ns_per_tick);
		_query_ids              = std::move(rhs._query_ids);
		_next_query_id          = std::move(rhs._next_query_id);
		_last_results           = std::move(rhs._last_results);
		_query_pools            = std::move(rhs._query_pools);
		_current_stack          = std::move(rhs._current_stack);
		_current_command_buffer = std::move(rhs._current_command_buffer);
		_query_result_buffer    = std::move(rhs._query_result_buffer);
		_query_used             = std::move(rhs._query_used);

		return *this;
	}
	Profiler::~Profiler() { _wait_done(); }
	void Profiler::_wait_done() {
		_query_pools.pop_while([&](auto& pool) {
			auto status = _device.getQueryPoolResults<std::uint32_t>(*pool,
			                                                         0,
			                                                         _next_query_id,
			                                                         _query_result_buffer,
			                                                         sizeof(std::uint32_t),
			                                                         vk::QueryResultFlagBits::eWait);

			INVARIANT(status == vk::Result::eSuccess, "getQueryPoolResults failed!");
			return true;
		});
	}

	void Profiler::reset() noexcept { _last_results.reset(); }

	void Profiler::start(vk::CommandBuffer cb) {
		if(!_active || _full)
			return;

		INVARIANT(_current_stack.empty(), "Profiler::start can not be nested!");

		cb.resetQueryPool(*_query_pools.head(), 0, _query_ids);
		std::fill(_query_used.begin(), _query_used.end(), false);

		cb.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands,
		                  *_query_pools.head(),
		                  _last_results.query_id_begin());

		_query_used[_last_results.query_id_begin()] = true;

		_current_stack.emplace_back(&_last_results);
		_current_command_buffer = cb;
	}
	void Profiler::end() {
		if(_active && !_full) {
			INVARIANT(_current_stack.size() == 1, "Unbalanced profiler stack!");
			auto& cb = _current_command_buffer.get_or_throw(
			        "No active command buffer! Has Profiler::start been called?");

			cb.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands,
			                  *_query_pools.head(),
			                  _last_results.query_id_end());

			_query_used[_last_results.query_id_end()] = true;

			_current_stack.pop_back();
			INVARIANT(_current_stack.empty(), "Unbalanced calls to Profiler::push!");

			// request timestamps for all unused queryIds
			for(auto i = 0u; i < _next_query_id; i++) {
				if(!_query_used[i]) {
					cb.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *_query_pools.head(), i);
				}
			}
		}

		_query_result_buffer.resize(_next_query_id);

		_query_pools.pop_while([&](auto& pool) {
			// receivce results
			auto status = _device.getQueryPoolResults<std::uint32_t>(*pool,
			                                                         0,
			                                                         _next_query_id,
			                                                         _query_result_buffer,
			                                                         sizeof(std::uint32_t),
			                                                         vk::QueryResultFlags{});

			if(status == vk::Result::eNotReady) {
				return false;
			}

			_update_result(_last_results, _query_result_buffer);

			return true;
		});

		if(_active || _full) {
			_full = !_query_pools.advance_head();
		}

		_current_command_buffer = util::nothing;
	}

	void Profiler::_update_result(Profiler_result& result, const std::vector<std::uint32_t>& data) {
		auto begin = data[result.query_id_begin()];
		auto end   = data[result.query_id_end()];
		auto diff  = end - begin;
		result.update_time(diff * _ns_per_tick / 1'000'000.0);

		for(auto& sub : result) {
			_update_result(sub, data);
		}
	}

	auto Profiler::push(const std::string& name, vk::PipelineStageFlagBits stage) -> Push_raii {
		if(!_active || _full)
			return {};

		INVARIANT(!_current_stack.empty(), "Profiler::push called without calling start first!");
		auto& cb = _current_command_buffer.get_or_throw(
		        "No active command buffer! Has Profiler::start been called?");

		auto& result = _current_stack.back()->get_or_add(name, [&] { return _generate_query_id(); });
		_current_stack.emplace_back(&result);

		cb.writeTimestamp(stage, *_query_pools.head(), result.query_id_begin());

		_query_used[result.query_id_begin()] = true;

		return Push_raii(*this);
	}

	void Profiler::_pop() {
		INVARIANT(_current_stack.size() > 1, "Profiler::_pop called without calling push first!");
		auto& cb = _current_command_buffer.get_or_throw(
		        "No active command buffer! Has Profiler::start been called?");

		cb.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands,
		                  *_query_pools.head(),
		                  _current_stack.back()->query_id_end());

		_query_used[_current_stack.back()->query_id_end()] = true;

		_current_stack.pop_back();
	}

	auto Profiler::_generate_query_id() -> std::uint32_t {
		INVARIANT(_next_query_id < _query_ids,
		          "More Profiler::push calls than supported!"
		          "Requested "
		                  << (_next_query_id + 1)
		                  << ", supported "
		                  << _query_ids);

		return _next_query_id++;
	}
}
