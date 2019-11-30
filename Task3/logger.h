#ifndef DECLARATION_H
#define DECLARATION_H

#define LOG_DEBUG_ON 1
#define LOG_INFO_ON 1
#define LOG_ERROR_ON 1

void logDebug(const char *format, ...) {
	if (LOG_DEBUG_ON == 1) {
		fprintf(stdout, format, ...);
	}
}

void logInfo(const char *format, ...) {
	if (LOG_INFO_ON == 1) {
		fprintf(stdout, format, ...);
	}
}

void logError(const char *format, ...) {
	if (LOG_ERROR_ON == 1) {
		fprintf(stderr, format, ...);
	}
}

#endif