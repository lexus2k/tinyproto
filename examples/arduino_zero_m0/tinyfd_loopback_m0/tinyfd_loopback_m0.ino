/*
 * This example sends back every buffer received over UART.
 *
 * !README!
 * The sketch is developed to perform UART tests between Arduino
 * and PC.
 * 1. Burn this program to Arduino
 * 2. Compile tiny_loopback tool (see tools folder) for your system
 * 3. Connect Arduino TX and RX lines to your PC com port
 * 4. Run tiny_loopback on the PC (use correct port name on your system)
 * 5. tiny_loopback will print the test speed results
 *
 * Also, this example demonstrates how to pass data between 2 systems
 * By default the sketch and tiny_loopback works as 115200 speed.
 */

/* !!! WARNING !!! THIS SKETCH ONLY FOR ARDUINO ZERO/M0 !!! */

#define HAVE_SERIALUSB
#include <TinyProtocol.h>
// We need this hack for very small controllers.
#include <proto/fd/tiny_fd_int.h>

/* Creating protocol object is simple. Lets define 64 bytes as maximum. *
 * size for the packet and use 4 packets in outgoing queue.             */
Tiny::ProtoFd<FD_MIN_BUF_SIZE(64,4)>  proto;

void onReceive(void *_, Tiny::IPacket &pkt)
{
    if ( proto.write(pkt) == TINY_ERR_TIMEOUT )
    {
        // Do what you need to do if there is no place to put new frame to.
        // But never use blocking operations inside callback
    }
}

void setup()
{
    /* No timeout, since we want non-blocking UART operations. */
    SerialUSB.setTimeout(0);
    /* Initialize serial protocol for test purposes */
    SerialUSB.begin(115200);
    /* Lets use 8-bit checksum, available on all platforms */
    proto.enableCheckSum();
    /* Lets process all incoming frames */
    proto.setReceiveCallback( onReceive );
    /* Redirect all protocol communication to SerialUSB UART */
    proto.beginToSerialUSB();
}

void loop()
{
    if (SerialUSB.available())
    {
        proto.run_rx();
    }
    proto.run_tx();
}
