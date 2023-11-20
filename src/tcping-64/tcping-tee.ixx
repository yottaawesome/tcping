module;

#include <fstream>
#include <stdio.h>
#include <stdarg.h>

export module tcping:tee;

/*
Don't make fun of me, Unix guys.  I know about `which tee`.  Tee isn't always available on windows systems, however.
 */
export 
{
    class tee
    {
        public:
            tee()
            {
                flag = 0;
                enable_output = true;
            }

            ~tee()
            {
                this->Close();
            }

            void Open(char* filename)
            {
                if (flag != 0)
                    outfile.close();
                outfile.open(filename);
                flag = 1;
            }

            void OpenAppend(char* filename)
            {
                if (flag != 0)
                    outfile.close();
                outfile.open(filename, std::ofstream::out | std::ofstream::app);
                flag = 1;
            }

            void Close()
            {
                if (flag != 0)
                    outfile.close();
                flag = 0;
            }

            void p(const char* text)
            {
                if (enable_output == false)
                    return;

                printf(text);
                if (flag == 1) {
                    outfile << text;
                    outfile.flush();
                }
                fflush(stdout);
            }

            void pf(const char* format, ...)
            {
                if (enable_output == false) 
                    return;

                char buffer[256];
                va_list args;
                va_start(args, format);
                //vsprintf(buffer, format, args);
                vsprintf_s(buffer, 256, format, args);


                va_end(args);

                this->p(buffer);
            }

            void enable(bool onoff)
            {
                enable_output = onoff;
            }

        private:
            std::ofstream outfile;
            int flag;
            bool enable_output;
    };
}