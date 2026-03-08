/*
 * plugin-main.cpp
 *
 * OBS Studio Plugin: Auto Monitor Only
 *
 * Automatically sets the audio monitoring type of every new audio source
 * to OBS_MONITORING_TYPE_MONITOR_ONLY ("Monitor only (mute output)").
 *
 * Tools → "Auto Monitor Only" opens a dialog with a checkbox to enable/disable.
 * State persists in OBS global config across restarts.
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <callback/signal.h>
#include <callback/calldata.h>
#include <util/config-file.h>

#include <QDialog>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("auto-monitor-only", "en-US")

#define PLUGIN_NAME    "Auto Monitor Only"
#define PLUGIN_VERSION "1.2.0"
#define CONFIG_SECTION "AutoMonitorOnly"
#define CONFIG_KEY     "Enabled"

/* --------------------------------------------------------------------------
 * Enabled state
 * -------------------------------------------------------------------------- */

static bool g_enabled = true;

static config_t *get_config()
{
#if LIBOBS_API_MAJOR_VER >= 31
	return obs_frontend_get_app_config();
#else
	return obs_frontend_get_global_config();
#endif
}

static bool load_enabled()
{
	config_t *cfg = get_config();
	if (!cfg)
		return true;

	config_set_default_bool(cfg, CONFIG_SECTION, CONFIG_KEY, true);
	return config_get_bool(cfg, CONFIG_SECTION, CONFIG_KEY);
}

static void save_enabled(bool enabled)
{
	config_t *cfg = get_config();
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

	if (obs_source_get_monitoring_type(source) ==
	    OBS_MONITORING_TYPE_MONITOR_ONLY)
		return;

	blog(LOG_INFO,
	     "[" PLUGIN_NAME "] Setting '%s' -> Monitor Only (mute output)",
	     obs_source_get_name(source));

	obs_source_set_monitoring_type(source,
				       OBS_MONITORING_TYPE_MONITOR_ONLY);
}

/* --------------------------------------------------------------------------
 * Settings dialog
 * -------------------------------------------------------------------------- */

class AutoMonitorDialog : public QDialog {
public:
	explicit AutoMonitorDialog(QWidget *parent = nullptr)
		: QDialog(parent)
	{
		setWindowTitle(PLUGIN_NAME);
		setFixedSize(320, 120);

		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(16, 16, 16, 16);
		layout->setSpacing(12);

		m_checkbox = new QCheckBox(
			"Automatically set new audio sources to\n"
			"\"Monitor Only (mute output)\"",
			this);
		m_checkbox->setChecked(g_enabled);
		layout->addWidget(m_checkbox);

		/* OK / Cancel buttons */
		auto *btn_layout = new QHBoxLayout();
		btn_layout->addStretch();

		auto *ok = new QPushButton("OK", this);
		auto *cancel = new QPushButton("Cancel", this);
		ok->setDefault(true);

		btn_layout->addWidget(ok);
		btn_layout->addWidget(cancel);
		layout->addLayout(btn_layout);

		connect(ok, &QPushButton::clicked, this, [this]() {
			bool new_state = m_checkbox->isChecked();
			if (new_state != g_enabled) {
				g_enabled = new_state;
				save_enabled(g_enabled);
				blog(LOG_INFO,
				     "[" PLUGIN_NAME "] %s via settings dialog.",
				     g_enabled ? "Enabled" : "Disabled");


			}
			accept();
		});

		connect(cancel, &QPushButton::clicked, this,
			&QDialog::reject);
	}

private:
	QCheckBox *m_checkbox;
};

/* --------------------------------------------------------------------------
 * Tools menu callback
 * -------------------------------------------------------------------------- */

static void on_tools_menu_click(void *private_data)
{
	(void)private_data;

	auto *main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	AutoMonitorDialog dialog(main_window);
	dialog.exec();
}

/* --------------------------------------------------------------------------
 * Global signal: "source_create"
 * -------------------------------------------------------------------------- */

static void on_source_create(void *private_data, calldata_t *cd)
{
	(void)private_data;

	if (!g_enabled)
		return;

	obs_source_t *source = nullptr;
	if (!calldata_get_ptr(cd, "source", &source) || !source)
		return;

	apply_monitor_only(source);
}

/* --------------------------------------------------------------------------
 * Frontend event callback
 * -------------------------------------------------------------------------- */

static void on_frontend_event(enum obs_frontend_event event,
			      void *private_data)
{
	(void)private_data;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		g_enabled = load_enabled();
		blog(LOG_INFO, "[" PLUGIN_NAME "] Loaded — feature is %s.",
		     g_enabled ? "ENABLED" : "DISABLED");
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
		signal_handler_connect(sh, "source_create", on_source_create,
				       nullptr);
		blog(LOG_INFO,
		     "[" PLUGIN_NAME "] Connected to source_create signal.");
	} else {
		blog(LOG_WARNING,
		     "[" PLUGIN_NAME "] Could not get global signal handler.");
	}

	obs_frontend_add_event_callback(on_frontend_event, nullptr);

	obs_frontend_add_tools_menu_item(PLUGIN_NAME, on_tools_menu_click,
					 nullptr);

	blog(LOG_INFO, "[" PLUGIN_NAME "] Loaded successfully.");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[" PLUGIN_NAME "] Unloading...");

	obs_frontend_remove_event_callback(on_frontend_event, nullptr);

	signal_handler_t *sh = obs_get_signal_handler();
	if (sh)
		signal_handler_disconnect(sh, "source_create", on_source_create,
					  nullptr);

	blog(LOG_INFO, "[" PLUGIN_NAME "] Unloaded.");
}

const char *obs_module_description(void)
{
	return "Automatically sets all new audio sources to "
	       "\"Monitor Only (mute output)\". "
	       "Configure via Tools -> " PLUGIN_NAME ".";
}

const char *obs_module_name(void)
{
	return PLUGIN_NAME;
}
