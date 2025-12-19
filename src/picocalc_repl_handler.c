#include "picocalc_repl_handler.h"
#include "debug.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* REPL request/response structure for inter-core communication */
typedef struct {
    char code[512];
    char output[1024];
    bool complete;
    bool error;
    uint32_t magic;
} repl_message_t;

#define REPL_MAGIC 0x5245504C  /* "REPL" */
#define REPL_TIMEOUT_MS 5000

/* Error message strings */
static const char *repl_error_messages[] = {
    "Success",
    "Timeout waiting for response",
    "Syntax error in Lua code",
    "Runtime error in Lua code",
    "Out of memory",
    "REPL is busy"
};

static volatile bool repl_busy = false;

repl_error_t repl_init(void) {
    /* REPL initialization - multicore FIFO is already initialized by SDK */
    repl_busy = false;
    DEBUG_PRINTF("[REPL] Handler initialized\n");
    return REPL_OK;
}

const char *repl_error_string(repl_error_t error) {
    if (error >= 0 && error < sizeof(repl_error_messages) / sizeof(repl_error_messages[0])) {
        return repl_error_messages[error];
    }
    return "Unknown error";
}

bool repl_is_available(void) {
    return !repl_busy;
}

repl_error_t repl_execute(const char *code, char **output) {
    if (!code || !output) {
        return REPL_ERR_RUNTIME;
    }
    
    if (repl_busy) {
        return REPL_ERR_BUSY;
    }
    
    repl_busy = true;
    
    /* Prepare request message */
    repl_message_t request;
    memset(&request, 0, sizeof(request));
    request.magic = REPL_MAGIC;
    strncpy(request.code, code, sizeof(request.code) - 1);
    request.code[sizeof(request.code) - 1] = '\0';
    request.complete = false;
    request.error = false;
    
    /* Send request to Core 0 via FIFO */
    /* Note: We send the struct in chunks since FIFO is 32-bit */
    uint32_t *data = (uint32_t *)&request;
    size_t words = (sizeof(request) + 3) / 4;
    
    for (size_t i = 0; i < words; i++) {
        multicore_fifo_push_blocking(data[i]);
    }
    
    DEBUG_PRINTF("[REPL] Sent code to Core 0: %.50s...\n", code);
    
    /* Wait for response with timeout */
    absolute_time_t start = get_absolute_time();
    repl_message_t response;
    bool got_response = false;
    
    while (!got_response) {
        /* Check for timeout */
        if (absolute_time_diff_us(start, get_absolute_time()) > REPL_TIMEOUT_MS * 1000) {
            DEBUG_PRINTF("[REPL] Timeout waiting for response\n");
            repl_busy = false;
            return REPL_ERR_TIMEOUT;
        }
        
        /* Check if data available */
        if (multicore_fifo_rvalid()) {
            /* Receive response */
            uint32_t *resp_data = (uint32_t *)&response;
            for (size_t i = 0; i < words; i++) {
                resp_data[i] = multicore_fifo_pop_blocking();
            }
            
            /* Verify magic */
            if (response.magic == REPL_MAGIC && response.complete) {
                got_response = true;
            }
        }
        
        sleep_ms(10);
    }
    
    repl_busy = false;
    
    /* Check for errors */
    if (response.error) {
        /* Allocate output for error message */
        *output = malloc(strlen(response.output) + 1);
        if (*output) {
            strcpy(*output, response.output);
        }
        
        /* Determine error type from message */
        if (strstr(response.output, "syntax")) {
            return REPL_ERR_SYNTAX;
        } else {
            return REPL_ERR_RUNTIME;
        }
    }
    
    /* Success - allocate and copy output */
    *output = malloc(strlen(response.output) + 1);
    if (!*output) {
        return REPL_ERR_NO_MEMORY;
    }
    
    strcpy(*output, response.output);
    DEBUG_PRINTF("[REPL] Got response: %.50s...\n", response.output);
    
    return REPL_OK;
}