/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file external.c
 * \headerfile external.h
 * \brief Implements the 'external proxy' mode of obfsproxy.
 **/

#include "util.h"

#include "container.h"
#include "crypt.h"
#include "network.h"
#include "protocol.h"
#include "managed.h"
#include "main.h"

#include <event2/event.h>

/**
   Launch external proxy.
*/
int
launch_external_proxy(const char *const *begin)
{
  smartlist_t *configs = smartlist_create();
  const char *const *end;


  /* Find the subsets of argv that define each configuration.
     Each configuration's subset consists of the entries in argv from
     its recognized protocol name, up to but not including the next
     recognized protocol name. */
  if (!*begin || !is_supported_protocol(*begin))
    usage();

  do {
    end = begin+1;
    while (*end && !is_supported_protocol(*end))
      end++;
    if (log_do_debug()) {
      smartlist_t *s = smartlist_create();
      char *joined;
      const char *const *p;
      for (p = begin; p < end; p++)
        smartlist_add(s, (void *)*p);
      joined = smartlist_join_strings(s, " ", 0, NULL);
      log_debug("Configuration %d: %s", smartlist_len(configs)+1, joined);
      free(joined);
      smartlist_free(s);
    }
    if (end == begin+1) {
      log_warn("No arguments for configuration %d", smartlist_len(configs)+1);
      usage();
    } else {
      config_t *cfg = config_create(end - begin, begin);
      if (!cfg) {
        smartlist_free(configs);
        return -1; /* diagnostic already issued */
      }
      smartlist_add(configs, cfg);
    }
    begin = end;
  } while (*begin);
  obfs_assert(smartlist_len(configs) > 0);

  /* Configurations have been established; proceed with initialization. */
  obfsproxy_init();

  /* Open listeners for each configuration. */
  SMARTLIST_FOREACH(configs, config_t *, cfg, {
    if (!open_listeners(get_event_base(), cfg)) {
      log_error("Failed to open listeners for configuration %d", cfg_sl_idx+1);
    }
  });

  /* We are go for launch. */
  event_base_dispatch(get_event_base());

  /* Cleanup and exit! */
  obfsproxy_cleanup();

  SMARTLIST_FOREACH(configs, config_t *, cfg, config_free(cfg));
  smartlist_free(configs);

  return 0;
}
