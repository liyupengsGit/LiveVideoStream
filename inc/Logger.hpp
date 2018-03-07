#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <unistd.h>

#ifdef HAVE_LOG4CPP

#include "log4cpp/Category.hh"
#include "log4cpp/FileAppender.hh"
#include "log4cpp/PatternLayout.hh"

#define LOG(__level)  log4cpp::Category::getRoot() << log4cpp::Priority::__level << __FILE__ << ":" << __LINE__ << "\n\t"

inline void initLogger(log4cpp::Priority::PriorityLevel level)
{
    // initialize log4cpp
    log4cpp::Category &log = log4cpp::Category::getRoot();
    log4cpp::Appender *app = new log4cpp::FileAppender("root", fileno(stderr));

    auto plt = new log4cpp::PatternLayout();
    plt->setConversionPattern("%d [%-6p] - %m%n");
    app->setLayout(plt);
    log.addAppender(app);

    log.setPriority(level);
}

#endif

#endif // LOGGER_HPP

