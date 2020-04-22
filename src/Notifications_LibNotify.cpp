#include "Notifications.hpp"

#include <libnotify/notify.h>

NotificationsHandler::NotificationsHandler()
{
	notify_init("Japanese for Me");
}
NotificationsHandler::~NotificationsHandler()
{
	notify_uninit();
}

void NotificationsHandler::sendNotification(const char* text)
{
	NotifyNotification* notification =
	    notify_notification_new("Japanese for Me", text, "dialog-information");
	notify_notification_show(notification, nullptr);
	g_object_unref(G_OBJECT(notification));
}
