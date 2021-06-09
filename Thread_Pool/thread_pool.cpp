#include <cstddef>
#include "thread_pool.h"


ThreadPool::ThreadPool( std::size_t nthreads,
	bool enabled )
	:
	m_enabled{enabled}
{
	m_pool.reserve( nthreads );
	if ( enabled )
	{
		run();
	}
}

ThreadPool& ThreadPool::getInstance( std::size_t nThreads,
	bool enabled )
{
	static ThreadPool instance{nThreads, enabled};
	return instance;
}

ThreadPool::~ThreadPool() noexcept
{
	stop();
}

ThreadPool::ThreadPool( ThreadPool&& rhs ) noexcept
	:
	m_enabled{std::move( rhs.m_enabled.load() )},
	m_pool{std::move( rhs.m_pool )},
	m_tasks{std::move( rhs.m_tasks )}
{}

ThreadPool& ThreadPool::operator=( ThreadPool&& rhs ) noexcept
{
	if ( this != &rhs )
	{
		m_enabled = std::move( rhs.m_enabled.load() );
		m_pool = std::move( rhs.m_pool );
		m_tasks = std::move( rhs.m_tasks );
	}
	return *this;
}

void ThreadPool::start()
{
	if ( !M_ENABLED )
	{
		m_enabled.store( true, std::memory_order_relaxed );
		run();
	}
}

void ThreadPool::stop() noexcept
{
	if ( M_ENABLED )
	{
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

void ThreadPool::enable() noexcept
{
	m_enabled.store( true, std::memory_order_relaxed );
}

void ThreadPool::disable() noexcept
{
	m_enabled.store( false, std::memory_order_relaxed );
}

bool ThreadPool::isEnabled() const noexcept
{
	return M_ENABLED;
}

//===================================================
//	\function	resize
//	\brief  adds # or subtracts -# threads to the ThreadPool
//	\date	25/9/2019 4:00
bool ThreadPool::resize( int n )
{
	if ( !isEnabled() )
	{
		return false;
	}
	if ( n > 0 )
	{
		// add threads
		return true;
	}
	else
	{
		// remove threads
		n = -n;
		{
			std::lock_guard<std::mutex> lg{ m_mu };
			if ( m_pool.size() - n < 0 )
			{
				stop();
			}
			else
			{
				for ( size_t i = m_pool.size() - 1; i >= m_pool.size() - n; --i )
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

void ThreadPool::run()
{
	std::size_t nthreads = m_pool.capacity();

	auto threadFun = [this] ()
	{
		while( true )
		{
			std::unique_lock<std::mutex> lg{ m_mu };
			while( m_tasks.empty() && M_ENABLED )
			{
				m_cond.wait( lg );
			}

			if ( !M_ENABLED )
			{
				break;
			}

			if ( !m_tasks.empty() )
			{// there is a task available
				Task task = std::move( m_tasks.front() );
				m_tasks.pop();
				lg.unlock();
				task();
			}
		}// thread sleeps forever until there's a task available
	};

	// place threads in the pool
	for( std::size_t ti = 0; ti < nthreads; ++ti )
	{
		m_pool.emplace_back( std::thread{std::move(threadFun)} );
	}
}
