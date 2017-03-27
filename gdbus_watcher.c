#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _WatcherClass WatcherClass;
typedef struct _Watcher Watcher;

struct _WatcherClass {
	GObjectClass parent_class;
};
struct _Watcher {
	GObject parent_instance;
	GVariant *hosts;
	GVariant *items;
};
enum {
	PROP_0,
	PROP_HOSTS,
	PROP_ITEMS
};
static Watcher *watcher = NULL;
static GDBusNodeInfo *intro_data = NULL;
static const gchar *watcher_path = "/StatusNotifierWatcher";
static const gchar xml_data[] =
	"<!DOCTYPE node PUBLIC '-//freedesktop//DTD D-BUS Object Introspection 1.0//EN' 'http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd'>"
	"<node>"
	"	<interface name='org.kde.StatusNotifierWatcher'>"
	"		<method name='RegisterStatusNotifierItem'>"
	"			<arg name='service' type='s' direction='in'/>"
	"		</method>"
	"		<method name='RegisterStatusNotifierHost'>"
	"			<arg name='service' type='s' direction='in'/>"
	"		</method>"
	"		<property name='RegisteredStatusNotifierItems' type='as' access='read'>"
	"			<annotation name='org.qtproject.QtDBus.QtTypeName.Out0' value='QStringList'/>"
	"		</property>"
	"		<property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
	"		<property name='ProtocolVersion' type='i' access='read'/>"
	"		<signal name='StatusNotifierItemRegistered'>"
	"			<arg type='s'/>"
	"		</signal>"
	"		<signal name='StatusNotifierItemUnregistered'>"
	"			<arg type='s'/>"
	"		</signal>"
	"		<signal name='StatusNotifierHostRegistered'>"
	"		</signal>"
	"		<signal name='StatusNotifierHostUnregistered'>"
	"		</signal>"
	"	</interface>"
	"</node>";

static GType watcher_get_type (void);
G_DEFINE_TYPE (Watcher, watcher, G_TYPE_OBJECT)

static void watcher_finalize (GObject *object) {
	Watcher *watcher = (Watcher*)object;
	g_variant_unref(watcher->hosts);
	g_variant_unref(watcher->items);
	G_OBJECT_CLASS (watcher_parent_class)->finalize (object);
}

static void watcher_init (Watcher *object) {
	object->hosts = g_variant_new_strv(NULL, 0);
	object->items = g_variant_new_strv(NULL, 0);
}

static void watcher_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
	Watcher *watcher = (Watcher*)object;
	switch (prop_id) {
		case PROP_HOSTS:
			g_value_set_variant(value, watcher->hosts);
			break;
		case PROP_ITEMS:
			g_value_set_variant(value, watcher->items);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void watcher_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
	Watcher *watcher = (Watcher*)object;
	switch (prop_id) {
		case PROP_HOSTS:
			g_variant_unref(watcher->hosts);
			watcher->hosts = g_value_dup_variant(value);
			break;
		case PROP_ITEMS:
			g_variant_unref(watcher->items);
			watcher->items = g_value_dup_variant(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void watcher_class_init (WatcherClass *class) {
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);

	gobject_class->finalize = watcher_finalize;
	gobject_class->set_property = watcher_set_property;
	gobject_class->get_property = watcher_get_property;

	g_object_class_install_property(gobject_class, PROP_HOSTS,
			g_param_spec_variant("hosts", "Hosts", "Hosts", G_VARIANT_TYPE_STRING_ARRAY, NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ITEMS,
			g_param_spec_variant("items", "Items", "Items", G_VARIANT_TYPE_STRING_ARRAY, NULL,
			G_PARAM_READWRITE));
}

static void watcher_remove_from_array(Watcher *watcher, GVariant *prop, const gchar *prop_name, const gchar *name) {
	gsize size;
	gchar **orig = g_variant_dup_strv(prop, &size);
	if(size > 0) {
		const gchar *new[size - 1];
		printf("Removing %s to %s, new size: %lu\n", name, prop_name, size - 1);
		int j = 0;
		for(int i = 0; i < size; i++, j++) {
			if(g_strcmp0(orig[i], name) == 0) {
				if(i < size - 1)
					i++;
				else
					break;
			}
			new[j] = orig[i];
		}
		g_object_set(watcher, prop_name, g_variant_new_strv(new, size - 1), NULL);
		g_free((gchar *) name);
	}
	g_strfreev(orig);

}
static void watcher_add_to_array(Watcher *watcher, GVariant *prop, const gchar *prop_name, const gchar *name) {
	gsize size;
	gchar ** orig = g_variant_dup_strv(prop, &size);
	const gchar *new[size + 1];
	printf("Adding %s to %s, new size: %lu\n", name, prop_name, size + 1);
	for(int i = 0; i < size; i++)
		new[i] = orig[i];
	new[size] = name;
	g_object_set(watcher, prop_name, g_variant_new_strv(new, size + 1), NULL);
	g_strfreev(orig);
}

static void item_appeared_handler(GDBusConnection *c, const gchar *name, const gchar *owner, gpointer user_data) {
	watcher_add_to_array(watcher, watcher->items, "items", user_data);
}

static void item_vanished_handler(GDBusConnection *c, const gchar *name, gpointer user_data) {
	printf("Item %s has vanished\n", name);
	watcher_remove_from_array(watcher, watcher->items, "items", user_data);
	g_dbus_connection_emit_signal(c, NULL, watcher_path, "org.freedesktop.DBus.Properties",
		"StatusNotifierItemUnregistered", g_variant_new("(s)", name), NULL);

}
static void host_appeared_handler(GDBusConnection *c, const gchar *name, const gchar *owner, gpointer user_data) {
	watcher_add_to_array(watcher, watcher->hosts, "hosts", user_data);
}
static void host_vanished_handler(GDBusConnection *c, const gchar *name, gpointer user_data) {
	printf("Host %s has vanished\n", name);
	watcher_remove_from_array(watcher, watcher->hosts, "hosts", user_data);
}
static void handle_method_call(GDBusConnection *c, const gchar *sender, const gchar *obj_path,
		const gchar *int_name, const gchar *method_name, GVariant *param, GDBusMethodInvocation *invoc,
		gpointer user_data) {
	const gchar *tmp;
	g_variant_get(param, "(&s)", &tmp);
	const gchar *service = g_strdup(tmp);

	printf("%s called method '%s', args '%s'\n", sender, method_name, service);
	if(g_strcmp0(method_name, "RegisterStatusNotifierItem") == 0) {
		g_dbus_method_invocation_return_value(invoc, NULL);
		g_dbus_connection_emit_signal(c, NULL, watcher_path, "org.freedesktop.DBus.Properties",
				"StatusNotifierItemRegistered", g_variant_new("(s)", sender),  NULL);
		g_bus_watch_name(G_BUS_TYPE_SESSION, sender,
			G_BUS_NAME_OWNER_FLAGS_NONE, item_appeared_handler, item_vanished_handler, (gpointer) service, NULL);
	}
	else if(g_strcmp0(method_name, "RegisterStatusNotifierHost") == 0) {
		g_dbus_method_invocation_return_value(invoc, NULL);
		g_dbus_connection_emit_signal(c, NULL, watcher_path, "org.freedesktop.DBus.Properties",
				"StatusNotifierHostRegistered", g_variant_new("(s)", sender), NULL);
		g_bus_watch_name(G_BUS_TYPE_SESSION, sender,
			G_BUS_NAME_OWNER_FLAGS_NONE, host_appeared_handler, host_vanished_handler, (gpointer) service, NULL);
	}

}

static GVariant * handle_get_property(GDBusConnection *connection, const gchar *sender, const gchar *obj_path,
		const gchar *int_name, const gchar *prop_name, GError **error, gpointer user_data) {
	GVariant *ret;
	printf("%s requested property '%s'\n", sender, prop_name);
	if(g_strcmp0(prop_name, "RegisteredStatusNotifierItems") == 0) {
		g_object_get(watcher, "items",  &ret, NULL);
		g_variant_ref_sink(ret);
	}
	else if(g_strcmp0(prop_name, "IsStatusNotifierHostRegistered") == 0) {
		gsize size;
		gchar **tmp = g_variant_dup_strv(watcher->hosts, &size);
		g_strfreev(tmp);
		printf("Size: %lu\n", size);
		ret = g_variant_new_boolean(size > 0);
	}
	else if(g_strcmp0(prop_name, "ProtocolVersion") == 0) {
		//TODO find out what this should actually be
		ret = g_variant_new_int32(1);
	}
	return ret;
}

static const GDBusInterfaceVTable int_vtable = {
	handle_method_call,
	handle_get_property,
	NULL
};

static void on_bus_acquired(GDBusConnection *c, const gchar *name, gpointer user_data) {
	guint reg_id;

	printf("Acquired connection to bus\n");
	reg_id = g_dbus_connection_register_object(c, watcher_path, intro_data->interfaces[0], &int_vtable,
			NULL, NULL, NULL);
	g_assert(reg_id > 0);

}
static void on_name_acquired(GDBusConnection *c, const gchar *name, gpointer user_data) {
	printf("Name successfully acquired\n");
}

static void on_name_lost(GDBusConnection *c, const gchar *name, gpointer user_data) {
	printf("Could not register name, Watcher might already exist\n");
	exit(1);
}

int main() {
	guint stat_watch;
	GMainLoop *loop;
	//Is it org.kde or org.freedesktop?
	const gchar name[] = "org.kde.StatusNotifierWatcher";

	intro_data = g_dbus_node_info_new_for_xml(xml_data, NULL);
	g_assert(intro_data != NULL);
	watcher = g_object_new(watcher_get_type(), NULL);

	loop = g_main_loop_new(NULL, FALSE);
	stat_watch = g_bus_own_name(G_BUS_TYPE_SESSION, name, G_BUS_NAME_OWNER_FLAGS_REPLACE, on_bus_acquired,
			on_name_acquired, on_name_lost, NULL, NULL);
	g_main_loop_run(loop);
	g_bus_unown_name(stat_watch);
	g_dbus_node_info_unref(intro_data);
	g_main_loop_unref(loop);
	g_variant_unref(watcher->items);
	g_variant_unref(watcher->hosts);
}
