#pragma once



# include <iostream>

# define DEBUG_LEVEL_ALL (0)
# define DEBUG_LEVEL_INFO (1)
# define DEBUG_LEVEL_WARNING (2)
# define DEBUG_LEVEL_ERROR (3)
# define DEBUG_LEVEL_WTF (4)

#define DEBUG_LEVEL DEBUG_LEVEL_ALL

# define setDebugLevel(new_level) level = new_level

# define debug(debug_data, debug_level)	if ((debug_level) >= DEBUG_LEVEL)	{ if (debug_level >= DEBUG_LEVEL_WARNING) {	std::cout << __FILE__ << " line: " << __LINE__ << " "; } std::cout << debug_data << std::endl; }
