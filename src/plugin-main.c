/*
 * auto-monitor-only.c
 *
 * OBS Studio Plugin: Auto Monitor Only
 *
 * Automatically sets the audio monitoring type of every new audio source
 * to OBS_MONITORING_TYPE_MONITOR_ONLY ("Monitor only (mute output)").
 *
 * A checkbox under Tools → "Auto Monitor Only (Toggle)" toggles the feature.
 * The enabled state is persisted in OBS's global config (global.ini).
 *
 * Build:  See CMakeLists.txt
 * Install: Copy .so/.dll/.dylib to <OBS_PLUGIN_DIR>
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <callback/signal.h>
#include <callback/calldata.h>
#include <util/config-file.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("auto-monitor-only", "en-US")

#define PLUGIN_NAME    "Auto Monitor Only"
#define PLUGIN_VERSION "1.1.0"
#define CONFIG_SECTION "AutoMonitorOnly"
#define CONFIG_KEY     "Enabled"

/* --------------------------------------------------------------------------
 * Enabled state
 *
 * Persisted in OBS's global.ini via the frontend config API.
 * Default: enabled.
 * -------------------------------------------------------------------------- */

static bool g_enabled = true;

static bool load_enabled(void)
{
#if LIBOBS_API_MAJOR_VER >= 31
	config_t *cfg = obs_frontend_get_app_config();
#else
	config_t *cfg = obs_frontend_get_global_config();
#endif
	if (!cfg)
		return true;

	config_set_default_bool(cfg, CONFIG_SECTION, CONFIG_KEY, true);
	return config_get_bool(cfg, CONFIG_SECTION, CONFIG_KEY);
}

static void save_enabled(bool enabled)
{
#if LIBOBS_API_MAJOR_VER >= 31
	config_t *cfg = obs_frontend_get_app_config();
#else
	config_t *cfg = obs_frontend_get_global_config();
#endif
	if (!cfg)
		return;

	config_set_bool(cfg, CONFIG_SECTION, CONFIG_KEY, enabled);
	config_save_safe(cfg, "tmp", NULL);
}

/* --------------------------------------------------------------------------
 * Core logic
 * -------------------------------------------------------------------------- */

static void apply_monitor_only(obs_source_t *source)
{
	if (!source || !g_enabled)
		return;

	uint32_t caps = obs_source_get_output_flags(source);
	if (!(caps & OBS_SOURCE_AUDIO))
		return;

	if (obs_source_get_monitoring_type(source) == OBS_MONITORING_TYPE_MONITOR_ONLY)
		return;

	blog(LOG_INFO, "[" PLUGIN_NAME "] Setting '%s' -> Monitor Only (mute output)", obs_source_get_name(source));

	obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
}

static bool enum_apply_monitor_only(void *param, obs_source_t *source)
{
	(void)param;
	apply_monitor_only(source);
	return true;
}

/* --------------------------------------------------------------------------
 * Tools menu callback
 *
 * Toggles g_enabled and saves to config. Re-enabling sweeps existing sources
 * immediately so nothing is missed while the plugin was off.
 *
 * The C frontend API does not expose a way to update a menu item label or
 * checkmark after creation. The current state is always readable in the OBS
 * log. For a native checkmark, see the Qt approach in README.md.
 * -------------------------------------------------------------------------- */

static void on_tools_menu_click(void *private_data)
{
	(void)private_data;

	g_enabled = !g_enabled;
	save_enabled(g_enabled);

	blog(LOG_INFO, "[" PLUGIN_NAME "] %s via Tools menu.", g_enabled ? "Enabled" : "Disabled");

	if (g_enabled) {
		blog(LOG_INFO, "[" PLUGIN_NAME "] Sweeping existing sources after re-enable.");
		obs_enum_sources(enum_apply_monitor_only, NULL);
	}
}

/* --------------------------------------------------------------------------
 * Global signal: "source_create"
 * -------------------------------------------------------------------------- */

static void on_source_create(void *private_data, calldata_t *cd)
{
	(void)private_data;

	if (!g_enabled)
		return;

	obs_source_t *source = NULL;
	if (!calldata_get_ptr(cd, "source", &source) || !source)
		return;

	apply_monitor_only(source);
}

/* --------------------------------------------------------------------------
 * Frontend event callback
 * -------------------------------------------------------------------------- */

static void on_frontend_event(enum obs_frontend_event event, void *private_data)
{
	(void)private_data;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		g_enabled = load_enabled();
		blog(LOG_INFO, "[" PLUGIN_NAME "] Loaded — feature is %s.", g_enabled ? "ENABLED" : "DISABLED");

		if (g_enabled)
			obs_enum_sources(enum_apply_monitor_only, NULL);
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		if (g_enabled) {
			blog(LOG_INFO, "[" PLUGIN_NAME "] Scene collection changed — re-applying Monitor Only.");
			obs_enum_sources(enum_apply_monitor_only, NULL);
		}
		break;

	default:
		break;
	}
}

/* --------------------------------------------------------------------------
 * Module lifecycle
 * -------------------------------------------------------------------------- */

bool obs_module_load(void)
{
	blog(LOG_INFO, "[" PLUGIN_NAME "] v" PLUGIN_VERSION " loading...");

	signal_handler_t *sh = obs_get_signal_handler();
	if (sh) {
		signal_handler_connect(sh, "source_create", on_source_create, NULL);
		blog(LOG_INFO, "[" PLUGIN_NAME "] Connected to source_create signal.");
	} else {
		blog(LOG_WARNING, "[" PLUGIN_NAME "] Could not get global signal handler.");
	}

	obs_frontend_add_event_callback(on_frontend_event, NULL);

	obs_frontend_add_tools_menu_item(PLUGIN_NAME " (Toggle)", on_tools_menu_click, NULL);

	blog(LOG_INFO, "[" PLUGIN_NAME "] Loaded successfully.");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[" PLUGIN_NAME "] Unloading...");

	obs_frontend_remove_event_callback(on_frontend_event, NULL);

	signal_handler_t *sh = obs_get_signal_handler();
	if (sh)
		signal_handler_disconnect(sh, "source_create", on_source_create, NULL);

	blog(LOG_INFO, "[" PLUGIN_NAME "] Unloaded.");
}

const char *obs_module_description(void)
{
	return "Automatically sets all new audio sources to "
	       "\"Monitor Only (mute output)\". "
	       "Toggle via Tools -> " PLUGIN_NAME " (Toggle).";
}

const char *obs_module_name(void)
{
	return PLUGIN_NAME;
}
