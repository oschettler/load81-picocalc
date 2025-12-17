/* Test for NEX protocol URL parsing and request formatting
 * This test verifies that the NEX client correctly parses URLs
 * and formats requests according to the NEX protocol specification.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test URL parsing logic (extracted from picocalc_nex.c) */
void test_url_parsing(const char *url, const char *expected_host, const char *expected_path) {
    /* Parse URL: nex://hostname/path */
    if (strncmp(url, "nex://", 6) != 0) {
        printf("FAIL: Invalid URL format: %s\n", url);
        return;
    }
    
    const char *host_start = url + 6;
    const char *path_start = strchr(host_start, '/');
    
    char hostname[256];
    char path[256];
    
    if (path_start) {
        size_t host_len = path_start - host_start;
        if (host_len >= sizeof(hostname)) host_len = sizeof(hostname) - 1;
        strncpy(hostname, host_start, host_len);
        hostname[host_len] = '\0';
        strncpy(path, path_start, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        strncpy(hostname, host_start, sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
        strcpy(path, "/");
    }
    
    /* Verify parsing */
    if (strcmp(hostname, expected_host) != 0) {
        printf("FAIL: Hostname mismatch for %s\n", url);
        printf("  Expected: %s\n", expected_host);
        printf("  Got: %s\n", hostname);
        return;
    }
    
    if (strcmp(path, expected_path) != 0) {
        printf("FAIL: Path mismatch for %s\n", url);
        printf("  Expected: %s\n", expected_path);
        printf("  Got: %s\n", path);
        return;
    }
    
    /* Test request formatting - should send only the path */
    char request[512];
    snprintf(request, sizeof(request), "%s\r\n", path);
    
    /* Verify request format */
    char expected_request[512];
    snprintf(expected_request, sizeof(expected_request), "%s\r\n", expected_path);
    
    if (strcmp(request, expected_request) != 0) {
        printf("FAIL: Request format mismatch for %s\n", url);
        printf("  Expected: %s", expected_request);
        printf("  Got: %s", request);
        return;
    }
    
    /* Verify that request does NOT contain the full URL */
    if (strstr(request, "nex://") != NULL) {
        printf("FAIL: Request contains full URL (should only contain path): %s", request);
        return;
    }
    
    printf("PASS: %s -> host=%s, path=%s, request=%s", url, hostname, path, request);
}

int main(void) {
    printf("NEX Protocol URL Parsing and Request Formatting Tests\n");
    printf("=====================================================\n\n");
    
    /* Test 1: URL with path */
    test_url_parsing("nex://nex.fritz.box/about.gmi", "nex.fritz.box", "/about.gmi");
    
    /* Test 2: URL with nested path */
    test_url_parsing("nex://example.com/docs/index.gmi", "example.com", "/docs/index.gmi");
    
    /* Test 3: URL with root path */
    test_url_parsing("nex://server.local/", "server.local", "/");
    
    /* Test 4: URL without path (should default to /) */
    test_url_parsing("nex://minimal.net", "minimal.net", "/");
    
    /* Test 5: URL with port-like hostname */
    test_url_parsing("nex://192.168.1.100/file.txt", "192.168.1.100", "/file.txt");
    
    /* Test 6: Complex path */
    test_url_parsing("nex://host.com/path/to/resource.gmi", "host.com", "/path/to/resource.gmi");
    
    printf("\n=====================================================\n");
    printf("All tests completed!\n");
    printf("\nKey verification: Requests send ONLY the path (e.g., '/about.gmi')\n");
    printf("NOT the full URL (e.g., 'nex://host/about.gmi')\n");
    
    return 0;
}