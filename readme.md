<h1 align="center">
	<a href="https://github.com/KeyC0de/KeyThreadPool">Key Thread Pool</a>
</h1>
<hr>


I wanted to implement a thread pool to test my understanding of various modern C++ features and multithreading as well as have it to adorn many of my other projects that could benefit from one.

**Design**:

- there is an array of threads - `m_pool`
- there is a FIFO queue of tasks (a `Task` should be a wrapper for a Callable object) - `m_tasks`
- when a client creates a task he `enqueue`()s it into the task's queue
- all threads are started up front in the pool and they never quit running; if there's no task in the queue they wait/sleep.
- every incoming `Task` `notify`ies() (using a condition variable) a single thread to handle it.
- if a `Task` returns a result the client can send a query for its value later
- the `ThreadPool` has a switch On/Off flag to indicate whether it's running
- Thread synchronization: a `condition_variable` along with a mutex variable perform thread synchronization - `m_cond`, `m_mu`
- `m_mu` is used to synchronize access to shared data i.e. the 2 containers
- `m_cond` notifies threads upon task arrival and puts them to sleep when there are no tasks

The key implementation question I asked myself is "how can one create a container of callable objects"? In general a container of generic objects, in order to accomodate functions of any signature. `std::any` came up to my mind, but I thought it would possibly too much, making the entire class a template that is. So I questioned whether I can circumvent this somehow.

And I thought of a bare wrapper `std::function<void()>` could possibly work, as the type of the task queue, as almost all c++ callables can be converted to `an std::function<T>`. But this would be only an indirection, inside this `std::function` I would need to pass up the arguments up front (however many the client supplies). Alternatively I could use my own [**functionRef**](https://github.com/KeyC0de/functionRef).

Then I thought that I also want to provide return values to the client. The only way to do this in C++ in an generic way I know of is a `std::future`. And then I realized I need another level of indirection, a `std::packaged_task` which will wrap the `std::function` which will wrap the call to our target (member) function. And I must also make sure the `packaged_task` lives long enough for it to return the value, ie dynamic allocation through a `std::unique_ptr` or `std::shared_ptr` would be suitable. I decided on a `std::shared_ptr` as `std::unique_ptr` was unfortunately not enough.

After blood and tears spent I'm proud of what it turned out to be.

I used Windows 8.1 x86_64, Visual Studio 2017, C++17 to build the project.


# Contribute

Please submit any bugs you find through GitHub repository 'Issues' page with details describing how to replicate the problem. If you liked it or you learned something new give it a star, clone it, laugh at it, contribute to it whatever. I appreciate all of it. Enjoy.


# License

Distributed under the GNU GPL V3 License. See "GNU GPL license.txt" for more information.


# Contact

email: *nik.lazkey@gmail.com*</br>
website: *www.keyc0de.net*


# Acknowledgements

Thread pool Wikipedia [link](https://en.wikipedia.org/wiki/Thread_pool#:~:text=In%20computer%20programming%2C%20a%20thread,execution%20by%20the%20supervising%20program.) is surprisingly well written.
