#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

#define TAG "main"

// --------------------------------------------------------------------------------------
// Usage

void
main_usage(const char* exe_name)
{
    fprintf(stdout,
      "Usage:\n"
      "  %s --adc-stream-device <path> [--bind-port [n]] [--bind-host [addr]] [--listen-backlog [n]]\n"
      "     [--max-concurrent-connections [n]] [--webroot [path]]...\n"
      "     [--ws-bind-host [addr]] [--ws-bind-port [n]]\n"
      "\n"
      "Options:\n"
      "  --bind-host [addr]                 Bind host/address (default %s)\n"
      "  --bind-port [n]                    Bind TCP/IP port (default %d)\n"
      "  --listen-backlog [n]               Listen backlog (default %d)\n"
      "  --max-concurrent-connections [n]   Max concurrent clients (default %d)\n"
      "  --webroot [path]                   Static file root; may be repeated (default %s)\n"
      "  --adc-stream-device <path>         Serial device delivering MCU ADC bitstream (e.g. /dev/ttyUSB0)\n"
      "  --ws-bind-host [addr]              WebSocket bind host/address (default %s)\n"
      "  --ws-bind-port [n]                 WebSocket bind TCP/IP port (default %d)\n"
      "  -h, --help                         Show this help\n"
      "\n",
      exe_name,
      HTTP_BIND_HOST_DEFAULT,
      HTTP_BIND_PORT_DEFAULT,
      HTTP_LISTEN_BACKLOG_DEFAULT,
      HTTP_MAX_CONCURRENT_CONNECTIONS_DEFAULT,
      HTTP_STATIC_DIR_DEFAULT,
      WS_BIND_HOST_DEFAULT,
      WS_BIND_PORT_DEFAULT);
}

// --------------------------------------------------------------------------------------
// Config init

void
main_config_init(main_config_t* cfg)
{
    if (cfg == NULL)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->server_config.bind_host = HTTP_BIND_HOST_DEFAULT;
    cfg->server_config.bind_port = HTTP_BIND_PORT_DEFAULT;
    cfg->server_config.listen_backlog = HTTP_LISTEN_BACKLOG_DEFAULT;
    cfg->server_config.max_concurrent_connections = HTTP_MAX_CONCURRENT_CONNECTIONS_DEFAULT;

    cfg->server_config.static_directories = cfg->webroots;
    cfg->server_config.static_directory_count = 0;

    cfg->help_requested = false;

    cfg->webroots[0] = HTTP_STATIC_DIR_DEFAULT;
    cfg->server_config.static_directory_count = 1;

    cfg->ws_bind_host = WS_BIND_HOST_DEFAULT;
    cfg->ws_bind_port = WS_BIND_PORT_DEFAULT;
}

// --------------------------------------------------------------------------------------
// Helpers

static bool
_parse_int_strict(const char* s, int32_t* out)
{
    long v;
    char* end = NULL;

    if (s == NULL || *s == '\0' || out == NULL)
        return false;

    v = strtol(s, &end, 10);

    if (end == s || *end != '\0')
        return false;

    if (v < -2147483648L || v > 2147483647L)
        return false;

    *out = (int32_t)v;
    return true;
}

// --------------------------------------------------------------------------------------

static const char*
_getopt_optional_arg(int argc, char** argv)
{
    if (optarg != NULL)
        return optarg;

    if (optind < argc && argv[optind] != NULL && argv[optind][0] != '-')
        return argv[optind++];

    return NULL;
}

// --------------------------------------------------------------------------------------

static dterr_t*
_fail_badarg(const char* fmt, ...)
{
    dterr_t* dterr = NULL;
    va_list ap;
    char buf[512];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    dterr = dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "%s", buf);
    return dterr;
}

// --------------------------------------------------------------------------------------

static dterr_t*
_fail_range(const char* fmt, ...)
{
    dterr_t* dterr = NULL;
    va_list ap;
    char buf[512];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    dterr = dterr_new(DTERR_RANGE, DTERR_LOC, NULL, "%s", buf);
    return dterr;
}

// --------------------------------------------------------------------------------------

static dterr_t*
_add_static_directory(main_config_t* cfg, const char* path)
{
    dterr_t* dterr = NULL;
    int32_t count = 0;

    DTERR_ASSERT_NOT_NULL(cfg);
    DTERR_ASSERT_NOT_NULL(path);

    if (path[0] == '\0')
    {
        dterr = _fail_badarg("Invalid --webroot value");
        goto cleanup;
    }

    count = cfg->server_config.static_directory_count;

    if (count < 0 || count >= HTTP_MAX_STATIC_DIRECTORIES)
    {
        dterr = _fail_range("Too many --webroot values; max is %d", HTTP_MAX_STATIC_DIRECTORIES);
        goto cleanup;
    }

    cfg->webroots[count] = path;
    cfg->server_config.static_directory_count++;

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------

static dterr_t*
_clear_static_directories(main_config_t* cfg)
{
    dterr_t* dterr = NULL;
    int32_t i = 0;

    DTERR_ASSERT_NOT_NULL(cfg);

    for (i = 0; i < HTTP_MAX_STATIC_DIRECTORIES; i++)
        cfg->webroots[i] = NULL;

    cfg->server_config.static_directory_count = 0;

cleanup:
    return dterr;
}

// --------------------------------------------------------------------------------------

static dterr_t*
_validate_cfg(const char* exe_name, const main_config_t* cfg)
{
    const dthttpd_linux_socket_config_t* s = NULL;

    if (cfg == NULL)
        return dterr_new(DTERR_ARGUMENT_NULL, DTERR_LOC, NULL, "cfg is NULL");

    if (cfg->help_requested)
        return NULL;

    s = &cfg->server_config;

    if (s->bind_host == NULL || s->bind_host[0] == '\0')
    {
        main_usage(exe_name);
        return dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "Invalid --bind-host value");
    }

    if (s->bind_port <= 0 || s->bind_port > 65535)
    {
        main_usage(exe_name);
        return dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "Invalid --bind-port value: %d", s->bind_port);
    }

    if (s->listen_backlog <= 0)
    {
        main_usage(exe_name);
        return dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "Invalid --listen-backlog value: %d", s->listen_backlog);
    }

    if (s->max_concurrent_connections <= 0)
    {
        main_usage(exe_name);
        return dterr_new(
          DTERR_BADARG, DTERR_LOC, NULL, "Invalid --max-concurrent-connections value: %d", s->max_concurrent_connections);
    }

    if (s->static_directory_count <= 0 || s->static_directory_count > HTTP_MAX_STATIC_DIRECTORIES)
    {
        main_usage(exe_name);
        return dterr_new(DTERR_RANGE, DTERR_LOC, NULL, "Invalid static directory count: %d", s->static_directory_count);
    }

    if (cfg->adc_stream_device == NULL || cfg->adc_stream_device[0] == '\0')
    {
        main_usage(exe_name);
        return dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "--adc-stream-device is required");
    }

    if (cfg->ws_bind_host == NULL || cfg->ws_bind_host[0] == '\0')
    {
        main_usage(exe_name);
        return dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "Invalid --ws-bind-host value");
    }

    if (cfg->ws_bind_port <= 0 || cfg->ws_bind_port > 65535)
    {
        main_usage(exe_name);
        return dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "Invalid --ws-bind-port value: %d", cfg->ws_bind_port);
    }

    return NULL;
}

// --------------------------------------------------------------------------------------
// Parser

dterr_t*
main_parse_args(int argc, char** argv, main_config_t* cfg)
{
    dterr_t* dterr = NULL;
    bool saw_static_dir = false;

    static struct option long_options[] = { { "bind-host", optional_argument, 0, 'b' },
        { "bind-port", optional_argument, 0, 'p' },
        { "listen-backlog", optional_argument, 0, 'l' },
        { "max-concurrent-connections", optional_argument, 0, 'm' },
        { "webroot", required_argument, 0, 's' },
        { "adc-stream-device", required_argument, 0, 'u' },
        { "ws-bind-host", optional_argument, 0, 'B' },
        { "ws-bind-port", optional_argument, 0, 'P' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 } };

    DTERR_ASSERT_NOT_NULL(argv);
    DTERR_ASSERT_NOT_NULL(cfg);

    main_config_init(cfg);

    optind = 1;

    for (;;)
    {
        int opt = getopt_long(argc, argv, "b::p::l::m::s:u:B::P::h", long_options, NULL);
        if (opt == -1)
            break;

        switch (opt)
        {
            case 'b':
            {
                const char* s = _getopt_optional_arg(argc, argv);
                if (s != NULL)
                    cfg->server_config.bind_host = s;
                break;
            }

            case 'p':
            {
                const char* s = _getopt_optional_arg(argc, argv);
                if (s != NULL)
                {
                    int32_t v = 0;
                    if (!_parse_int_strict(s, &v))
                    {
                        dterr = _fail_badarg("Invalid --bind-port value: %s", s);
                        goto cleanup;
                    }
                    cfg->server_config.bind_port = v;
                }
                break;
            }

            case 'l':
            {
                const char* s = _getopt_optional_arg(argc, argv);
                if (s != NULL)
                {
                    int32_t v = 0;
                    if (!_parse_int_strict(s, &v))
                    {
                        dterr = _fail_badarg("Invalid --listen-backlog value: %s", s);
                        goto cleanup;
                    }
                    cfg->server_config.listen_backlog = v;
                }
                break;
            }

            case 'm':
            {
                const char* s = _getopt_optional_arg(argc, argv);
                if (s != NULL)
                {
                    int32_t v = 0;
                    if (!_parse_int_strict(s, &v))
                    {
                        dterr = _fail_badarg("Invalid --max-concurrent-connections value: %s", s);
                        goto cleanup;
                    }
                    cfg->server_config.max_concurrent_connections = v;
                }
                break;
            }

            case 's':
            {
                const char* s = optarg;

                if (!saw_static_dir)
                {
                    saw_static_dir = true;
                    DTERR_C(_clear_static_directories(cfg));
                }

                DTERR_C(_add_static_directory(cfg, s));
                break;
            }

            case 'u':
            {
                const char* s = optarg;
                if (s == NULL || s[0] == '\0')
                {
                    dterr = _fail_badarg("Invalid --adc-stream-device value");
                    goto cleanup;
                }
                cfg->adc_stream_device = s;
                break;
            }

            case 'B':
            {
                const char* s = _getopt_optional_arg(argc, argv);
                if (s != NULL)
                    cfg->ws_bind_host = s;
                break;
            }

            case 'P':
            {
                const char* s = _getopt_optional_arg(argc, argv);
                if (s != NULL)
                {
                    int32_t v = 0;
                    if (!_parse_int_strict(s, &v))
                    {
                        dterr = _fail_badarg("Invalid --ws-bind-port value: %s", s);
                        goto cleanup;
                    }
                    cfg->ws_bind_port = v;
                }
                break;
            }

            case 'h':
                cfg->help_requested = true;
                main_usage(argv[0]);
                goto cleanup;

            default:
                main_usage(argv[0]);
                dterr = dterr_new(DTERR_BADARG, DTERR_LOC, NULL, "Bad arguments %c", opt);
                goto cleanup;
        }
    }

    if (optind < argc)
    {
        main_usage(argv[0]);
        dterr = _fail_badarg("Unexpected positional argument: %s", argv[optind]);
        goto cleanup;
    }

    DTERR_C(_validate_cfg(argv[0], cfg));

cleanup:
    return dterr;
}