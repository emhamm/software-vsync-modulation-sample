#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <vsyncalter.h>
#include <debug.h>
#include <math.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "unity.h"
#include "file_platform.h"
#include "network_platform.h"
//#include "version.h"

#define TEST_FILE_PATH "/tmp/test_file_platform.txt"
#define TEST_CONTENT "Test content for file platform"

void setUp(void) {
	// Clean up any existing test file
	unlink(TEST_FILE_PATH);
}

void tearDown(void) {
	// Clean up test file
	unlink(TEST_FILE_PATH);
}

// ============================================================================
// FILE PLATFORM TESTS
// ============================================================================

void test_os_open_file_valid(void) {
	file_handle_t handle = os_open_file(TEST_FILE_PATH, O_RDWR_PLATFORM | O_CREAT);
	TEST_ASSERT_NOT_EQUAL(INVALID_FILE_HANDLE, handle);
	TEST_ASSERT_EQUAL(0, os_close_file(handle));
}

void test_os_open_file_null_path(void) {
	file_handle_t handle = os_open_file(NULL, O_RDWR_PLATFORM);
	TEST_ASSERT_EQUAL(INVALID_FILE_HANDLE, handle);
}

void test_os_close_file_valid(void) {
	file_handle_t handle = os_open_file(TEST_FILE_PATH, O_RDWR_PLATFORM | O_CREAT);
	TEST_ASSERT_NOT_EQUAL(INVALID_FILE_HANDLE, handle);
	int result = os_close_file(handle);
	TEST_ASSERT_EQUAL(0, result);
}

void test_os_close_file_invalid(void) {
	int result = os_close_file(INVALID_FILE_HANDLE);
	TEST_ASSERT_EQUAL(-1, result);
}

void test_os_file_exists_valid(void) {
	file_handle_t handle = os_open_file(TEST_FILE_PATH, O_RDWR_PLATFORM | O_CREAT);
	TEST_ASSERT_NOT_EQUAL(INVALID_FILE_HANDLE, handle);
	os_close_file(handle);
	int result = os_file_exists(TEST_FILE_PATH);
	TEST_ASSERT_EQUAL(0, result);
}

void test_os_file_exists_nonexistent(void) {
	int result = os_file_exists("/tmp/nonexistent_file_xyz.txt");
	TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_os_file_exists_null_path(void) {
	int result = os_file_exists(NULL);
	TEST_ASSERT_EQUAL(-1, result);
}

void test_os_get_error_valid(void) {
	char buffer[256];
	const char* error_msg = os_get_error(buffer, sizeof(buffer));
	TEST_ASSERT_NOT_NULL(error_msg);
	TEST_ASSERT_TRUE(strlen(error_msg) > 0);
}

void test_os_get_error_null_buffer(void) {
	const char* error_msg = os_get_error(NULL, 256);
	TEST_ASSERT_EQUAL_STRING("Invalid buffer", error_msg);
}

// ============================================================================
// NETWORK PLATFORM TESTS
// ============================================================================

void test_os_network_init(void) {
	int result = os_network_init();
	TEST_ASSERT_EQUAL(0, result);
}

void test_os_close_socket_valid_tcp(void) {
	os_socket_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
	TEST_ASSERT_NOT_EQUAL(OS_INVALID_SOCKET, sockfd);
	int result = os_close_socket(sockfd);
	TEST_ASSERT_EQUAL(0, result);
}

void test_os_close_socket_valid_udp(void) {
	os_socket_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	TEST_ASSERT_NOT_EQUAL(OS_INVALID_SOCKET, sockfd);
	int result = os_close_socket(sockfd);
	TEST_ASSERT_EQUAL(0, result);
}

void test_os_close_socket_invalid(void) {
	int result = os_close_socket(OS_INVALID_SOCKET);
	TEST_ASSERT_EQUAL(-1, result);
}

void test_os_close_socket_bound_tcp(void) {
	os_socket_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
	TEST_ASSERT_NOT_EQUAL(OS_INVALID_SOCKET, sockfd);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;

	int bind_result = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
	TEST_ASSERT_EQUAL(0, bind_result);

	int result = os_close_socket(sockfd);
	TEST_ASSERT_EQUAL(0, result);
}

// ============================================================================
// GENLOCK LIBRARY TESTS
// ============================================================================

void test_get_phy_name() {

	char name[32];
	int i = 0;
	const char* device_str = find_first_dri_card();
	for (i = 0 ; i < 4; i++) {
		TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, false, false));
		// Check if valid PHY
		if (get_phy_name(i, name, sizeof(name)) == false) {
			TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());
			continue;
		}

		printf("Found %s\n", name);

		// It's a valid PHY on pipe.  Proceed with remaining tests.

		// Test 1: Null buffer
		TEST_ASSERT_FALSE(get_phy_name(i, nullptr, 32));

		// Test 2: Zero size buffer
		TEST_ASSERT_FALSE(get_phy_name(i, name, 0));


		// Simulate uninitialized library
		TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());

		TEST_ASSERT_FALSE(get_phy_name(i, name, sizeof(name)));

		// Simulate valid init but invalid pipe
		TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, false, false));

		TEST_ASSERT_FALSE(get_phy_name(VSYNC_ALL_PIPES, name, sizeof(name)));

		// Simulate valid PHY setup

		TEST_ASSERT_TRUE(get_phy_name(i, name, sizeof(name)));
		TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());

	}
}

void test_synchronize_vsync(void)
{
	int result, pipe;
	char name[32];
	const char* device_str = find_first_dri_card();
	for (pipe = 0 ; pipe < VSYNC_ALL_PIPES; pipe++) {

		TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, false, false));
		set_log_level_str("info");
		// Check if valid PHY
		if (get_phy_name(pipe, name, sizeof(name)) == false) {
			TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());
			continue;
		}

		// Case 1: Library not initialized
		TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());
		// Case: PHY list is null
		result = synchronize_vsync(1.0, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_NOT_EQUAL(0, result);


		TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, false, false));

		set_log_level_str("info");
		result = synchronize_vsync(0.5, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: With stepping
		result = synchronize_vsync(1.5, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: Too huge delta. e.g -50
		result = synchronize_vsync(-50.5, pipe, 0.01, 0.0, 1000, 50, true, true);
		TEST_ASSERT_NOT_EQUAL(0, result);

		// Case: Large shift
		result = synchronize_vsync(1.0, pipe, 4.0, 0.1, 1000, 50, false, false);
		TEST_ASSERT_NOT_EQUAL(0, result);

		// Case: Large shift1
		result = synchronize_vsync(1.0, pipe, 0.01, 4.0, 1000, 50, false, false);
		TEST_ASSERT_NOT_EQUAL(0, result);

		// Case: All pipes
		result = synchronize_vsync(1.0, VSYNC_ALL_PIPES, 0.01, 0.02, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);
	}
}

void test_get_vsync(void)
{
	int result, pipe;
	char name[32];
	uint64_t vsync_array[VSYNC_MAX_TIMESTAMPS];
	const char* device_str = find_first_dri_card();
	for (pipe = 0 ; pipe < VSYNC_ALL_PIPES; pipe++) {
		TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, false, false));

		// Check if valid PHY
		if (get_phy_name(pipe, name, sizeof(name)) == false) {
			TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());
			continue;
		}
		// Case 1: Null array pointer
		result = get_vsync(device_str, nullptr, 5, pipe);
		TEST_ASSERT_NOT_EQUAL(0, result);

		// Case 2: Zero size requested
		memset(vsync_array,0,sizeof(vsync_array));
		result = get_vsync(device_str, vsync_array, 0, pipe);
		TEST_ASSERT_NOT_EQUAL(0, result);
		TEST_ASSERT_EQUAL(0, vsync_array[0]);

				// Case 2: Zero size requested
		memset(vsync_array,0,sizeof(vsync_array));
		result = get_vsync(device_str, vsync_array, 5, pipe);
		TEST_ASSERT_EQUAL(0, result);
		TEST_ASSERT_EQUAL(0, vsync_array[5]);

		// Case 3: Invalid device string (simulate internal error or open failure)
		memset(vsync_array,0,sizeof(vsync_array));
		result = get_vsync("/dev/invalid", vsync_array, 5, pipe);
		TEST_ASSERT_NOT_EQUAL(0, result);
		TEST_ASSERT_EQUAL(0, vsync_array[5]);

		result = get_vsync("", vsync_array, 5, pipe);
		TEST_ASSERT_NOT_EQUAL(0, result);
		TEST_ASSERT_EQUAL(0, vsync_array[5]);

		result = get_vsync(nullptr, vsync_array, 5, pipe);
		TEST_ASSERT_NOT_EQUAL(0, result);
		TEST_ASSERT_EQUAL(0, vsync_array[5]);

		// Case 4: Valid input, single vsync, default pipe
		result = get_vsync(device_str, vsync_array, VSYNC_MAX_TIMESTAMPS, pipe);
		TEST_ASSERT_EQUAL_INT(0, result);
		TEST_ASSERT_NOT_EQUAL(0, vsync_array[VSYNC_MAX_TIMESTAMPS]);
		TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());
	}
}

void test_frequency_set(void) {
	const char* device_str = find_first_dri_card();
	double pll_clock = 0.0;
	double modified_pll_clock = 0.0;
	int ret = 0, pipe;

	set_log_level_str("debug");
	vsync_lib_init(device_str, false, false);
	set_log_level_str("error");
	for (pipe = 0 ; pipe < VSYNC_ALL_PIPES; pipe++) {
		pll_clock = get_pll_clock(pipe);
		if (pll_clock <= 0.0)
			continue;
		modified_pll_clock = pll_clock + ((pll_clock * 0.01) / 100);
		printf("%lf,  %lf\n", pll_clock, modified_pll_clock);
		ret = set_pll_clock(modified_pll_clock, pipe, 0.01, VSYNC_DEFAULT_WAIT_IN_MS);
		TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret, "Failed to set modified PLL clock");
		sleep(1);
		ret = set_pll_clock(pll_clock, pipe, 0.01, VSYNC_DEFAULT_WAIT_IN_MS);
		TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret, "Failed to restore original PLL clock");
	}

	vsync_lib_uninit();
}

void test_get_vblank_interval(void)
{
	int pipe;
	char name[32];
	const char* device_str = find_first_dri_card();
	TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, false, false));
	set_log_level_str("error");
	// get_vblank_interval doesn't need vsync_lib_init.
	// vsync_lib_init is for get_phy_name to get enabled PHYs
	for (pipe = 0 ; pipe < VSYNC_ALL_PIPES; pipe++) {

		// Check if valid PHY
		if (get_phy_name(pipe, name, sizeof(name)) == false) {
			continue;
		}
		double interval = get_vblank_interval(device_str, pipe, 5);
		TEST_ASSERT_TRUE_MESSAGE(interval > 0.0, "Expected interval > 0.0 for valid input");

		// Case: Max stamps
		interval = get_vblank_interval(device_str, pipe, VSYNC_MAX_TIMESTAMPS);
		TEST_ASSERT_TRUE_MESSAGE(interval > 0.0, "Expected interval > 0.0 for valid input");

		// Case: 0 stamps
		interval = get_vblank_interval(device_str, pipe, 0);
		TEST_ASSERT_TRUE_MESSAGE(interval == 0.0, "Expected interval > 0.0 for valid input");

		// Case: 1 stamp
		interval = get_vblank_interval(device_str, pipe, 1);
		TEST_ASSERT_TRUE_MESSAGE(interval == 0.0, "Expected interval > 0.0 for valid input");

		// Case:  Invalid stamp count (higher)
		interval = get_vblank_interval(device_str, pipe, VSYNC_MAX_TIMESTAMPS+1);
		TEST_ASSERT_TRUE_MESSAGE(interval == 0.0, "Expected interval > 0.0 for valid input");

		// Case: Invalid device
		interval = get_vblank_interval("/dev/dri/invalid", pipe, VSYNC_MAX_TIMESTAMPS);
		TEST_ASSERT_TRUE_MESSAGE(interval == 0.0, "Expected interval > 0.0 for valid input");

		// Case: Empty device id
		interval = get_vblank_interval("", pipe, VSYNC_MAX_TIMESTAMPS);
		TEST_ASSERT_TRUE_MESSAGE(interval == 0.0, "Expected interval > 0.0 for valid input");

		// Case: nullptr device Id
		interval = get_vblank_interval(nullptr, pipe, VSYNC_MAX_TIMESTAMPS);
		TEST_ASSERT_TRUE_MESSAGE(interval == 0.0, "Expected interval > 0.0 for valid input");

	}

	TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());
}

void test_drm_info(void)
{
	const char* device_str = find_first_dri_card();
	TEST_ASSERT_EQUAL_INT(0,print_drm_info(device_str));
	TEST_ASSERT_EQUAL_INT(1,print_drm_info("/dev/dri/invalid"));
	TEST_ASSERT_EQUAL_INT(1,print_drm_info(""));
	TEST_ASSERT_EQUAL_INT(1,print_drm_info(nullptr));
}

void test_m_n(void)
{
	int result, pipe;
	char name[32];
	const char* device_str = find_first_dri_card();
	TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, true, false));
		set_log_level_str("info");
	for (pipe = 0 ; pipe < VSYNC_ALL_PIPES; pipe++) {


		// Check if valid PHY
		if (get_phy_name(pipe, name, sizeof(name)) == false) {
			continue;
		}

		if (strcmp(name, "M_N") != 0 ) {
			continue;
		}

		printf("Found %s\n", name);

		set_log_level_str("info");
		result = synchronize_vsync(0.1, pipe, 0.001, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);


		result = synchronize_vsync(0.1, pipe, 0.001, 0.01, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: no commit, no reset
		result = synchronize_vsync(0.1, pipe, 0.001, 0.1, 1000, 50, false, false);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: Too huge delta. e.g -50
		result = synchronize_vsync(-50.5, pipe, 0.01, 0.0, 1000, 50, true, true);
		TEST_ASSERT_NOT_EQUAL(0, result);

		// Case: Large shift
		result = synchronize_vsync(1.0, pipe, 4.0, 0.1, 1000, 50, false, false);
		TEST_ASSERT_NOT_EQUAL(0, result);

		// Case: Large shift1
		result = synchronize_vsync(1.0, pipe, 0.01, 4.0, 1000, 50, false, false);
		TEST_ASSERT_NOT_EQUAL(0, result);

		// Case: All pipes
		result = synchronize_vsync(1.0, VSYNC_ALL_PIPES, 0.01, 0.02, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);
	}

	// Case 1: Library not initialized
	TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());
}


void test_logging(void)
{
	int pipe, result;
	char name[32];
	const char* device_str = find_first_dri_card();
	TEST_ASSERT_EQUAL_INT(0,vsync_lib_init(device_str, false, false));
	// get_vblank_interval doesn't need vsync_lib_init.
	// vsync_lib_init is for get_phy_name to get enabled PHYs
	for (pipe = 0 ; pipe < VSYNC_ALL_PIPES; pipe++) {
		// Check if valid PHY
		if (get_phy_name(pipe, name, sizeof(name)) == false) {
			continue;
		}
		set_log_mode("[UNITTEST]");
		set_log_level_str("none");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case : Level ERROR
		set_log_level_str("error");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case : Level WARNING
		set_log_level_str("warning");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case : Level INFO
		set_log_level_str("info");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case : Level DEBUG
		set_log_level_str("debug");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case : Level TRACE
		set_log_level_str("trace");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case : Level NONE
		set_log_level_str("none");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case : Invalid Level
		set_log_level_str("invalid");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: str Level nullptr
		set_log_level_str(nullptr);
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: str Level error
		set_log_level_str("error");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: str Level warning
		set_log_level_str("warning");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: str Level info
		set_log_level_str("info");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: str Level debug
		set_log_level_str("debug");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);

		// Case: str Level trace
		set_log_level_str("trace");
		result = synchronize_vsync(0.05, pipe, 0.01, 0.1, 1000, 50, true, true);
		TEST_ASSERT_EQUAL_INT(0, result);
	}

	TEST_ASSERT_EQUAL_INT(0,vsync_lib_uninit());

}


int main(int argc, char **argv) {
	UNITY_BEGIN();

	// Check for optional flag
	int run_mn_test = 0;
	if (argc > 1 && strcmp(argv[1], "--run-mn-test") == 0) {
		run_mn_test = 1;
	}

	// Platform abstraction tests
	printf("\n=== File Platform Tests ===\n");
	RUN_TEST(test_os_open_file_valid);
	RUN_TEST(test_os_open_file_null_path);
	RUN_TEST(test_os_close_file_valid);
	RUN_TEST(test_os_close_file_invalid);
	RUN_TEST(test_os_file_exists_valid);
	RUN_TEST(test_os_file_exists_nonexistent);
	RUN_TEST(test_os_file_exists_null_path);
	RUN_TEST(test_os_get_error_valid);
	RUN_TEST(test_os_get_error_null_buffer);

	printf("\n=== Network Platform Tests ===\n");
	RUN_TEST(test_os_network_init);
	RUN_TEST(test_os_close_socket_valid_tcp);
	RUN_TEST(test_os_close_socket_valid_udp);
	RUN_TEST(test_os_close_socket_invalid);
	RUN_TEST(test_os_close_socket_bound_tcp);

	// GenLock library tests
	printf("\n=== GenLock Library Tests ===\n");
	RUN_TEST(test_frequency_set);
	RUN_TEST(test_get_phy_name);
	RUN_TEST(test_synchronize_vsync);
	RUN_TEST(test_get_vsync);
	RUN_TEST(test_get_vblank_interval);
	RUN_TEST(test_drm_info);
	RUN_TEST(test_logging);

	if (run_mn_test) {
		RUN_TEST(test_m_n);
	}

	return UNITY_END();
}
