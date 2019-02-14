#include <iostream>
// #include <fstream>
#include <stdio.h>      // standard input / output functions
#include <stdlib.h>
#include <string.h>     // string function definitions
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions

void ReadAll(int USB) {
  std::cout<<"R:"<<std::endl;
  char buffer[24];
  int n = 0;
  do {
    memset(buffer, '\0', sizeof buffer);
    n = read( USB, &buffer, 17 );
    std::cout<<buffer<<std::endl;
  // printf("%s\n", buffer);
  } while (n == 17);
}

void ReadAll2(int USB) {
  int n = 0,
      spot = 0;
  char buf = '\0';

  /* Whole response*/
  char response[1024];
  memset(response, '\0', sizeof response);

  do {
    n = read( USB, &buf, 1 );
    sprintf( &response[spot], "%c", buf );
    spot += n;
  } while( buf != 'T' && n > 0);

  if (n < 0) {
    std::cout << "Error reading: " << strerror(errno) << std::endl;
  }
  else if (n == 0) {
    std::cout << "Read nothing!" << std::endl;
  }
  else {
    std::cout << "Response: " << response << std::endl;
  }
}
void Config(int USB) {
    struct termios tty;
    struct termios tty_old;
    memset (&tty, 0, sizeof tty);

    /* Error Handling */
    if ( tcgetattr ( USB, &tty ) != 0 ) {
      std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
    }

    /* Save old tty parameters */
    tty_old = tty;

    // cfmakeraw(&tty);

    /* Set Baud Rate */
    cfsetospeed (&tty, (speed_t)B9600);
    cfsetispeed (&tty, (speed_t)B9600);

    /* Setting other Port Stuff */
    tty.c_cflag     &=  ~CSTOPB;
    tty.c_cflag     &=  ~CRTSCTS;           // no flow control
    tty.c_cc[VMIN]   =  1;                  // read doesn't block
    tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
    tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

    /* Make raw */
    cfmakeraw(&tty);

    /* Flush Port, then applies attributes */
    tcflush( USB, TCIFLUSH );
    if ( tcsetattr ( USB, TCSANOW, &tty ) != 0) {
         std::cout << "Error " << errno << " from tcsetattr" << std::endl;
    }
}


void ReadWait(int USB) {
  for (int i = 0; i < 5; ++i)
    ReadAll2(USB);
}

int main () {
    const char *kPumpOnString  =   "L1                 ";
    const char *kPumpOffString =   "L0                 ";
    const char *kReconnectString = "M                  ";
    // printf("size of string: %u\n", sizeof kPumpOnString);
    // return 0;
    // const char *kPumpOffString = "L0                 ";
    // std::string str;
    // std::fstream f;
    // f.open("/dev/ttyUSB1");
    // int USB = open( "/dev/ttyUSB0", O_RDWR| O_NOCTTY );
    int USB = open( "/dev/ttyUSB0", O_RDWR);
    Config(USB);
    //
  // while(1) {
      // ReadAll2(USB);
      // usleep(10000);
    // }
    ReadAll(USB);
       printf("Reconnect\n");
      int n_written = write( USB, kReconnectString, 19 );
      if (n_written != 19) {
        printf("Failed to write!\n");
        return -1;
      }
      // It takes about 1ms per char!!!!
    while (1) {
      usleep(50000);
    // ReadAll2(USB);
       printf("Pump on\n");
      n_written = write( USB, kPumpOnString, 19 );
      if (n_written != 19) {
        printf("Failed to write!\n");
        return -1;
      }
      usleep(1000000);
      ReadAll(USB);
    // ReadAll2(USB);
       printf("Pump off\n");
      n_written = write( USB, kPumpOffString, 19);
      if (n_written != 19) {
        printf("Failed to write!\n");
        return -1;
      }
    }
    return 0;
}
