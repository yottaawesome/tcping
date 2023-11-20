#include <string>
import tcping;

const int kDefaultServerPort = 80;
const int HTTP_GET = 0;
const int HTTP_HEAD = 1;
const int HTTP_POST = 2;

struct PingParams
{
	int times_to_ping = 4;
	int offset = 0;  // because I don't feel like writing a whole command line parsing thing, I just want to accept an optional -t.  // well, that got out of hand quickly didn't it? -Future Eli
	double ping_interval = 1;
	int include_timestamp = 0;
	int beep_mode = 0;  // 0 is off, 1 is down, 2 is up, 3 is on change, 4 is constantly
	int ping_timeout = 2000;
	bool blocking = false;
	int relookup_interval = -1;
	int auto_exit_on_success = 0;
	int force_send_byte = 0;

	int include_url = 0;
	int use_http = 0;
	int http_cmd = 0;

	int include_jitter = 0;
	int jitter_sample_size = 0;

	int only_changes = 0;

	// for http mode
	char* serverptr;
	char* docptr = NULL;
	char server[2048];
	char document[2048];

	// for --tee
	char logfile[256];
	int use_logfile = 0;
	int show_arg_header = 0;
	bool tee_mode_append = false;

	// preferred IP version
	int ipv = 0;

	// http proxy server and port
	int proxy_port = 3128;
	char proxy_server[2048]{ 0 };

	char proxy_credentials[2048];
	int using_credentials = 0;

	// Flags for "read from filename" support
	int no_statistics = 0;  // no_statistics flag kills the statistics finale in the cases where we are reading entries from a file
	int reading_from_file = 0;  // setting this flag so we can mangle the other settings against it post parse.  For instance, it moves the meaning of -n and -t
	char urlfile[256];
	int file_times_to_loop = 1;
	bool file_loop_count_was_specific = false;   // ugh, since we are taking over the -n and -t options, but we don't want a default of 4 but we *do* want 4 if they specified 4

	int giveup_count = 0;
	int use_color = 0;

	int use_source_address = 0;
	std::string src_address = "";

	int nPort = kDefaultServerPort;

	int always_print_domain = 0;

	std::string pcHost;
};

extern int DoWinsock_Single(
	PingParams& params,
	tee &out
);

extern int DoWinsock_Multi(
	PingParams& params,
	tee& out
);
