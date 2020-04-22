#include "Notifications.hpp"

#include <iostream>

NotificationsHandler::NotificationsHandler(){};
NotificationsHandler::~NotificationsHandler(){};

void NotificationsHandler::sendNotification(const char* text)
{
	std::cout << "Notifications not supported. Check ReadMe for build instructions\n";
	std::cout << text << "\n";
}
