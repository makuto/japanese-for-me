#pragma once

class NotificationsHandler
{
public:
	NotificationsHandler();
	~NotificationsHandler();

	void sendNotification(const char* text);
};
