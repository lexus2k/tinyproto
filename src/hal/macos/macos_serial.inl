/*
    Copyright 2017-2022 (C) Alexey Dynda

    This file is part of Tiny Protocol Library.

    GNU General Public License Usage

    Protocol Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Protocol Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Protocol Library.  If not, see <http://www.gnu.org/licenses/>.

    Commercial License Usage

    Licensees holding valid commercial Tiny Protocol licenses may use this file in
    accordance with the commercial license agreement provided in accordance with
    the terms contained in a written agreement between you and Alexey Dynda.
    For further information contact via email on github account.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <IOKit/serial/ioss.h>

#define DEBUG_SERIAL 0
#define DEBUG_SERIAL_TX DEBUG_SERIAL
#define DEBUG_SERIAL_RX DEBUG_SERIAL

// Helper function to convert numeric baud rate to speed_t constant
static speed_t baud_to_speed(uint32_t baud)
{
    switch(baud) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B500000
        case 500000: return B500000;
#endif
#ifdef B576000
        case 576000: return B576000;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
#ifdef B1000000
        case 1000000: return B1000000;
#endif
#ifdef B1152000
        case 1152000: return B1152000;
#endif
#ifdef B1500000
        case 1500000: return B1500000;
#endif
#ifdef B2000000
        case 2000000: return B2000000;
#endif
#ifdef B2500000
        case 2500000: return B2500000;
#endif
#ifdef B3000000
        case 3000000: return B3000000;
#endif
#ifdef B3500000
        case 3500000: return B3500000;
#endif
#ifdef B4000000
        case 4000000: return B4000000;
#endif
        default: return 0; // Custom baud rate
    }
}

// Helper function to check if a file descriptor is a PTY
static int is_pty(int fd)
{
    return isatty(fd);
}

void tiny_serial_close(tiny_serial_handle_t port)
{
    if ( port >= 0 )
    {
        close(port);
    }
}

tiny_serial_handle_t tiny_serial_open(const char *name, uint32_t baud)
{
    struct termios options;
    struct termios oldt;
    
    // Auto-convert tty. to cu. for macOS real serial devices
    char alt_name[256];
    if (strstr(name, "/dev/tty.") != NULL) {
        snprintf(alt_name, sizeof(alt_name), "/dev/cu.%s", name + 9);
        name = alt_name;
    }

    int fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if ( fd == -1 )
    {
        perror("ERROR: Failed to open serial device");
        return TINY_SERIAL_INVALID;
    }
    
    // Set back to blocking mode after successful open
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    if ( tcgetattr(fd, &oldt) == -1 )
    {
        close(fd);
        return TINY_SERIAL_INVALID;
    }
    options = oldt;
    cfmakeraw(&options);

    options.c_lflag &= ~ICANON;
    options.c_lflag &= ~(ECHO | ECHOCTL | ECHONL);
    options.c_cflag |= HUPCL;
    options.c_cflag |= (CLOCAL | CREAD);  // Enable receiver and local mode

    options.c_oflag &= ~ONLCR; /* set NO CR/NL mapping on output */
    options.c_iflag &= ~ICRNL; /* set NO CR/NL mapping on input */

    // Disable ALL flow control
    options.c_cflag &= ~CRTSCTS;
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1; // 100 ms

    // Set baud rate
    speed_t speed = baud_to_speed(baud);
    int baud_set = 0;
    
    if (speed != 0) {
        // Standard baud rate - use termios functions
        if (cfsetispeed(&options, speed) == 0 && cfsetospeed(&options, speed) == 0) {
            baud_set = 1;
        }
    }

    // Set the new options for the port...
    if ( tcsetattr(fd, TCSANOW, &options) == -1 )
    {
        close(fd);
        return TINY_SERIAL_INVALID;
    }
    
    // If we couldn't set a standard baud rate, try custom rate
    if (!baud_set) {
        speed_t custom_speed = baud;
        if ( ioctl(fd, IOSSIOSPEED, &custom_speed) == -1 )
        {
            fprintf(stderr, "WARNING: Failed to set custom baud rate %u: %s\n", baud, strerror(errno));
            // Don't close - some devices don't support this but still work
        }
    }
    
    // Turn off DTR and RTS (only for real serial ports, not PTYs)
    if (!is_pty(fd)) {
        int status;
        if (ioctl(fd, TIOCMGET, &status) == 0) {
            status &= ~TIOCM_DTR;
            status &= ~TIOCM_RTS;
            ioctl(fd, TIOCMSET, &status);
        }
    }

    // Flush any buffered characters
    tcflush(fd, TCIOFLUSH);

    return fd;
}

int tiny_serial_send(tiny_serial_handle_t port, const void *buf, int len)
{
    return tiny_serial_send_timeout(port, buf, len, 100);
}

int tiny_serial_send_timeout(tiny_serial_handle_t port, const void *buf, int len, uint32_t timeout_ms)
{
    int ret;
    struct pollfd fds = {.fd = port, .events = POLLOUT | POLLWRNORM};
write_poll:
    ret = poll(&fds, 1, timeout_ms);
    if ( ret < 0 )
    {
        if ( errno == EINTR )
        {
            goto write_poll;
        }
        return ret;
    }
    if ( ret == 0 || !(fds.revents & (POLLOUT | POLLWRNORM)) )
    {
        return 0;
    }
    ret = write(port, buf, len);
    if ( (ret < 0) && (errno == EAGAIN || errno == EINTR) )
    {
        return 0;
    }
    if ( ret > 0 )
    {
#if DEBUG_SERIAL_TX == 1
        struct timeval tv;
        gettimeofday(&tv, NULL);
        for ( int i = 0; i < ret; i++ )
            printf("%08llu: TX: 0x%02X '%c'\n", tv.tv_usec / 1000ULL + tv.tv_sec * 1000ULL,
                   (uint8_t)(((const char *)buf)[i]), ((const char *)buf)[i]);
#endif
    }
    return ret;
}

int tiny_serial_read(tiny_serial_handle_t port, void *buf, int len)
{
    return tiny_serial_read_timeout(port, buf, len, 100);
}

int tiny_serial_read_timeout(tiny_serial_handle_t port, void *buf, int len, uint32_t timeout_ms)
{
    struct pollfd fds = {.fd = port, .events = POLLIN | POLLRDNORM};
    int ret;
read_poll:
    ret = poll(&fds, 1, timeout_ms);
    if ( ret < 0 )
    {
        if ( errno == EINTR )
        {
            goto read_poll;
        }
        return ret;
    }
    if ( ret == 0 || !(fds.revents & (POLLIN | POLLRDNORM)) )
    {
        return 0;
    }
    ret = read(port, buf, len);
    if ( (ret < 0) && (errno == EAGAIN || errno == EINTR) )
    {
        return 0;
    }
    if ( ret > 0 )
    {
#if DEBUG_SERIAL_RX == 1
        struct timeval tv;
        gettimeofday(&tv, NULL);
        for ( int i = 0; i < ret; i++ )
            printf("%08llu: RX: 0x%02X '%c'\n", tv.tv_usec / 1000ULL + tv.tv_sec * 1000ULL,
                   (uint8_t)(((char *)buf)[i]), ((char *)buf)[i]);
#endif
    }
    return ret;
}