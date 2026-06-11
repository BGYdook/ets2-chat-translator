/**
 * @file scs_telemetry.h
 * @brief Minimal SCS Telemetry SDK types for ETS2/ATS plugin development.
 *        Extracted from the official SCS SDK v1.12.1
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ======================== Basic Types ========================
typedef unsigned __int8         scs_u8_t;
typedef unsigned __int16        scs_u16_t;
typedef signed __int32          scs_s32_t;
typedef unsigned __int32        scs_u32_t;
typedef unsigned __int64        scs_u64_t;
typedef signed __int64          scs_s64_t;
typedef float                   scs_float_t;
typedef double                  scs_double_t;
typedef const char *            scs_string_t;
typedef void *                  scs_context_t;
typedef scs_u64_t               scs_timestamp_t;
typedef scs_s32_t               scs_result_t;
typedef scs_s32_t               scs_log_type_t;
typedef scs_u32_t               scs_event_t;
typedef scs_u32_t               scs_value_type_t;

// ======================== Calling Convention ========================
#define SCSAPIFUNC              __stdcall
#define SCSAPI_RESULT           scs_result_t SCSAPIFUNC
#define SCSAPI_VOID             void SCSAPIFUNC
#define SCSAPI_RESULT_FPTR(fn)  scs_result_t (SCSAPIFUNC *fn)
#define SCSAPI_VOID_FPTR(fn)    void (SCSAPIFUNC *fn)

// ======================== Result Codes ========================
const scs_result_t SCS_RESULT_ok                = 0;
const scs_result_t SCS_RESULT_unsupported       = -1;
const scs_result_t SCS_RESULT_invalid_parameter = -2;
const scs_result_t SCS_RESULT_generic_error     = -7;

// ======================== Log Types ========================
const scs_log_type_t SCS_LOG_TYPE_message = 0;
const scs_log_type_t SCS_LOG_TYPE_warning = 1;
const scs_log_type_t SCS_LOG_TYPE_error   = 2;

// ======================== Version ========================
#define SCS_MAKE_VERSION(major, minor) (((major) << 16) | (minor))
#define SCS_GET_MAJOR_VERSION(v)       (((v) >> 16) & 0xffff)
#define SCS_GET_MINOR_VERSION(v)       ((v) & 0xffff)

#define SCS_TELEMETRY_VERSION_1_00     SCS_MAKE_VERSION(1, 0)
#define SCS_TELEMETRY_VERSION_1_01     SCS_MAKE_VERSION(1, 1)
#define SCS_TELEMETRY_VERSION_CURRENT  SCS_TELEMETRY_VERSION_1_01

// ======================== Function Pointer Types ========================
typedef SCSAPI_VOID_FPTR(scs_log_t)(const scs_log_type_t type, const scs_string_t message);

typedef SCSAPI_VOID_FPTR(scs_telemetry_event_callback_t)(
    const scs_event_t event, const void *const event_info, const scs_context_t context);

typedef SCSAPI_RESULT_FPTR(scs_telemetry_register_for_event_t)(
    const scs_event_t event, const scs_telemetry_event_callback_t callback, const scs_context_t context);

typedef SCSAPI_RESULT_FPTR(scs_telemetry_unregister_from_event_t)(const scs_event_t event);

typedef SCSAPI_VOID_FPTR(scs_telemetry_channel_callback_t)(
    const scs_string_t name, const scs_u32_t index, const void *const value, const scs_context_t context);

typedef SCSAPI_RESULT_FPTR(scs_telemetry_register_for_channel_t)(
    const scs_string_t name, const scs_u32_t index, const scs_value_type_t type,
    const scs_u32_t flags, const scs_telemetry_channel_callback_t callback, const scs_context_t context);

typedef SCSAPI_RESULT_FPTR(scs_telemetry_unregister_from_channel_t)(
    const scs_string_t name, const scs_u32_t index, const scs_value_type_t type);

// ======================== Init Params Structures ========================
struct scs_sdk_init_params_v100_t
{
    scs_string_t    game_name;
    scs_string_t    game_id;
    scs_u32_t       game_version;
#ifdef _WIN64
    scs_u32_t       _padding;
#endif
    scs_log_t       log;
};

struct scs_telemetry_init_params_t
{
    void method_indicating_this_is_not_a_c_struct(void) {}
};

struct scs_telemetry_init_params_v101_t : public scs_telemetry_init_params_t
{
    scs_sdk_init_params_v100_t              common;
    scs_telemetry_register_for_event_t      register_for_event;
    scs_telemetry_unregister_from_event_t   unregister_from_event;
    scs_telemetry_register_for_channel_t    register_for_channel;
    scs_telemetry_unregister_from_channel_t unregister_from_channel;
};

#ifdef __cplusplus
}
#endif
