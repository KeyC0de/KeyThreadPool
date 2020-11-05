#include <cstddef>
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
//			and dispatches to work upon an incoming callable object
//			Singleton move only class
//=============================================================
class ThreadPool final
{
	using Task = std::function<void()>;

	explicit ThreadPool( std::size_t nthreads,
		bool enabled )
		:
		m_enabled{ enabled }
	{
		m_pool.reserve( nthreads );
		if ( enabled )
		{
			run();
		}
	}
public:
	static ThreadPool& getInstance(
		std::size_t nthreads = std::thread::hardware_concurrency(),
		bool enabled = true )
	{
		static ThreadPool instance{nthreads, enabled};
		return instance;
	}
	
	~ThreadPool() noexcept
	{
		stop();
	}

	ThreadPool( ThreadPool const& ) = delete;
	ThreadPool& operator=( const ThreadPool& rhs ) = delete;

	void start()
	{
		if ( !M_ENABLED )
		{
			m_enabled.store( true, std::memory_order_relaxed );
			run();
		}
	}

	void stop() noexcept
	{
		if ( M_ENABLED )
		{// if already running
			m_enabled.store( false, std::memory_order_relaxed );
			m_cond.notify_all();
			for ( auto& t : m_pool )
			{
				if ( t.joinable() )
				{
					t.join();
				}
			}
		}
	}
	
	template<typename Callback, typename... TArgs>
	decltype(auto) enqueue( Callback&& f, TArgs&&... args )
	{
		using ReturnType = std::invoke_result_t<Callback, TArgs...>;
		using FuncType = ReturnType(TArgs...);
		using Wrapped = std::packaged_task<FuncType>;

		if ( M_ENABLED )
		{
			std::shared_ptr<Wrapped> smartFunctionPointer = std::make_shared<Wrapped>(std::move(f));
			std::future<ReturnType> fu = smartFunctionPointer->get_future();

			auto task = [
				smartFunctionPointer = std::move( smartFunctionPointer ),
					args = std::make_tuple( std::forward<TArgs>(args)... )
					]() -> void
			{// packaged_task wrapper function
				std::apply( (*smartFunctionPointer), std::move( args ) );
				return;
			};

			{
				std::scoped_lock<std::mutex> lg{ m_mu };
				m_tasks.emplace( std::move( task ) );
			}
			m_cond.notify_one();
			return fu;
		}
		else
		{	// TODO: rework exception handling
			throw std::runtime_error("Cannot enqueue tasks in an inactive Thread Pool!");
		}
	}

	inline void enable() noexcept {
		m_enabled.store( true, std::memory_order_relaxed );
	}

	inline void disable() noexcept {
		m_enabled.store( false, std::memory_order_relaxed );
	}

	inline bool isEnabled() const noexcept {
		return M_ENABLED;
	}

	//===================================================
	//	\function	resize
	//	\brief  adds # or subtracts -# threads to the ThreadPool
	//	\date	25/9/2019 4:00
	bool resize( int n )
	{
		if ( !isEnabled() )
			return false;
		if (n > 0)
		{	// add threads

			return true;
		}
		else
		{	// remove threads
			n = -n;
			{
				std::scoped_lock<std::mutex> lg{ m_mu };
				if ( m_pool.size() - n < 0 )
				{
					stop();
				}
				else
				{
					for ( int i = m_pool.size() - 1; i >= m_pool.size() - n; i-- )
					{
						if ( m_pool[i].joinable() )
						{
							m_pool[i].join();
						}
					}
				}
			}
			return true;
		}
	}

private:
	std::atomic<bool> m_enabled;
	std::vector<std::thread> m_pool;
	std::queue<Task> m_tasks;
	std::condition_variable m_cond;
	std::mutex m_mu;

	void run()
	{
		std::size_t nthreads = m_pool.capacity();

		auto f = [this] ()
		{
			while( true )
			{
				std::unique_lock<std::mutex> lg{ m_mu };
				while( m_tasks.empty() && M_ENABLED )
					m_cond.wait( lg );

				if ( !M_ENABLED )
					break;

				if ( !m_tasks.empty() )
				{// there is a task available
					Task task = std::move( m_tasks.front() );
					m_tasks.pop();
					lg.unlock();
					task();
				}
			}// while threadPool is enabled
			//return;
		};// thread function

		// place threads in the pool
		// and assign them tasks
		for( std::size_t ti = 0; ti < nthreads; ++ti )
		{
			m_pool.emplace_back( std::thread{ std::move(f) } );// launch thread
		}
	}

};
/////////////////////////////////////////////////////////////////////////