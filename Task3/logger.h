#ifndef DECLARATION_H
#define DECLARATION_H

#define LOG_DEBUG_ON 1
#define LOG_INFO_ON 1
#define LOG_ERROR_ON 1

void logDebug(const char *format, ...) {
	if (LOG_DEBUG_ON == 1) {
		int d; 
  		char *str;
  		va_list factor;
  		va_start(factor, format);

  		for (char *c = format; *c; c++) {
  		    if (*c != '%') {
  		    	fprintf(stdout, "%c", c);
  		        continue;
  		    }

  		    switch (*++c) {
  		        case 'd': 
  		            d = va_arg(factor, int);
  		            fprintf(stdout, "%d", d);
  		            break;
  		        case 's': 
  		            str = va_arg(factor, char*);
  		            fprintf(stdout, "%s", str);
  		            break;
  		        default:
  		            fprintf(stdout, "%c", c);
  		    }
  		}
  		va_end(factor);
	}
}

void logInfo(const char *format, ...) {
	if (LOG_INFO_ON == 1) {
		int d; 
  		char *str;
  		va_list factor;
  		va_start(factor, format);

  		for (char *c = format; *c; c++) {
  		    if (*c != '%') {
  		    	fprintf(stdout, "%c", c);
  		        continue;
  		    }

  		    switch (*++c) {
  		        case 'd': 
  		            d = va_arg(factor, int);
  		            fprintf(stdout, "%d", d);
  		            break;
  		        case 's': 
  		            str = va_arg(factor, char*);
  		            fprintf(stdout, "%s", str);
  		            break;
  		        default:
  		            fprintf(stdout, "%c", c);
  		    }
  		}
  		va_end(factor);
	}
}

void logError(const char *format, ...) {
	if (LOG_ERROR_ON == 1) {
		int d; 
  		char *str;
  		va_list factor;
  		va_start(factor, format);

  		for (char *c = format; *c; c++) {
  		    if (*c != '%') {
  		    	fprintf(stderr, "%c", c);
  		        continue;
  		    }

  		    switch (*++c) {
  		        case 'd': 
  		            d = va_arg(factor, int);
  		            fprintf(stderr, "%d", d);
  		            break;
  		        case 's': 
  		            str = va_arg(factor, char*);
  		            fprintf(stderr, "%s", str);
  		            break;
  		        default:
  		            fprintf(stderr, "%c", c);
  		    }
  		}
  		va_end(factor);
	}
}

#endif