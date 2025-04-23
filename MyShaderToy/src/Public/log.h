#pragma once

#include <iostream>

#ifdef NDEBUG
    #define debug_log(msg) do {} while(0)
#else
    #define debug_log(msg) std::cout << msg << std::endl
#endif