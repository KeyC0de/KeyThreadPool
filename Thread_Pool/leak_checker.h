#pragma once

#ifdef _DEBUG
#	define _CRTDBG_MAP_ALLOC
#	define _CRTDBG_MAP_ALLOC_NEW
#	include <crtdbg.h>
#	include <assert.h>
#endif

bool anyMemoryLeaks()
{
	// confirms integrity of memory blocks allocated by debug heap
	assert(_CrtCheckMemory());
	if (_CrtDumpMemoryLeaks())
	{
		//std::cout << "Memory leaks!\n";
		return true;
	}
	else
	{
		//std::cout << "No leaks\n";
		return false;
	}
}