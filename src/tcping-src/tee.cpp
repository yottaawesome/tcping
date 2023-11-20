
#include <fstream>
#include <stdio.h>
#include <stdarg.h>

/*

Don't make fun of me, Unix guys.  I know about `which tee`.  Tee isn't always available on windows systems, however.

*/

class tee
{
public:
	tee();
	~tee(void);
	void Open(char* filename);
	void OpenAppend(char* filename);
	void Close();
	void p(const char* text);
	void pf(const char* format, ...);
	void enable(bool onoff);

private:
	std::ofstream outfile;
	int flag;
	bool enable_output;
};











void tee::pf(const char* format, ...)


void tee::enable(bool onoff)







