#pragma once

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/ring_buffer.hpp>

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>


namespace mirrage::graphic {

	class Device;


	class Profiler_result {
	  public:
		explicit Profiler_result(std::string                  name,
		                         std::uint32_t                qid_begin,
		                         std::uint32_t                qid_end,
		                         std::vector<Profiler_result> sub_elements = {});

		template <typename F>
		auto get_or_add(const std::string& name, F&& id_source) -> Profiler_result&
		{
			auto iter = _sub_results_lookup.find(name);
			if(iter != _sub_results_lookup.end()) {
				return _sub_results.at(iter->second);
			}

			_sub_results_lookup.emplace(name, _sub_results.size());
			_sub_results.emplace_back(name, id_source(), id_source());
			return _sub_results.back();
		}
		void update_time(double new_time);
		auto query_id_begin() const noexcept { return _query_id_begin; }
		auto query_id_end() const noexcept { return _query_id_end; }
		void reset() noexcept;

		// read-only interface
		auto& name() const noexcept { return _name; }
		auto  time_ms() const noexcept
		{
			return _time_ms_index < 0 ? 0 : _time_ms.at(std::size_t(_time_ms_index));
		}
		auto  time_avg_ms() const noexcept { return _time_avg_ms; }
		auto  time_min_ms() const noexcept { return _time_min_ms; }
		auto  time_max_ms() const noexcept { return _time_max_ms; }
		auto& subs() const noexcept { return _sub_results; }

		auto begin() const noexcept { return _sub_results.begin(); }
		auto end() const noexcept { return _sub_results.end(); }

		auto begin() noexcept { return _sub_results.begin(); }
		auto end() noexcept { return _sub_results.end(); }

	  private:
		static constexpr auto history_size = 60;
		using Lookup_table                 = std::unordered_map<std::string, std::size_t>;
		using Time_history                 = std::array<double, history_size>;

		std::string                  _name;
		std::uint32_t                _query_id_begin = 0;
		std::uint32_t                _query_id_end   = 0;
		Time_history                 _time_ms;
		int                          _time_ms_index = -1;
		double                       _time_avg_ms   = 0;
		double                       _time_min_ms   = 0;
		double                       _time_max_ms   = 0;
		std::vector<Profiler_result> _sub_results;
		Lookup_table                 _sub_results_lookup;
	};

	/**
	 * @brief GPU profiler
	 *
	 * Usage:
	 * auto profiler = Profiler();
	 * // ...
	 * profiler.start(cmd_buffer);
	 * {
	 *     auto _ = push("deferred");
	 *     {
	 *         auto _ = push("geometry");
	 *     }
	 *     {
	 *         auto _ = push("lighting");
	 *     }
	 * }
	 * profiler.end();
	 *
	 * for(auto&& e : profiler.results()) {
	 *     cout << e.name() << ": "<<e.time_ms() << " : " << e.time_avg_ms() << "\n";
	 *     for(auto&& e : profiler.results()) {
	 *         cout << "  " << e.name() << ": "<<e.time_ms() << " : " << e.time_avg_ms() << "\n";
	 *     }
	 * }
	 */
	class Profiler {
	  public:
		class Push_raii {
		  public:
			Push_raii(Push_raii&&) noexcept;
			Push_raii& operator=(Push_raii&&) noexcept;
			~Push_raii();

		  private:
			friend class Profiler;
			Push_raii();
			explicit Push_raii(Profiler&);
			Profiler* _profiler;
		};

	  public:
		explicit Profiler(Device&, std::uint32_t max_elements = 32);

		void enable() noexcept { _active_requested = true; }
		void disable() noexcept { _active_requested = false; }
		void reset() noexcept;

		void start(vk::CommandBuffer);
		auto push(const std::string& name,
		          vk::PipelineStageFlagBits = vk::PipelineStageFlagBits::eAllCommands) -> Push_raii;
		auto push(const char* name, vk::PipelineStageFlagBits stage = vk::PipelineStageFlagBits::eAllCommands)
		        -> Push_raii
		{
			if(_active)
				return push(std::string(name), stage);
			else
				return {};
		}
		void end();

		auto& results() const noexcept { return _last_results; }

	  private:
		friend class Push_raii;

		struct Query_pool_entry {
			vk::UniqueQueryPool pool;
			std::uint32_t       requested_queries = 0;

			Query_pool_entry() = default;
			Query_pool_entry(Device& device, std::uint32_t max_count);
			Query_pool_entry(Query_pool_entry&& rhs) noexcept
			  : pool(std::move(rhs.pool)), requested_queries(rhs.requested_queries)
			{
			}
			Query_pool_entry& operator=(Query_pool_entry&& rhs) noexcept
			{
				pool              = std::move(rhs.pool);
				requested_queries = std::move(rhs.requested_queries);
				return *this;
			}
		};

		using Query_pools  = util::ring_buffer<Query_pool_entry>;
		using Result_stack = std::vector<Profiler_result*>;

		vk::Device      _device;
		double          _ns_per_tick;
		std::uint32_t   _query_ids;
		std::uint32_t   _next_query_id = 0;
		Profiler_result _last_results;
		Query_pools     _query_pools;
		bool            _active           = false;
		bool            _active_requested = false;
		bool            _full             = false;

		Result_stack                   _current_stack;
		util::maybe<vk::CommandBuffer> _current_command_buffer;

		std::vector<std::uint32_t> _query_result_buffer;
		std::vector<char>          _query_used;

		void _pop();

		auto _generate_query_id() -> std::uint32_t;

		void _update_result(Profiler_result&, const std::vector<std::uint32_t>& data);
	};
} // namespace mirrage::graphic
