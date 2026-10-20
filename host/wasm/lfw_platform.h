#ifndef LFW_HOST_WASM_PLATFORM_H
#define LFW_HOST_WASM_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus)
typedef char16_t lfw_char16;
#else
typedef uint16_t lfw_char16;
#endif

#define LFW_ABI_VERSION 1u

#define LFW_LOG_LOG 0u
#define LFW_LOG_DEBUG 1u
#define LFW_LOG_WARN 2u
#define LFW_LOG_ERROR 3u

typedef struct LfwPlatform {
  uint32_t abi_version;
  uint32_t struct_size;
  void* user;

  double (*clock_now)(void* user);
  int32_t (*clock_add)(void* user,
                       uint32_t cb_id);
  void (*clock_del)(void* user, int32_t handle);
  int32_t (*clock_hidden)(void* user);
  int32_t (*timeout_add)(void* user, uint32_t cb_id, double ms);
  void (*timeout_del)(void* user, int32_t timer_id);
  int32_t (*render_add)(void* user, uint32_t cb_id);
  void (*render_del)(void* user, int32_t handle);

  void (*log)(void* user, uint32_t level, const lfw_char16* s, uint32_t len);
} LfwPlatform;


uint32_t lfw_abi_version(void);

uint32_t lfw_scratch_ptr(void);
uint32_t lfw_scratch_capacity(void);

int32_t lfw_platform_init(const LfwPlatform* table);

uint32_t lfw_str_put(const lfw_char16* s, uint32_t len);

int32_t lfw_create(uint32_t dev);
int32_t lfw_add_zip(const lfw_char16* memfs_dir, uint32_t len);
int32_t lfw_load(void);
int32_t lfw_boot(uint32_t stage_h, uint32_t fighter_a_h, uint32_t fighter_b_h, uint32_t players,
                 uint32_t vs);
int32_t lfw_frame(double dt_ms);

double lfw_clock_now(void);
uint32_t lfw_frames(void);
uint32_t lfw_entities(void);
uint32_t lfw_ghosts(void);
double lfw_lifetime(void);
double lfw_total_hp(void);
uint32_t lfw_warn_count(void);
uint32_t lfw_error_count(void);
uint32_t lfw_log_count(void);
uint32_t lfw_data_count(uint32_t which);
uint32_t lfw_dump_state(void);
uint32_t lfw_digest(void);
uint32_t lfw_last_error_len(void);

void lfw_invoke(uint32_t cb_id);

int32_t lfw_probe_schedule(double timeout_ms);
uint32_t lfw_probe_hits(uint32_t which);

void lfw_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
