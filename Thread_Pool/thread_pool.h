#pragma once

#include <future>
#include <queue>

#define M_ENABLED m_enabled.load( std::memory_order_relaxed )


//============================================================
//	\class	ThreadPool
//
//	\author	KeyC0de
//	\date	25/9/2019 3:55
//
//	\brief	A class which encapsulates a Pool of threads
//			and dispatches work on demand - i.e. upon an incoming callable object
//			Singleton move only class
//=============================================================
class ThreadPool final
{
	using Task = std::function<void()>;

	explicit ThreadPool( std::size_t nthreads, bool enabled );
public:
	static ThreadPool& getInstance( std::size_t nThreads
		= std::thread::hardware_concurrency(), bool enabled = true );
	
	~ThreadPool() noexcept;
	ThreadPool( ThreadPool const& ) = delete;
	ThreadPool& operator=( const ThreadPool& rhs ) = delete;
	ThreadPool( ThreadPool&& rhs ) noexcept;
	ThreadPool& operator=( ThreadPool&& rhs ) noexcept;

	void start();
	void stop() noexcept;
	
	template<typename Callback, typename... TArgs>
	decltype( auto ) enqueue( Callback&& f,
		TArgs&&... args )
	{
		using ReturnType = std::invoke_result_t<Callback, TArgs...>;
		using FuncType = ReturnType(TArgs...);
		using Wrapped = std::packaged_task<FuncType>;

		if ( M_ENABLED )
		{
			std::shared_ptr<Wrapped> smartFunctionPointer =
				std::make_shared<Wrapped>( std::move( f ) );
			std::future<ReturnType> fu = smartFunctionPointer->get_future();

			auto task = [
				smartFunctionPointer = std::move( smartFunctionPointer ),
					args = std::make_tuple( std::forward<TArgs>( args )... )
					]() -> void
			{// packaged_task wrapper function
				std::apply( (*smartFunctionPointer),
					std::move( args ) );
				return;
			};

			{
				std::scoped_lock<std::mutex> lg{m_mu};
				m_tasks.emplace( std::move( task ) );
			}
			m_cond.notify_one();
			return fu;
		}
		else
		{	// TODO: rework exception handling
			throw std::runtime_error( "Cannot enqueue tasks in an inactive Thread Pool!" );
		}
	}

	void enable() noexcept;
	void disable() noexcept;
	bool isEnabled() const noexcept;
	//===================================================
	//	\function	resize
	//	\brief  adds # or subtracts -# threads to the ThreadPool
	//	\date	25/9/2019 4:00
	bool resize( int n );
private:
	std::atomic<bool> m_enabled;
	std::vector<std::thread> m_pool;
	std::queue<Task> m_tasks;
	std::condition_variable m_cond;
	std::mutex m_mu;
private:
	void run();
};
