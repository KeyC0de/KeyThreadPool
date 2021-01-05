#include <iostream>
#include "thread_pool.h"

#include <sstream>
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

	ThreadPool& threadPool = ThreadPool::getInstance(std::thread::hardware_concurrency(), true);
	threadPool.start();

	threadPool.enqueue(spitId);
	threadPool.enqueue(&spitId);
	threadPool.enqueue(sayAndNoReturn);
	
	auto f1 = threadPool.enqueue([]() -> int
	{
		std::cout << "lambda 1\n";
		return 1;
	});
	
	auto sayWhatRet = threadPool.enqueue(sayWhat, 100);
	
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

	auto sayWhatRet2 = threadPool.enqueue(std::bind(&sayWhat,11000));

	//threadPool.enqueue(std::function<void(int)>{Member{}.sayCheese(100)});

	std::cout << "f1 type = " << typeid(f2).name() << '\n';
	
	try
	{
		std::cout << f1.get() << '\n';
		std::cout << f2.get() << '\n';
		std::cout << f3.get() << '\n';
		std::cout << sayWhatRet.get()  << '\n';
		std::cout << sayWhatRet2.get() << '\n';
	}
	catch (const std::exception& ex)
	{
		ex.what();
	}

	std::system( "pause" );
	return EXIT_SUCCESS;
}
