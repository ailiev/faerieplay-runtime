#include "processor.h"

#include <pir/common/logging.h>
#include <pir/common/utils-macros.h>


OPEN_NS

Log::logger_t StreamProcessor::logger; // declared extern in processor.h

INSTANTIATE_STATIC_INIT (StreamProcessor);

CLOSE_NS
