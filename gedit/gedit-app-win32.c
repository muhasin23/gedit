#include "gedit-app-win32.h"

#define SAVE_DATADIR DATADIR
#undef DATADIR
#include <io.h>
#include <conio.h>
#define _WIN32_WINNT 0x0500
#include <windows.h>
#define DATADIR SAVE_DATADIR
#undef SAVE_DATADIR


G_DEFINE_TYPE (GeditAppWin32, gedit_app_win32, GEDIT_TYPE_APP)

static void
gedit_app_win32_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_app_win32_parent_class)->finalize (object);
}

static gchar *
gedit_app_win32_help_link_id_impl (GeditApp    *app,
                                   const gchar *name,
                                   const gchar *link_id)
{
	if (link_id)
	{
		return g_strdup_printf ("http://library.gnome.org/users/gedit/stable/%s",
		                        link_id);
	}
	else
	{
		return g_strdup ("http://library.gnome.org/users/gedit/stable/");
	}
}

static void
gedit_app_win32_class_init (GeditAppWin32Class *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GeditAppClass *app_class = GEDIT_APP_CLASS (klass);

	object_class->finalize = gedit_app_win32_finalize;

	app_class->help_link_id = gedit_app_win32_help_link_id_impl;
}

static void
setup_path (void)
{
	gchar *path;
	gchar *installdir;
	gchar *bin;

	installdir = g_win32_get_package_installation_directory_of_module (NULL);

	bin = g_build_filename (installdir, "bin", NULL);
	g_free (installdir);

	/* Set PATH to include the gedit executable's folder */
	path = g_build_path (";", bin, g_getenv ("PATH"), NULL);
	g_free (bin);

	if (!g_setenv ("PATH", path, TRUE))
	{
		g_warning ("Could not set PATH for gedit");
	}

	g_free (path);
}

static void
prep_console (void)
{
	/* If we open gedit from a console get the stdout printing */
	if (fileno (stdout) != -1 &&
		_get_osfhandle (fileno (stdout)) != -1)
	{
		/* stdout is fine, presumably redirected to a file or pipe */
	}
	else
	{
		typedef BOOL (* WINAPI AttachConsole_t) (DWORD);

		AttachConsole_t p_AttachConsole =
			(AttachConsole_t) GetProcAddress (GetModuleHandle ("kernel32.dll"),
							  "AttachConsole");

		if (p_AttachConsole != NULL && p_AttachConsole (ATTACH_PARENT_PROCESS))
		{
			freopen ("CONOUT$", "w", stdout);
			dup2 (fileno (stdout), 1);
			freopen ("CONOUT$", "w", stderr);
			dup2 (fileno (stderr), 2);
		}
	}
}

static void
gedit_app_win32_init (GeditAppWin32 *self)
{
	setup_path ();
	prep_console ();
}

/* ex:ts=8:noet: */