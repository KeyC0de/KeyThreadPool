#include <cstddef>


class LeakChecker
{
public:
	LeakChecker()
	{
		std::cerr << "Memory leak checker setup" << '\n';
		setupLeakChecker();
	}
	~LeakChecker()
	{
		if (anyMemoryLeaks())
			std::cerr << "Leaking.." << '\n';
		else
			std::cerr << "No leaks. : )\n";
	}
	static inline void setupLeakChecker()
	{
#if _DEBUG
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF || _CRTDBG_REPORT_FLAG || _CRTDBG_LEAK_CHECK_DF);
#endif
		return;
	}
};
LeakChecker leakChecker{};

///======================================================================
/// \class ThreadPool
///
/// \author Nikos Lazaridis (KeyC0de)
/// \date 25/9/2019
/// \brief A class which encapsulates a Pool of threads and dispatches to work upon an incoming callable object
/// \brief This version demands functions with arguments to be passed through the facilities of std::bind
///======================================================================
#define M_ENABLED m_enabled.load(std::memory_order_relaxed)

class ThreadPool final
{
	using Task = std::function<void()>;
public:
	explicit ThreadPool(std::size_t nthreads = std::thread::hardware_concurrency(),
		bool enabled = true)
		: m_enabled{ enabled }
	{
		m_pool.reserve(nthreads);
		if (enabled)
		{
			run();
		}
	}

	~ThreadPool() noexcept
	{
		stop();
	}

	ThreadPool(ThreadPool const&) = delete;
	ThreadPool& operator=(const ThreadPool& rhs) = delete;

	void start()
	{
		if (!M_ENABLED)
		{
			m_enabled.store(true, std::memory_order_relaxed);
			run();
		}
	}

	void stop() noexcept
	{
		if (M_ENABLED)
		{// if already running
			m_enabled.store(false, std::memory_order_relaxed);
			m_cond.notify_all();
			for (auto& t : m_pool)
			{
				if (t.joinable())
				{
					t.join();
				}
			}
		}
	}

	template<typename Callback>
	auto enqueue(Callback f) -> std::future<std::invoke_result_t<Callback>>
	{
		using ReturnType = std::invoke_result_t<Callback>;
		using Wrapped = std::promise<ReturnType>;

		if (M_ENABLED)
		{
			std::shared_ptr<Wrapped> pPromise = std::make_shared<Wrapped>();
			std::future<ReturnType> fu = pPromise->get_future();

			auto myTask =
				[
				pPromise = std::move(pPromise),
				task = std::move(f)]() mutable -> void
			{
				execute(*pPromise, task);
				return;
			};

			{
				std::lock_guard<std::mutex> lg{ m_mu };
				m_tasks.emplace(std::move(myTask));
			}
			m_cond.notify_one();
			return fu;
		}
		else
		{
			throw std::runtime_error("Cannot enqueue tasks in an inactive Thread Pool!");
		}
	}

	inline void enable() noexcept {
		m_enabled.store(true, std::memory_order_relaxed);
	}

	inline void disable() noexcept {
		m_enabled.store(false, std::memory_order_relaxed);
	}

	inline bool isEnabled() const noexcept {
		return M_ENABLED;
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

		auto f = [this]()
		{
			while (true)
			{
				std::unique_lock<std::mutex> lg{ m_mu };
				m_cond.wait(lg, [&] () { return !M_ENABLED || !m_tasks.empty(); });

				if (!M_ENABLED)
					break;

				if (!m_tasks.empty())
				{// there is a task available
					Task task = std::move(m_tasks.front());
					m_tasks.pop();
					lg.unlock();
					task();
				}
			}
			//return;
		};// thread function

		// place threads in the pool
		// and assign them tasks
		for (std::size_t ti = 0; ti < nthreads; ++ti)
		{
			m_pool.emplace_back(std::thread{ std::move(f) });// launch thread
		}
	}

	template<typename ReturnType, typename Callback>
	static void execute(std::promise<ReturnType>& promise, Callback& f)
	{
		promise.set_value(f());
	}

	// specialization for void return type
	template<typename Callback>
	static void execute(std::promise<void>& promise, Callback& f)
	{
		f();
		promise.set_value();
	}

};
/////////////////////////////////////////////////////////////////////////

// Create some work to test the Thread Pool
void spitId()
{
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
	std::cout << "\nthread #" << id << " says " << arg << " and returns... ";
	if (id > 7000)
		return 'X';
	return 'a';
}

struct Member
{
	int i_ = 4;
	void sayCheese(int i)
	{
		std::cout << "CHEESEE!" << '\n';
		std::cout << i + i_ << '\n';
	}
};

int vv() { std::puts("nothing"); return 0; }
int vs(const std::string& str) { std::puts(str.c_str()); return 0; }

int main()
{
	std::cout.sync_with_stdio(false);
	std::locale loc{ "en_US.utf-8" };
	std::locale::global(loc);

	ThreadPool threadPool{ std::thread::hardware_concurrency(), true };
	threadPool.start();

	threadPool.enqueue(spitId);
	threadPool.enqueue(&spitId);
	threadPool.enqueue(sayAndNoReturn);

	auto f1 = threadPool.enqueue([]() -> int
	{
		std::cout << "lambda 1\n";
		return 1;
	});

	auto sayWhatRet = threadPool.enqueue(std::bind(sayWhat, 100));

	Member member{ 1 };
	threadPool.enqueue(std::bind(&Member::sayCheese, member, 100));



	auto f2 = threadPool.enqueue([]()
	{
		std::cout << "lambda 2\n";
		return 2;
	});
	auto f3 = threadPool.enqueue([]()
	{
		return sayWhat(100);
	});

	auto sayWhatRet2 = threadPool.enqueue(std::bind(&sayWhat, 11000));

	//threadPool.enqueue(std::function<void(int)>{Member{}.sayCheese(100)});

	std::cout << "f1 type = " << typeid(f2).name() << '\n';

	try
	{
		std::cout << f1.get() << '\n';
		std::cout << f2.get() << '\n';
		std::cout << f3.get() << '\n';
		std::cout << sayWhatRet.get() << '\n';
		std::cout << sayWhatRet2.get() << '\n';
	}
	catch (const std::exception& ex)
	{
		ex.what();
	}

	return EXIT_SUCCESS;
}
