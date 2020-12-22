#pragma once
/*
	协程管理
*/

#include <vector>
#include <list>
#include <queue>
#include <functional>
#include <limits>
#include <experimental/coroutine>
#include <assert.h>

namespace coroutine_await
{
#if defined _WIN64
	typedef unsigned long long uint64_t;
#endif

	uint64_t get_cur_tick();

	class awaitable;

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

			return handle.promise().awaitable_ptr == nullptr;
		}

		bool close()
		{
			if (!handle)
				return true;

			handle.destroy();
			handle = nullptr;
			id = 0;

			return true;
		}

		struct promise_type
		{
			awaitable* awaitable_ptr{ nullptr };

			promise_type() { }
			~promise_type() { }

			auto get_return_object()
			{
				awaitable_ptr = nullptr;
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
				bool suspend = awaitable_ptr != nullptr;
				awaitable_ptr = nullptr;

				return std::experimental::suspend_if(suspend);
			}

			void return_void()
			{
				// 协程结束前调用
			}

			void unhandled_exception()
			{
				// 异常时调用
			}

			void set_awaitable(awaitable* _awaitable)
			{
				awaitable_ptr = _awaitable;
			}
		};

		// coroutine句柄
		handle_type handle;
		uint64_t id;
	};

	class awaitable
	{
	public:
		awaitable() { handle = nullptr; }

		virtual bool can_resume() = 0;

		void resume()
		{
			if (handle != nullptr)
			{
				handle.resume();
			}
		}

		bool is_done() const
		{
			if (handle == nullptr)
				return true;

			return handle.done();
		}

	protected:
		void on_suspend(coroutine_t::handle_type _awaiting_handle)
		{
			handle = _awaiting_handle;
			handle.promise().set_awaitable(this);
		}

	private:
		coroutine_t::handle_type handle;
	};

	// 等待指定的时间
	class wait_for_seconds : public awaitable
	{
	public:
		wait_for_seconds(float seconds) : awaitable(), timeout_seconds(seconds)
		{
			start_tick = get_cur_tick();
		}

		virtual bool can_resume() override
		{
			uint64_t cur_tick = get_cur_tick();
			uint64_t sep = cur_tick - start_tick;

			return (sep / 1000.0f >= timeout_seconds);
		}

		bool await_ready()
		{
			// 返回Awaitable实例是否已经ready。协程开始会调用此函数，如果返回true，表示你想得到的结果已经得到了，协程不需要执行了。所以大部分情况这个函数的实现是要return false。
			return false;
		}

		void await_suspend(coroutine_t::handle_type _awaiting_handle)
		{
			// 挂起awaitable。该函数会传入一个coroutine_handle类型的参数。这是一个由编译器生成的变量。在此函数中调用handle.resume()，就可以恢复协程
			start_tick = get_cur_tick();

			awaitable::on_suspend(_awaiting_handle);
		}

		float await_resume()
		{
			// 当协程重新运行时，会调用该函数。这个函数的返回值就是co_await运算符的返回值。
			uint64_t cur_tick = get_cur_tick();
			float wait_seconds = (cur_tick - start_tick) / 1000.0f;

			return wait_seconds;
		}

	private:
		uint64_t start_tick;
		float timeout_seconds;
	};

	// 等待下一帧
	class wait_for_frame : public awaitable
	{
	public:
		wait_for_frame() : awaitable()
		{
			start_tick = get_cur_tick();
		}

		virtual bool can_resume() override
		{
			uint64_t cur_tick = get_cur_tick();
			return cur_tick > start_tick;
		}

		bool await_ready()
		{
			// 返回Awaitable实例是否已经ready。协程开始会调用此函数，如果返回true，表示你想得到的结果已经得到了，协程不需要执行了。所以大部分情况这个函数的实现是要return false。
			return false;
		}

		void await_suspend(coroutine_t::handle_type _awaiting_handle)
		{
			// 挂起awaitable。该函数会传入一个coroutine_handle类型的参数。这是一个由编译器生成的变量。在此函数中调用handle.resume()，就可以恢复协程
			awaitable::on_suspend(_awaiting_handle);
		}

		void await_resume()
		{
			// 当协程重新运行时，会调用该函数。这个函数的返回值就是co_await运算符的返回值。
		}

	private:
		uint64_t start_tick;
	};

	// 等待指定的事件
	template<typename T>
	class wait_for_event : public awaitable
	{
	public:
		wait_for_event(int _event_id, float _seconds) :
			awaitable(), event_id(_event_id), timeout_seconds(_seconds)
		{
			return_value = nullptr;
			start_tick = get_cur_tick();
		}

		virtual bool can_resume() override
		{
			uint64_t cur_tick = get_cur_tick();
			uint64_t sep = cur_tick - start_tick;

			return (sep / 1000.0f >= timeout_seconds);
		}

		int get_event_id() const
		{
			return event_id;
		}

		void set_return_value(const T* _value)
		{
			return_value = _value;
		}

		bool await_ready()
		{
			// 返回Awaitable实例是否已经ready。协程开始会调用此函数，如果返回true，表示你想得到的结果已经得到了，协程不需要执行了。所以大部分情况这个函数的实现是要return false。
			return false;
		}

		void await_suspend(coroutine_t::handle_type _awaiting_handle)
		{
			// 挂起awaitable。该函数会传入一个coroutine_handle类型的参数。这是一个由编译器生成的变量。在此函数中调用handle.resume()，就可以恢复协程
			start_tick = get_cur_tick();

			awaitable::on_suspend(_awaiting_handle);
		}

		const T* await_resume()
		{
			// 当协程重新运行时，会调用该函数。这个函数的返回值就是co_await运算符的返回值。
			return return_value;
		}

	private:
		// ms
		uint64_t start_tick;
		float timeout_seconds;
		int event_id;
		const T* return_value;
	};

	// 等待指定的协程完成
	class wait_for_coroutine : public awaitable
	{
	public:
		wait_for_coroutine(uint64_t _id) :
			awaitable(), wait_coroutine_id(_id) {
		}

		virtual bool can_resume() override;

		bool await_ready()
		{
			// 返回Awaitable实例是否已经ready。协程开始会调用此函数，如果返回true，表示你想得到的结果已经得到了，协程不需要执行了。所以大部分情况这个函数的实现是要return false。
			return false;
		}

		void await_suspend(coroutine_t::handle_type _awaiting_handle)
		{
			// 挂起awaitable。该函数会传入一个coroutine_handle类型的参数。这是一个由编译器生成的变量。在此函数中调用handle.resume()，就可以恢复协程
			awaitable::on_suspend(_awaiting_handle);
		}

		void await_resume()
		{
			// 当协程重新运行时，会调用该函数。这个函数的返回值就是co_await运算符的返回值。
		}

	private:
		uint64_t wait_coroutine_id;
	};

	// 等待指定的协程完成
	class wait_for_coroutine_group : public awaitable
	{
	public:
		wait_for_coroutine_group(uint64_t* _coroutine_group, size_t _count) :
			awaitable(), wait_coroutine_groups(_coroutine_group), count(_count) {
		}

		virtual bool can_resume() override;

		bool await_ready()
		{
			// 返回Awaitable实例是否已经ready。协程开始会调用此函数，如果返回true，表示你想得到的结果已经得到了，协程不需要执行了。所以大部分情况这个函数的实现是要return false。
			return false;
		}

		void await_suspend(coroutine_t::handle_type _awaiting_handle)
		{
			// 挂起awaitable。该函数会传入一个coroutine_handle类型的参数。这是一个由编译器生成的变量。在此函数中调用handle.resume()，就可以恢复协程
			awaitable::on_suspend(_awaiting_handle);
		}

		void await_resume()
		{
			// 当协程重新运行时，会调用该函数。这个函数的返回值就是co_await运算符的返回值。
		}

	private:
		uint64_t* wait_coroutine_groups;
		size_t count;
	};

	class coroutine_manager
	{
	public:
		static coroutine_manager* instance;

	public:
		coroutine_manager(uint64_t tick) : cur_tick(tick)
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

				awaitable* _awaitable = coroutines[i].handle.promise().awaitable_ptr;
				if (_awaitable == nullptr)
					continue;

				if (_awaitable->can_resume())
					_awaitable->resume();
			}
		}

		// 触发指定的事件
		template<typename T>
		void trigger_event(int event_id, const T* ret_value)
		{
			for (size_t i = 0; i < coroutines.size(); i++)
			{
				if (coroutines[i].is_done())
					continue;

				wait_for_event<T>* _awaitable = dynamic_cast<wait_for_event<T>*>(coroutines[i].handle.promise().awaitable_ptr);
				if (_awaitable == nullptr)
					continue;

				if (_awaitable->get_event_id() != event_id)
					continue;

				if (_awaitable->is_done())
					continue;

				_awaitable->set_return_value(ret_value);
				_awaitable->resume();
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

	inline uint64_t get_cur_tick()
	{
		return coroutine_manager::instance->get_tick();
	}

	inline bool wait_for_coroutine::can_resume()
	{
		return !coroutine_manager::instance->exists_coroutine(wait_coroutine_id);
	}

	inline bool wait_for_coroutine_group::can_resume()
	{
		for (size_t i = 0; i < count; i++)
		{
			if (coroutine_manager::instance->exists_coroutine(wait_coroutine_groups[i]))
				return false;
		}

		return true;
	}
}