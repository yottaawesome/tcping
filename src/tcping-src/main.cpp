const char* TCPING_VERSION = "0.39";
const char* TCPING_DATE = "Dec 30 2017";

#pragma comment(lib, "Ws2_32.lib")

#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

import tcping;
import std;
import std.compat;

void usage(int argc, char* argv[]) 
{
	std::cout << "--------------------------------------------------------------" << std::endl;
	std::cout << "tcping.exe by Eli Fulkerson " << std::endl;
	std::cout << "Please see http://www.elifulkerson.com/projects/ for updates. " << std::endl;
	std::cout << "--------------------------------------------------------------" << std::endl;
	std::cout << std::endl;
	std::cout << "Usage: " << argv[0] << " [-flags] server-address [server-port]" << std::endl << std::endl;
	std::cout << "Usage (full): " << argv[0] << " [-t] [-d] [-i interval] [-n times] [-w ms] [-b n] [-r times] [-s] [-v] [-j] [-js size] [-4] [-6] [-c] [-g count] [-S source_address] [--file] [--tee filename] [-h] [-u] [--post] [--head] [--proxy-port port] [--proxy-server server] [--proxy-credentials username:password] [-f] server-address " << "[server-port]" << std::endl << std::endl;
	std::cout << " -t     : ping continuously until stopped via control-c" << std::endl;
	std::cout << " -n 5   : for instance, send 5 pings" << std::endl;
	std::cout << " -i 5   : for instance, ping every 5 seconds" << std::endl;
	std::cout << " -w 0.5 : for instance, wait 0.5 seconds for a response" << std::endl;
	std::cout << " -d     : include date and time on each line" << std::endl;
	std::cout << " -b 1   : enable beeps (1 for on-down, 2 for on-up," << std::endl;
	std::cout << "                        3 for on-change, 4 for always)" << std::endl;
	std::cout << " -r 5   : for instance, relookup the hostname every 5 pings" << std::endl;
	std::cout << " -s     : automatically exit on a successful ping"<< std::endl;                  //[Modification 14 Apr 2011 by Michael Bray, mbray@presidio.com]
	std::cout << " -v     : print version and exit" << std::endl;
	std::cout << " -j     : include jitter, using default rolling average"<< std::endl;
	std::cout << " -js 5  : include jitter, with a rolling average size of (for instance) 5." << std::endl;
	std::cout << " --tee  : mirror output to a filename specified after '--tee'" << std::endl;
	std::cout << " --append : Append to the --tee filename rather than overwriting it" << std::endl;
	std::cout << " -4     : prefer ipv4" << std::endl;
	std::cout << " -6     : prefer ipv6" << std::endl;
	std::cout << " -c     : only show an output line on changed state" << std::endl;
	std::cout << " --file : treat the \"server-address\" as a filename instead, loop through file line by line" << std::endl;
	std::cout << "          Note: --file is incompatible with options such as -j and -c as it is looping through different targets" << std::endl;
	std::cout << "          Optionally accepts server-port.  For example, \"example.org 443\" is valid." << std::endl;
	std::cout << "          Alternately, use -p to force a port at command line for everything in the file." << std::endl;
	std::cout << " -g 5   : for instance, give up if we fail 5 times in a row" << std::endl;
	std::cout << " -S _X_ : Specify source address _X_.  Source must be a valid IP for the client computer." << std::endl;
	std::cout << " -p _X_ : Alternate method to specify port" << std::endl;
	std::cout << " --fqdn : Print domain name on each line if available" << std::endl;
	std::cout << " --ansi : Use ANSI color sequences (cygwin)" << std::endl;
	std::cout << " --color: Use Windows color sequences" << std::endl;
	
	std::cout << std::endl << "HTTP Options:" << std::endl;
	std::cout << " -h     : HTTP mode (use url without http:// for server-address)" << std::endl;
	std::cout << " -u     : include target URL on each line" << std::endl;
	std::cout << " --post : use POST rather than GET (may avoid caching)" << std::endl;
	std::cout << " --head : use HEAD rather than GET" << std::endl;
	std::cout << " --proxy-server : specify a proxy server " << std::endl;
	std::cout << " --proxy-port   : specify a proxy port " << std::endl;
	std::cout << " --proxy-credentials : specify 'Proxy-Authorization: Basic' header in format username:password" << std::endl;
	std::cout << std::endl << "Debug Options:" << std::endl;
	std::cout << " -f     : force tcping to send at least one byte" << std::endl;
	std::cout << " --header : include a header with original args and date.  Implied if using --tee." << std::endl;
	std::cout << " --block  : use a 'blocking' socket to connect.  This prevents -w from working and uses the" << std::endl;
	std::cout << "            default timeout (as long as 20 seconds in my case).  However it can detect an actively" << std::endl;
	std::cout << "            refused connection vs a timeout." << std::endl;
	std::cout << std::endl << "\tIf you don't pass server-port, it defaults to " << kDefaultServerPort << "." << std::endl;
}

int main(int argc, char* argv[]) 
{
    // Do we have enough command line arguments?
    if (argc < 2) 
	{
        usage(argc, argv);
        return 1;
    }

	PingParams params{};

	for (int x = 0; x < argc; x++) 
	{
		if (!strcmp(argv[x], "/?") || !strcmp(argv[x], "?") || !strcmp(argv[x], "--help") || !strcmp(argv[x], "-help")) 
		{
			usage(argc, argv);
			return 1;
		}

		if (!strcmp(argv[x], "--proxy-port")) 
		{
			params.proxy_port = atoi(argv[x + 1]);
			params.offset = x + 1;
		}

		if (!strcmp(argv[x], "--proxy-server")) 
		{
			sprintf_s(params.proxy_server, sizeof(params.proxy_server), argv[x + 1]);
			params.offset = x + 1;
		}

		if (!strcmp(argv[x], "--proxy-credentials")) 
		{
			sprintf_s(params.proxy_credentials, sizeof(params.proxy_credentials), argv[x + 1]);
			params.using_credentials = 1;
			params.offset = x + 1;
		}

		// force IPv4
		if (!strcmp(argv[x], "-4")) 
		{
			params.ipv = 4;
			params.offset = x;
		}

		// force IPv6
		if (!strcmp(argv[x], "-6")) 
		{
			params.ipv = 6;
			params.offset = x;
		}

		// ping continuously
		if (!strcmp(argv[x], "-t")) 
		{
			params.times_to_ping = -1;
			params.file_loop_count_was_specific = true;
			params.offset = x;
			std::cout << std::endl << "** Pinging continuously.  Press control-c to stop **" << std::endl;
		}

		// Number of times to ping
		if (!strcmp(argv[x], "-n")) 
		{
			params.times_to_ping = atoi(argv[x + 1]);
			params.file_loop_count_was_specific = true;
			params.offset = x + 1;
		}

		// Give up
		if (!strcmp(argv[x], "-g")) 
		{
			params.giveup_count = atoi(argv[x + 1]);
			params.offset = x + 1;
		}

		// exit on first successful ping
		if (!strcmp(argv[x], "-s")) 
		{
			params.auto_exit_on_success = 1;
			params.offset = x;
		}

		if (!strcmp(argv[x], "--header")) 
		{
			params.show_arg_header = 1;
			params.offset = x;
		}

		if (!strcmp(argv[x], "--block")) 
		{
			params.blocking = true;
			params.offset = x;
		}

		if (!strcmp(argv[x], "-p")) 
		{
			params.nPort = atoi(argv[x + 1]);
			params.offset = x + 1;
		}

		if (!strcmp(argv[x], "--ansi")) 
		{
			params.use_color = 1;
			params.offset = x;
		}

		if (!strcmp(argv[x], "--color")) 
		{
			params.use_color = 2;
			params.offset = x;
		}

		if (!strcmp(argv[x], "--fqdn")) 
		{
			params.always_print_domain = 1;
			params.offset = x;
		}

		// tee to a log file
		if (!strcmp(argv[x], "--tee")) 
		{
			strcpy_s(params.logfile, sizeof(params.logfile), static_cast<const char*>(argv[x + 1]));
			params.offset = x + 1;
			params.use_logfile = 1;
			params.show_arg_header = 1;
		}

		if (!strcmp(argv[x], "--append")) 
		{
			params.tee_mode_append = true;
			params.offset = x;
		}

		// read from a text file
		if (!strcmp(argv[x], "--file")) 
		{
			params.offset = x;
			params.no_statistics = 1;
			params.reading_from_file = 1;
		}

        // http mode
        if (!strcmp(argv[x], "-h")) 
		{
			params.use_http = 1;
			params.offset = x;
        }

        // http mode - use get
        if (!strcmp(argv[x], "--get")) 
		{
			params.use_http = 1; //implied
			params.http_cmd = HTTP_GET;
			params.offset = x;
        }

        // http mode - use head
        if (!strcmp(argv[x], "--head")) 
		{
			params.use_http = 1; //implied
			params.http_cmd = HTTP_HEAD;
			params.offset = x;
        }

        // http mode - use post
        if (!strcmp(argv[x], "--post")) 
		{
			params.use_http = 1; //implied
			params.http_cmd = HTTP_POST;
			params.offset = x;
        }

        // include url per line
        if (!strcmp(argv[x], "-u")) 
		{
			params.include_url = 1;
			params.offset = x;
        }

        // force send a byte
        if (!strcmp(argv[x], "-f")) 
		{
			params.force_send_byte = 1;
			params.offset = x;
        }

        // interval between pings
        if (!strcmp(argv[x], "-i")) 
		{
			params.ping_interval = atof(argv[x+1]);
			params.offset = x+1;
        }

        // wait for response
        if (!strcmp(argv[x], "-w")) 
		{
			params.ping_timeout = (int)(1000 * atof(argv[x + 1]));
			params.offset = x+1;
        }

		// source address
		if (!strcmp(argv[x], "-S")) 
		{
			params.src_address = argv[x + 1];
			params.use_source_address = 1;
			params.offset = x + 1;
		}

        // optional datetimestamp output
        if (!strcmp(argv[x], "-d")) 
		{
			params.include_timestamp = 1;
			params.offset = x;
        }

        // optional jitter output
        if (!strcmp(argv[x], "-j")) 
		{
			params.include_jitter = 1;
			params.offset = x;
		}
     
		// optional jitter output (sample size)
		if (!strcmp(argv[x], "-js")) 
		{
			params.include_jitter = 1;
			params.offset = x;

            // obnoxious special casing if they actually specify the default 0
            if (!strcmp(argv[x+1], "0")) 
			{
				params.jitter_sample_size = 0;
				params.offset = x+1;
            } 
			else 
			{
                if (atoi(argv[x+1]) == 0) 
				{
					params.offset = x;
                } 
				else 
				{
					params.jitter_sample_size = atoi(argv[x+1]);
					params.offset = x+1;
                }
            }
            //			cout << "offset coming out "<< offset << endl;
        }

        // optional hostname re-lookup
        if (!strcmp(argv[x], "-r")) 
		{
			params.relookup_interval = atoi(argv[x+1]);
			params.offset = x+1;
        }
		
		 // optional output minimization
        if (!strcmp(argv[x], "-c")) 
		{
			params.only_changes = 1;
			params.offset = x;
			std::cout << std::endl << "** Only displaying output for state changes. **" << std::endl;
        }

        // optional beepage
        if (!strcmp (argv[x], "-b")) 
		{
			params.beep_mode = atoi(argv[x+1]);
			params.offset = x+1;
            switch (params.beep_mode)
			{
            case 0:
                break;
            case 1:
				std::cout << std::endl << "** Beeping on \"down\" - (two beeps) **" << std::endl;
                break;
            case 2:
				std::cout << std::endl << "** Beeping on \"up\"  - (one beep) **" << std::endl;
                break;
            case 3:
				std::cout << std::endl << "** Beeping on \"change\" - (one beep up, two beeps down) **" << std::endl;
                break;
            case 4:
				std::cout << std::endl << "** Beeping constantly - (one beep up, two beeps down) **" << std::endl;
                break;
            }

        }

        // dump version and quit
        if (!strcmp(argv[x], "-v") || !strcmp(argv[x], "--version")) 
		{
            //cout << "tcping.exe 0.30 Nov 13 2015" << endl;
			std::cout << "tcping.exe " << TCPING_VERSION << " " << TCPING_DATE << std::endl;
			std::cout << "compiled: " << __DATE__ << " " << __TIME__ << std::endl;
			std::cout << std::endl;
			std::cout << "tcping.exe by Eli Fulkerson " << std::endl;
			std::cout << "Please see http://www.elifulkerson.com/projects/ for updates. " << std::endl;
			std::cout << std::endl;
			std::cout << "-s option contributed 14 Apr 2011 by Michael Bray, mbray@presidio.com" << std::endl;
			std::cout << "includes base64.cpp Copyright (C) 2004-2008 René Nyffenegger" << std::endl;
            return 1;
        }
	}

	// open our logfile, if applicable
	tee out;
	if (params.use_logfile == 1 && params.logfile != NULL)
	{
		if (params.tee_mode_append == true)
		{
			out.OpenAppend(params.logfile);
		} 
		else 
		{
			out.Open(params.logfile);
		}
	}

	if (params.show_arg_header == 1)
	{
		out.p("-----------------------------------------------------------------\n");
		// print out the args
		out.p("args: ");
		for (int x = 0; x < argc; x++) 
		{
			out.pf("%s ", argv[x]);
		}
		out.p("\n");

		// and the date

		time_t rawtime;
		struct tm  timeinfo;
		char dateStr[11];
		char timeStr[9];

		errno_t err;

		_strtime_s(timeStr, sizeof(timeStr));

		std::time(&rawtime);

		err = localtime_s(&timeinfo, &rawtime);
		strftime(dateStr, 11, "%Y:%m:%d", &timeinfo);
		out.pf("date: %s %s\n", dateStr, timeStr);

		// and the attrib
		out.pf("tcping.exe v%s: http://www.elifulkerson.com/projects/tcping.php\n", TCPING_VERSION);
		out.p("-----------------------------------------------------------------\n");
	}

	// Get host and (optionally) port from the command line

	//char pcHost[2048] = "";
	
    if (argc >= 2 + params.offset)
	{
		if (!params.reading_from_file)
		{
			params.pcHost = argv[1 + params.offset];
		}
		else 
		{
			strcpy_s(params.urlfile, sizeof(params.urlfile), static_cast<const char*>(argv[params.offset + 1]));
		}
    } 
	else 
	{
		std::cout << "Check the last flag before server-address.  Did you specify a flag and forget its argument?" << std::endl;
		return 1;
    }

	// allow the -p option to win if we set it
    if (argc >= 3 + params.offset && params.nPort == kDefaultServerPort)
	{
		params.nPort = atoi(argv[2 + params.offset]);
    }

    // Do a little sanity checking because we're anal.
    int nNumArgsIgnored = (argc - 3 - params.offset);
    if (nNumArgsIgnored > 0) 
	{
		std::cout << nNumArgsIgnored << " extra argument" << (nNumArgsIgnored == 1 ? "" : "s") << " ignored.  FYI." << std::endl;
    }

    if (params.use_http == 1 && params.reading_from_file == 0)
	{   //added reading from file because if we are doing multiple http this message is just spam.
		params.serverptr = strchr(params.pcHost.data(), ':');
        if (params.serverptr != NULL)
		{
            ++params.serverptr;
            ++params.serverptr;
            ++params.serverptr;
        } 
		else 
		{
			params.serverptr = params.pcHost.data();
        }

		params.docptr = strchr(params.serverptr, '/');
        if (params.docptr != NULL)
		{
            *params.docptr = '\0';
            ++params.docptr;

			strcpy_s(params.server, sizeof(params.server), static_cast<const char*>(params.serverptr));
			strcpy_s(params.document, sizeof(params.document), static_cast<const char*>(params.docptr));
        } 
		else 
		{
			strcpy_s(params.server, sizeof(params.server), static_cast<const char*>(params.serverptr));
			params.document[0] = '\0';
        }

		out.pf("\n** Requesting %s from %s:\n", params.document, params.server);
		out.p("(for various reasons, kbit/s is an approximation)\n");
    }

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Start Winsock up
    WSAData wsaData;
    int nCode;
    if ((nCode = WSAStartup(MAKEWORD(1, 1), &wsaData)) != 0) 
	{
		std::cout << "WSAStartup() returned error code " << nCode << "." << std::endl;
        return 255;
    }

    // Call the main example routine.
	int retval;

	out.p("\n");

	if (!params.reading_from_file)
	{
		retval = DoWinsock_Single(
			params,
			out
		);
	}
	else 
	{
		if (params.file_loop_count_was_specific)
		{
			params.file_times_to_loop = params.times_to_ping;
		}
		params.times_to_ping = 1;
		retval = DoWinsock_Multi(
			params,
			out
		);
	}

    // Shut Winsock back down and take off.
    WSACleanup();
    return retval;
}

