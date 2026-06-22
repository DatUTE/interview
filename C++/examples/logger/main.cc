#include "logger.h"

int main()
{
    int userId = 100;
    double balance = 1234.56;

    LOG_INFO("User=", userId,
             ", balance=", balance);

    LOG_WARN("Disk usage ",
             85,
             "%");

    LOG_ERROR("Cannot connect to ",
              "192.168.1.1");

    return 0;
}