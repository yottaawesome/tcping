module;

#include <winsock2.h>
#include <time.h>
#include <stdio.h>
#include <ws2tcpip.h>

export module tcping:core;
import std;
import std.compat;
import :tee;
import :wsutil;
import :base64;
import :pingparams;
import :constants;

namespace
{
	// Prototypes
	SOCKET EstablishConnection(ADDRINFO* address, int ping_timeout, int force_send_byte, ADDRINFO* src_address, int& errcode, bool blocking);
	SOCKET HTTP_EstablishConnection(ADDRINFO* address, ADDRINFO* src_address);

	void formatIP(std::string& abuffer, ADDRINFO* address);

	LARGE_INTEGER cpu_frequency;
	LARGE_INTEGER response_timer1;
	LARGE_INTEGER response_timer2;

	LARGE_INTEGER http_timer1;
	LARGE_INTEGER http_timer2;

	int CTRL_C_ABORT;

	constexpr int BufferSize = 1024;

	constexpr auto NUM_PROBES = 4;

	bool SendHttp(SOCKET sd, char* server, char* document, int http_cmd, int using_proxy, int using_credentials, char* hashed_credentials)
	{
		char message[1024];
		char cmd[5];

		switch (http_cmd)
		{
			case HTTP_GET:
				strcpy_s(cmd, sizeof(cmd), "GET");
				break;
			case HTTP_HEAD:
				strcpy_s(cmd, sizeof(cmd), "HEAD");
				break;
			case HTTP_POST:
				strcpy_s(cmd, sizeof(cmd), "POST");
				break;
		}

		if (document == nullptr)
		{
			document = (char*)"/";
		}

		if (using_credentials == 0)
		{
			if (using_proxy == 0)
			{
				sprintf_s(message, sizeof(message), "%s /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: tcping.exe (www.elifulkerson.com)\r\n\r\n", cmd, document, server);
			}
			else
			{
				sprintf_s(message, sizeof(message), "%s http://%s/%s HTTP/1.1\r\nConnection: close\r\nUser-Agent: tcping.exe (www.elifulkerson.com)\r\n\r\n", cmd, server, document);
			}
		}
		else
		{
			if (using_proxy == 0)
			{
				sprintf_s(message, sizeof(message), "%s /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: tcping.exe (www.elifulkerson.com)\r\nProxy-Authorization: Basic %s\r\n\r\n", cmd, document, server, hashed_credentials);
			}
			else
			{
				sprintf_s(message, sizeof(message), "%s http://%s/%s HTTP/1.1\r\nConnection: close\r\nUser-Agent: tcping.exe (www.elifulkerson.com)\r\nProxy-Authorization: Basic %s\r\n\r\n", cmd, server, document, hashed_credentials);
			}
		}

		const int messageLen = (int)strlen(message);

		// Send the string to the server
		if (send(sd, message, messageLen, 0) != SOCKET_ERROR)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	//// ReadReply /////////////////////////////////////////////////////////
	// Read the reply packet and check it for sanity.  Returns -1 on
	// error, 0 on connection closed, > 0 on success.

	int ReadReply(SOCKET sd, int& bytes_received, int& http_status)
	{
		// Read reply from server
		char acReadBuffer[BufferSize];
		char acTrashBuffer[BufferSize];

		int nTotalBytes = 0;
		int nNewBytes = 0;
		while (1)
		{

			if (nTotalBytes < BufferSize) {
				nNewBytes = recv(sd, acReadBuffer + nTotalBytes, BufferSize - nTotalBytes, 0);
			}
			else {
				nNewBytes = recv(sd, acTrashBuffer, BufferSize, 0);
			}

			if (nNewBytes == SOCKET_ERROR)
			{
				return -1;
			}
			else if (nNewBytes == 0)
			{
				break;
			}

			nTotalBytes += nNewBytes;
		}

		bytes_received = nTotalBytes;

		//parse out the http status from the first line of the response
		char* statusptr;
		char* tmpptr;

		// hop over the initial "HTTP/1.1"
		statusptr = strchr(acReadBuffer, ' ');
		++statusptr;
		tmpptr = strchr(statusptr, ' ');

		// should be at the " " past the error code now
		*tmpptr = '\0';

		http_status = atoi(statusptr);
		return 0;
	}

	void controlc()
	{
		if (CTRL_C_ABORT == 1)
		{
			std::cout.flush();
			std::cout << "Wow, you really mean it.  I'm sorry... I'll stop now. :(" << std::endl;
			exit(1);
		}
		std::cout << "Control-C" << std::endl;
		CTRL_C_ABORT = 1;
	}

	void COLOR_RESET(int use_color)
	{
		if (use_color == 1)
		{
			printf("%c[%dm", 0x1B, 0);
		}

		if (use_color == 2)
		{
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, 7);
		}
	}

	void COLOR_RED(int use_color)
	{
		if (use_color == 1)
		{
			printf("%c[%dm", 0x1B, 31);
		}

		if (use_color == 2)
		{
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, 12);
		}
	}

	int DoWinsock(PingParams& params, tee& out)
	{
		COLOR_RESET(params.use_color);

		char web_server[2048];
		int using_proxy = 0;
		// if we are using a http proxy server, the pcHost needs to be the server address 
		if (params.proxy_server[0] == 0)
		{
			sprintf_s(web_server, sizeof(web_server), "%s", params.pcHost.c_str());
			//web_server[0] = 0;
		}
		else
		{
			sprintf_s(web_server, sizeof(web_server), "%s", params.pcHost.c_str());
			//@@ Fix this later.  sprintf_s wasn't happy, so we went back and disabled the warnings just for this one
			sprintf(params.pcHost.data(), "%s", params.proxy_server);
			params.nPort = params.proxy_port;
			using_proxy = 1;
		}

		char hashed_credentials[2048];
		if (params.using_credentials == 1)
		{
			const std::string s(params.proxy_credentials);
			std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.length());
			sprintf(hashed_credentials, "%s", encoded.c_str());
		}

		SetConsoleCtrlHandler((PHANDLER_ROUTINE)&controlc, TRUE);
		CTRL_C_ABORT = 0;

		int bytes_received = 0;
		int http_status = 0;

		double bps = 0;

		int number_of_pings = 0;	// total number of tcpings issued
		double running_total_ms = 0;	// running total of values of pings... divide by number_of_pings for avg
		double lowest_ping = 50000;		// lowest ping in series ... starts high so it will drop
		double max_ping = 0;			// highest ping in series

		double running_total_ms_http = 0;
		double lowest_ping_http = 50000;
		double max_ping_http = 0;

		int success_counter = 0;
		int failure_counter = 0;
		int deferred_counter = 0;

		int sequential_failure_counter = 0;

		int loopcounter = 0;			// number of probes to send

		// Timestamp variablees
		time_t rawtime;
		//struct tm * timeinfo;
		struct tm  timeinfo;
		char dateStr[11];
		char timeStr[9];

		int beep_flag = -1;  // 0 for we're down, 1 for we're up
		double response_time;
		double http_response_time;

		int have_valid_target = 1;

		// jitter rolling average
		std::vector<double> jitterbuffer(params.jitter_sample_size);
		std::vector<double> http_jitterbuffer(params.jitter_sample_size);

		int jitterpos = 0;
		int success_flag = 0;   // For jitter rolling average we have to remember if we were successful *this* cycle, not just accumulating things
		double j;

		// I'm deliberately using abs() for jitter because negative numbers screw with max/min
		double current_jitter = 0;
		double running_total_abs_jitter = 0;
		double lowest_abs_jitter = 50000;
		double max_abs_jitter = 0;

		double current_http_jitter = 0;
		double running_total_abs_http_jitter = 0;
		double lowest_abs_http_jitter = 50000;
		double max_abs_http_jitter = 0;

		bool last_cycle_success = false;
		int number_same_cycles = 0;

		ADDRINFO hint, * AddrInfo, * AI;
		char p[6];
		int r;
		int found;

		sprintf_s(p, sizeof(p), "%d", params.nPort);
		memset(&hint, 0, sizeof(hint));
		hint.ai_family = PF_UNSPEC;
		hint.ai_socktype = SOCK_STREAM;

		// Find the server's address	
		r = getaddrinfo(params.pcHost.data(), p, &hint, &AddrInfo);

		if (r != 0)
		{
			if (params.relookup_interval == -1)
			{
				COLOR_RED(params.use_color);
				out.pf("DNS: Could not find host - %s, aborting\n", params.pcHost);
				COLOR_RESET(params.use_color);
				return 3;
			}
			else
			{
				have_valid_target = 0;
			}
		}
		found = 0;
		for (AI = AddrInfo; AI != nullptr; AI = AI->ai_next)
		{
			if ((AI->ai_family == AF_UNSPEC && params.ipv == 0) ||
				(AI->ai_family == AF_INET && params.ipv != 6) ||
				(AI->ai_family == AF_INET6 && params.ipv != 4))
			{
				found = 1;
				break;
			}
		}
		if (found == 0)
		{
			if (params.relookup_interval == -1)
			{
				COLOR_RED(params.use_color);
				out.pf("DNS: No valid host found in AddrInfo for that type\n");
				COLOR_RESET(params.use_color);
				return 3;
			}
			else
			{
				have_valid_target = 0;
			}
		}

		// source IP
		ADDRINFO* SRCAI = nullptr;

		if (params.use_source_address != 0)
		{
			r = getaddrinfo(params.src_address.c_str(), nullptr, nullptr, &SRCAI);

			if (r != 0)
			{
				COLOR_RED(params.use_color);
				out.pf("-S:  You specified '%s' as a source address, couldn't do anything with that, aborting.\n", params.src_address);
				COLOR_RESET(params.use_color);
				return 4;
			}

			SOCKET sd = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);

			r = bind(sd, SRCAI->ai_addr, (int)SRCAI->ai_addrlen);

			if (r == SOCKET_ERROR)
			{
				COLOR_RED(params.use_color);
				out.pf("Binding local source address '%s' failed with error %u\n", params.src_address, WSAGetLastError());
				COLOR_RESET(params.use_color);
				return 5;
			}

			closesocket(sd);
		}

		int errorcode_stash = 0;

		while ((loopcounter < params.times_to_ping || params.times_to_ping == -1) && CTRL_C_ABORT == 0)
		{
			success_flag = 0;

			if (((number_of_pings % params.relookup_interval == 0) && (params.relookup_interval != -1) && number_of_pings > 0) || have_valid_target == 0)
			{
				//freeaddrinfo(AddrInfo);   // freeing from the previous cycle @@ don't know if this works this thing still seems to leak
				// Find the server's address
				// Duplicate code here because dealing with resource leaks, getaddrinfo and the
				// differing IPV4 vs IPV6 structures was just obnoxious.
				found = 0;
				r = getaddrinfo(params.pcHost.c_str(), p, &hint, &AddrInfo);

				if (r != 0)
				{
					out.pf("DNS: Could not find host - %s\n", params.pcHost);
					have_valid_target = 0;
				}
				for (AI = AddrInfo; AI != nullptr; AI = AI->ai_next)
				{
					if ((AI->ai_family == AF_UNSPEC && params.ipv == 0) ||
						(AI->ai_family == AF_INET && params.ipv != 6) ||
						(AI->ai_family == AF_INET6 && params.ipv != 4))
					{
						have_valid_target = 1;
						found = 1;
						std::string abuffer;
						formatIP(abuffer, AI);
						out.pf("DNS: %s is %s\n", params.pcHost, abuffer.c_str());
						break;
					}
					if (found == 0)
					{
						out.pf("DNS: No valid host found in AddrInfo for that type\n");
					}
				}
			}

			if (params.include_timestamp == 1)
			{
				errno_t err;
				//_strtime( timeStr );
				_strtime_s(timeStr, sizeof(timeStr));

				std::time(&rawtime);

				//timeinfo = localtime ( &rawtime );
				err = localtime_s(&timeinfo, &rawtime);

				strftime(dateStr, 11, "%Y:%m:%d", &timeinfo);
				out.pf("%s %s ", dateStr, timeStr);
			}

			if (have_valid_target == 1)
			{
				SOCKET sd;

				// apparently... QueryPerformanceCounter isn't thread safe unless we do this
				SetThreadAffinityMask(GetCurrentThread(), 1);

				// start the timer right before we do the connection
				QueryPerformanceFrequency((LARGE_INTEGER*)&cpu_frequency);
				QueryPerformanceCounter((LARGE_INTEGER*)&response_timer1);

				// Connect to the server
				if (params.use_http == 0)
				{
					sd = EstablishConnection(AI, params.ping_timeout, params.force_send_byte, SRCAI, errorcode_stash, params.blocking);
				}
				else
				{
					sd = HTTP_EstablishConnection(AI, SRCAI);
				}

				// grab the timeout as early as possible
				QueryPerformanceCounter((LARGE_INTEGER*)&response_timer2);

				if (sd == INVALID_SOCKET)
				{
					if (params.only_changes == 1)
					{
						if (last_cycle_success == false)
						{
							// no change, so kill the output
							out.enable(false);
							number_same_cycles += 1;
						}
						else
						{
							out.enable(true);
							if (number_of_pings > 0)
							{
								out.pf("(%d successful)\n", number_same_cycles);
							}
							number_same_cycles = 0;
						}
						last_cycle_success = false;
					}
					std::string abuffer;
					formatIP(abuffer, AI);

					COLOR_RED(params.use_color);
					if (params.always_print_domain == 0)
					{
						out.pf("Probing %s:%d/tcp - ", abuffer.c_str(), params.nPort);
					}
					else
					{
						out.pf("Probing %s:%d/tcp - ", params.pcHost, params.nPort);
					}

					//if (WSAGetLastError() != 0) {
					if (errorcode_stash != 0)
					{
						out.p(WSAGetLastErrorMessage("", errorcode_stash).c_str());
					}
					else
					{
						out.p("No response");
					}
					failure_counter++;
					sequential_failure_counter++;

					if (params.beep_mode == 4 || params.beep_mode == 1 || (params.beep_mode == 3 && beep_flag == 1))
					{
						std::cout << " " << char(7) << "*" << char(7) << "*";
					}
					beep_flag = 0;
				}
				else
				{
					if (params.only_changes == 1)
					{
						if (last_cycle_success == true)
						{
							// no change, so kill the output

							out.enable(false);
							number_same_cycles += 1;
						}
						else
						{
							out.enable(true);
							if (number_of_pings > 0)
							{
								out.pf("(%d unsuccessful)\n", number_same_cycles);
							}

							number_same_cycles = 0;
						}
						last_cycle_success = true;
						sequential_failure_counter = 0;
					}

					std::string abuffer;
					formatIP(abuffer, AI);

					if (params.always_print_domain == 0)
					{
						out.pf("Probing %s:%d/tcp - ", abuffer.c_str(), params.nPort);
					}
					else
					{
						out.pf("Probing %s:%d/tcp - ", params.pcHost, params.nPort);
					}

					if (params.use_http == 0)
					{
						out.p("Port is open");
						success_counter++;
						success_flag = 1;
					}
					else
					{
						// consider only incrementing if http response @@
						out.p("HTTP is open");
						success_counter++;
						success_flag = 1;

						// send http send/response
						SetThreadAffinityMask(GetCurrentThread(), 1);

						QueryPerformanceFrequency((LARGE_INTEGER*)&cpu_frequency);
						QueryPerformanceCounter((LARGE_INTEGER*)&http_timer1);

						SendHttp(sd, web_server, params.docptr, params.http_cmd, using_proxy, params.using_credentials, hashed_credentials);
						ReadReply(sd, bytes_received, http_status);
						QueryPerformanceCounter((LARGE_INTEGER*)&http_timer2);
						closesocket(sd);
					}

					if (params.beep_mode == 4 || params.beep_mode == 2 || (params.beep_mode == 3 && beep_flag == 0))
					{
						std::cout << " *" << char(7);
					}
					beep_flag = 1;
				}
				// Shut connection down
				if (ShutdownConnection(sd))
				{
					// room here for connection shutdown success check...
				}
				else
				{
					// room here for connection shutdown failure check...
				}

				response_time = ((double)((response_timer2.QuadPart - response_timer1.QuadPart) * (double)1000.0 / (double)cpu_frequency.QuadPart));
				http_response_time = ((double)((http_timer2.QuadPart - http_timer1.QuadPart) * (double)1000.0 / (double)cpu_frequency.QuadPart));

				out.pf(" - time=%0.3fms ", response_time);

				if (params.use_http == 1)
				{
					if (params.include_url == 1)
					{
						if (params.docptr != nullptr)
						{
							out.pf("page:http://%s/%s ", params.pcHost, params.docptr);
						}
						else
						{
							out.pf("page:http://%s ", params.pcHost);
						}
					}

					out.pf("rcv_time=%0.3f status=%d bytes=%d ", http_response_time, http_status, bytes_received);

					bps = bytes_received * 1000 / http_response_time;
					bps = bps * 8 / 1000;

					out.pf("kbit/s=~%0.3f ", bps);
				}

				// Calculate the statistics...
				number_of_pings++;

				if (sd != INVALID_SOCKET)
				{
					running_total_ms += response_time;

					if (response_time < lowest_ping)
					{
						lowest_ping = response_time;
					}

					if (response_time > max_ping)
					{
						max_ping = response_time;
					}

					if (params.use_http == 1)
					{
						running_total_ms_http += http_response_time;

						if (http_response_time < lowest_ping_http)
						{
							lowest_ping_http = http_response_time;
						}

						if (http_response_time > max_ping_http)
						{
							max_ping_http = http_response_time;
						}
					}
				}

				/*
				  Two ways to measure jitter.  If jitter_sample_size == 0, then its a total/times, non inclusive of the current go.
				  Otherwise, we calculate it based on the prior [jitter_sample_size] values, non inclusive.
				 */

				if (params.include_jitter == 1 && success_counter > 1)
				{
					if (params.jitter_sample_size == 0)
					{
						// we didn't specify a sample size, so no rolling average
						current_jitter = response_time - ((running_total_ms - response_time) / (success_counter - 1));

						//out.pf("jitter=%0.3f ", response_time - ((running_total_ms - response_time) / (success_counter - 1)));
						out.pf("jitter=%0.3f ", current_jitter);

						if (max_abs_jitter < abs(current_jitter))
						{
							max_abs_jitter = abs(current_jitter);
						}

						if (lowest_abs_jitter > abs(current_jitter))
						{
							lowest_abs_jitter = abs(current_jitter);
						}

						running_total_abs_jitter += abs(current_jitter);

						if (params.use_http == 1)
						{
							current_http_jitter = http_response_time - ((running_total_ms_http - http_response_time) / (success_counter - 1));
							//out.pf("rcv_jitter=%0.3f ", http_response_time - ((running_total_ms_http - http_response_time) / (success_counter - 1)));
							out.pf("rcv_jitter=%0.3f ", current_http_jitter);

							if (max_abs_http_jitter < abs(current_http_jitter))
							{
								max_abs_http_jitter = abs(current_http_jitter);
							}

							if (lowest_abs_http_jitter > abs(current_http_jitter))
							{
								lowest_abs_http_jitter = abs(current_http_jitter);
							}

							running_total_abs_http_jitter += abs(current_http_jitter);
						}
					}
					else
					{

						j = 0;

						for (int x = 0; x < min(params.jitter_sample_size, success_counter - 1); x++)
						{
							j = j + jitterbuffer[x];
						}
						current_jitter = response_time - (j / min(success_counter - 1, params.jitter_sample_size));
						//out.pf("jitter=%0.3f ", response_time - (j / min(success_counter - 1, jitter_sample_size)));
						out.pf("jitter=%0.3f ", current_jitter);

						if (max_abs_jitter < abs(current_jitter))
						{
							max_abs_jitter = abs(current_jitter);
						}

						if (lowest_abs_jitter > abs(current_jitter))
						{
							lowest_abs_jitter = abs(current_jitter);
						}

						running_total_abs_jitter += abs(current_jitter);

						if (params.use_http == 1)
						{
							j = 0;
							for (int x = 0; x < min(params.jitter_sample_size, success_counter - 1); x++)
							{
								j = j + http_jitterbuffer[x];
							}
							current_http_jitter = http_response_time - (j / min(success_counter - 1, params.jitter_sample_size));
							//out.pf("rcv_jitter=%0.3f ", http_response_time - (j / min(success_counter - 1, jitter_sample_size)));
							out.pf("rcv_jitter=%0.3f ", current_http_jitter);

							if (max_abs_http_jitter < abs(current_http_jitter))
							{
								max_abs_http_jitter = abs(current_http_jitter);
							}

							if (lowest_abs_http_jitter > abs(current_http_jitter))
							{
								lowest_abs_http_jitter = abs(current_http_jitter);
							}

							running_total_abs_http_jitter += abs(current_http_jitter);
						}
					}
				}

				if (success_flag == 1 && params.jitter_sample_size > 0)
				{
					jitterbuffer[jitterpos] = response_time;
					http_jitterbuffer[jitterpos] = http_response_time;
					jitterpos++;

					// simple rolling average - go back to the beginning of the array once we fill it
					if (jitterpos == params.jitter_sample_size)
					{
						jitterpos = 0;
					}
				}

				COLOR_RESET(params.use_color);
				out.p("\n");

				loopcounter++;
				if ((loopcounter == params.times_to_ping) || ((params.auto_exit_on_success == 1) && (success_counter > 0)))
				{
					break;
				}

				if (sequential_failure_counter >= params.giveup_count && params.giveup_count != 0)
				{
					break;
				}

			}
			else
			{
				// no valid target
				response_time = 0;
				deferred_counter++;
				out.p("No host to ping.\n");
			}

			int zzz = 0;
			double wakeup = (params.ping_interval * 1000) - response_time;
			if (wakeup > 0)
			{
				while (zzz < wakeup && CTRL_C_ABORT == 0)
				{
					Sleep(10);
					zzz += 10;
				}
			}
		}

		out.enable(true);

		if (!params.no_statistics)
		{
			std::string abuffer;
			if (have_valid_target == 1)
			{
				formatIP(abuffer, AI);
			}
			else
			{
				// if we have a bouncing DNS host, we don't have an IP to format correctly, so just spit out what they gave us as an argument...
				abuffer = params.pcHost;
			}

			out.pf("\nPing statistics for %s:%d\n", abuffer.c_str(), params.nPort);
			out.pf("     %d probes sent. \n", number_of_pings);

			float fail_percent = 100 * (float)failure_counter / ((float)success_counter + (float)failure_counter);

			// What is this?  Its quadruple %%%% because we are passing through printf *twice*
			out.pf("     %d successful, %d failed.  (%0.2f%%%% fail)\n", success_counter, failure_counter, fail_percent);

			if (deferred_counter > 0)
			{
				out.pf("     %d skipped due to failed DNS lookup.\n", deferred_counter);
			}
			if (success_counter > 0)
			{
				if (failure_counter > 0)
				{
					out.p("Approximate trip times in milli-seconds (successful connections only):\n");
				}
				else
				{
					out.p("Approximate trip times in milli-seconds:\n");
				}

				out.pf("     Minimum = %0.3fms, Maximum = %0.3fms, Average = %0.3fms\n", lowest_ping, max_ping, running_total_ms / success_counter);

				if (params.use_http == 1)
				{
					out.p("Approximate download times in milli-seconds:\n");
					out.pf("     Minimum = %0.3fms, Maximum = %0.3fms, Average = %0.3fms\n", lowest_ping_http, max_ping_http, running_total_ms_http / success_counter);
				}

				if (params.include_jitter && success_counter > 1)
				{
					out.p("Jitter:\n");
					out.pf("     Minimum = %0.3fms, Maximum = %0.3fms, Average = %0.3fms\n", lowest_abs_jitter, max_abs_jitter, running_total_abs_jitter / (success_counter - 1));
					if (params.use_http)
					{
						out.p("HTTP response jitter:\n");
						out.pf("     Minimum = %0.3fms, Maximum = %0.3fms, Average = %0.3fms\n", lowest_abs_http_jitter, max_abs_http_jitter, running_total_abs_http_jitter / (success_counter - 1));
					}
				}
			}
			else
			{
				out.p("Was unable to connect, cannot provide trip statistics.\n");
			}
		}
		freeaddrinfo(AddrInfo);

		// report our total, abject failure.
		if (success_counter == 0)
		{
			return 1;
		}

		// return our intermittent failure
		if (success_counter > 0 && failure_counter > 0)
		{
			return 2;
		}

		return 0;
	}

	//// EstablishConnection ///////////////////////////////////////////////
	// Connects to a given address, on a given port, both of which must be
	// in network byte order.  Returns newly-connected socket if we succeed,
	// or INVALID_SOCKET if we fail.

	SOCKET EstablishConnection(ADDRINFO* address, int ping_timeout, int force_send_byte, ADDRINFO* src_address, int& errorcode, bool blocking)
	{
		LARGE_INTEGER timer1;
		LARGE_INTEGER timer2;
		LARGE_INTEGER cpu_freq;

		double time_so_far;

		// Create a stream socket
		SOCKET sd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		//-------------------------
		// Set the socket I/O mode: In this case FIONBIO
		// enables or disables the blocking mode for the
		// socket based on the numerical value of iMode.
		// If iMode = 0, blocking is enabled;
		// If iMode != 0, non-blocking mode is enabled.
		u_long iMode = 1;

		// ok - the -w option, if enabled, will make this other than -1.  So this leaves us on blocking mode
		// ... which will just use the default timeout length.  BUT, since its blocking mode, we can get a useful
		// result from the connect() function, which means we can detect 10061 - connection refused vs a timeout.
		if (blocking == true)
		{
			//cout << " no timeout @@ " << endl;
			iMode = 0;
		}
		//u_long iMode = 0;
		ioctlsocket(sd, FIONBIO, &iMode);

		int r;
		if (src_address != nullptr)
		{
			r = bind(sd, src_address->ai_addr, (int)src_address->ai_addrlen);
		}

		QueryPerformanceFrequency((LARGE_INTEGER*)&cpu_freq);
		QueryPerformanceCounter((LARGE_INTEGER*)&timer1);

		int conResult = -999;
		conResult = connect(sd, address->ai_addr, (int)address->ai_addrlen);

		if (conResult == SOCKET_ERROR && iMode == 0)
		{
			//cout << "result " << conResult << " current error: " << WSAGetLastError() << endl;
			errorcode = WSAGetLastError();
			closesocket(sd);
			return INVALID_SOCKET;
		}

		char* sendy = (char*)".";
		int size = 1;
		int sendstatus = 1000;

		bool done = false;

		while (!done && !CTRL_C_ABORT)
		{
			if (force_send_byte == 0)
			{
				sendstatus = send(sd, nullptr, 0, 0);   // should return 0 below
			}
			else
			{
				sendstatus = send(sd, sendy, size, 0);   // should return sizeof(sendy) below
			}

			// one error code is if you send a send of size 0, the other is if you actually send data.
			if (sendstatus == size && force_send_byte == 1)
			{
				closesocket(sd);
				errorcode = WSAGetLastError();
				return sd;
			}

			if (sendstatus == 0 && force_send_byte == 0)
			{
				closesocket(sd);
				errorcode = WSAGetLastError();
				return sd;
			}

			QueryPerformanceCounter((LARGE_INTEGER*)&timer2);

			time_so_far = ((double)((timer2.QuadPart - timer1.QuadPart) * (double)1000.0 / (double)cpu_freq.QuadPart));

			if (time_so_far >= ping_timeout)
			{
				done = true;
			}
			else if (time_so_far < 200)
			{  // the idea here is to not grind the processor too hard if the precision gained isn't going to be useful.
				Sleep(0);
			}
			else
			{
				Sleep(1);
			}
		}

		closesocket(sd);
		errorcode = WSAGetLastError();
		return INVALID_SOCKET;
	}

	SOCKET HTTP_EstablishConnection(ADDRINFO* address, ADDRINFO* src_address)
	{
		// Create a stream socket

		SOCKET sd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		// would be nice to limit the huge timeout in cases where the tcp connection times out (as opposed
		// to bouncing off a closed port, but this stuff doesn't work for some reason.
		/*int timeout = 10;
		int tosize = sizeof(timeout);
		setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO , (char*)&timeout, (int)&tosize);
		setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO , (char*)&timeout, (int)&tosize);
		*/

		int r;
		if (src_address != nullptr)
		{
			r = bind(sd, src_address->ai_addr, (int)src_address->ai_addrlen);
			// temporary
			//if (r == SOCKET_ERROR) {
	//			wprintf(L"bind failed with error %u\n", WSAGetLastError());
			//}
			//else {
	//			wprintf(L"bind returned success\n");
			//}
		}

		if (sd != INVALID_SOCKET)
		{
			if (connect(sd, address->ai_addr, (int)address->ai_addrlen) == SOCKET_ERROR)
			{
				sd = INVALID_SOCKET;
			}
		}

		return sd;
	}

	void formatIP(std::string& abuffer, ADDRINFO* address)
	{
		char buffer[46];
		//char* buffer = new char[46];
		DWORD bufferlen = 46;
		DWORD ret;

		switch (address->ai_family)
		{
		case AF_INET:
			struct sockaddr_in* sockaddr_ipv4;
			sockaddr_ipv4 = (struct sockaddr_in*)address->ai_addr;
			//sprintf(buffer, inet_ntoa(sockaddr_ipv4->sin_addr));
			sprintf_s(buffer, sizeof(buffer), inet_ntoa(sockaddr_ipv4->sin_addr));
			break;

		case AF_INET6:
			// inet_ntop is not available on XP, do not use
			//inet_ntop(address->ai_family, address->ai_addr, buffer, bufferlen);

			ret = getnameinfo(address->ai_addr, (int)address->ai_addrlen, buffer, sizeof(buffer), nullptr, 0, NI_NUMERICHOST);
			break;
		}
		abuffer = buffer;
	}
}

export
{
	int DoWinsock_Single(PingParams& params, tee& out)
	{
		return DoWinsock(params, out);
	}

	int DoWinsock_Multi(PingParams& params, tee& out)
	{
		int retval;

		int count = 0;
		while (count < params.file_times_to_loop || params.file_times_to_loop == -1)
		{
			std::ifstream input(params.urlfile);
			std::string line;

			//while (std::getline(input, line))
			while (std::getline(input, line))
			{
				std::stringstream ss(line);
				std::string line_ip;
				int line_port;

				if (ss >> line_ip)
				{
					if (ss >> line_port)
					{
						//out.p("success");
					}
					else
					{
						line_port = params.nPort;
					}
				}
				else
				{
					break;
				}

				char pcHost_f[255];
				//strcpy_s(pcHost_f, sizeof(pcHost_f), line.c_str());
				strcpy_s(pcHost_f, sizeof(pcHost_f), line_ip.c_str());

				params.nPort = line_port;

				retval = DoWinsock(params, out);

				int zzz = 0;
				double wakeup = (params.ping_interval * 1000);
				if (wakeup > 0)
				{
					while (zzz < wakeup && CTRL_C_ABORT == 0)
					{
						Sleep(10);
						zzz += 10;
					}
					if (CTRL_C_ABORT == 1)
					{  // need to be explicit here since breaking the ping loop on an individual host doesn't return in multi mode
						return retval;
					}
				}
			}
			count++;
		}
		return retval;
	}
}
