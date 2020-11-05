#include <iostream>

using namespace std::literals::chrono_literals;

///======================================================================
/// \class ThreadPool
///
/// \author Nikos Lazaridis (KeyC0de)
/// \date 21/9/2019
/// \brief A class which encapsulates a Pool of available threads each dispatched to do work whenever a Task arrives in the Task Queue
///======================================================================
class ThreadPool final
{
	using Task = std::function<void()>;

	std::vector<std::thread> m_pool;
	std::queue<Task> m_tasks;
	std::condition_variable m_cond;
	std::mutex m_mu;
	bool m_benabled;

	void start(std::size_t nthreads)
	{
		for (unsigned i = 0u; i < nthreads; i++)
		{
			m_pool.emplace_back([this]
			{
				while (true)
				{
					Task task;
					{
						std::unique_lock<std::mutex> ul{ m_mu };
						while (m_benabled && m_tasks.empty())
							m_cond.wait(ul);
						if (!m_benabled && m_tasks.empty())
							break;
						// there's work to do
						task = std::move(m_tasks.front());
						m_tasks.pop();
					}
					// execute task without blocking other threads
					task();
				}
			});//
		}//end for
	}
public:

	explicit ThreadPool(std::size_t nthreads) noexcept
		: m_benabled{ true }
	{
		start(nthreads);
	}

	ThreadPool(const ThreadPool& tp) = delete;
	ThreadPool& operator=(const ThreadPool& tp) = delete;

	~ThreadPool()
	{
		stop();
	}

	template<typename Callable>
	decltype(auto) enqueue(Callable task)   // noexcept(noexcept(std::future<decltype(task())>))
	{
		auto taskWrapper = std::make_shared<std::packaged_task<decltype(task()) ()>>(std::move(task));
		{
			std::unique_lock<std::mutex> ul{ m_mu };
			m_tasks.emplace([=]() {
				(*taskWrapper)();           // run the task after placing it
			});
		}
		m_cond.notify_one();
		return taskWrapper->get_future();   // get the result
	}

	void stop() noexcept
	{
		{
			std::unique_lock<std::mutex> ul{ m_mu };
			m_benabled = false;
			m_cond.notify_all();
		}

		for (auto& t : m_pool)
			if (t.joinable())
				t.join();
	}
};
/////////////////////////////////////////////////////////////////////////

// Create some work to test the Thread Pool
void spitId() {
	std::cout << "thread #" << std::this_thread::get_id() << '\n';
}

void sayAndNoReturn()
{
	auto tid = std::this_thread::get_id();
	std::cout << "thread #" << tid << "says " << " and returns... ";
	std::cout << typeid(tid).name() << '\n';    // std::thread::id
}

char sayWhat(int arg)
{
	auto tid = std::this_thread::get_id();
	std::stringstream sid;
	sid << tid;
	int id = std::stoi(sid.str());
	std::cout << "thread #" << id << " says " << arg << " and returns... ";
	if (id > 7000)
		return 'X';
	return 'a';
}


int main()
{
	/// \brief has to result in facilities of std::bind to pass arguments along with the function
	ThreadPool threadPool{ std::thread::hardware_concurrency() };

	auto f1 = threadPool.enqueue([]()
	{
		std::cout << "lambda 1\n";
		return 1;
	});
	auto f2 = threadPool.enqueue([]()
	{
		std::cout << "lambda 2\n";
		return 2;
	});
	auto sayWhatRet2 = threadPool.enqueue(std::bind(&sayWhat, 11000));
	std::cout << "f1 type = " << typeid(f1).name() << '\n';
	std::cout << "f2 type = " << typeid(f2).name() << '\n';
	threadPool.enqueue(spitId);
	threadPool.enqueue(&spitId);
	threadPool.enqueue(sayAndNoReturn);
	std::cout << f1.get() + f2.get() << '\n';
	std::cout << sayWhatRet2.get() << '\n';
}
