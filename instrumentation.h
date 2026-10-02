#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <sys/resource.h>

// Process CPU time (user + system) and the high-water resident set.
// On macOS ru_maxrss is bytes. On Linux it is kilobytes.

struct Usage {
  double cpu_seconds;
  long peak_rss_bytes;
};

inline Usage current_usage() {
  rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
  Usage usage;
  usage.cpu_seconds = static_cast<double>(ru.ru_utime.tv_sec) +
                      static_cast<double>(ru.ru_utime.tv_usec) / 1e6 +
                      static_cast<double>(ru.ru_stime.tv_sec) +
                      static_cast<double>(ru.ru_stime.tv_usec) / 1e6;
#ifdef __APPLE__
  usage.peak_rss_bytes = ru.ru_maxrss;
#else
  usage.peak_rss_bytes = ru.ru_maxrss * 1024L;
#endif
  return usage;
}

#endif
