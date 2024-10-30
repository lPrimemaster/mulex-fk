#include <iostream>
#include <chrono>

// This is GCC only I believe				   
#define ASSERT_THROW(cond) 							   \
if(!(cond)) 			   							   \
{ 						   							   \
	throw std::runtime_error(						   \
		std::string(__FILE__)						   \
		+ ":"										   \
		+ std::to_string(__LINE__)					   \
		+ "@"										   \
		+ std::string(__PRETTY_FUNCTION__) 			   \
	); 						   						   \
}


// https://stackoverflow.com/questions/5419356/is-there-a-way-to-redirect-stdout-stderr-to-a-string
struct cout_redirect
{
    cout_redirect(std::streambuf* new_buffer) : _old(std::cout.rdbuf(new_buffer)) { }
    ~cout_redirect( ) { std::cout.rdbuf(_old); }

private:
    std::streambuf* _old;
};

struct timed_block
{
	timed_block(const std::string& msg = "", bool autostart = true) : _autostart(autostart), _msg(msg)
	{
		if(_autostart) mstart();
	}

	inline void mstart()
	{
		_tp = std::chrono::steady_clock::now();
	}

	inline float mstop()
	{
		auto duration = std::chrono::steady_clock::now() - _tp;
		return roundDuration(duration);
	}

	~timed_block()
	{
		if(_autostart)
		{
			auto ms = mstop();
			if(_msg.empty())
			{
				std::cout << "Block time: " << ms << " ms." << std::endl;
			}
			else
			{
				std::cout << _msg << ": " << ms << " ms." << std::endl;
			}
		}
	}

private:
	std::chrono::time_point<std::chrono::steady_clock> _tp;
	bool _autostart;
	std::string _msg;

	inline float roundDuration(const std::chrono::steady_clock::duration& d)
	{
		using namespace std::chrono;
		// if(duration_cast<milliseconds>(d).count() > 0)
		// {
		// }
		return duration_cast<microseconds>(d).count() / 1000.0f;
		// return 0.0f;
	}
};
