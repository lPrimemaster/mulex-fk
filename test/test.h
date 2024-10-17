#include <iostream>

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
