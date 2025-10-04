#pragma once

void initWiFi(long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt);
bool isWiFiConnected();
void handleWiFiReconnection(unsigned long now, unsigned long &lastWiFiAttempt);
void handleNTPRetry(unsigned long now, bool timeSynced, long gmtOffsetSec, int daylightOffsetSec, unsigned long &lastNtpAttempt);