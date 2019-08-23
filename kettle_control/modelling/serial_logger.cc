
#include <fstream>
#include <stdio.h>      // standard input / output functions
#include <stdlib.h>
#include <string.h>     // string function definitions
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions
#include <iostream>

static constexpr char kStartChar = 'T';
int fd_;
static constexpr unsigned kStatusLength = 13; // includes \n

int Connect(const char *path) {
  fd_ = open(path, O_RDWR | O_NOCTTY); //TODO: use O_SYNC?
  if (fd_ <= 0) {
    printf("Failed to open serial device %s\n", path);
    return -1;
  }
  // Now configure:
  struct termios tty;
  if (tcgetattr ( fd_, &tty ) != 0 ) {
    std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
    return -1;
  }

  // Set Baud Rate
  cfsetospeed (&tty, (speed_t)B57600);
  cfsetispeed (&tty, (speed_t)B57600);

  // Setting other Port Stuff
  tty.c_cflag     &=  ~CSTOPB;
  tty.c_cflag     &=  ~CRTSCTS;           // no flow control
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag &= ~CRTSCTS;
 /* enable receiver, ignore status lines */
 tty.c_cflag |= CREAD | CLOCAL;
 /* disable input/output flow control, disable restart chars */
 tty.c_iflag &= ~(IXON | IXOFF | IXANY);
 /* disable canonical input, disable echo,
 disable visually erase chars,
 disable terminal-generated signals */
 tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
 /* disable output processing */
 tty.c_oflag &= ~OPOST;
  tty.c_cc[VMIN]   =  1;                  // read doesn't block
  tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
  tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

  /* Make raw */
  cfmakeraw(&tty);

  /* Flush Port, then applies attributes */
  tcflush( fd_, TCIFLUSH );
  if (tcsetattr ( fd_, TCSANOW, &tty ) != 0) {
    std::cout << "Error " << errno << " from tcsetattr" << std::endl;
    return -1;
  }
  return 0;
}

int main() {
  if (Connect("/dev/ttyACM0")) {
    printf("Failed to connect.\n");
    return -1;
  }
  while (true) {
    // Read until we get to the start bit: 'T'
    int first_byte = '\0';
    int current_read;
    do {
      current_read = read(fd_, &first_byte, 1);
      // std::cout<<"."<<std::endl;
      if (current_read < 0) {
        printf("Failed to Read A\n");
        break;
      }
    } while (first_byte != (int)kStartChar);
    if (current_read < 0) {
        printf("Failed to Read for start char\n");
      // If we are having read problems, raise flag and keep trying
      continue;
    }
    char ret[kStatusLength + 1];
    ret[0] = kStartChar;
    unsigned chars_read = 1;
    do {
      current_read = read(fd_, ret + chars_read, kStatusLength - chars_read);
      // std::cout<<"."<<std::endl;
      if (current_read < 0) {
        printf("Failed to Read B\n");
        break;
      }
      if (ret[chars_read] == '\n') {
        printf("got return\n");
        break;
      }
      chars_read += current_read;
    } while (chars_read < kStatusLength);
    if (current_read < 0) {
        printf("Failed to Read C\n");
      // If we are having read problems, raise flag and keep trying
      continue;
    }
    ret[chars_read] = '\0';
    // Now we have the correct number of chars, aligned correctly.
    // See if it parses:
  char values[40];
  time_t t = time(NULL);
  struct tm *tmp = localtime(&t);
  strftime(values, 40, "%D %T %s ->  ", tmp);
  // std::cout << values << ret << std::endl;


  std::fstream raw_log_file;
  raw_log_file.open("./rawfile", std::fstream::out | std::fstream::app);
  if(!raw_log_file.is_open()) {
    printf("Failed to open raw log file at %s\n", "./rawfile");
    return -1;
  }
  raw_log_file << values << ret << std::endl;
  raw_log_file.close();
  } // end while

  return 0;
}
