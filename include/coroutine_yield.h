#pragma once
/*
	类c#的协程，协程函数的格式如下：
	yield_constructor constructor;
	coroutine_t coroutine_func(...)
	{
		co_yield &yield_constructor;
	}
*/

#include <vector>
#include <queue>
#include <experimental/coroutine>

namespace coroutine_yield
{
#if defined _WIN64
	typedef unsigned long long uint64_t;
#endif

	uint64_t get_cur_tick();

	class yield_constructor
	{
	public:
		virtual ~yield_constructor() {}
		virtual void start() = 0;
		virtual bool can_resume() = 0;
		virtual int trigger(int _event_id, void* _result) { return -1; }
	};

	struct coroutine_t
	{
		// 内部属性
		struct promise_type;
		using handle_type = std::experimental::coroutine_handle<promise_type>; //type alias

		coroutine_t(handle_type h) :
			handle(h), id(0)
		{
		}

		coroutine_t(const coroutine_t& s) :
			handle(s.handle), id(s.id)
		{
		}

		coroutine_t& operator=(const coroutine_t& s)
		{
			handle = s.handle;
			id = s.id;

			return *this;
		}

		~coroutine_t()
		{
		}

		bool is_done()
		{
			if (!handle)
				return true;

			if (handle.done())
				return true;

			auto _promise = &handle.promise();

			return handle.promise().constructor == nullptr;
		}

		bool close()
		{
			if (handle == nullptr)
				return true;

			handle.destroy();
			handle = nullptr;
			id = 0;

			return true;
		}

		struct promise_type
		{
			// wait constructor
			yield_constructor* constructor{ nullptr };

			promise_type() { }
			~promise_type() { }

			auto get_return_object() 
			{
				// 创建协程句柄
				return coroutine_t{ handle_type::from_promise(*this) };
			}

			auto initial_suspend()
			{
				// 初始化协程时调用
				// 返回suspend_never，表示协程创建后不用中断，直接执行
				// 返回suspend_always，表示协程创建后中断，并不执行，使用handler.resume来执行
				return std::experimental::suspend_never{};
			}

			auto final_suspend() 
			{
				// 协程结束时调用
				bool suspend = constructor != nullptr;
				constructor = nullptr;

				return std::experimental::suspend_if(suspend);
			}

			void return_void() 
			{
			}

			auto yield_value(yield_constructor* _constructor)
			{
				// co_yield()时调用
				if (_constructor != nullptr) 
				{
					_constructor->start();
				}

				constructor = _constructor;

				return std::experimental::suspend_always{};
			}

			void unhandled_exception() 
			{
			}
		};

		// coroutine句柄
		handle_type handle;
		uint64_t id;
	};

	// 协程函数的格式
	typedef coroutine_t(*coroutine_func) (...);

	// 协程管理器
	class coroutine_manager
	{
	public:
		static coroutine_manager* instance;

	public:
		coroutine_manager(uint64_t tick) :
			cur_tick (tick)
		{
		}

		~coroutine_manager() 
		{
		}

		uint64_t get_tick() const
		{
			return cur_tick;
		}

		void update(uint64_t tick)
		{
			cur_tick = tick;

			for (size_t i = 0; i < coroutines.size(); i++) 
			{
				if (coroutines[i].is_done()) 
				{
					if (coroutines[i].handle != nullptr)
					{
						coroutines[i].close();
						free_indexes.emplace(i);
					}

					continue;
				}

				yield_constructor* constructor = coroutines[i].handle.promise().constructor;
				if (constructor == nullptr)
					continue;

				if (constructor->can_resume())
				{
					coroutines[i].handle.resume();
				}
			}
		}

		// 触发指定的事件
		void trigger_event(int event_id, void* result) 
		{
			for (size_t i = 0; i < coroutines.size(); i++)
			{
				if (coroutines[i].is_done())
					continue;

				auto constructor = coroutines[i].handle.promise().constructor;
				if (nullptr == constructor)
					continue;

				int res = constructor->trigger(event_id, result);
				if (res < 0)
					continue;

				if (res >= 0)
				{
					coroutines[i].handle.resume();
				}
				
				break;
			}
		}

		// 创建新协程
		uint64_t create_coroutine(coroutine_t handler)
		{
			if (handler.is_done())
				return (uint64_t)0;

			size_t index;
			if (free_indexes.size() > 0)
			{
				index = free_indexes.front();
				free_indexes.pop();
			}
			else
			{
				index = coroutines.size();
				coroutines.emplace_back(coroutine_t(handler));
			}

			if (index > std::numeric_limits<unsigned int>::max())
			{
				return (uint64_t)0;
			}

			++serial;
			if (serial == 0)
				serial = 1;

			uint64_t id = ((uint64_t)index << 32) | serial;
			coroutines[index] = handler;
			coroutines[index].id = id;

			return id;
		}

		// 删除指定的协程，如果是当前协程，则此协程暂停后删除
		bool destroy_coroutine(uint64_t id)
		{
			unsigned int _index = (id >> 32);

			if (_index > coroutines.size())
				return false;

			if (coroutines[_index].id != id)
				return false;

			if (coroutines[_index].is_done())
				return false;

			coroutines[_index].close();

			free_indexes.emplace(_index);

			return true;
		}

		const coroutine_t* get_coroutine(uint64_t id)
		{
			unsigned int _index = (id >> 32);

			if (_index > coroutines.size())
				return nullptr;

			if (coroutines[_index].id != id)
				return nullptr;

			if (coroutines[_index].is_done())
				return nullptr;

			return &coroutines[_index];
		}

		bool exists_coroutine(uint64_t id)
		{
			return get_coroutine(id) != nullptr;
		}

	private:
		std::vector< coroutine_t> coroutines;
		std::queue< size_t> free_indexes;

		unsigned int serial{ 0 };
		uint64_t cur_tick;
	};

	// 等待指定的时间
	class wait_for_seconds : public yield_constructor
	{
	public:
		wait_for_seconds(float seconds) : timeout_seconds(seconds)
		{
		}

		void start()
		{
			start_tick = get_cur_tick();
		}

		bool can_resume() 
		{
			uint64_t cur_tick = get_cur_tick();
			uint64_t sep = cur_tick - start_tick;

			return (sep / 1000.0f >= timeout_seconds);
		}

	private:
		// ms
		uint64_t start_tick{ 0 };
		float timeout_seconds;
	};

	// 等待下一帧
	class wait_for_frame : public yield_constructor
	{
	public:
		wait_for_frame() 
		{
		}

		void start()
		{
		}

		bool can_resume()
		{
			return true;
		}
	};

	// 等待指定的事件
	class wait_for_event : public yield_constructor
	{
	public:
		wait_for_event(int _event_id, float seconds) :
			event_id(_event_id), timeout_seconds(seconds)
		{
		}

		void start()
		{
			start_tick = get_cur_tick();
			triggered = false;
		}

		bool can_resume() 
		{
			uint64_t cur_tick = get_cur_tick();
			uint64_t sep = cur_tick - start_tick;

			return (sep / 1000.0f >= timeout_seconds);
		}

		// 返回-1: 不符, 0: 已触发, 1: 还未触发
		virtual int trigger(int _event_id, void* _result) override
		{
			if (event_id != _event_id)
				return -1;

			if (triggered)
				return 0;

			triggered = true;
			result = _result;

			return 1;
		}

		int get_event_id() const
		{
			return event_id;
		}

	private:
		// ms
		uint64_t start_tick{ 0 };
		bool triggered{ false };

		int event_id;
		float timeout_seconds;
		
	public:
		void* result{ nullptr };
	};

	// 等待指定的协程完成
	class wait_for_coroutine : public yield_constructor
	{
	public:
		wait_for_coroutine(uint64_t _coroutine_id) : coroutine_id(_coroutine_id) 
		{
		}

		void start()
		{
		}

		bool can_resume() 
		{
			return !coroutine_manager::instance->exists_coroutine(coroutine_id);
		}

		uint64_t getcoroutine_id() const 
		{
			return coroutine_id;
		}

	private:
		uint64_t coroutine_id;
	};

	// 等待指定的协程完成
	class wait_for_coroutine_group : public yield_constructor
	{
	public:
		wait_for_coroutine_group(uint64_t* _coroutine_group, size_t _count) :
			wait_coroutine_groups(_coroutine_group), count(_count)
		{
		}

		void start()
		{
		}

		bool can_resume()
		{
			for (size_t i = 0; i < count; i++)
			{
				if (coroutine_manager::instance->exists_coroutine(wait_coroutine_groups[i]))
					return false;
			}

			return true;
		}

	private:
		uint64_t* wait_coroutine_groups;
		size_t count;
	};

	inline uint64_t get_cur_tick()
	{
		return coroutine_manager::instance->get_tick();
	}
}
